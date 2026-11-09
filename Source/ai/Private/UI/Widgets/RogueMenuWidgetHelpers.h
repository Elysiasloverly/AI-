#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextBlock.h"

namespace RogueMenuWidgetHelpers
{
    // Optional WBP decoration. Other styles can omit the image or provide their own material.
    inline void SetCardSeal(UUserWidget* Card, float State)
    {
        if (auto* Seal = Cast<UImage>(Card->GetWidgetFromName(TEXT("CardStateSeal"))))
            if (auto* Ink = Seal->GetDynamicMaterial()) Ink->SetScalarParameterValue(TEXT("State"), State);
    }

	inline ESlateVisibility GetMenuVisibility(bool bVisible)
	{
		return bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	}

	inline void SetManagedText(UTextBlock* TextBlock, const FText& Text)
	{
		if (TextBlock == nullptr)
		{
			return;
		}

		TextBlock->SetText(Text);
		TextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	inline void SetManagedBorderColor(UBorder* Border, const FLinearColor& Color)
	{
		if (Border == nullptr)
		{
			return;
		}

		Border->SetBrushColor(Color);
		Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}
