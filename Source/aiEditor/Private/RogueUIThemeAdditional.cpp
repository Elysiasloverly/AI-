#include "RogueUIThemeBuilder.h"
#include "RogueAudioSetup.h"
#include "RogueWidgetBuilder.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UI/Widgets/RogueShopWidgetBase.h"
#include "UI/Widgets/RogueNightTransitionWidgetBase.h"
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/Widgets/RogueSettingsMenuWidgetBase.h"
#include "UI/Widgets/RoguePauseMenuWidgetBase.h"
#include "UI/Widgets/RogueDeathScreenWidgetBase.h"
#include "UI/Widgets/RogueReviveCountdownWidgetBase.h"
#include "UI/Widgets/RogueMultiplayerWidgetBase.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "UI/Widgets/RogueBossHealthBarWidgetBase.h"

namespace RogueAdditionalUI
{
    const FLinearColor Ink(.035f,.025f,.045f,1);
    void At(UCanvasPanel* Parent,UWidget* Child,FVector2D Pos,FVector2D Size,
        FAnchors Anchors=FAnchors(0,0),FVector2D Alignment=FVector2D::ZeroVector)
    {
        check(Child);Child->RemoveFromParent();UCanvasPanelSlot* Slot=Parent->AddChildToCanvas(Child);
        Slot->SetAnchors(Anchors);Slot->SetAlignment(Alignment);Slot->SetPosition(Pos);Slot->SetSize(Size);
    }
    void Fill(UCanvasPanel* Parent,UWidget* Child)
    {
        At(Parent,Child,FVector2D::ZeroVector,FVector2D::ZeroVector,FAnchors(0,0,1,1));
    }
    void Font(UTextBlock* Text,int32 Size)
    {
        if(!Text)return;FSlateFontInfo Info=Text->GetFont();Info.Size=Size;Text->SetFont(Info);
    }
    // Reparent existing named widgets so Blueprint events and native BindWidget references survive.
    struct FLayout
    {
        UWidgetBlueprint* BP;TMap<FName,UWidget*> Existing;UCanvasPanel* Root;
        explicit FLayout(UWidgetBlueprint* In):BP(In)
        {
            TArray<UWidget*> Widgets;BP->WidgetTree->GetAllWidgets(Widgets);
            for(UWidget* W:Widgets)Existing.Add(W->GetFName(),W);
            BP->Modify();BP->WidgetTree->Modify();
            Root=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RestyledRoot"));
            BP->WidgetTree->RootWidget=Root;
        }
        UWidget* Get(FName Name)const{return Existing.FindRef(Name);}
        UTextBlock* Label(FName Name,const TCHAR* Default,int32 Size=20)
        {
            UTextBlock* W=Cast<UTextBlock>(Get(Name));
            if(!W){W=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);W->SetText(FText::FromString(Default));}
            W->SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(),Size));W->SetColorAndOpacity(Ink);return W;
        }
        UCanvasPanel* Panel(FVector2D Size,bool bPage=false,FName Name=TEXT("RestyledPanel"))
        {
            UBorder* Background=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),bPage?TEXT("Background"):TEXT("ModalShade"));
            Background->SetBrushColor(FLinearColor(.01f,.012f,.03f,.72f));Fill(Root,Background);
            UBorder* Paper=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);
            Paper->SetPadding(FMargin(40));Paper->SetHorizontalAlignment(HAlign_Fill);Paper->SetVerticalAlignment(VAlign_Fill);
            At(Root,Paper,FVector2D::ZeroVector,Size,FAnchors(.5f,.5f),FVector2D(.5f,.5f));
            UCanvasPanel* Body=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RestyledBody"));Paper->AddChild(Body);return Body;
        }
        void Put(UCanvasPanel* Body,FName Name,FVector2D Pos,FVector2D Size)
        {
            At(Body,Get(Name),Pos,Size);
        }
        void Button(UCanvasPanel* Body,FName Name,FVector2D Pos,FVector2D Size)
        {
            UButton* W=CastChecked<UButton>(Get(Name));
            if(UTextBlock* Text=Cast<UTextBlock>(W->GetContent()))Font(Text,20);
            At(Body,W,Pos,Size);
        }
        void Input(UCanvasPanel* Body,FName Name,FVector2D Pos,FVector2D Size)
        {
            UEditableTextBox* W=CastChecked<UEditableTextBox>(Get(Name));
            FEditableTextBoxStyle Style=W->WidgetStyle;Style.SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(),20));
            Style.SetForegroundColor(Ink);Style.SetFocusedForegroundColor(Ink);Style.SetReadOnlyForegroundColor(Ink);
            Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(FLinearColor(.98f,.96f,.92f),3.f,FLinearColor(.58f,.53f,.48f),1));
            Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(FLinearColor(1,.98f,.94f),3.f,FLinearColor(.5f,.055f,.09f),1));
            Style.SetBackgroundImageFocused(Style.BackgroundImageHovered);Style.SetBackgroundImageReadOnly(Style.BackgroundImageNormal);
            Style.SetPadding(FMargin(14,10));W->WidgetStyle=Style;At(Body,W,Pos,Size);
        }
    };
}

