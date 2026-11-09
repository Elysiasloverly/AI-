#include "RogueAudioSetup.h"
#include "Combat/Weapons/RogueWeaponCatalog.h"
#include "RogueEditorAssetUtils.h"
#include "Audio/RogueAudioConfig.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Weapons/RogueWeapon_Mortar.h"
#include "Components/Button.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Enemies/RogueEnemy.h"
#include "UObject/UnrealType.h"
#include "EditorAssetLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWaveProcedural.h"
#include "Blueprint/UserWidget.h"
#include "Framework/RogueGameState.h"
#include "Engine/Engine.h"
#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueSettingsMenuWidgetBase.h"
#include "UI/Widgets/RogueMainMenuWidgetBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"

namespace
{
    template<class T> T* Ensure(const TCHAR* Path, bool& bCreated)
    {
        if (auto* Existing = LoadObject<T>(nullptr, Path)) { bCreated = false; return Existing; }
        FString PackageName, ObjectName; FString(Path).Split(TEXT("."), &PackageName, &ObjectName);
        auto* Object = NewObject<T>(CreatePackage(*PackageName), *ObjectName, RF_Public | RF_Standalone | RF_Transactional);
        FAssetRegistryModule::AssetCreated(Object); Object->MarkPackageDirty(); bCreated = true; return Object;
    }
}

bool URogueAudioSetup::EnsureAudioAssets()
{
    bool bCreated = false;
    auto* Master = Ensure<USoundClass>(TEXT("/Game/Audio/Classes/SC_Master.SC_Master"), bCreated);
    auto* Music = Ensure<USoundClass>(TEXT("/Game/Audio/Classes/SC_Music.SC_Music"), bCreated);
    auto* SFX = Ensure<USoundClass>(TEXT("/Game/Audio/Classes/SC_SFX.SC_SFX"), bCreated);
    auto* UI = Ensure<USoundClass>(TEXT("/Game/Audio/Classes/SC_UI.SC_UI"), bCreated);
    if (!Master || !Music || !SFX || !UI) return false;
    for (USoundClass* Child : {Music, SFX, UI})
    {
        if (!Child->ParentClass) Child->ParentClass = Master;
        if (Child->ParentClass == Master) Master->ChildClasses.AddUnique(Child);
    }
    Music->Properties.bIsMusic = true;
    UI->Properties.bIsUISound = true;
    auto* Mix = Ensure<USoundMix>(TEXT("/Game/Audio/Mixes/SM_UserVolumes.SM_UserVolumes"), bCreated);
    if (bCreated) { Mix->Duration = -1; Mix->FadeInTime = 0; Mix->FadeOutTime = .05f; Mix->bApplyEQ = false; }
    auto* Config = Ensure<URogueAudioConfig>(TEXT("/Game/Data/Audio/DA_AudioConfig.DA_AudioConfig"), bCreated);
    if (!Config || !Mix) return false;
    if (!Config->MusicClass) Config->MusicClass = Music;
    if (!Config->EffectsClass) Config->EffectsClass = SFX;
    if (!Config->UIClass) Config->UIClass = UI;
    if (!Config->UserMix) Config->UserMix = Mix;
    if (bCreated)
    {
        Config->Results.bLoop = false;
        auto* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
        if (!Table) return false;
        // 新资产只建中央插槽；音效由用户在 DA_AudioConfig 填写。
        for (const auto& Descriptor : RogueWeaponCatalog::GetAll())
        {
            if (const auto* Row = Table->FindRow<FRogueWeaponTableRow>(Descriptor.RowName, TEXT("Audio slots")))
            {
                auto& Entry = Config->Weapons.AddDefaulted_GetRef();
                Entry.WeaponName = Row->DisplayName;
                Entry.WeaponClass = RogueWeaponCatalog::ResolveClass(Descriptor.RowName, *Row);
            }
        }
    }
    if (Config->Enemies.IsEmpty())
    {
        auto AddEnemy = [&](UClass* Class, const FString& Label)
        {
            if (!Class) { return; }
            auto& Entry = Config->Enemies.AddDefaulted_GetRef();
            Entry.EnemyName = Label;
            Entry.EnemyClass = Class;
        };
        AddEnemy(ARogueEnemy::StaticClass(), TEXT("所有敌人默认（原生基类）"));
        TArray<FString> Paths = UEditorAssetLibrary::ListAssets(TEXT("/Game/BP/Enemy"), true, false);
        Paths.Sort();
        for (const FString& Path : Paths)
            if (auto* BP = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(Path)))
                if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(ARogueEnemy::StaticClass()))
                    AddEnemy(BP->GeneratedClass, BP->GetName());
    }
    UObject* Assets[] = {Master, Music, SFX, UI, Mix, Config};
    for (UObject* Asset : Assets)
        if (!URogueEditorAssetUtils::SaveAsset(Asset)) return false;
    return Config->Weapons.Num() >= 6;
}

