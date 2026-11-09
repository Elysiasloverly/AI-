#include "RogueEditorAssetUtils.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "UnrealClient.h"

FIntPoint URogueEditorAssetUtils::ResizePreviewWindow(APlayerController* Owner,int32 Width,int32 Height)
{
    if(!Owner||!Owner->GetWorld())return FIntPoint::ZeroValue;
    auto* View=Owner->GetWorld()->GetGameViewport();if(!View)return FIntPoint::ZeroValue;
    if(auto Window=View->GetWindow();Window.IsValid())Window->Resize(FVector2D(Width,Height));
    return View->Viewport?View->Viewport->GetSizeXY():FIntPoint::ZeroValue;
}
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "GameFramework/PlayerController.h"

ARogueCombatAccentEffect* URogueEditorAssetUtils::PlayPreviewAccent(UObject* Context,const FRogueCombatAccentSettings& Settings,FVector Location,FVector Direction,bool bImpact)
{
    UWorld* World=IsValid(Context)?Context->GetWorld():nullptr;
    if(!World||!World->IsGameWorld())return nullptr;
    auto* Pool=World->GetSubsystem<URogueCombatPoolSubsystem>();
    return Pool?Pool->PlayCombatAccent(Settings,Location,Direction,bImpact):nullptr;
}

bool URogueEditorAssetUtils::InjectPreviewInput(APlayerController* Controller,UInputAction* Action,FVector2D Axis)
{
    if(!Controller||!Action||!Controller->GetWorld()||!Controller->GetWorld()->IsGameWorld())return false;
    auto* Local=Controller->GetLocalPlayer();
    auto* Input=Local?ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Local):nullptr;
    if(!Input)return false;
    Input->InjectInputForAction(Action,FInputActionValue(Axis),{},{});return true;
}

#include "Engine/StaticMesh.h"
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UObjectIterator.h"

void URogueEditorAssetUtils::RefreshMapCollision()
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    for(TObjectIterator<UStaticMeshComponent> It;It;++It)
    {
        if(It->IsRegistered())It->RecreatePhysicsState();
    }
}

#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"

int32 URogueEditorAssetUtils::MigrateNativeMapDefaults(const FString& SourcePath,const FString& DestinationPath)
{
    if(!SourcePath.StartsWith(TEXT("/Game/Map/"))||!DestinationPath.StartsWith(TEXT("/Game/Map/")))return -1;
    int32 Count=0;
    FSoftObjectPathFixupArchive Archive([&](FSoftObjectPath& Path)
    {
        if(Path.GetLongPackageName()==SourcePath)
        {
            const FString Name=FPackageName::GetShortName(DestinationPath);
            Path=FSoftObjectPath(DestinationPath+TEXT(".")+Name);++Count;
        }
    });
    for(TObjectIterator<UClass> It;It;++It)
    {
        UClass* Class=*It;
        if(Class->ClassGeneratedBy==nullptr&&Class->ClassDefaultObject)Class->ClassDefaultObject->Serialize(Archive);
    }
    return Count;
}

#include "PhysicsEngine/BodySetup.h"

int32 URogueEditorAssetUtils::CopyMapCollision(UStaticMesh* Source,UStaticMesh* Destination)
{
    if(!Source||!Destination||Source==Destination||!Source->GetBodySetup())return -1;
    Destination->Modify();Destination->CreateBodySetup();
    auto* Body=Destination->GetBodySetup();Body->Modify();
    Body->AggGeom=Source->GetBodySetup()->AggGeom;
    Body->CollisionTraceFlag=Source->GetBodySetup()->CollisionTraceFlag;
    Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Destination->MarkPackageDirty();
    return Body->AggGeom.ConvexElems.Num();
}
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"

#include "EditorAssetLibrary.h"
#include "Engine/CurveTable.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Framework/RogueGameMode.h"