bool URogueUIThemeBuilder::BuildAdditionalLayouts(UWidgetBlueprint* BP)
{
    using namespace RogueAdditionalUI;
    if(!BP||!BP->WidgetTree)return false;
    const FString Name=BP->GetName();
    static const TSet<FString> Supported={TEXT("WBP_SettingsMenu"),TEXT("WBP_PauseMenu"),TEXT("WBP_DeathScreen"),TEXT("WBP_ResurrectionScreen"),TEXT("WBP_MultiplayerMenu"),TEXT("WBP_RoomBrowser"),TEXT("WBP_Lobby"),TEXT("WBP_RoomEntry"),TEXT("WBP_LobbyPlayerEntry"),TEXT("WBP_Tutorial"),TEXT("WBP_NightTransition")};
    if(!Supported.Contains(Name))return false;
    if(Name==TEXT("WBP_SettingsMenu") && !URogueAudioSetup::AddMusicControls(BP)) return false;
    FLayout L(BP);
    if(Name==TEXT("WBP_SettingsMenu"))
    {
        UCanvasPanel* Body=L.Panel(FVector2D(1000,810));
        At(Body,L.Label(TEXT("Text_Title"),TEXT("设置"),32),FVector2D::ZeroVector,FVector2D(920,48));
        At(Body,L.Label(TEXT("SettingsHint"),TEXT("音效与背景音乐独立调节，即时生效并保存。"),17),FVector2D(0,60),FVector2D(920,30));
        for(int32 I=0;I<6;++I)
        {
            const int32 VisualRow=I==0?0:I==5?1:I+1;
            const float Y=112+VisualRow*78;
            UTextBlock* Label=L.Label(*FString::Printf(TEXT("Text_Label%d"),I),TEXT(""));
            At(Body,Label,FVector2D(0,Y+12),FVector2D(360,32));
            if(I==3)
            {
                UButton* Mode=CastChecked<UButton>(L.Get(TEXT("Button_Row3Main")));
                UTextBlock* Value=L.Label(TEXT("Text_Value3"),TEXT(""));
                Value->RemoveFromParent();Mode->ClearChildren();Mode->AddChild(Value);Value->SetJustification(ETextJustify::Center);
                L.Button(Body,TEXT("Button_Row3Main"),FVector2D(450,Y),FVector2D(430,52));
                Font(Cast<UTextBlock>(L.Get(TEXT("Text_Value3"))),20);
            }
            else
            {
                L.Button(Body,I==5?FName(TEXT("Button_MusicMinus")):FName(*FString::Printf(TEXT("Button_Row%dMinus"),I)),FVector2D(450,Y),FVector2D(52,52));
                UTextBlock* Value=L.Label(*FString::Printf(TEXT("Text_Value%d"),I),TEXT(""));Value->SetJustification(ETextJustify::Center);
                At(Body,Value,FVector2D(514,Y+12),FVector2D(302,32));
                L.Button(Body,I==5?FName(TEXT("Button_MusicPlus")):FName(*FString::Printf(TEXT("Button_Row%dPlus"),I)),FVector2D(828,Y),FVector2D(52,52));
            }
        }
        UTextBlock* Status=L.Label(TEXT("Text_Status"),TEXT(""),17);Status->SetAutoWrapText(true);
        At(Body,Status,FVector2D(0,574),FVector2D(920,48));
        L.Button(Body,TEXT("Button_Back"),FVector2D(0,646),FVector2D(180,52));
        L.Button(Body,TEXT("Button_Apply"),FVector2D(660,646),FVector2D(220,52));
    }
    else if(Name==TEXT("WBP_PauseMenu"))
    {
        UCanvasPanel* Body=L.Panel(FVector2D(620,530));
        UTextBlock* Title=L.Label(TEXT("Text_Title"),TEXT("菜单"),32);Title->SetJustification(ETextJustify::Center);
        At(Body,Title,FVector2D::ZeroVector,FVector2D(540,48));
        UTextBlock* Hint=L.Label(TEXT("PauseHint"),TEXT("战斗仍在继续，请留意周围的敌人。"),17);Hint->SetJustification(ETextJustify::Center);
        At(Body,Hint,FVector2D(0,64),FVector2D(540,32));
        const FName Buttons[]={TEXT("Button_Resume"),TEXT("Button_Settings"),TEXT("Button_Quit")};
        for(int32 I=0;I<3;++I)L.Button(Body,Buttons[I],FVector2D(60,134+I*84),FVector2D(420,58));
        UTextBlock* Footer=L.Label(TEXT("PauseFooter"),TEXT("Esc  返回游戏"),16);Footer->SetJustification(ETextJustify::Center);
        At(Body,Footer,FVector2D(0,408),FVector2D(540,28));
    }
    else if(Name==TEXT("WBP_DeathScreen"))
    {
        UCanvasPanel* Body=L.Panel(FVector2D(940,500));
        UTextBlock* Title=L.Label(TEXT("Text_Title"),TEXT("本轮结束"),34);Title->SetJustification(ETextJustify::Center);
        At(Body,Title,FVector2D::ZeroVector,FVector2D(860,56));
        UTextBlock* Summary=L.Label(TEXT("Text_Summary"),TEXT(""),22);Summary->SetAutoWrapText(true);Summary->SetJustification(ETextJustify::Center);Summary->SetLineHeightPercentage(1.5f);
        At(Body,Summary,FVector2D(30,100),FVector2D(800,170));
        L.Button(Body,TEXT("Button_Restart"),FVector2D(0,330),FVector2D(260,56));
        L.Button(Body,TEXT("MainMenuButton"),FVector2D(300,330),FVector2D(260,56));
        L.Button(Body,TEXT("Button_Quit"),FVector2D(600,330),FVector2D(260,56));
    }
    else if(Name==TEXT("WBP_ResurrectionScreen"))
    {
        UCanvasPanel* Body=L.Panel(FVector2D(760,340));
        UTextBlock* Title=L.Label(TEXT("TextBlock_845"),TEXT("濒死状态"),32);Title->SetJustification(ETextJustify::Center);
        At(Body,Title,FVector2D::ZeroVector,FVector2D(680,48));
        UTextBlock* Hint=L.Label(TEXT("TextBlock_920"),TEXT("距离复活还有"),20);Hint->SetJustification(ETextJustify::Center);
        At(Body,Hint,FVector2D(0,68),FVector2D(680,32));
        UTextBlock* Count=L.Label(TEXT("TextBlock"),TEXT("10"),48);Count->SetJustification(ETextJustify::Center);
        At(Body,Count,FVector2D(0,110),FVector2D(680,80));
        UTextBlock* Footer=L.Label(TEXT("ReviveFooter"),TEXT("等待队友守住战场，倒计时结束后自动复苏。"),17);Footer->SetJustification(ETextJustify::Center);
        At(Body,Footer,FVector2D(0,218),FVector2D(680,30));
    }
    else if(Name==TEXT("WBP_MultiplayerMenu")||Name==TEXT("WBP_RoomBrowser")||Name==TEXT("WBP_Lobby"))
    {
        const bool bChoice=Name==TEXT("WBP_MultiplayerMenu"),bBrowser=Name==TEXT("WBP_RoomBrowser");
        const float Width=bChoice?920:1040;
        UCanvasPanel* Body=L.Panel(bChoice?FVector2D(1000,680):FVector2D(1120,800),true);
        At(Body,L.Label(TEXT("RoomTitle"),TEXT("房间大厅"),32),FVector2D::ZeroVector,FVector2D(Width,48));
        UTextBlock* Desc=L.Label(TEXT("Description"),TEXT(""),18);Desc->SetAutoWrapText(true);
        At(Body,Desc,FVector2D(0,58),FVector2D(Width,44));
        if(bChoice)
        {
            At(Body,L.Label(TEXT("NameLabel"),TEXT("玩家昵称")),FVector2D(0,120),FVector2D(920,30));
            L.Input(Body,TEXT("PlayerNameInput"),FVector2D(0,158),FVector2D(920,52));
            At(Body,L.Label(TEXT("RoomLabel"),TEXT("房间名称 · 创建房间时使用")),FVector2D(0,242),FVector2D(920,30));
            L.Input(Body,TEXT("RoomNameInput"),FVector2D(0,280),FVector2D(920,52));
            L.Button(Body,TEXT("CreateRoomButton"),FVector2D(0,386),FVector2D(440,64));
            L.Button(Body,TEXT("JoinRoomButton"),FVector2D(480,386),FVector2D(440,64));
            L.Button(Body,TEXT("BackButton"),FVector2D(0,528),FVector2D(180,52));
        }
        else if(bBrowser)
        {
            At(Body,L.Label(TEXT("NameLabel"),TEXT("玩家昵称")),FVector2D(0,110),FVector2D(400,28));
            L.Input(Body,TEXT("PlayerNameInput"),FVector2D(0,144),FVector2D(600,52));
            L.Button(Body,TEXT("RefreshButton"),FVector2D(680,144),FVector2D(360,52));
            At(Body,L.Label(TEXT("RoomsLabel"),TEXT("局域网房间")),FVector2D(0,218),FVector2D(1040,30));
            L.Put(Body,TEXT("Entries"),FVector2D(0,260),FVector2D(1040,242));
            At(Body,L.Label(TEXT("IPLabel"),TEXT("直连房主 IP · 例如 192.168.1.10:7777"),18),FVector2D(0,530),FVector2D(1040,30));
            L.Input(Body,TEXT("IPInput"),FVector2D(0,568),FVector2D(730,52));
            L.Button(Body,TEXT("JoinIPButton"),FVector2D(770,568),FVector2D(270,52));
            L.Button(Body,TEXT("BackButton"),FVector2D(0,666),FVector2D(180,52));
        }
        else
        {
            At(Body,L.Label(TEXT("AddressText"),TEXT("房主地址："),18),FVector2D(0,112),FVector2D(1040,30));
            At(Body,L.Label(TEXT("PlayersLabel"),TEXT("玩家名单")),FVector2D(0,176),FVector2D(1040,30));
            L.Put(Body,TEXT("Entries"),FVector2D(0,218),FVector2D(1040,310));
            L.Button(Body,TEXT("ReadyButton"),FVector2D(0,562),FVector2D(260,56));
            L.Button(Body,TEXT("StartButton"),FVector2D(300,562),FVector2D(440,56));
            L.Button(Body,TEXT("BackButton"),FVector2D(780,562),FVector2D(260,56));
        }
        UTextBlock* Status=L.Label(TEXT("StatusText"),TEXT(""),17);Status->SetAutoWrapText(true);
        At(Body,Status,bChoice?FVector2D(210,528):(bBrowser?FVector2D(210,666):FVector2D(0,652)),FVector2D(bChoice?710:(bBrowser?830:1040),52));
    }
    else if(Name==TEXT("WBP_RoomEntry")||Name==TEXT("WBP_LobbyPlayerEntry"))
    {
        USizeBox* Size=BP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("RestyledRowSize"));Size->SetHeightOverride(80);BP->WidgetTree->RootWidget=Size;
        UBorder* Paper=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("RestyledRowPaper"));Paper->SetPadding(FMargin(0));Size->AddChild(Paper);
        Paper->SetHorizontalAlignment(HAlign_Fill);Paper->SetVerticalAlignment(VAlign_Fill);
        UCanvasPanel* Body=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RestyledRowBody"));Paper->AddChild(Body);
        UTextBlock* Title=L.Label(TEXT("TitleText"),TEXT(""),20);Title->SetClipping(EWidgetClipping::ClipToBounds);
        At(Body,Title,FVector2D(20,23),FVector2D(510,34));
        At(Body,L.Label(TEXT("DetailText"),TEXT(""),18),FVector2D(-450,26),FVector2D(270,30),FAnchors(1,0));
        L.Button(Body,TEXT("ActionButton"),FVector2D(-160,16),FVector2D(140,48));
        CastChecked<UCanvasPanelSlot>(L.Get(TEXT("ActionButton"))->Slot)->SetAnchors(FAnchors(1,0));
    }
    else if(Name==TEXT("WBP_Tutorial"))
    {
        UCanvasPanel* Body=L.Panel(FVector2D(1140,820),true,TEXT("TutorialPanel"));
        At(Body,L.Label(TEXT("TitleText"),TEXT("游戏指南"),30),FVector2D::ZeroVector,FVector2D(1060,50));
        L.Put(Body,TEXT("TutorialScroll"),FVector2D(0,88),FVector2D(1060,560));
        UTextBlock* Text=CastChecked<UTextBlock>(L.Get(TEXT("TutorialText")));Font(Text,19);Text->SetAutoWrapText(true);Text->SetLineHeightPercentage(1.35f);
        L.Button(Body,TEXT("CloseButton"),FVector2D(840,684),FVector2D(220,52));
    }
    else if(Name==TEXT("WBP_NightTransition"))
    {
        // Non-modal notice: no fullscreen mask, leaves the battlefield visible and interactive.
        UBorder* Panel=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("NoticeBackground"));Panel->SetPadding(FMargin(32));Panel->SetHorizontalAlignment(HAlign_Fill);Panel->SetVerticalAlignment(VAlign_Fill);
        At(L.Root,Panel,FVector2D(0,140),FVector2D(960,244),FAnchors(.5f,0),FVector2D(.5f,0));
        UCanvasPanel* Body=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("NoticeBody"));Panel->AddChild(Body);
        UTextBlock* Title=L.Label(TEXT("TitleText"),TEXT("暮色将至"),26);Title->SetJustification(ETextJustify::Center);At(Body,Title,FVector2D::ZeroVector,FVector2D(896,40));
        UTextBlock* Count=L.Label(TEXT("CountdownText"),TEXT("距离黑夜降临：30 秒"),32);Count->SetJustification(ETextJustify::Center);At(Body,Count,FVector2D(0,50),FVector2D(896,54));
        UTextBlock* Desc=L.Label(TEXT("DescriptionText"),TEXT(""),18);Desc->SetAutoWrapText(true);Desc->SetJustification(ETextJustify::Center);At(Body,Desc,FVector2D(20,120),FVector2D(856,60));
    }
    else return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status!=BS_Error;
}

