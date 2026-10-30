#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/RogueMusicSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

struct FRogueUIButtonAudioTestAccess
{
    static int32 Bind(URogueMusicSubsystem* Audio, UUserWidget* Widget) { return Audio->BindButtonSounds(Widget); }
    static void Config(URogueMusicSubsystem* Audio, URogueAudioConfig* Config) { Audio->LoadedConfig = Config; }
    static FName Pressed() { return GET_FUNCTION_NAME_CHECKED(URogueMusicSubsystem, HandleUIButtonPressed); }
    static FName Hovered() { return GET_FUNCTION_NAME_CHECKED(URogueMusicSubsystem, HandleUIButtonHovered); }
    static uint32 Played(URogueMusicSubsystem* Audio) { return Audio->UIButtonPressedCount + Audio->UIButtonHoveredCount; }
};

namespace RogueUIButtonAudioTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    // Reuse a concrete project widget class without invoking game/menu construction in this device-free fixture.
    UUserWidget* MakeWidget(UClass* Class)
    {
        auto* Widget = NewObject<UUserWidget>(GetTransientPackage(), Class);
        Widget->WidgetTree = NewObject<UWidgetTree>(Widget);
        Widget->WidgetTree->RootWidget = Widget->WidgetTree->ConstructWidget<UVerticalBox>();
        return Widget;
    }
    UButton* AddButton(UUserWidget* Widget)
    {
        auto* Button = Widget->WidgetTree->ConstructWidget<UButton>();
        CastChecked<UVerticalBox>(Widget->WidgetTree->RootWidget)->AddChildToVerticalBox(Button);
        return Button;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueUIButtonBindingTest,
    "Rogue.Audio.UI.IdempotentBindingPreservesStyleAndBusinessEvents", RogueUIButtonAudioTests::Flags)
