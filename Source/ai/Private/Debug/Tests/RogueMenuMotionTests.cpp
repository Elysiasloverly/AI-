#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueMenuMotionExtension.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"

struct FRogueMenuMotionTestAccess
{
    static int32 Count(URogueMenuMotionExtension* E){return E->Buttons.Num();}
    static void Hide(URogueMenuMotionExtension* E){E->VisibilityChanged(ESlateVisibility::Collapsed);}
};
namespace RogueMenuMotionTests
{
    constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    UUserWidget* Menu()
    {
        UClass* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/UMG/Menus/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
        if(!Class)return nullptr;auto* W=NewObject<UUserWidget>(GetTransientPackage(),Class);return W->Initialize()?W:nullptr;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMenuMotionRestoreTest,"Rogue.UI.MenuPolish.MotionPreservesAuthoredTransformsAndInput",RogueMenuMotionTests::Flags)
bool FRogueMenuMotionRestoreTest::RunTest(const FString&)
{
    auto* W=RogueMenuMotionTests::Menu();if(!TestNotNull(TEXT("Actual menu initializes"),W))return false;
    auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("SinglePlayerButton")));if(!TestNotNull(TEXT("Action remains bound by name"),B))return false;
    const FVector2D Position=CastChecked<UCanvasPanelSlot>(B->Slot)->GetPosition();
    FWidgetTransform Authored;Authored.Translation=FVector2D(8,13);Authored.Scale=FVector2D(.9f,1.1f);Authored.Angle=2;
    B->SetRenderTransform(Authored);B->SetRenderTransformPivot(FVector2D(.2f,.3f));B->SetRenderOpacity(.8f);const FButtonStyle Style=B->GetStyle();
    URogueMenuMotionExtension::Attach(W);URogueMenuMotionExtension::Attach(W);auto* E=W->GetExtension<URogueMenuMotionExtension>();
    TestEqual(TEXT("Attaching twice is idempotent"),W->GetExtensions(URogueMenuMotionExtension::StaticClass()).Num(),1);E->Construct();
    TestEqual(TEXT("All six menu actions participate"),FRogueMenuMotionTestAccess::Count(E),6);
    E->Tick(FGeometry(),.05f);TestTrue(TEXT("Entry moves presentation"),B->GetRenderTransform().Translation.Y>Authored.Translation.Y);
    TestEqual(TEXT("Canvas layout remains authored"),CastChecked<UCanvasPanelSlot>(B->Slot)->GetPosition(),Position);
    TestTrue(TEXT("Input remains enabled during entry"),B->GetIsEnabled());
    TestTrue(TEXT("Motion leaves style intact"),B->GetStyle().Normal==Style.Normal);
    FRogueMenuMotionTestAccess::Hide(E);
    TestTrue(TEXT("Hiding restores translation"),B->GetRenderTransform().Translation.Equals(Authored.Translation));
    TestTrue(TEXT("Hiding restores non-uniform scale"),B->GetRenderTransform().Scale.Equals(Authored.Scale));
    TestTrue(TEXT("Hiding restores pivot"),B->GetRenderTransformPivot().Equals(FVector2D(.2f,.3f)));
    TestEqual(TEXT("Hiding restores authored opacity"),B->GetRenderOpacity(),.8f);
    E->Tick(FGeometry(),.05f);E->bEnabled=false;E->Tick(FGeometry(),.05f);
    TestTrue(TEXT("Disabling motion restores translation"),B->GetRenderTransform().Translation.Equals(Authored.Translation));
    TestEqual(TEXT("Disabling motion restores angle"),B->GetRenderTransform().Angle,Authored.Angle);
    E->Destruct();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMenuTypographyAssetsTest,"Rogue.UI.MenuPolish.EmbeddedFontsAndMainMenuActions",RogueMenuMotionTests::Flags)
bool FRogueMenuTypographyAssetsTest::RunTest(const FString&)
{
    for(const TCHAR* Name:{TEXT("FF_BakaBody"),TEXT("FF_BakaSemibold"),TEXT("FF_BakaDisplay")})
    {
        auto* Face=LoadObject<UFontFace>(nullptr,*(FString(TEXT("/Game/UMG/Fonts/Faces/"))+Name+TEXT(".")+Name));
        if(!TestNotNull(Name,Face))return false;
        TestTrue(TEXT("Font bytes are embedded; no system font dependency"),Face->GetFontFaceData()->GetData().Num()>100000);
        TestTrue(TEXT("Font is inlined for packaged builds"),Face->GetLoadingPolicy()==EFontLoadingPolicy::Inline);
    }
    auto* Body=LoadObject<UFont>(nullptr,TEXT("/Game/UMG/Fonts/F_BakaUI.F_BakaUI"));
    auto* Heading=LoadObject<UFont>(nullptr,TEXT("/Game/UMG/Fonts/F_BakaDisplay.F_BakaDisplay"));
    if(!TestNotNull(TEXT("Body family"),Body)||!TestNotNull(TEXT("Heading family"),Heading))return false;
    TestTrue(TEXT("Regular and semibold use different embedded faces"),Body->CompositeFont.DefaultTypeface.Fonts[0].Font.GetFontFaceAsset()!=Body->CompositeFont.DefaultTypeface.Fonts[1].Font.GetFontFaceAsset());
    auto* W=RogueMenuMotionTests::Menu();if(!TestNotNull(TEXT("Menu class loads"),W))return false;
    for(const TCHAR* Name:{TEXT("SinglePlayerButton"),TEXT("MultiplayerButton"),TEXT("SkillTreeButton"),TEXT("TutorialButton"),TEXT("SettingsButton"),TEXT("QuitButton")})
        TestNotNull(Name,Cast<UButton>(W->GetWidgetFromName(Name)));
    auto* Caption=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("MenuSeasonTitle")));
    if(TestNotNull(TEXT("Display heading"),Caption))TestTrue(TEXT("Display uses serif family"),Caption->GetFont().FontObject==Heading);
    auto* Petals=W->GetWidgetFromName(TEXT("MenuAmbientPetals"));if(TestNotNull(TEXT("Ambient layer"),Petals))TestTrue(TEXT("Ambient layer cannot block clicks"),Petals->GetVisibility()==ESlateVisibility::HitTestInvisible);
    auto* Backdrop=LoadObject<UMaterial>(nullptr,TEXT("/Game/GameAssets/ArtDirection/MenuStory/Materials/M_MenuStoryBackdrop.M_MenuStoryBackdrop"));
    if(TestNotNull(TEXT("Menu painting"),Backdrop))TestTrue(TEXT("Real shading-model bitfield is unlit"),Backdrop->GetShadingModels().HasShadingModel(MSM_Unlit));
    return true;
}
#endif
