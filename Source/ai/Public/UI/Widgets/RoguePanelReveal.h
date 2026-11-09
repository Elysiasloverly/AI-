#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

/** One-shot entrance, re-armed only after a hidden view. Does not defer buttons or alter input. */
struct FRoguePanelReveal
{
    float Age=0;bool bVisible=false;
    void SetVisible(UUserWidget* Widget,bool bNewVisible)
    {if(bNewVisible&&!bVisible){Age=0;Widget->SetRenderOpacity(0);}bVisible=bNewVisible;if(!bVisible)Widget->SetRenderOpacity(1);}
    void Tick(UUserWidget* Widget,float Delta)
    {if(!bVisible||Age>=.4f)return;Age+=FMath::Max(0.f,Delta);Widget->SetRenderOpacity(FMath::SmoothStep(0.f,.4f,Age));}
};
