#include "UI/RogueWidgetAuthoring.h"
#include "RogueUIThemeBuilder.h"
#include "RogueWidgetBuilder.h"
#include "Engine/Font.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
bool URogueUIThemeBuilder::RefineCardDetails(UWidgetBlueprint* BP)
{
    if(!BP||!BP->WidgetTree)return false;const FString Name=BP->GetName();
    if(Name!=TEXT("WBP_Shop")&&Name!=TEXT("WBP_ShopOfferCard")&&Name!=TEXT("WBP_UpgradeCard"))return false;
    BP->Modify();BP->WidgetTree->Modify();auto Find=[&](const TCHAR* N){return BP->WidgetTree->FindWidget(FName(N));};
    auto Rect=[&](const TCHAR* N,FVector2D Pos,FVector2D Size)
    {if(auto* W=Find(N))if(auto* S=Cast<UCanvasPanelSlot>(W->Slot)){S->SetPosition(Pos);S->SetSize(Size);}};
    if(Name==TEXT("WBP_Shop"))
    {
        if(auto* W=Find(TEXT("Border_Background")))if(auto* S=Cast<UCanvasPanelSlot>(W->Slot))S->SetSize(FVector2D(1340,860));
        Rect(TEXT("UniformGridPanel_Offers"),FVector2D(25,112),FVector2D(1200,584));
        Rect(TEXT("Button_Refresh"),FVector2D(670,712),FVector2D(300,48));
        Rect(TEXT("Button_Close"),FVector2D(995,712),FVector2D(230,48));
    }
    else
    {
        const bool Shop=Name==TEXT("WBP_ShopOfferCard");
        if(Shop)
        {
            if(auto* Root=Cast<USizeBox>(Find(TEXT("CardRoot"))))Root->SetHeightOverride(270);
            Rect(TEXT("Text_Desc"),FVector2D(0,44),FVector2D(282,114));
            Rect(TEXT("Text_Cost"),FVector2D(0,178),FVector2D(282,28));
            Rect(TEXT("Spirit_CardSeal"),FVector2D(181,119),FVector2D(94,94));
        }
        if(auto* Desc=Cast<UTextBlock>(Find(TEXT("Text_Desc")))){Desc->SetAutoWrapText(true);Desc->SetLineHeightPercentage(1.16f);}
        if(auto* Body=Cast<UCanvasPanel>(Find(TEXT("CardBody"))))
        {
            auto AddRule=[&](FName N,float Y)
            {
                UBorder* Rule=Cast<UBorder>(BP->WidgetTree->FindWidget(N));
                if(!Rule){Rule=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),N);Body->AddChildToCanvas(Rule);}
                Rule->SetBrushColor(FLinearColor(.24f,.15f,.12f,.16f));Rule->SetVisibility(ESlateVisibility::HitTestInvisible);
                auto* S=CastChecked<UCanvasPanelSlot>(Rule->Slot);S->SetPosition(FVector2D(0,Y));S->SetSize(FVector2D(282,1));
            };
            AddRule(TEXT("TitleHairline"),35);if(Shop)AddRule(TEXT("CostHairline"),167);
        }
    }
    return RogueWidgetAuthoring::Finish(BP, false);
}

bool URogueUIThemeBuilder::BuildShopWorldLabel(UWidgetBlueprint* Label,UBlueprint* Shop)
{
    if(!Label||!Label->WidgetTree||!Shop||!Shop->SimpleConstructionScript)return false;
    Label->Modify();Label->WidgetTree->Modify();
    auto* Frame=Label->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ShopSignFrame"));
    Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.025f,.04f,.065f,.86f),6.f,FLinearColor(.66f,.43f,.17f,.75f),1.f));
    Frame->SetPadding(FMargin(12,5));Frame->SetVisibility(ESlateVisibility::HitTestInvisible);Label->WidgetTree->RootWidget=Frame;
    auto* Text=Label->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("ShopSignText"));
    Text->SetText(FText::FromString(TEXT("香霖堂")));Text->SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(),18));
    Text->SetColorAndOpacity(FLinearColor(.98f,.88f,.64f,1));Text->SetJustification(ETextJustify::Center);Frame->AddChild(Text);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Label);FKismetEditorUtilities::CompileBlueprint(Label);
    if(Label->Status==BS_Error)return false;
    Shop->Modify();USimpleConstructionScript* SCS=Shop->SimpleConstructionScript;FVector Location(0,0,280);USCS_Node* Sign=nullptr;
    for(USCS_Node* N:SCS->GetAllNodes())
    {
        if(auto* Old=Cast<UTextRenderComponent>(N->ComponentTemplate)){Location=Old->GetRelativeLocation();Old->SetVisibility(false);}
        if(N->GetVariableName()==TEXT("ShopWorldLabel"))Sign=N;
    }
    if(!Sign)
    {
        Sign=SCS->CreateNode(UWidgetComponent::StaticClass(),TEXT("ShopWorldLabel"));SCS->AddNode(Sign);
        if(auto* CDO=Cast<AActor>(Shop->GeneratedClass->GetDefaultObject()))Sign->SetParent(CDO->GetRootComponent());
    }
    auto* Component=CastChecked<UWidgetComponent>(Sign->ComponentTemplate);
    Component->SetWidgetClass(Label->GeneratedClass.Get());Component->SetWidgetSpace(EWidgetSpace::Screen);
    Component->SetDrawAtDesiredSize(true);Component->SetDrawSize(FVector2D(150,42));Component->SetPivot(FVector2D(.5f,1.f));
    Component->SetRelativeLocation(Location);Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);Component->SetCanEverAffectNavigation(false);Component->SetCastShadow(false);
    Component->SetVisibility(true);Component->SetTickWhenOffscreen(false);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Shop);FKismetEditorUtilities::CompileBlueprint(Shop);
    Label->MarkPackageDirty();Shop->MarkPackageDirty();return Shop->Status!=BS_Error;
}