UUserWidget* CreateCardPagePreview(APlayerController* Owner,int32 Page);
UUserWidget* URogueUIThemeBuilder::CreatePagePreview(APlayerController* Owner,int32 Page)
{
    if(!Owner)return nullptr;if(Page<=2)return CreateCardPagePreview(Owner,Page);
    if(Page==13)
    {
        // Match production HUD layers: the notice is below shop and ignores all pointer hit tests.
        UUserWidget* Shop=CreateCardPagePreview(Owner,1);if(!Shop)return nullptr;
        Shop->RemoveFromParent();Shop->AddToViewport(64);
        UClass* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/UMG/Menus/InGame/WBP_NightTransition.WBP_NightTransition_C"));
        UUserWidget* Night=CreateWidget<UUserWidget>(Owner,Class);if(Night){Night->AddToViewport(60);Night->SetVisibility(ESlateVisibility::HitTestInvisible);}
        return Shop;
    }
    const TCHAR* Paths[]={TEXT("Common/WBP_SettingsMenu"),TEXT("InGame/WBP_PauseMenu"),TEXT("InGame/WBP_DeathScreen"),TEXT("InGame/WBP_ResurrectionScreen"),TEXT("Multiplayer/WBP_MultiplayerMenu"),TEXT("Multiplayer/WBP_RoomBrowser"),TEXT("Multiplayer/WBP_Lobby"),TEXT("MainMenu/WBP_Tutorial"),TEXT("InGame/WBP_NightTransition")};
    FString Path=Page==12?TEXT("/Game/UMG/HUD/WBP_PlayerVitals"):FString(TEXT("/Game/UMG/Menus/"))+Paths[FMath::Clamp(Page-3,0,8)];
    const FString ClassPath=Path+TEXT(".")+FPaths::GetCleanFilename(Path)+TEXT("_C");
    UUserWidget* W=CreateWidget<UUserWidget>(Owner,LoadClass<UUserWidget>(nullptr,*ClassPath));if(!W)return nullptr;W->AddToViewport(200);
    if(auto* Settings=Cast<URogueSettingsMenuWidgetBase>(W))
    {
        FRogueSettingsMenuViewData V;V.bVisible=true;V.TitleText=FText::FromString(TEXT("设置"));V.bCanApply=true;V.StatusText=FText::FromString(TEXT("音量即时保存；画面修改需点击应用。"));
        const TCHAR* Labels[]={TEXT("音效音量"),TEXT("画质等级"),TEXT("分辨率"),TEXT("显示模式"),TEXT("限制帧率"),TEXT("背景音乐音量")};const TCHAR* Values[]={TEXT("80%"),TEXT("高"),TEXT("1920 × 1080"),TEXT("窗口模式"),TEXT("120 FPS"),TEXT("60%")};
        for(int32 I=0;I<6;++I){FRogueSettingsRowViewData R;R.LabelText=FText::FromString(Labels[I]);R.ValueText=FText::FromString(Values[I]);R.bUseAdjustButtons=I!=3;V.Rows.Add(R);}Settings->UpdateSettingsView(V);
    }
    if(auto* Pause=Cast<URoguePauseMenuWidgetBase>(W)){FRoguePauseMenuViewData V;V.bVisible=true;V.TitleText=FText::FromString(TEXT("稍作休整"));Pause->UpdatePauseView(V);}
    if(auto* Death=Cast<URogueDeathScreenWidgetBase>(W)){FRogueDeathViewData V;V.bVisible=true;V.TitleText=FText::FromString(TEXT("本轮结束"));V.SummaryText=FText::FromString(TEXT("存活 1260 秒  |  击败 2048 个敌人\n全员进入濒死状态，点击“再来一局”开始新一轮。"));Death->UpdateDeathView(V);}
    if(auto* Revive=Cast<URogueReviveCountdownWidgetBase>(W))Revive->UpdateReviveCountdown(7,true);
    if(auto* Menu=Cast<URogueMultiplayerWidgetBase>(W))
    {
        Menu->Page=0; // Sample rows only; no searching, joining, readiness RPCs or network side effects.
        auto Set=[W](FName Name,const TCHAR* Value){if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(Name)))T->SetText(FText::FromString(Value));};
        if(Page==8||Page==9)
        {
            UScrollBox* Entries=Cast<UScrollBox>(W->GetWidgetFromName(TEXT("Entries")));if(Entries)Entries->ClearChildren();
            for(int32 I=0;I<3&&Entries;++I)
            {
                auto* Row=CreateWidget<URogueRoomEntryWidgetBase>(Owner,Menu->EntryWidgetClass);Entries->AddChild(Row);
                if(Page==8){FRogueRoomInfo Room;Room.Index=I;Room.Name=I==0?TEXT("博丽神社 · 一起守住幻想乡"):I==1?TEXT("雾之湖的伙伴"):TEXT("红魔馆夜游");Room.Players=I+1;Room.Capacity=4;Room.Ping=I*7+3;Row->SetRoom(Room);}
                else Row->SetPlayer(I==0?TEXT("灵梦"):I==1?TEXT("魔理沙"):TEXT("琪露诺"),I!=2,I==0);
            }
        }
        if(Page==9){if(auto* Start=Cast<UButton>(W->GetWidgetFromName(TEXT("StartButton"))))Start->SetIsEnabled(false);Set(TEXT("RoomTitle"),TEXT("博丽神社 · 3 / 4 人"));Set(TEXT("AddressText"),TEXT("房主地址：192.168.1.10:7777"));Set(TEXT("StatusText"),TEXT("还有玩家未准备，全部准备后由房主开始。"));}
        if(Page==8)Set(TEXT("StatusText"),TEXT("找到 3 个房间。选择房间加入，或输入房主 IP。"));
    }
    if(auto* Vitals=Cast<URogueVitalsWidgetBase>(W))
    {
        // Preview replaces the HUD's existing instance so demonstration values never draw twice.
        for(TObjectIterator<URogueVitalsWidgetBase> It;It;++It)
            if(*It!=Vitals && It->GetWorld()==Owner->GetWorld() && It->GetOwningPlayer()==Owner && It->IsInViewport())It->RemoveFromParent();
        FRoguePlayerVitalsView V;V.bDashUsable=true;V.bDashReady=true;V.DashCooldownPercent=1;V.bVisible=true;V.bHasArmor=true;V.CurrentHealth=72;V.MaxHealth=100;V.HealthPercent=.72f;V.CurrentArmor=28;V.MaxArmor=50;V.ArmorPercent=.56f;V.Experience=8;V.ExperienceToNextLevel=20;V.ExperiencePercent=.4f;V.Level=5;V.CurrentWave=12;V.Money=348;Vitals->UpdateVitals(V);
        UClass* BossClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/UMG/HUD/WBP_BossHealthBar.WBP_BossHealthBar_C"));
        auto* Boss=CreateWidget<URogueBossHealthBarWidgetBase>(Owner,BossClass);Boss->AddToViewport(201);FRogueBossHealthView B;B.bVisible=true;B.SpawnId=FGuid::NewGuid();B.CurrentHealth=1750;B.MaxHealth=3000;Boss->UpdateBossHealth(B);
    }
    return W;
}


