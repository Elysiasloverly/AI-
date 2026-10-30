#include "UI/Widgets/RogueTutorialWidgetBase.h"
#include "Audio/RogueMusicSubsystem.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "UI/RogueHUD.h"

void URogueTutorialWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueTutorialWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	URogueMusicSubsystem::BindWidgetButtonSounds(this);
    if (Chapter0Button) Chapter0Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Chapter0Clicked);
    if (Chapter1Button) Chapter1Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Chapter1Clicked);
    if (Chapter2Button) Chapter2Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Chapter2Clicked);
    if (Chapter3Button) Chapter3Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Chapter3Clicked);
    if (Chapter4Button) Chapter4Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Chapter4Clicked);
    if (Chapter5Button) Chapter5Button->OnClicked.AddUniqueDynamic(this,&ThisClass::Chapter5Clicked);
    SelectChapter(0);

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

void URogueTutorialWidgetBase::SelectChapter(int32 Index)
{
    if (!ChapterSwitcher || Index<0 || Index>=ChapterSwitcher->GetNumWidgets()) return;
    ChapterSwitcher->SetActiveWidgetIndex(Index);
    if (auto* Scroll=Cast<UScrollBox>(ChapterSwitcher->GetActiveWidget())) Scroll->ScrollToStart();
    UButton* Buttons[]={Chapter0Button,Chapter1Button,Chapter2Button,Chapter3Button,Chapter4Button,Chapter5Button};
    for (int32 I=0;I<UE_ARRAY_COUNT(Buttons);++I) if (Buttons[I]) Buttons[I]->SetIsEnabled(I!=Index);
}
