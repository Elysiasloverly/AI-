#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

namespace RogueMenuWidgetHelpers
{
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
