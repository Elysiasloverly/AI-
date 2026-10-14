#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueMenuViewData.h"
#include "RogueDeathScreenWidgetBase.generated.h"

class ARogueHUD;

UCLASS(Abstract, Blueprintable)
class AI_API URogueDeathScreenWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdateDeathView(const FRogueDeathViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Death")
	void RequestRestartRound();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Death")
	void RequestQuitGame();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Death")
	void OnDeathViewUpdated(const FRogueDeathViewData& InViewData);

private:
	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};
