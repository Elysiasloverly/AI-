#include "RogueUIPolishBuilder.h"
#include "UI/RogueWidgetAuthoring.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/RichTextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Fonts/CompositeFont.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "AssetToolsModule.h"
#include "Factories/FontFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
namespace RogueUIPolish
{
    const FLinearColor Ink(.045f,.063f,.10f,1),Muted(.20f,.24f,.29f,1),Red(.48f,.075f,.09f,1),Paper(.97f,.945f,.89f,1);
    using RogueWidgetAuthoring::Finish;
    template<typename T> T* Get(UWidgetBlueprint* BP, const TCHAR* Name)
    {
        return RogueWidgetAuthoring::FindOrCreate<T>(BP, Name);
    }
    void Place(UCanvasPanel* C,UWidget* W,FVector2D P,FVector2D S,FAnchors A=FAnchors(0),FVector2D Align=FVector2D::ZeroVector)
    {W->RemoveFromParent();auto* CS=C->AddChildToCanvas(W);CS->SetAnchors(A);CS->SetAlignment(Align);CS->SetPosition(P);CS->SetSize(S);}
    UTextBlock* Text(UWidgetBlueprint* BP,UFont* Font,const TCHAR* Name,const FString& Value,int32 Size,FLinearColor Color,bool Bold=false)
    {auto* T=Get<UTextBlock>(BP,Name);T->SetText(FText::FromString(Value));T->SetFont(FSlateFontInfo(Font,Size,Bold?TEXT("Semibold"):TEXT("Regular")));T->SetColorAndOpacity(Color);T->SetShadowOffset(FVector2D::ZeroVector);T->SetVisibility(ESlateVisibility::HitTestInvisible);return T;}
    bool InsideButton(UWidget* W){for(auto* P=W->GetParent();P;P=P->GetParent())if(Cast<UButton>(P))return true;return false;}
}
void URogueUIPolishBuilder::ConfigureMenuBackdrop(UMaterial* Material)
{
    if(!Material)return;
    // Set the real shading-model bitfield too; changing the legacy enum through reflection alone can leave DefaultLit cached.
    Material->Modify();Material->SetShadingModel(MSM_Unlit);Material->PostEditChange();Material->MarkPackageDirty();
}
UFont* URogueUIPolishBuilder::MakeFont(const FString& Name,UFontFace* Regular,UFontFace* Semibold)
{
    if(!Regular||!Semibold||!Name.StartsWith(TEXT("F_Baka")))return nullptr;
    const FString Path=TEXT("/Game/UMG/Fonts/")+Name+TEXT(".")+Name;auto* Font=LoadObject<UFont>(nullptr,*Path);
    if(!Font)Font=Cast<UFont>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(Name,TEXT("/Game/UMG/Fonts"),UFont::StaticClass(),NewObject<UFontFactory>()));
    if(!Font)return nullptr;Font->Modify();Font->FontCacheType=EFontCacheType::Runtime;auto& C=Font->CompositeFont;C.DefaultTypeface.Fonts.Reset();C.SubTypefaces.Reset();C.FallbackTypeface.Typeface.Fonts.Reset();
    for(FName Weight:{FName(TEXT("Regular")),FName(TEXT("Semibold")),FName(TEXT("Bold"))}){auto& E=C.DefaultTypeface.Fonts.AddDefaulted_GetRef();E.Name=Weight;E.Font=FFontData(Weight==TEXT("Regular")?Regular:Semibold);}
    if(auto* Fallback=LoadObject<UFontFace>(nullptr,TEXT("/Engine/EngineFonts/Faces/DroidSansFallback.DroidSansFallback"))){auto& E=C.FallbackTypeface.Typeface.Fonts.AddDefaulted_GetRef();E.Name=TEXT("Regular");E.Font=FFontData(Fallback);}
    C.FallbackTypeface.ScalingFactor=1;Font->MarkPackageDirty();return Font;
}
bool URogueUIPolishBuilder::PolishTypography(UWidgetBlueprint* BP,UFont* Body,UFont* Heading)
{
    using namespace RogueUIPolish;if(!BP||!BP->WidgetTree||!Body||!Heading)return false;BP->Modify();BP->WidgetTree->Modify();TArray<UWidget*> Widgets;BP->WidgetTree->GetAllWidgets(Widgets);
    const FString Page=BP->GetName();
    for(auto* W:Widgets)
    {
        if(auto* T=Cast<UTextBlock>(W))
        {
            T->Modify();auto F=T->GetFont();F.FontObject=Body;F.TypefaceFontName=TEXT("Regular");F.LetterSpacing=0;
            const FString N=T->GetName();const bool Button=InsideButton(T);
            const bool Title=N.Contains(TEXT("Title"))||N==TEXT("Text_Header")||N==TEXT("HeaderText");
            const bool Card=Page.Contains(TEXT("Card"))||Page.Contains(TEXT("TreeNode"));
            if(Title){F.TypefaceFontName=TEXT("Semibold");if(!Card&&F.Size>=26)F.FontObject=Heading;}
            if(Button&&(!Card||Title)&&!N.Contains(TEXT("Desc")))F.TypefaceFontName=TEXT("Semibold");
            // Badge/damage text keeps its deliberate compact sizes. Improve long-copy metrics without scaling whole pages.
            if(N.Contains(TEXT("Desc"))||N.Contains(TEXT("TutorialText"))){F.Size=FMath::Max(F.Size,Card?18:19);T->SetLineHeightPercentage(1.22f);}
            if(Page.Contains(TEXT("Tutorial"))&&N.EndsWith(TEXT("Body")))T->SetLineHeightPercentage(1.2f);
            T->SetFont(F);
            if(Card||Button||Page.Contains(TEXT("Menu"))||Page.Contains(TEXT("Tutorial")))T->SetShadowOffset(FVector2D::ZeroVector);
        }
        else if(auto* Edit=Cast<UEditableTextBox>(W))
        {Edit->Modify();auto& Font=Edit->WidgetStyle.TextStyle.Font;Font.FontObject=Body;Font.TypefaceFontName=TEXT("Regular");Font.LetterSpacing=0;}
    }
    if(Page==TEXT("WBP_UpgradeTree"))if(auto* Root=Cast<UCanvasPanel>(BP->WidgetTree->RootWidget))
    {auto* Veil=Get<UBorder>(BP,TEXT("MenuPageVeil"));Veil->SetBrushColor(FLinearColor(.035f,.05f,.09f,.9f));Veil->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Root,Veil,FVector2D::ZeroVector,FVector2D::ZeroVector,FAnchors(0,0,1,1));CastChecked<UCanvasPanelSlot>(Veil->Slot)->SetZOrder(-2);}
    return Finish(BP);
}
bool URogueUIPolishBuilder::BuildMainMenu(UWidgetBlueprint* BP,UFont* Body,UFont* Heading,UTexture2D* Logo)
{
    using namespace RogueUIPolish;if(!BP||!BP->WidgetTree||!Body||!Heading||!Logo)return false;
    // Keep existing named action widgets and all graph nodes; only the presentation hierarchy changes.
    const TCHAR* Names[]={TEXT("SinglePlayerButton"),TEXT("MultiplayerButton"),TEXT("SkillTreeButton"),TEXT("TutorialButton"),TEXT("SettingsButton"),TEXT("QuitButton")};
    TArray<UButton*> Actions;for(auto* N:Names){auto* B=Cast<UButton>(BP->WidgetTree->FindWidget(N));if(!B)return false;Actions.Add(B);}
    BP->Modify();BP->WidgetTree->Modify();auto* Root=Get<UCanvasPanel>(BP,TEXT("RootCanvas"));BP->WidgetTree->RootWidget=Root;
    auto* Panel=Get<UBorder>(BP,TEXT("MenuPanel"));Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.97f,.948f,.90f,.94f),22.f,FLinearColor(.53f,.46f,.36f,.42f),1));Panel->SetBrushColor(FLinearColor::White);Panel->SetPadding(FMargin(0));
    Root->ClearChildren();Place(Root,Panel,FVector2D(94,0),FVector2D(540,860),FAnchors(0,.5f),FVector2D(0,.5f));
    if(auto* Ambient=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GameAssets/ArtDirection/MenuStory/Materials/M_MenuPetals.M_MenuPetals")))
    {auto* Petals=Get<UImage>(BP,TEXT("MenuAmbientPetals"));Petals->SetBrushFromMaterial(Ambient);Petals->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Root,Petals,FVector2D::ZeroVector,FVector2D::ZeroVector,FAnchors(0,0,1,1));CastChecked<UCanvasPanelSlot>(Petals->Slot)->SetZOrder(-1);}
    auto* Content=Get<UCanvasPanel>(BP,TEXT("MenuContent"));Panel->SetContent(Content);Content->ClearChildren();
    auto* Eyebrow=Text(BP,Body,TEXT("MenuEyebrow"),TEXT("幻想乡手帖  /  冰霜异变"),17,Red,true);auto EF=Eyebrow->GetFont();EF.LetterSpacing=100;Eyebrow->SetFont(EF);Place(Content,Eyebrow,FVector2D(38,30),FVector2D(465,30));
    auto* Image=Get<UImage>(BP,TEXT("Image_GameLogo"));Image->SetBrushFromTexture(Logo,true);Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Scale=Get<UScaleBox>(BP,TEXT("GameLogoScale"));Scale->SetStretch(EStretch::ScaleToFit);Scale->SetContent(Image);Scale->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Content,Scale,FVector2D(36,67),FVector2D(458,180));
    Place(Content,Text(BP,Body,TEXT("SubtitleText"),TEXT("弹幕交错之间，开启新的异变之旅。"),19,Muted),FVector2D(40,251),FVector2D(465,34));
    auto* Rule=Get<UBorder>(BP,TEXT("MenuRule"));Rule->SetBrushColor(FLinearColor(.53f,.40f,.27f,.3f));Rule->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Content,Rule,FVector2D(40,298),FVector2D(460,1));
    const FVector2D Pos[]={FVector2D(36,324),FVector2D(36,416),FVector2D(36,496),FVector2D(36,594),FVector2D(278,594),FVector2D(36,686)};
    const FVector2D Sizes[]={FVector2D(468,76),FVector2D(468,64),FVector2D(468,64),FVector2D(226,56),FVector2D(226,56),FVector2D(468,44)};
    const TCHAR* Labels[]={TEXT("单人游戏"),TEXT("多人游戏"),TEXT("升级树"),TEXT("教程说明"),TEXT("设置"),TEXT("退出游戏")};
    for(int32 I=0;I<6;++I)
    {
        auto* Button=Actions[I];auto* Label=FindObject<UTextBlock>(BP->WidgetTree,*(FString(Names[I])+TEXT("Label")));if(!Label)return false;
        FButtonStyle S=Button->GetStyle();const bool Primary=I==0,Quiet=I>=3;
        const FLinearColor Base=Primary?Red:Quiet?FLinearColor(.94f,.92f,.86f,.75f):FLinearColor(.88f,.90f,.89f,1);
        S.Normal=FSlateRoundedBoxBrush(Base,12.f,Primary?FLinearColor(.31f,.035f,.055f,1):FLinearColor(.39f,.42f,.43f,.42f),1);
        S.Hovered=FSlateRoundedBoxBrush(Primary?FLinearColor(.62f,.12f,.15f,1):FLinearColor(.96f,.84f,.74f,1),12.f,Red,1.4f);
        S.Pressed=FSlateRoundedBoxBrush(Primary?FLinearColor(.36f,.045f,.07f,1):FLinearColor(.87f,.77f,.67f,1),12.f,Red,1.5f);
        S.Disabled=FSlateRoundedBoxBrush(FLinearColor(.65f,.66f,.65f,.7f),12.f);S.NormalPadding=S.PressedPadding=FMargin(0);Button->SetStyle(S);
        Button->ClearChildren();auto* Row=Get<UCanvasPanel>(BP,*(FString(TEXT("MenuActionBody"))+FString::FromInt(I)));Row->ClearChildren();Button->AddChild(Row);
        Label->SetFont(FSlateFontInfo(Body,I==0?27:I<3?24:21,TEXT("Semibold")));Label->SetText(FText::FromString(Labels[I]));Label->SetColorAndOpacity(Primary?Paper:Ink);Label->SetShadowOffset(FVector2D::ZeroVector);Label->SetJustification(ETextJustify::Left);Label->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(Row,Label,FVector2D(I<3?27:22,(Sizes[I].Y-Label->GetFont().Size*1.5f)*.5f),FVector2D(Sizes[I].X-60,Label->GetFont().Size*1.6f));Label->SetMinDesiredWidth(0);
        if(I<3){auto* Arrow=Text(BP,Body,*(FString(TEXT("MenuArrow"))+FString::FromInt(I)),TEXT("›"),32,Primary?Paper:Red);Place(Row,Arrow,FVector2D(-42,(Sizes[I].Y-48)*.5f),FVector2D(30,50),FAnchors(1,0));}
        Place(Content,Button,Pos[I],Sizes[I]);
    }
    Place(Content,Text(BP,Body,TEXT("HintText"),TEXT(""),17,Red),FVector2D(40,744),FVector2D(460,45));
    Place(Content,Text(BP,Body,TEXT("MenuFootnote"),TEXT("四季更迭，异变未歇。"),16,Muted),FVector2D(40,808),FVector2D(460,26));
    // All ambient art is a non-interactive layer. Existing named title remains for legacy graph references.
    auto* Title=Get<UTextBlock>(BP,TEXT("TitleText"));Title->SetVisibility(ESlateVisibility::Collapsed);Place(Content,Title,FVector2D::ZeroVector,FVector2D(1));
    auto* Badge=Get<UBorder>(BP,TEXT("MenuChapterBadge"));Badge->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.97f,.948f,.90f,.86f),16.f));Badge->SetBrushColor(FLinearColor::White);Badge->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Root,Badge,FVector2D(-48,36),FVector2D(380,108),FAnchors(1,0),FVector2D(1,0));
    auto* Stamp=Text(BP,Heading,TEXT("MenuSeasonTitle"),TEXT("霜华序章"),31,Red,true);Stamp->SetJustification(ETextJustify::Right);Place(Root,Stamp,FVector2D(-72,48),FVector2D(330,52),FAnchors(1,0),FVector2D(1,0));
    auto* Caption=Text(BP,Body,TEXT("MenuSeasonCaption"),TEXT("纸上幻想 · 弹幕旅途"),17,Muted);Caption->SetJustification(ETextJustify::Right);Place(Root,Caption,FVector2D(-72,102),FVector2D(330,30),FAnchors(1,0),FVector2D(1,0));
    return Finish(BP);
}
