#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "UI/RoguePresentationTheme.h"
#include "UI/RogueHUD.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeaponCatalog.h"
#include "Framework/RogueGameState.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

bool RogueOwnedItems::IsWeaponType(ERogueUpgradeType Type)
{ return RogueWeaponCatalog::IsWeaponQuantity(Type); }
TArray<FRogueOwnedItemView> RogueOwnedItems::BuildView(const ARogueCharacter* Player,const ARoguePlayerState* State)
{
    TArray<FRogueOwnedItemView> Result;if(!Player||!State)return Result;
    TMap<ERogueUpgradeType,const ARogueWeaponBase*> Weapons;
    for(const ARogueWeaponBase* Weapon:Player->GetWeapons())if(IsValid(Weapon)&&Weapon->GetEffectiveCount()>0)Weapons.Add(Weapon->GetCountUpgradeType(),Weapon);
    auto AddWeapon=[&](ERogueUpgradeType Type)
    {
        if(Result.ContainsByPredicate([Type](const FRogueOwnedItemView& V){return V.Type==Type;}))return;
        if(const auto* const* Found=Weapons.Find(Type))
        {FRogueOwnedItemView V;V.Type=Type;V.bWeapon=true;V.Count=(*Found)->GetEffectiveCount();V.PickCount=State->GetUpgradePickCount(Type);V.Name=FText::FromString((*Found)->GetConfig().DisplayName);Result.Add(V);}
    };
    // Initial loadout is present before any picks, including editor-configured initial quantities.
    for(const ARogueWeaponBase* Weapon:Player->GetWeapons())if(IsValid(Weapon)&&Weapon->GetCount()>0)AddWeapon(Weapon->GetCountUpgradeType());
    for(const auto& Pick:State->GetUpgradePickViews())
    {
        if(Pick.Count<=0)continue;
        if(IsWeaponType(Pick.Type)){AddWeapon(Pick.Type);continue;}
        FRogueOwnedItemView V;V.Type=Pick.Type;V.Count=V.PickCount=Pick.Count;V.Name=StaticEnum<ERogueUpgradeType>()->GetDisplayNameTextByValue(int64(Pick.Type));Result.Add(V);
    }
    // Accommodate debug grants and property replication arriving before the pick mirror.
    for(const ARogueWeaponBase* Weapon:Player->GetWeapons())if(IsValid(Weapon))AddWeapon(Weapon->GetCountUpgradeType());
    return Result;
}
void URogueOwnedItemWidget::SetItem(const FRogueOwnedItemView& Item, const URoguePresentationTheme* Theme, int32 DisplayCount)
{
    const bool bNew=!bInitialized;
    const int32 VisualCount = DisplayCount == INDEX_NONE ? Item.Count : FMath::Max(0, DisplayCount);
    const bool bCountChanged = bInitialized && DisplayedCount != VisualCount;
    const bool bRevealed = DisplayedCount <= 0 && VisualCount > 0;
    const bool bIconChanged=bNew||View.Type!=Item.Type||View.bWeapon!=Item.bWeapon;
    View=Item;bInitialized=true;DisplayedCount=VisualCount;
    if(bIconChanged&&ItemIcon)
    {
        auto* Material=Theme?(Item.bWeapon?Theme->GetWeaponIcon(Item.Type):Theme->GetCardIcon(Item.Type)):nullptr;
        if(Material)ItemIcon->SetBrushFromMaterial(Material);
    }
    if((bNew||bCountChanged)&&StackText)StackText->SetText(FText::AsNumber(FMath::Max(1,VisualCount)));
    if(bNew || bRevealed){AppearanceAge=0;bAnimating=true;if(!bBlueprintHandlesMotion)SetRenderOpacity(0);}
    if(bCountChanged){CountAge=0;bAnimating=true;}
    OnItemUpdated(Item);
}
void URogueOwnedItemWidget::PlayAcquisitionPulse()
{
    CountAge = 0.f;
    Velocity = FMath::Max(-130.f, Velocity - 85.f);
    bAnimating = true;
}