FString URogueUIThemeBuilder::InspectMenuLayers(APlayerController* Owner)
{
    if(!Owner)return TEXT("{}");
    UUserWidget* Shop=nullptr;UUserWidget* Night=nullptr;
    for(TObjectIterator<UUserWidget> It;It;++It)
    {
        if(It->GetWorld()!=Owner->GetWorld()||It->GetOwningPlayer()!=Owner||!It->IsInViewport()||!It->IsVisible())continue;
        if(It->IsA<URogueShopWidgetBase>())Shop=*It;
        if(It->IsA<URogueNightTransitionWidgetBase>())Night=*It;
    }
    auto* Viewport=UGameViewportSubsystem::Get(Owner->GetWorld());
    if(!Shop||!Night||!Viewport)return TEXT("{}");
    const int32 ShopZ=Viewport->GetWidgetSlot(Shop).ZOrder,NightZ=Viewport->GetWidgetSlot(Night).ZOrder;
    const auto* Close=Cast<UButton>(Shop->GetWidgetFromName(TEXT("Button_Close")));
    const auto* Title=Shop->GetWidgetFromName(TEXT("Text_Title"));
    return FString::Printf(TEXT("{\"night_z\":%d,\"shop_z\":%d,\"shop_above_countdown\":%s,\"countdown_ignores_input\":%s,\"shop_close_enabled\":%s,\"shop_title_laid_out\":%s}"),
        NightZ,ShopZ,ShopZ>NightZ?TEXT("true"):TEXT("false"),Night->GetVisibility()==ESlateVisibility::HitTestInvisible?TEXT("true"):TEXT("false"),
        Close&&Close->GetIsEnabled()?TEXT("true"):TEXT("false"),Title&&Title->GetCachedGeometry().GetLocalSize().X>0?TEXT("true"):TEXT("false"));
}
