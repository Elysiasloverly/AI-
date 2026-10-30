#include "RogueWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Font.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace RogueGuideBuilder
{
const FLinearColor Ink(.065f,.085f,.12f,1), Muted(.23f,.28f,.33f,1), Paper(.94f,.92f,.86f,1), Red(.46f,.10f,.18f,1), Blue(.08f,.27f,.34f,1);
template<class T> T* Make(UWidgetBlueprint* BP,FName Name) { return BP->WidgetTree->ConstructWidget<T>(T::StaticClass(),Name); }
UCanvasPanel* Reset(UWidgetBlueprint* BP)
{
    BP->Modify(); BP->WidgetTree->Modify();
    auto* Root=Make<UCanvasPanel>(BP,TEXT("GuideRoot")); BP->WidgetTree->RootWidget=Root; return Root;
}
void Stretch(UCanvasPanel* Root,UWidget* W,FMargin Margin=FMargin(0))
{
    auto* Slot=Root->AddChildToCanvas(W); Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(Margin);
}
UTextBlock* Text(UWidgetBlueprint* BP,FName Name,const FString& Value,int32 Font=22,FLinearColor Color=Ink)
{
    auto* W=Make<UTextBlock>(BP,Name);W->SetText(FText::FromString(Value));
    W->SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(),Font));W->SetColorAndOpacity(Color);
    W->SetAutoWrapText(true); W->SetLineHeightPercentage(1.35f); W->SetVisibility(ESlateVisibility::HitTestInvisible);return W;
}
UBorder* Frame(UWidgetBlueprint* BP,FName Name,FLinearColor Color,FMargin Padding=FMargin(24))
{
    auto* W=Make<UBorder>(BP,Name); W->SetBrush(FSlateRoundedBoxBrush(Color,10.f)); W->SetPadding(Padding);return W;
}
void Add(UVerticalBox* Box,UWidget* W,FMargin Margin=FMargin(0,0,0,12),bool Fill=false)
{
    auto* Slot=Box->AddChildToVerticalBox(W);Slot->SetPadding(Margin); if(Fill)Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}
UButton* Button(UWidgetBlueprint* BP,FName Name,const FString& Label)
{
    auto* W=Make<UButton>(BP,Name);FButtonStyle S;
    S.Normal=FSlateRoundedBoxBrush(FLinearColor(.84f,.87f,.85f,1),8.f);
    S.Hovered=FSlateRoundedBoxBrush(FLinearColor(.69f,.82f,.85f,1),8.f);
    S.Pressed=FSlateRoundedBoxBrush(FLinearColor(.59f,.72f,.76f,1),8.f);
    S.Disabled=FSlateRoundedBoxBrush(FLinearColor(.78f,.65f,.62f,1),8.f);
    W->SetStyle(S);
    auto* T=Text(BP,FName(*(Name.ToString()+TEXT("Label"))),Label,22);T->SetJustification(ETextJustify::Center);W->AddChild(T);
    Cast<UButtonSlot>(T->Slot)->SetPadding(FMargin(20,13));return W;
}
void Finish(UWidgetBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();
}
}

bool URogueWidgetBuilder::BuildVictoryScreen(UWidgetBlueprint* BP)
{
    using namespace RogueGuideBuilder;
    if(!BP||!BP->WidgetTree)return false;
    auto* Root=Reset(BP);Stretch(Root,Frame(BP,TEXT("Backdrop"),FLinearColor(.025f,.04f,.075f,1.f)));
    auto* Panel=Frame(BP,TEXT("VictoryPanel"),Paper,FMargin(58,42));
    auto* Slot=Root->AddChildToCanvas(Panel);Slot->SetAnchors(FAnchors(.5f,.5f));Slot->SetAlignment(FVector2D(.5f,.5f));Slot->SetSize(FVector2D(980,720));
    auto* Box=Make<UVerticalBox>(BP,TEXT("VictoryContent"));Panel->SetContent(Box);
    Add(Box,Text(BP,TEXT("SealText"),TEXT("百波终章  /  SPELL CLEAR"),20,Red),FMargin(0,0,0,18));
    Add(Box,Text(BP,TEXT("TitleText"),TEXT("异变解决"),52,Red),FMargin(0,0,0,12));
    Add(Box,Text(BP,TEXT("SummaryText"),TEXT("抵达第 100 波，神社的长夜终于迎来曙光。"),24),FMargin(0,0,0,36));
    auto* Stats=Make<UHorizontalBox>(BP,TEXT("ResultStats"));Add(Box,Stats,FMargin(0,0,0,32));
    const TCHAR* Names[]={TEXT("Wave"),TEXT("Time"),TEXT("Kills")};const TCHAR* Labels[]={TEXT("抵达波次"),TEXT("战斗用时"),TEXT("全队击败")};
    for(int32 I=0;I<3;++I)
    {
        const FString Name=Names[I];auto* Card=Frame(BP,FName(*(Name+TEXT("Card"))),FLinearColor(.85f,.88f,.85f,1));
        auto* C=Make<UVerticalBox>(BP,FName(*(Name+TEXT("Content"))));Card->SetContent(C);
        Add(C,Text(BP,FName(*(Name+TEXT("Label"))),Labels[I],20,Muted));
        Add(C,Text(BP,FName(*(Name+TEXT("Text"))),I==0?TEXT("100"):TEXT("—"),34,Blue),FMargin(0));
        auto* CS=Stats->AddChildToHorizontalBox(Card);CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));CS->SetPadding(FMargin(0,0,I<2?16:0,0));
    }
    Add(Box,Text(BP,TEXT("SaveHintText"),TEXT("已保存的冰砖与永久解锁进度会保留。"),20,Muted),FMargin(0,0,0,24));
    Add(Box,Button(BP,TEXT("MainMenuButton"),TEXT("返回主菜单")),FMargin(0));
    Finish(BP);return true;
}

