#include "UI/Widgets/RogueTutorialWidgetBase.h"

#include "Components/Button.h"
#include "UI/RogueHUD.h"

void URogueTutorialWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueTutorialWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton != nullptr)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &URogueTutorialWidgetBase::HandleCloseClicked);
	}
}

void URogueTutorialWidgetBase::HandleCloseClicked()
{
	if (ARogueHUD* HUD = OwningRogueHUD.Get())
	{
		HUD->CloseMainMenuTutorial();
	}
}
