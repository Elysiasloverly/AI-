#include "UI/RogueWidgetAuthoring.h"
#include "RogueUIPolishBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

namespace RogueBattleHudPolish
{
    const FLinearColor Cream(.93f,.92f,.85f,1),Muted(.60f,.69f,.76f,1),Vermilion(.82f,.20f,.26f,1);
    template<typename T>T* Find(UWidgetBlueprint* BP,const TCHAR* Name){return Cast<T>(BP->WidgetTree->FindWidget(Name));}
    template<typename T> T* Reuse(UWidgetBlueprint* BP, const TCHAR* Name)
    {
        return RogueWidgetAuthoring::FindOrCreate<T>(BP, Name, true);
    }
    void Place(UCanvasPanel* Parent,UWidget* Widget,FVector2D Position,FVector2D Size,FAnchors Anchors=FAnchors(0),FVector2D Alignment=FVector2D::ZeroVector)
    {
        Widget->Modify();Widget->RemoveFromParent();auto* Slot=Parent->AddChildToCanvas(Widget);
        Slot->SetAnchors(Anchors);Slot->SetAlignment(Alignment);Slot->SetPosition(Position);Slot->SetSize(Size);
    }
    void Label(UTextBlock* T,UFont* Font,int32 Size,FLinearColor Color,bool Bold=false)
    {
        T->Modify();T->SetFont(FSlateFontInfo(Font,Size,Bold?TEXT("Semibold"):TEXT("Regular")));
        T->SetColorAndOpacity(Color);T->SetShadowOffset(FVector2D::ZeroVector);T->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    void Panel(UBorder* Border,UMaterialInterface* Material)
    {
        // SetBrushFromMaterial preserves the previous RoundedBox outline; use a clean image brush.
        Border->Modify();FSlateBrush Brush;Brush.DrawAs=ESlateBrushDrawType::Image;Brush.SetResourceObject(Material);
        Brush.TintColor=FLinearColor::White;Border->SetBrush(Brush);Border->SetBrushColor(FLinearColor::White);
        Border->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    UImage* Icon(UWidgetBlueprint* BP,UCanvasPanel* Parent,const TCHAR* Name,const TCHAR* Asset,FVector2D Position,FVector2D Size,FLinearColor Tint)
    {
        auto* Image=Reuse<UImage>(BP,Name);Image->SetBrushFromMaterial(LoadObject<UMaterialInterface>(nullptr,Asset));
        Image->SetColorAndOpacity(Tint);Image->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Parent,Image,Position,Size);return Image;
    }
    void Bar(UProgressBar* Widget,UMaterialInterface* Fill,float Height,FLinearColor Color,bool Trail,bool TransparentBackground)
    {
        Widget->Modify();FProgressBarStyle Style=Widget->GetWidgetStyle();
        Style.BackgroundImage=FSlateRoundedBoxBrush(FLinearColor(.009f,.018f,.036f,TransparentBackground?0.f:.95f),Height*.5f);
        if(Trail)Style.FillImage=FSlateRoundedBoxBrush(FLinearColor::White,Height*.5f);
        else{Style.FillImage=FSlateBrush();Style.FillImage.DrawAs=ESlateBrushDrawType::Image;Style.FillImage.SetResourceObject(Fill);Style.FillImage.TintColor=FLinearColor::White;}
        Widget->SetWidgetStyle(Style);Widget->SetFillColorAndOpacity(Color);Widget->SetBorderPadding(FVector2D::ZeroVector);
        Widget->SetBarFillStyle(EProgressBarFillStyle::Scale);Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

bool URogueUIPolishBuilder::PolishBattleHUD(UWidgetBlueprint* BP,UMaterialInterface* Frame,UMaterialInterface* Fill)
{
    using namespace RogueBattleHudPolish;
    if(!BP||!BP->WidgetTree||!Frame||!Fill)return false;
    const FName Name=BP->GetFName();
    if(Name!=TEXT("WBP_PlayerVitals")&&Name!=TEXT("WBP_BossHealthBar")&&Name!=TEXT("WBP_KillFeedback")&&Name!=TEXT("WBP_BattlePresentation"))return false;
    auto* Body=LoadObject<UFont>(nullptr,TEXT("/Game/UMG/Fonts/F_BakaUI.F_BakaUI"));
    auto* Heading=LoadObject<UFont>(nullptr,TEXT("/Game/UMG/Fonts/F_BakaDisplay.F_BakaDisplay"));
    if(!Body||!Heading)return false;
    BP->Modify();BP->WidgetTree->Modify();
    if(Name==TEXT("WBP_PlayerVitals"))
    {
        auto* Status=Find<UCanvasPanel>(BP,TEXT("StatusPanel"));auto* FrameWidget=Find<UBorder>(BP,TEXT("VitalsFrame"));
        if(!Status||!FrameWidget||!Find<USizeBox>(BP,TEXT("VitalsPanel")))return false;
        Panel(Find<UBorder>(BP,TEXT("StatusFrame")),Frame);Panel(FrameWidget,Frame);FrameWidget->SetPadding(FMargin(24,20));
        if(auto* Slot=Cast<UCanvasPanelSlot>(Status->Slot))Slot->SetSize(FVector2D(260,100));
        auto* Wave=Find<UTextBlock>(BP,TEXT("WaveText"));Label(Wave,Heading,26,Cream);
        Place(Status,Wave,FVector2D(54,12),FVector2D(190,37));
        auto* Gold=Find<UTextBlock>(BP,TEXT("GoldText"));Label(Gold,Body,17,FLinearColor(.90f,.76f,.47f,1));
        Place(Status,Gold,FVector2D(54,58),FVector2D(190,26));
        Icon(BP,Status,TEXT("WaveSigil"),TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilWave.MI_SigilWave"),FVector2D(18,20),FVector2D(24),Vermilion);
        Icon(BP,Status,TEXT("GoldSigil"),TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilGold.MI_SigilGold"),FVector2D(21,62),FVector2D(18),FLinearColor(.90f,.76f,.47f,1));
        const TCHAR* Prefixes[]={TEXT("Health"),TEXT("Armor"),TEXT("Experience")};
        const TCHAR* Canvases[]={TEXT("HealthRowCanvas"),TEXT("ArmorRowCanvas"),TEXT("ExperienceRowCanvas")};
        const TCHAR* IconAssets[]={TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilHealth.MI_SigilHealth"),TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilArmor.MI_SigilArmor"),TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilExperience.MI_SigilExperience")};
        const FLinearColor Colors[]={FLinearColor(.85f,.18f,.30f,1),FLinearColor(.34f,.72f,.88f,1),FLinearColor(.92f,.73f,.37f,1)};
        for(int32 Index=0;Index<3;++Index)
        {
            const FString Prefix=Prefixes[Index];auto* Canvas=Find<UCanvasPanel>(BP,Canvases[Index]);if(!Canvas)return false;
            const bool Health=Index==0;const float Y=Health?34:31,Height=Health?20:Index==1?14:7;
            auto* Value=Find<UTextBlock>(BP,*(Prefix+TEXT("Text")));Label(Value,Body,Health?20:17,Index==0?Cream:Index==1?FLinearColor(.66f,.87f,.96f,1):FLinearColor(.89f,.78f,.55f,1));
            Place(Canvas,Value,FVector2D(30,0),FVector2D(Index==2?146:222,28));
            Icon(BP,Canvas,*(Prefix+TEXT("Sigil")),IconAssets[Index],FVector2D(1,2),FVector2D(21),Colors[Index]);
            auto* Main=Find<UProgressBar>(BP,*(Prefix+TEXT("Bar")));Bar(Main,Fill,Height,Colors[Index],false,Index!=2);Place(Canvas,Main,FVector2D(0,Y),FVector2D(366,Height));
            if(auto* Trail=Find<UProgressBar>(BP,*(Prefix+TEXT("DamageBar"))))
            {
                Bar(Trail,Fill,Height,Index==0?FLinearColor(.94f,.64f,.43f,.9f):FLinearColor(.71f,.90f,.97f,.9f),true,false);
                auto* Slot=Cast<UCanvasPanelSlot>(Trail->Slot);Slot->SetPosition(FVector2D(0,Y));Slot->SetSize(FVector2D(366,Height));
                CastChecked<UCanvasPanelSlot>(Main->Slot)->SetZOrder(1);
            }
            auto* Flash=Find<UBorder>(BP,*(Prefix+TEXT("Feedback")));
            if(Flash)
            {
                FSlateBrush Brush=Flash->Background;Brush.OutlineSettings.bUseBrushTransparency=true;Flash->SetBrush(Brush);
                auto* Slot=Cast<UCanvasPanelSlot>(Flash->Slot);Slot->SetPosition(FVector2D(-4,Y-4));Slot->SetSize(FVector2D(374,Height+8));
            }
            if(auto* Change=Find<UTextBlock>(BP,*(Prefix+TEXT("ChangeText"))))
            {
                Label(Change,Body,15,Colors[Index]);
                if(Index==2)Place(Canvas,Change,FVector2D(188,1),FVector2D(178,26));
            }
        }
        auto* DashCanvas=Find<UCanvasPanel>(BP,TEXT("DashCooldownCanvas"));auto* DashLabel=Find<UTextBlock>(BP,TEXT("DashLabel"));
        if(!DashCanvas||!DashLabel)return false;
        Label(DashLabel,Body,16,Muted);Place(DashCanvas,DashLabel,FVector2D(30,0),FVector2D(90,25));
        Icon(BP,DashCanvas,TEXT("DashSigil"),TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilDash.MI_SigilDash"),FVector2D(1,2),FVector2D(21),FLinearColor(.51f,.87f,.96f,1));
        auto* DashText=Find<UTextBlock>(BP,TEXT("DashCooldownText"));Label(DashText,Body,16,FLinearColor(.51f,.87f,.96f,1));
        auto* Dash=Find<UProgressBar>(BP,TEXT("DashCooldownBar"));Bar(Dash,Fill,8,FLinearColor(.38f,.82f,.96f,1),false,false);
        auto* DS=Cast<UCanvasPanelSlot>(Dash->Slot);DS->SetOffsets(FMargin(0,31,0,8));
    }
    else if(Name==TEXT("WBP_BossHealthBar"))
    {
        auto* Canvas=Find<UCanvasPanel>(BP,TEXT("BossPanel"));if(!Canvas)return false;
        auto* Slot=Cast<UCanvasPanelSlot>(Canvas->Slot);Slot->SetAnchors(FAnchors(.5f,0));Slot->SetAlignment(FVector2D(.5f,0));Slot->SetPosition(FVector2D(0,22));Slot->SetSize(FVector2D(760,136));
        Panel(Find<UBorder>(BP,TEXT("BossFrame")),Frame);
        auto* Title=Find<UTextBlock>(BP,TEXT("BossTitle"));Label(Title,Heading,24,Cream);Title->SetText(FText::FromString(TEXT("异变 · 强敌")));Title->SetJustification(ETextJustify::Left);Place(Canvas,Title,FVector2D(60,10),FVector2D(430,36));
        auto* Value=Find<UTextBlock>(BP,TEXT("HealthText"));Label(Value,Body,17,Muted);Value->SetJustification(ETextJustify::Right);Place(Canvas,Value,FVector2D(-24,16),FVector2D(210,28),FAnchors(1,0),FVector2D(1,0));
        Icon(BP,Canvas,TEXT("BossSigil"),TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilWave.MI_SigilWave"),FVector2D(24,16),FVector2D(25),Vermilion);
        for(const TCHAR* BarName:{TEXT("DamageBar"),TEXT("FillBar")})
        {
            const bool Trail=FString(BarName)==TEXT("DamageBar");auto* Widget=Find<UProgressBar>(BP,BarName);
            Bar(Widget,Fill,17,Trail?FLinearColor(.93f,.65f,.42f,1):FLinearColor(.86f,.15f,.25f,1),Trail,!Trail);
            Place(Canvas,Widget,FVector2D(24,57),FVector2D(712,17));
        }
    }
    else if(Name==TEXT("WBP_KillFeedback"))
    {
        auto* Seal=Find<UBorder>(BP,TEXT("SpiritSeal"));auto* Canvas=Find<UCanvasPanel>(BP,TEXT("FeedbackBody"));if(!Seal||!Canvas)return false;
        Panel(Seal,Frame);Seal->SetPadding(FMargin(16,10));
        auto* Slot=Cast<UCanvasPanelSlot>(Seal->Slot);Slot->SetSize(FVector2D(230,88));
        auto* Title=Find<UTextBlock>(BP,TEXT("KillText"));Label(Title,Heading,26,Cream);Place(Canvas,Title,FVector2D(34,-3),FVector2D(170,38));
        auto* Chain=Find<UTextBlock>(BP,TEXT("ChainText"));Label(Chain,Body,16,FLinearColor(.63f,.84f,.88f,1));Place(Canvas,Chain,FVector2D(34,36),FVector2D(170,26));
        Icon(BP,Canvas,TEXT("DefeatSigil"),TEXT("/Game/GameAssets/ArtDirection/BattleHUD/Materials/MI_SigilWave.MI_SigilWave"),FVector2D(1,8),FVector2D(23),Vermilion);
    }
    else
    {
        auto* Cue=Find<UBorder>(BP,TEXT("CuePanel"));auto* Canvas=Find<UCanvasPanel>(BP,TEXT("CueBody"));if(!Cue||!Canvas)return false;
        Panel(Cue,Frame);Cue->SetPadding(FMargin(0));auto* Slot=Cast<UCanvasPanelSlot>(Cue->Slot);Slot->SetAnchors(FAnchors(.5f,0));Slot->SetAlignment(FVector2D(.5f,0));Slot->SetPosition(FVector2D(0,169));Slot->SetSize(FVector2D(490,92));
        auto* Title=Find<UTextBlock>(BP,TEXT("CueTitle"));Label(Title,Heading,27,Cream);Place(Canvas,Title,FVector2D(84,10),FVector2D(390,40));
        auto* Caption=Find<UTextBlock>(BP,TEXT("CueSubtitle"));Label(Caption,Body,16,Muted);Place(Canvas,Caption,FVector2D(86,57),FVector2D(386,26));
        auto* Seal=Find<UImage>(BP,TEXT("CueSeal"));Place(Canvas,Seal,FVector2D(20,20),FVector2D(54));
    }
    return RogueWidgetAuthoring::Finish(BP);
}