void URogueOwnedItemWidget::PlayAttackPulse()
{
    if(!View.bWeapon)return;
    OnAttackPulse();if(bBlueprintHandlesMotion)return;
    // Add bounded velocity to the current spring, so rapid fire never snaps the icon to its base.
    Velocity=FMath::Max(-130.f,Velocity-85.f);bAnimating=true;
}
void URogueOwnedItemWidget::AdvanceMotion(float Delta)
{
    if(bBlueprintHandlesMotion||!bAnimating)return;
    const float DT=FMath::Clamp(Delta,0.f,.12f);AppearanceAge+=DT;CountAge+=DT;
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(DT/.016f));const float Step=DT/Steps;
    for(int32 I=0;I<Steps;++I)
    {Velocity+=(-210.f*Offset-24.f*Velocity)*Step;Offset=FMath::Clamp(Offset+Velocity*Step,-HopHeight,1.5f);}
    if(ItemIcon){ItemIcon->SetRenderTranslation(FVector2D(0,Offset));ItemIcon->SetRenderScale(FVector2D(1+FMath::Max(0.f,-Offset)*.004f));}
    if(StackText)StackText->SetRenderScale(FVector2D(1+.16f*(1-FMath::SmoothStep(0.f,.22f,CountAge))));
    SetRenderOpacity(FMath::SmoothStep(0.f,.18f,AppearanceAge));
    if(AppearanceAge>.18f&&CountAge>.22f&&FMath::Abs(Offset)<.025f&&FMath::Abs(Velocity)<.1f)
    {Offset=Velocity=0;bAnimating=false;if(ItemIcon){ItemIcon->SetRenderTranslation(FVector2D::ZeroVector);ItemIcon->SetRenderScale(FVector2D(1));}if(StackText)StackText->SetRenderScale(FVector2D(1));SetRenderOpacity(1);}
}
void URogueOwnedItemsWidget::NativeConstruct()
{Super::NativeConstruct();SetVisibility(ESlateVisibility::HitTestInvisible);RefreshAge=1;}
void URogueOwnedItemsWidget::RefreshItems()
{
    auto* PC=GetOwningPlayer();const auto* State=PC?PC->GetPlayerState<ARoguePlayerState>():nullptr;const auto* Player=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;
    const auto* Game=GetWorld()?GetWorld()->GetGameState<ARogueGameState>():nullptr;
    const auto* HUD = PC ? Cast<ARogueHUD>(PC->GetHUD()) : nullptr;
    const bool bVisible=Player&&State&&(!Game||!Game->IsGameOver())&&(!HUD||!HUD->IsPauseMenuOpen());SetVisibility(bVisible?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if (!Player || !State) return;
    if (!bVisible)
    {
        // Settlement keeps the last real inventory even if its Pawn is subsequently destroyed.
        Items = RogueOwnedItems::BuildView(Player, State);
        return;
    }
    if(SourceState.Get()!=State){SourceState=const_cast<ARoguePlayerState*>(State);PendingAttacks.Reset();}
    SetRenderOpacity(Player->IsAlive()?1.f:.5f);ApplyItems(RogueOwnedItems::BuildView(Player,State));
}
void URogueOwnedItemsWidget::ApplyItems(const TArray<FRogueOwnedItemView>& NewItems)
{
    if(!ItemsCanvas||!ItemWidgetClass)return;
    const auto UpdateTile = [&](URogueOwnedItemWidget* Tile, const FRogueOwnedItemView& Item)
    {
        const int32* HeldCount = DeferredCounts.Find(Item.Type);
        const int32 VisualCount = HeldCount ? *HeldCount : Item.Count;
        if (Tile->View.Count != Item.Count || Tile->View.PickCount != Item.PickCount ||
            Tile->GetDisplayedCount() != VisualCount)
            Tile->SetItem(Item, PresentationTheme, VisualCount);
        Tile->SetVisibility(VisualCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    };
    bool bOrderChanged=Items.Num()!=NewItems.Num();
    if(!bOrderChanged)for(int32 I=0;I<Items.Num();++I)if(Items[I].Type!=NewItems[I].Type||Items[I].bWeapon!=NewItems[I].bWeapon){bOrderChanged=true;break;}
    if(bOrderChanged)
    {
        auto Previous=Tiles;Tiles.Reset();
        for(const auto& Item:NewItems)
        {
            auto* Found=Previous.FindByPredicate([&](const auto& Tile){return Tile&&Tile->View.Type==Item.Type&&Tile->View.bWeapon==Item.bWeapon;});
            URogueOwnedItemWidget* Tile=Found?Found->Get():CreateWidget<URogueOwnedItemWidget>(GetOwningPlayer(),ItemWidgetClass);
            if(!Tile)continue;if(!Found)ItemsCanvas->AddChildToCanvas(Tile);
            UpdateTile(Tile, Item);Tiles.Add(Tile);
        }
        for(URogueOwnedItemWidget* Old:Previous)if(Old&&!Tiles.Contains(Old))Old->RemoveFromParent();
    }
    else for(int32 I=0;I<Tiles.Num();++I)if(NewItems.IsValidIndex(I))UpdateTile(Tiles[I], NewItems[I]);
    Items=NewItems;
    if(bOrderChanged)Arrange(FMath::Max(640.f,LastWidth));
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0;
    for(auto It=PendingAttacks.CreateIterator();It;++It)
    {
        auto* Tile=Tiles.FindByPredicate([&](const auto& T){return T&&T->View.bWeapon&&T->View.Type==It.Key();});
        if(Tile){(*Tile)->PlayAttackPulse();It.RemoveCurrent();}else if(Now>It.Value())It.RemoveCurrent();
    }
}
void URogueOwnedItemsWidget::NotifyWeaponAttack(ERogueUpgradeType Type)
{
    if(!RogueOwnedItems::IsWeaponType(Type))return;
    if(auto* Tile=Tiles.FindByPredicate([Type](const auto& T){return T&&T->View.bWeapon&&T->View.Type==Type;}))(*Tile)->PlayAttackPulse();
    else PendingAttacks.Add(Type,(GetWorld()?GetWorld()->GetTimeSeconds():0)+.5);
}
void URogueOwnedItemsWidget::RememberCardOrigin(const FGuid& ReceiptId, ERogueUpgradeType Type, const UWidget* Card)
{
    if (!ReceiptId.IsValid() || !Card || ConfirmedReceipts.Contains(ReceiptId)) return;
    const FGeometry& Geometry = GetPaintSpaceGeometry();
    const FVector2D ViewSize = Geometry.GetLocalSize();
    const FGeometry& CardGeometry = Card->GetPaintSpaceGeometry();
    if (ViewSize.X <= 0.f || ViewSize.Y <= 0.f || CardGeometry.GetLocalSize().IsNearlyZero()) return;

    const FVector2D Center = CardGeometry.LocalToAbsolute(CardGeometry.GetLocalSize() * .5f);
    const FVector2D LocalCenter = Geometry.AbsoluteToLocal(Center);
    CardOrigins.Add(ReceiptId, {Type, LocalCenter / ViewSize, 0.f});
    if (!DeferredCounts.Contains(Type))
    {
        const auto* Tile = Tiles.FindByPredicate([Type](const auto& Item) { return Item && Item->View.Type == Type; });
        DeferredCounts.Add(Type, Tile ? (*Tile)->GetDisplayedCount() : 0);
    }
}

void URogueOwnedItemsWidget::ConfirmCardAcquired(ERogueUpgradeType Type, const FGuid& ReceiptId, int32 PickCount, int32 DisplayCount)
{
    if (!AcquisitionIcon || !ReceiptId.IsValid() || PickCount <= 0 || ConfirmedReceipts.Contains(ReceiptId)) return;
    ConfirmedReceipts.Add(ReceiptId);
    FCardOrigin Origin{Type, FVector2D(.5f, .62f)};
    CardOrigins.RemoveAndCopyValue(ReceiptId, Origin);
    AcquisitionQueue.Add({Type, Origin.NormalizedPosition, PickCount, FMath::Max(0, DisplayCount), 0.f});
}

void URogueOwnedItemsWidget::FinishAcquisition(bool bArrived)
{
    if (AcquisitionQueue.IsEmpty()) return;
    const FCardFlight Finished = AcquisitionQueue[0];
    auto* Target = AcquisitionTarget.Get();
    AcquisitionTarget.Reset();
    AcquisitionQueue.RemoveAt(0);
    const bool bMoreFlights = AcquisitionQueue.ContainsByPredicate([&Finished](const FCardFlight& Flight)
    {
        return Flight.Type == Finished.Type;
    });
    bool bMoreRequests = false;
    for (const auto& Origin : CardOrigins) bMoreRequests |= Origin.Value.Type == Finished.Type;
    if (bArrived && (bMoreFlights || bMoreRequests)) DeferredCounts.Add(Finished.Type, Finished.DisplayCount);
    else if (!bMoreFlights && !bMoreRequests) DeferredCounts.Remove(Finished.Type);

    AcquisitionIcon->SetVisibility(ESlateVisibility::Collapsed);
    ApplyItems(Items);
    if (bArrived && Target) Target->PlayAcquisitionPulse();
}

void URogueOwnedItemsWidget::TickAcquisition(const FGeometry& Geometry, float Delta)
{
    if (!AcquisitionIcon || !ItemsCanvas || !PresentationTheme) return;
    // Requests can be rejected or time out. They never animate or hold a stale display count indefinitely.
    TArray<ERogueUpgradeType> ExpiredTypes;
    for (auto It = CardOrigins.CreateIterator(); It; ++It)
    {
        It.Value().Age += FMath::Max(0.f, Delta);
        if (It.Value().Age <= 10.f) continue;
        ExpiredTypes.AddUnique(It.Value().Type);
        It.RemoveCurrent();
    }
    for (ERogueUpgradeType Type : ExpiredTypes)
    {
        bool bPending = AcquisitionQueue.ContainsByPredicate([Type](const FCardFlight& Flight) { return Flight.Type == Type; });
        for (const auto& Origin : CardOrigins) bPending |= Origin.Value.Type == Type;
        if (!bPending) DeferredCounts.Remove(Type);
    }

    auto* IconSlot = Cast<UCanvasPanelSlot>(AcquisitionIcon->Slot);
    if (!IconSlot || AcquisitionQueue.IsEmpty()) return;
    auto& Flight = AcquisitionQueue[0];
    auto* Target = AcquisitionTarget.Get();
    if (!Target)
    {
        const auto* Tile = Tiles.FindByPredicate([&Flight](const auto& Item)
        {
            return Item && Item->View.Type == Flight.Type && Item->View.PickCount >= Flight.PickCount;
        });
        if (Tile && PresentationTheme->GetCardIcon(Flight.Type))
        {
            Target = Tile->Get();
            AcquisitionTarget = Target;
            AcquisitionIcon->SetBrushFromMaterial(PresentationTheme->GetCardIcon(Flight.Type));
            AcquisitionAge = 0.f;
        }
        else
        {
            Flight.WaitAge += FMath::Max(0.f, Delta);
            if (Flight.WaitAge > 5.f) FinishAcquisition(false);
            return;
        }
    }

    const auto* TargetSlot = Cast<UCanvasPanelSlot>(Target->Slot);
    if (!TargetSlot) { FinishAcquisition(false); return; }
    AcquisitionIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    AcquisitionAge += FMath::Clamp(Delta, 0.f, .05f);
    constexpr float Duration = .65f;
    if (AcquisitionAge >= Duration) { FinishAcquisition(true); return; }

    const FVector2D View = Geometry.GetLocalSize();
    const FVector2D Destination = TargetSlot->GetPosition() - .5f * TargetSlot->GetSize();
    const FVector2D Origin = Flight.NormalizedOrigin * View - View;
    const float Fly = FMath::SmoothStep(.06f, Duration, AcquisitionAge);
    const float Lift = FMath::Sin(PI * Fly) * 75.f;
    IconSlot->SetPosition(FMath::Lerp(Origin, Destination, Fly) - FVector2D(0.f, Lift));
    const float Size = FMath::Lerp(84.f, static_cast<float>(TargetSlot->GetSize().X), Fly);
    IconSlot->SetSize(FVector2D(Size));
    AcquisitionIcon->SetRenderOpacity(FMath::SmoothStep(0.f, .07f, AcquisitionAge));
    AcquisitionIcon->SetRenderTransformAngle(-9.f * FMath::Sin(PI * Fly));
}

void URogueOwnedItemsWidget::SetVitalsExclusionRightEdge(float RightEdge)
{
    if (!FMath::IsFinite(RightEdge) || FMath::IsNearlyEqual(VitalsExclusionRightEdge,RightEdge,1.f)) return;
    VitalsExclusionRightEdge=FMath::Max(0.f,RightEdge);
    if (LastWidth>0) Arrange(LastWidth);
}

void URogueOwnedItemsWidget::Arrange(float Width)
{
    LastWidth=Width;const float Size=FMath::Clamp(IconSize,32.f,80.f),Stride=Size+FMath::Clamp(Gap,0.f,20.f);
    const int32 Columns=FMath::Clamp(FMath::FloorToInt((Width-FMath::Max(ReservedLeftWidth,VitalsExclusionRightEdge)-RightPadding)/Stride),1,FMath::Clamp(MaxColumns,4,14));
    const int32 Rows=FMath::DivideAndRoundUp(Tiles.Num(),Columns);OccupiedHeight=Rows*Stride;
    for(int32 I=0;I<Tiles.Num();++I)if(auto* TileSlot=Cast<UCanvasPanelSlot>(Tiles[I]->Slot))
    {TileSlot->SetAnchors(FAnchors(1,1));TileSlot->SetAlignment(FVector2D(1,1));TileSlot->SetPosition(FVector2D(-RightPadding-(I%Columns)*Stride,-BottomPadding-(I/Columns)*Stride));TileSlot->SetSize(FVector2D(Size,Size));}
}
void URogueOwnedItemsWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta);const float Width=Geometry.GetLocalSize().X;
    if(Width>0&&!FMath::IsNearlyEqual(Width,LastWidth,1.f))Arrange(Width);
    for(URogueOwnedItemWidget* Tile:Tiles)if(Tile)Tile->AdvanceMotion(Delta);
    TickAcquisition(Geometry, Delta);
}