bool URogueWidgetBuilder::BuildTutorialFromJson(UWidgetBlueprint* BP,const FString& Json)
{
    using namespace RogueGuideBuilder;
    if(!BP||!BP->WidgetTree)return false;
    TSharedPtr<FJsonObject> Data;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Data)||!Data.IsValid())return false;
    const TArray<TSharedPtr<FJsonValue>>* Chapters;
    if(!Data->TryGetArrayField(TEXT("chapters"),Chapters)||Chapters->Num()!=6)return false;
    auto* Root=Reset(BP);Stretch(Root,Frame(BP,TEXT("Backdrop"),FLinearColor(.03f,.045f,.08f,1)));
    auto* Panel=Frame(BP,TEXT("GuidePanel"),Paper,FMargin(32));Stretch(Root,Panel,FMargin(48,32));
    auto* Box=Make<UVerticalBox>(BP,TEXT("GuideContent"));Panel->SetContent(Box);
    Add(Box,Text(BP,TEXT("TitleText"),Data->GetStringField(TEXT("title")),36,Red),FMargin(0,0,0,8));
    Add(Box,Text(BP,TEXT("GuideHint"),TEXT("选择左侧章节  ·  鼠标滚轮阅读  ·  先学躲避，再组合自己的符卡"),20,Muted),FMargin(0,0,0,26));
    auto* Row=Make<UHorizontalBox>(BP,TEXT("GuideColumns"));Add(Box,Row,FMargin(0),true);
    auto* NavSize=Make<USizeBox>(BP,TEXT("ChapterNavigationSize"));NavSize->SetWidthOverride(260);
    auto* Nav=Make<UVerticalBox>(BP,TEXT("ChapterNavigation"));NavSize->SetContent(Nav);
    Row->AddChildToHorizontalBox(NavSize)->SetPadding(FMargin(0,0,28,0));
    auto* Switcher=Make<UWidgetSwitcher>(BP,TEXT("ChapterSwitcher"));auto* SS=Row->AddChildToHorizontalBox(Switcher);SS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    for(int32 I=0;I<6;++I)
    {
        const auto Chapter=(*Chapters)[I]->AsObject();const FString Prefix=FString::Printf(TEXT("Chapter%d"),I);
        Add(Nav,Button(BP,FName(*(Prefix+TEXT("Button"))),Chapter->GetStringField(TEXT("nav"))),FMargin(0,0,0,12));
        auto* Scroll=Make<UScrollBox>(BP,FName(*(Prefix+TEXT("Scroll"))));Switcher->AddChild(Scroll);
        auto* Page=Make<UVerticalBox>(BP,FName(*(Prefix+TEXT("Page"))));Scroll->AddChild(Page);
        Add(Page,Text(BP,FName(*(Prefix+TEXT("Title"))),Chapter->GetStringField(TEXT("title")),30,Blue),FMargin(0,0,0,16));
        int32 J=0;
        for(const auto& V:Chapter->GetArrayField(TEXT("cards")))
        {
            const auto Card=V->AsObject();const FString Name=Prefix+FString::Printf(TEXT("Card%d"),J++);
            auto* BG=Frame(BP,FName(*Name),FLinearColor(.88f,.89f,.84f,1),FMargin(24,18));
            auto* Content=Make<UVerticalBox>(BP,FName(*(Name+TEXT("Content"))));BG->SetContent(Content);
            Add(Content,Text(BP,FName(*(Name+TEXT("Heading"))),Card->GetStringField(TEXT("heading")),24,Red),FMargin(0,0,0,10));
            Add(Content,Text(BP,FName(*(Name+TEXT("Body"))),Card->GetStringField(TEXT("body")),22),FMargin(0));
            Add(Page,BG,FMargin(0,0,16,16));
        }
    }
    Add(Nav,Make<USizeBox>(BP,TEXT("NavigationSpace")),FMargin(0),true);
    Add(Nav,Button(BP,TEXT("CloseButton"),TEXT("返回主菜单")),FMargin(0));
    Finish(BP);return true;
}
