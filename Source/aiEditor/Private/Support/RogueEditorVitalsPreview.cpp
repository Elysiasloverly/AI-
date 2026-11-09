#include "RogueEditorAssetUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

bool URogueEditorAssetUtils::SetPreviewVitalsView(APlayerController* Owner,const FRoguePlayerVitalsView& View)
{
    if(!Owner||!Owner->IsLocalController()||Owner->GetWorld()->WorldType!=EWorldType::PIE)return false;
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Owner->GetWorld(),Widgets,URogueVitalsWidgetBase::StaticClass(),false);
    for(auto* Widget:Widgets)
        if(Widget->GetOwningPlayer()==Owner)
        {CastChecked<URogueVitalsWidgetBase>(Widget)->UpdateVitals(View);return true;}
    return false;
}
