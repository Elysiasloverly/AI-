// 历史风格生成器：仅供显式重建/研究；不会在启动或当前UI维护入口自动运行。
#include "RogueUIThemeBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"

bool URogueUIThemeBuilder::RefineArtPresentation(UWidgetBlueprint* BP)
{
    if(!BP || !BP->WidgetTree)return false;
    BP->Modify();BP->WidgetTree->Modify();
    const FLinearColor Ink(.035f,.035f,.065f,1),Red(.42f,.055f,.075f,1);
    TArray<UWidget*> Widgets;BP->WidgetTree->GetAllWidgets(Widgets);
    for(UWidget* Widget:Widgets)
    {
        if(auto* Button=Cast<UButton>(Widget))
        {
            Button->Modify();FButtonStyle Style=Button->GetStyle();
            Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.95f,.925f,.86f,1),9.f,FLinearColor(.32f,.24f,.24f,.65f),1.f);
            Style.Hovered=FSlateRoundedBoxBrush(FLinearColor(.98f,.84f,.73f,1),9.f,Red,1.6f);
            Style.Pressed=FSlateRoundedBoxBrush(FLinearColor(.84f,.65f,.56f,1),9.f,Red,2.f);
            Style.Disabled=FSlateRoundedBoxBrush(FLinearColor(.64f,.63f,.62f,.8f),9.f,FLinearColor(.28f,.27f,.29f,.45f),1.f);
            Style.NormalForeground=Ink;Style.HoveredForeground=Ink;Style.PressedForeground=Ink;
            Style.DisabledForeground=FLinearColor(.20f,.20f,.24f,1);
            Button->SetStyle(Style);
        }
        else if(auto* Border=Cast<UBorder>(Widget);Border && Border->GetFName()==TEXT("Border_Card"))
        {
            Border->Modify();Border->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.98f,.96f,.90f,1),12.f,FLinearColor(.40f,.25f,.24f,.5f),1.f));
            Border->SetBrushColor(FLinearColor::White);
        }
        else if(auto* Label=Cast<UTextBlock>(Widget))
        {
            if(Label->GetFName()==TEXT("Text_Desc"))
            {Label->Modify();Label->SetLineHeightPercentage(1.2f);}
            // Some designer labels have an explicit white colour and do not inherit Button foreground.
            // Keep them legible on the light paper states without touching non-button headings.
            for(UWidget* Parent=Label->GetParent();Parent;Parent=Parent->GetParent())
            {
                if(Cast<UButton>(Parent))
                {Label->Modify();Label->SetColorAndOpacity(FSlateColor(Ink));break;}
            }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);return true;
}