bool URogueAudioSetup::AddMusicControls(UWidgetBlueprint* BP)
{
    if (!BP || !BP->WidgetTree) return false;
    UWidgetTree* Tree = BP->WidgetTree;
    UButton* TemplateButton = Cast<UButton>(Tree->FindWidget(TEXT("Button_Row0Minus")));
    UTextBlock* TemplateText = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Label0")));
    if (!TemplateButton || !TemplateText) return false;
    UPanelWidget* Parent = TemplateButton->GetParent();
    if (!Parent) return false;
    TemplateText->SetText(FText::FromString(TEXT("音效音量")));
    for (const TCHAR* Name : {TEXT("Text_Label5"), TEXT("Text_Value5")})
    {
        auto* Text = Cast<UTextBlock>(Tree->FindWidget(Name));
        if (!Text)
        {
            Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
            Text->SetFont(TemplateText->GetFont()); Text->SetColorAndOpacity(TemplateText->GetColorAndOpacity());
            Parent->AddChild(Text);
        }
        Text->SetText(FText::FromString(FString(Name).Contains(TEXT("Label")) ? TEXT("背景音乐音量") : TEXT("100%")));
    }
    for (const TCHAR* Name : {TEXT("Button_MusicMinus"), TEXT("Button_MusicPlus")})
    {
        if (Tree->FindWidget(Name)) continue;
        auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        Button->SetStyle(TemplateButton->GetStyle()); Button->SetColorAndOpacity(TemplateButton->GetColorAndOpacity());
        Button->SetBackgroundColor(TemplateButton->GetBackgroundColor());
        auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%s_Label"), Name));
        if (auto* Original = Cast<UTextBlock>(TemplateButton->GetContent()))
        { Label->SetFont(Original->GetFont()); Label->SetColorAndOpacity(Original->GetColorAndOpacity()); }
        Label->SetText(FText::FromString(FString(Name).Contains(TEXT("Minus")) ? TEXT("−") : TEXT("＋")));
        Button->AddChild(Label);
        Parent->AddChild(Button);
    }
    return true;
}

USoundBase* URogueAudioSetup::CreateSilentPreviewSound()
{
    auto* Wave = NewObject<USoundWaveProcedural>();
    Wave->SetSampleRate(48000); Wave->NumChannels = 1; Wave->Duration = 60.f;
    TArray<uint8> Silence; Silence.SetNumZeroed(48000 * 2 * 60);
    Wave->QueueAudio(Silence.GetData(), Silence.Num());
    return Wave;
}

UWidget* URogueAudioSetup::FindPreviewWidget(UUserWidget* Widget, FName Name)
{
    return Widget ? Widget->GetWidgetFromName(Name) : nullptr;
}

bool URogueAudioSetup::SetPreviewBattleState(UObject* Context, bool bGameOver, bool bBossAlive, bool bNight)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
    auto* State = World ? World->GetGameState<ARogueGameState>() : nullptr;
    if (!State || !State->HasAuthority()) return false;
    State->SetRunStateSnapshot(State->GetCurrentWave(), State->GetEnemiesDefeated(), State->GetRunTimeSeconds(), bGameOver, bBossAlive);
    FRogueBattleStageView Stage = State->GetBattleStage(); Stage.Phase = bNight ? ERogueBattleStage::Night : ERogueBattleStage::Day;
    State->SetBattleStage(Stage); return true;
}

UUserWidget* URogueAudioSetup::OpenPreviewSettings(UObject* Context)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    auto* HUD = PC ? Cast<ARogueHUD>(PC->GetHUD()) : nullptr;
    if (!HUD || !HUD->IsMainMenuMode()) return nullptr;
    HUD->OpenMainMenuSettings();
    for (TObjectIterator<URogueSettingsMenuWidgetBase> It; It; ++It)
        if (It->GetWorld() == World && It->IsVisible()) return *It;
    return nullptr;
}

bool URogueAudioSetup::ClickPreviewButton(UUserWidget* Widget, FName Name)
{
    auto* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr;
    if (!Button || !Button->GetIsEnabled()) return false;
    Button->OnClicked.Broadcast(); return true;
}


UUserWidget* URogueAudioSetup::FindPreviewMainMenu(UObject* Context)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
    for (TObjectIterator<URogueMainMenuWidgetBase> It; It; ++It)
        if (It->GetWorld() == World && It->IsInViewport() && It->IsVisible()) return *It;
    return nullptr;
}

bool URogueAudioSetup::PreviewButtonAudio(UUserWidget* Widget, FName Name, bool bPressed)
{
    auto* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr;
    if (!Button || !Button->GetIsEnabled()) return false;
    // Exercise the real button delegates without launching its navigation/gameplay action.
    if (bPressed) Button->OnPressed.Broadcast(); else Button->OnHovered.Broadcast();
    return true;
}


bool URogueAudioSetup::PreviewButtonPointer(UUserWidget* Widget, FName Name, bool bEnter)
{
    auto* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr;
    const TSharedPtr<SWidget> SlateButton = Button ? Button->GetCachedWidget() : nullptr;
    if (!Button || !Button->GetIsEnabled() || !SlateButton.IsValid()) return false;
    const FGeometry& Geometry = Button->GetCachedGeometry();
    const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
    const TSet<FKey> Keys;
    const FPointerEvent Event(0, Position, Position, Keys, FKey(), 0.f, FModifierKeysState());
    // Drive SButton's actual enter/leave transitions; staying inside must not emit another hover.
    if (bEnter) SlateButton->OnMouseEnter(Geometry, Event); else SlateButton->OnMouseLeave(Event);
    return true;
}