FString URogueEditorAssetUtils::InspectEnemyPreview(ARogueEnemy* Enemy)
{
    if (!IsValid(Enemy)) { return TEXT("{}"); }
    const auto* Member = Enemy->GetRogueAttributeSet();
    const auto* Default = Enemy->GetDefaultSubobjectByName(TEXT("AttributeSet"));
    const auto* ASC = Enemy->GetAbilitySystemComponent();
    const auto* Set = ASC ? ASC->GetSet<URogueAttributeSet>() : nullptr;
    return FString::Printf(TEXT("{\"member_present\":%s,\"default_present\":%s,\"asc_present\":%s,\"gas_health\":%.6f,\"gas_max_health\":%.6f,\"mirror_health\":%.6f,\"boss\":%s,\"dead\":%s,\"pooled\":%s,\"spawn_id\":\"%s\",\"world_type\":%d,\"member_name\":\"%s\",\"default_name\":\"%s\",\"asc_attribute_name\":\"%s\"}"),
        Member ? TEXT("true") : TEXT("false"), Default ? TEXT("true") : TEXT("false"), Set ? TEXT("true") : TEXT("false"),
        Set ? Set->GetHealth() : -1.f, Set ? Set->GetMaxHealth() : -1.f, Enemy->GetCurrentHealth(),
        Enemy->IsBoss() ? TEXT("true") : TEXT("false"), Enemy->IsDead() ? TEXT("true") : TEXT("false"),
        Enemy->IsAvailableInPool() ? TEXT("true") : TEXT("false"), *Enemy->GetSpawnCycleId().ToString(),
        Enemy->GetWorld() ? int32(Enemy->GetWorld()->WorldType) : -1,
        *GetNameSafe(Member), *GetNameSafe(Default), *GetNameSafe(Set));
}

bool URogueEditorAssetUtils::ApplyPreviewDamage(AActor* Source, AActor* Target, float Damage,int32 HitVisualStyle)
{
    return IsValid(Target) && Target->GetWorld() && Target->GetWorld()->IsGameWorld()
        && RogueAbilitySystem::ApplyDamageToTarget(Source, Target, Damage, FGameplayTag(),nullptr,uint8(FMath::Clamp(HitVisualStyle,0,5)));
}
bool URogueEditorAssetUtils::SetPreviewArmor(ARogueCharacter* P,float Maximum,float Current)
{
    if(!IsValid(P)||!P->GetWorld()->IsGameWorld()||!P->HasAuthority()||!P->GetAbilitySystemComponent())return false;
    auto* ASC=P->GetAbilitySystemComponent();ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxArmorAttribute(),FMath::Max(0.f,Maximum));
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetArmorAttribute(),FMath::Clamp(Current,0.f,FMath::Max(0.f,Maximum)));return true;
}
TArray<AActor*> URogueEditorAssetUtils::SpawnOrbitPreview(ARogueCharacter* P,TSubclassOf<ARogueReturningTalisman> TC,TSubclassOf<ARogueOrbitingBlade> BC,int32 Talismans,int32 Blades)
{
    TArray<AActor*> Out;if(!IsValid(P)||!P->GetWorld()->IsGameWorld()||!P->HasAuthority())return Out;
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for(int32 I=0;TC&&I<FMath::Clamp(Talismans,0,12);++I)
        if(auto* C=P->GetWorld()->SpawnActor<ARogueReturningTalisman>(TC,P->GetActorLocation(),FRotator::ZeroRotator,Params))
        {C->Configure(P,I,Talismans,0,2000,30,1,1);Out.Add(C);}
    for(int32 I=0;BC&&I<FMath::Clamp(Blades,0,6);++I)
        if(auto* B=P->GetWorld()->SpawnActor<ARogueOrbitingBlade>(BC,P->GetActorLocation(),FRotator::ZeroRotator,Params))
        {B->ActivatePooledBlade(P);B->ConfigureBlade(P,I,Blades,150,120,0,0);Out.Add(B);}
    return Out;
}

bool URogueEditorAssetUtils::OpenPreviewUpgradeSelection(ARogueCharacter* Player)
{
    if (!IsValid(Player) || !Player->GetWorld() || !Player->GetWorld()->IsGameWorld()) { return false; }
    auto* Mode = Player->GetWorld()->GetAuthGameMode<ARogueGameMode>();
    if (!Mode) { return false; }
    Mode->StartUpgradeSelection(Player);
    auto* State = Player->GetPlayerState<ARoguePlayerState>();
    if(State&&!State->IsAwaitingUpgradeChoice())State->Server_ToggleUpgradeSelection();
    return State && State->IsAwaitingUpgradeChoice();
}

bool URogueEditorAssetUtils::SaveAsset(UObject* Asset)
{
	if (Asset == nullptr)
	{
		return false;
	}

	Asset->MarkPackageDirty();
	if (UPackage* Package = Asset->GetOutermost())
	{
		Package->SetDirtyFlag(true);
	}

	return UEditorAssetLibrary::SaveAsset(Asset->GetPathName(), false);
}

