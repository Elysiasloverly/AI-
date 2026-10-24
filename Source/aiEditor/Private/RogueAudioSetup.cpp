#include "RogueAudioSetup.h"
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
        // 按武器表当前内容迁移，保留用户已经填写的音效；只在第一次创建时执行。
        const FName Order[] = {TEXT("Projectile"), TEXT("Scythe"), TEXT("Rocket"), TEXT("Laser"), TEXT("HellTower"), TEXT("Mortar")};
        UClass* Fallback[] = {ARogueWeapon_Projectile::StaticClass(), ARogueWeapon_Scythe::StaticClass(), ARogueWeapon_Rocket::StaticClass(), ARogueWeapon_Laser::StaticClass(), ARogueWeapon_HellTower::StaticClass(), ARogueWeapon_Mortar::StaticClass()};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Order); ++Index)
        {
            FName Name = Order[Index];
            if (const auto* Row = Table->FindRow<FRogueWeaponTableRow>(Name, TEXT("Audio migration")))
            {
                auto& Entry = Config->Weapons.AddDefaulted_GetRef();
                Entry.WeaponName = Row->DisplayName; Entry.WeaponClass = Row->WeaponClass ? Row->WeaponClass.Get() : Fallback[Index]; Entry.AttackAudio = Row->AttackAudio;
            }
        }
    }
    if (Config->Enemies.IsEmpty())
    {
        auto* AudioProperty = FindFProperty<FStructProperty>(ARogueEnemy::StaticClass(), TEXT("AttackAudio"));
        auto AddEnemy = [&](UClass* Class, const FString& Label)
        {
            if (!Class) return;
            auto& Entry = Config->Enemies.AddDefaulted_GetRef(); Entry.EnemyName = Label; Entry.EnemyClass = Class;
            if (AudioProperty)
            {
                const auto* Value = AudioProperty->ContainerPtrToValuePtr<FRogueAttackAudioSettings>(Class->GetDefaultObject());
                const auto* Parent = Class->GetSuperClass() && Class->GetSuperClass()->IsChildOf(ARogueEnemy::StaticClass())
                    ? AudioProperty->ContainerPtrToValuePtr<FRogueAttackAudioSettings>(Class->GetSuperClass()->GetDefaultObject()) : nullptr;
                // 继承来的相同值保持为空，未来改中央基类时仍能传递到所有子类。
                if (!Parent || !FRogueAttackAudioSettings::StaticStruct()->CompareScriptStruct(Value, Parent, 0)) Entry.AttackAudio = *Value;
            }
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