bool FRogueUIButtonBindingTest::RunTest(const FString&)
{
    using namespace RogueUIButtonAudioTests;
    UClass* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UMG/Menus/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
    if (!TestNotNull(TEXT("The project's actual main menu class exists"), Class)) return false;
    auto* Instance = NewObject<UGameInstance>(GEngine);
    auto* Audio = NewObject<URogueMusicSubsystem>(Instance);
    UUserWidget* Widget = MakeWidget(Class);
    UButton* Button = AddButton(Widget);
    auto* OldSound = NewObject<USoundWave>();
    FButtonStyle CustomStyle = Button->GetStyle();
    CustomStyle.Normal.TintColor = FSlateColor(FLinearColor(.1f, .2f, .3f, .4f));
    CustomStyle.NormalPadding = FMargin(11, 17, 23, 29);
    CustomStyle.PressedPadding = FMargin(3, 5, 7, 9);
    CustomStyle.PressedSlateSound.SetResourceObject(OldSound);
    CustomStyle.HoveredSlateSound.SetResourceObject(OldSound);
    Button->SetStyle(CustomStyle);
    FScriptDelegate Business;
    Business.BindUFunction(Audio, GET_FUNCTION_NAME_CHECKED(URogueMusicSubsystem, RefreshMusicState));
    Button->OnClicked.AddUnique(Business);
    Button->OnPressed.AddUnique(Business);
    Button->SetBackgroundColor(FLinearColor(.6f, .5f, .4f, .3f));
    const FLinearColor Background = Button->GetBackgroundColor();

    TestEqual(TEXT("The initial button is handled"), FRogueUIButtonAudioTestAccess::Bind(Audio, Widget), 1);
    TestEqual(TEXT("A second construction/binding handles the same button safely"), FRogueUIButtonAudioTestAccess::Bind(Audio, Widget), 1);
    TestEqual(TEXT("Exactly one hover callback remains"), Button->OnHovered.GetAllObjects().Num(), 1);
    TestEqual(TEXT("One audio callback plus the original pressed action remain"), Button->OnPressed.GetAllObjects().Num(), 2);
    TestTrue(TEXT("The original click action survives"), Button->OnClicked.Contains(Business));
    TestTrue(TEXT("The original pressed action survives"), Button->OnPressed.Contains(Business));
    TestTrue(TEXT("Sound is bound before clicked actions, using OnPressed"),
        Button->OnPressed.Contains(Audio, FRogueUIButtonAudioTestAccess::Pressed()));
    TestFalse(TEXT("No second click audio callback is added"), Button->OnClicked.Contains(Audio, FRogueUIButtonAudioTestAccess::Pressed()));
    CustomStyle.PressedSlateSound = FSlateSound();
    CustomStyle.HoveredSlateSound = FSlateSound();
    TestTrue(TEXT("Every original style field is retained except built-in sounds"),
        FButtonStyle::StaticStruct()->CompareScriptStruct(&CustomStyle, &Button->GetStyle(), 0));
    TestTrue(TEXT("The separate background tint survives"), Button->GetBackgroundColor() == Background);
    TestTrue(TEXT("Audio configuration never disables the button"), Button->GetIsEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueUIButtonDynamicTest,
    "Rogue.Audio.UI.DynamicNestedButtonsAndEmptyConfiguration", RogueUIButtonAudioTests::Flags)
bool FRogueUIButtonDynamicTest::RunTest(const FString&)
{
    using namespace RogueUIButtonAudioTests;
    UClass* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UMG/Menus/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
    if (!TestNotNull(TEXT("A concrete project widget is available"), Class)) return false;
    auto* Instance = NewObject<UGameInstance>(GEngine);
    auto* Audio = NewObject<URogueMusicSubsystem>(Instance);
    auto* OtherInstance = NewObject<UGameInstance>(GEngine);
    auto* OtherAudio = NewObject<URogueMusicSubsystem>(OtherInstance);
    UUserWidget* Widget = MakeWidget(Class);
    UButton* InitialButton = AddButton(Widget);
    TestEqual(TEXT("Binding works before any sound asset is configured"), FRogueUIButtonAudioTestAccess::Bind(Audio, Widget), 1);

    UUserWidget* Nested = MakeWidget(Class);
    UButton* DynamicButton = AddButton(Nested);
    CastChecked<UVerticalBox>(Widget->WidgetTree->RootWidget)->AddChildToVerticalBox(Nested);
    TestEqual(TEXT("New child user-widget button is discovered on refresh"), FRogueUIButtonAudioTestAccess::Bind(Audio, Widget), 2);
    TestTrue(TEXT("Foreign widget tree receives hover sound"), DynamicButton->OnHovered.Contains(Audio, FRogueUIButtonAudioTestAccess::Hovered()));
    TestEqual(TEXT("The original button still has only one sound binding"), InitialButton->OnPressed.GetAllObjects().Num(), 1);
    TestFalse(TEXT("Another local game instance does not receive this player's UI event"),
        DynamicButton->OnPressed.Contains(OtherAudio, FRogueUIButtonAudioTestAccess::Pressed()));

    DynamicButton->OnPressed.Broadcast();
    DynamicButton->OnHovered.Broadcast();
    TestEqual(TEXT("A missing config safely requests no playback"), FRogueUIButtonAudioTestAccess::Played(Audio), uint32(0));
    auto* Config = NewObject<URogueAudioConfig>();
    TestTrue(TEXT("Menu music remains an optional independent slot"), Config->Menu.Sound.IsNull());
    TestTrue(TEXT("Menu music is editable in the central asset"),
        FindFProperty<FProperty>(URogueAudioConfig::StaticClass(), TEXT("Menu"))->HasAnyPropertyFlags(CPF_Edit));
    TestTrue(TEXT("Button interaction is editable in the same asset"),
        FindFProperty<FProperty>(URogueAudioConfig::StaticClass(), TEXT("Buttons"))->HasAnyPropertyFlags(CPF_Edit));
    FRogueUIButtonAudioTestAccess::Config(Audio, Config);
    DynamicButton->OnPressed.Broadcast();
    DynamicButton->OnHovered.Broadcast();
    Config->Buttons.bEnabled = false;
    FRogueUIButtonAudioTestAccess::Bind(Audio, Widget);
    TestTrue(TEXT("Empty or disabled sounds leave dynamic buttons interactive"), DynamicButton->GetIsEnabled());
    TestEqual(TEXT("Empty or disabled sound slots produce no playback"), FRogueUIButtonAudioTestAccess::Played(Audio), uint32(0));
    TestEqual(TEXT("Existing sound effects volume remains unchanged"), Audio->GetSoundEffectsVolume(), 1.f);
    TestEqual(TEXT("Existing background music volume remains unchanged"), Audio->GetMusicVolume(), 1.f);
    TestEqual(TEXT("A null widget is harmless"), FRogueUIButtonAudioTestAccess::Bind(Audio, nullptr), 0);
    TestEqual(TEXT("Public binding rejects a null widget"), URogueMusicSubsystem::BindWidgetButtonSounds(nullptr), 0);
    return true;
}
#endif
