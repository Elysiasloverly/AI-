#pragma once
#include "Blueprint/UserWidget.h"

namespace RogueHUDWidgetFactory
{
	template <typename TWidget>
	void ResolveWidgetClass(const TSoftClassPtr<TWidget>& Path, TSubclassOf<TWidget>& InOutClass)
	{
		if (InOutClass == nullptr && !Path.IsNull())
		{
			InOutClass = Path.LoadSynchronous();
		}
	}
	template <typename WidgetType>
	WidgetType* CreateRogueMenuWidget(APlayerController* PlayerController, TSubclassOf<WidgetType> WidgetClass, ARogueHUD* OwningHUD, int32 ZOrder)
	{
		if (PlayerController == nullptr || WidgetClass == nullptr || OwningHUD == nullptr)
		{
			return nullptr;
		}

		WidgetType* Widget = CreateWidget<WidgetType>(PlayerController, WidgetClass);
		if (Widget == nullptr)
		{
			return nullptr;
		}

		Widget->SetOwningRogueHUD(OwningHUD);
		Widget->AddToViewport(ZOrder);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
		return Widget;
	}
}