AActor* URogueEditorAssetUtils::SpawnPreviewActor(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FTransform Transform)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !ActorClass) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor=World->SpawnActor<AActor>(ActorClass,Transform,Params);
    // Preview enemies bypass the normal spawn subsystem, so register them for real weapon targeting.
    if(auto* Enemy=Cast<ARogueEnemy>(Actor))if(auto* Tracker=World->GetSubsystem<URogueEnemyTrackerSubsystem>())Tracker->RegisterEnemy(Enemy);
    return Actor;
}


bool URogueEditorAssetUtils::ConnectWorldPositionOffset(UMaterial* Material, UMaterialExpression* Expression)
{
    if (!IsValid(Material) || !IsValid(Expression)) { return false; }
    Material->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Expression);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return true;
}

bool URogueEditorAssetUtils::ImportGrowthCurveTable(UCurveTable* Table, const FString& Json, bool bMergeRows)
{
    if (!IsValid(Table)) { return false; }
    TStrongObjectPtr<UCurveTable> Imported(bMergeRows ? NewObject<UCurveTable>() : nullptr);
    UCurveTable* Source = bMergeRows ? Imported.Get() : Table;
    const TArray<FString> Problems = Source->CreateTableFromJSONString(Json, RCIM_Cubic);
    if (!Problems.IsEmpty())
    {
        for (const FString& Problem : Problems) { UE_LOG(LogTemp, Error, TEXT("[GrowthCurve] %s"), *Problem); }
        return false;
    }
    // 保留Rich模式，用户可以直接切换关键帧插值/切线；初始线性取值避免自动曲线过冲。
    for (const auto& Row : Source->GetRichCurveRowMap())
    {
        FRichCurve& Curve = *Row.Value;
        Curve.PreInfinityExtrap = RCCE_Constant;
        Curve.PostInfinityExtrap = RCCE_Linear;
        for (auto Key = Curve.GetKeyHandleIterator(); Key; ++Key) { Curve.SetKeyInterpMode(*Key, RCIM_Linear); }
        if (bMergeRows)
        {
            // 只替换传入的行，保留其它行的关键帧、切线和外推设置。
            Table->RemoveRow(Row.Key);
            if (Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
            {
                FSimpleCurve& Destination = Table->AddSimpleCurve(Row.Key);
                Destination.SetKeyInterpMode(RCIM_Linear);
                Destination.PreInfinityExtrap = Curve.PreInfinityExtrap;
                Destination.PostInfinityExtrap = Curve.PostInfinityExtrap;
                for (auto Key = Curve.GetKeyHandleIterator(); Key; ++Key)
                    Destination.AddKey(Curve.GetKeyTime(*Key), Curve.GetKeyValue(*Key));
            }
            else { Table->AddRichCurve(Row.Key) = Curve; }
        }
    }
    Table->MarkPackageDirty();
    return true;
}

void URogueEditorAssetUtils::StartWidgetPreviewWindow(int32 Width,int32 Height)
{
    if(!GEditor || GEditor->PlayWorld)return;
    auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->NewWindowWidth=FMath::Clamp(Width,640,3840);
    Settings->NewWindowHeight=FMath::Clamp(Height,360,2160);
    FRequestPlaySessionParams Params;
    Params.WorldType=EPlaySessionWorldType::PlayInEditor;
    GEditor->RequestPlaySession(Params);
}

#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "UI/Widgets/RogueCardMotionWidgetBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Serialization/JsonSerializer.h"

bool URogueEditorAssetUtils::ApplyPreviewUpgrade(ARogueCharacter* Player,ERogueUpgradeType Type,int32 Times)
{
    if(!IsValid(Player)||!Player->GetWorld()||!Player->GetWorld()->IsGameWorld()||!Player->HasAuthority())return false;
    if(int32(Type)<0||int32(Type)>=StaticEnum<ERogueUpgradeType>()->NumEnums()-1)return false;
    for(int32 I=0;I<FMath::Clamp(Times,0,20);++I){FRogueUpgradeOption Pick;Pick.Type=Type;Pick.Magnitude=1;Player->ApplyUpgrade(Pick);}
    Player->RefreshWeaponBonusSnapshotFromAttributes();return true;
}
FString URogueEditorAssetUtils::InspectOwnedItemsPreview(APlayerController* Owner)
{
    auto* HUD=Owner?Cast<ARogueHUD>(Owner->GetHUD()):nullptr;auto* Bar=HUD?HUD->GetOwnedItemsWidget():nullptr;
    TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();Root->SetBoolField(TEXT("bar"),Bar!=nullptr);
    TArray<TSharedPtr<FJsonValue>> Items;
    if(Bar)
    {
        auto* Canvas=Cast<UCanvasPanel>(Bar->GetWidgetFromName(TEXT("ItemsCanvas")));
        for(int32 I=0;Canvas&&I<Canvas->GetChildrenCount();++I)
        {
            auto* Tile=Cast<URogueOwnedItemWidget>(Canvas->GetChildAt(I));if(!Tile)continue;
            auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("type"),StaticEnum<ERogueUpgradeType>()->GetNameStringByValue(int64(Tile->View.Type)));
            V->SetNumberField(TEXT("displayed_count"), Tile->GetDisplayedCount());
            V->SetNumberField(TEXT("count"),Tile->View.Count);V->SetBoolField(TEXT("weapon"),Tile->View.bWeapon);
            if(auto* CS=Cast<UCanvasPanelSlot>(Tile->Slot)){V->SetNumberField(TEXT("x"),CS->GetPosition().X);V->SetNumberField(TEXT("y"),CS->GetPosition().Y);}
            if(auto* Icon=Cast<UImage>(Tile->GetWidgetFromName(TEXT("ItemIcon"))))V->SetNumberField(TEXT("hop"),Icon->GetRenderTransform().Translation.Y);
            Items.Add(MakeShared<FJsonValueObject>(V));
        }
        Root->SetNumberField(TEXT("height"),Bar->GetOccupiedHeight());
        const FGeometry& Geometry = Bar->GetPaintSpaceGeometry();
        const FVector2D Size = Geometry.GetLocalSize();
        Root->SetNumberField(TEXT("width"), Size.X);
        Root->SetNumberField(TEXT("canvas_height"), Size.Y);
        if (auto* Flight = Cast<UImage>(Bar->GetWidgetFromName(TEXT("AcquisitionIcon"))))
        {
            Root->SetBoolField(TEXT("flight_visible"), Flight->IsVisible());
            if (auto* Slot = Cast<UCanvasPanelSlot>(Flight->Slot))
            {
                const FVector2D Position = Slot->GetPosition() + Size;
                Root->SetNumberField(TEXT("flight_x"), Position.X);
                Root->SetNumberField(TEXT("flight_y"), Position.Y);
            }
        }
        // FGeometry does not retain all fields across Python reflection. Serialize native coordinates.
        TArray<UUserWidget*> Cards;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Owner, Cards, URogueCardMotionWidgetBase::StaticClass(), false);
        TArray<TSharedPtr<FJsonValue>> CardViews;
        for (const UUserWidget* Card : Cards)
        {
            if (!Card || Card->GetOwningPlayer() != Owner || !Card->IsVisible()) continue;
            const FGeometry& CardGeometry = Card->GetPaintSpaceGeometry();
            if (CardGeometry.GetLocalSize().IsNearlyZero()) continue;
            const auto Position = Geometry.AbsoluteToLocal(CardGeometry.LocalToAbsolute(CardGeometry.GetLocalSize() * .5f));
            auto View = MakeShared<FJsonObject>();
            View->SetStringField(TEXT("widget"), Card->GetPathName());
            View->SetNumberField(TEXT("x"), Position.X);
            View->SetNumberField(TEXT("y"), Position.Y);
            View->SetNumberField(TEXT("normalized_x"), Size.X > 0 ? Position.X / Size.X : 0);
            View->SetNumberField(TEXT("normalized_y"), Size.Y > 0 ? Position.Y / Size.Y : 0);
            CardViews.Add(MakeShared<FJsonValueObject>(View));
        }
        Root->SetArrayField(TEXT("cards"), CardViews);
    }
    Root->SetArrayField(TEXT("items"),Items);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));return Text;
}

TArray<FString> URogueEditorAssetUtils::FindBlueprintVariableReferences(UBlueprint* Blueprint,const TArray<FName>& Names)
{
    TArray<FString> Result;if(!Blueprint)return Result;
    TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);
    for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
        if(const auto* Variable=Cast<UK2Node_Variable>(Node);Variable&&Names.Contains(Variable->GetVarName()))
            Result.Add(Blueprint->GetPathName()+TEXT(":")+Graph->GetName()+TEXT(":")+Variable->GetVarName().ToString());
    return Result;
}
bool URogueEditorAssetUtils::ValidateBlueprint(UBlueprint* Blueprint)
{
    if(!Blueprint)return false;
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status!=BS_Error;
}

int32 URogueEditorAssetUtils::GetBalanceLayoutVersion(UObject* Asset)
{
    return Asset?Asset->GetLinkerCustomVersion(FGuid(0x7813CF72,0x2EBB4E2A,0xA614905F,0xC30A8DD1)):-1;
}
