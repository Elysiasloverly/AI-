#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueMenuViewData.h"
#include "RoguePauseMenuWidgetBase.generated.h"

class ARogueHUD;

UCLASS(Abstract, Blueprintable)
class AI_API URoguePauseMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdatePauseView(const FRoguePauseMenuViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Pause")
	void RequestResumeGame();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Pause")
	void RequestOpenSettings();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Pause")
	void RequestQuitGame();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Pause")
	void OnPauseViewUpdated(const FRoguePauseMenuViewData& InViewData);

private:
	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};
