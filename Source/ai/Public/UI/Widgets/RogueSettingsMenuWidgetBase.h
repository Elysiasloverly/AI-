#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueMenuViewData.h"
#include "RogueSettingsMenuWidgetBase.generated.h"

class ARogueHUD;

UCLASS(Abstract, Blueprintable)
class AI_API URogueSettingsMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdateSettingsView(const FRogueSettingsMenuViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestBackToPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestApplySettings();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestVolumeDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestVolumeUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings", meta=(DisplayName="降低背景音乐音量"))
	void RequestMusicVolumeDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings", meta=(DisplayName="提高背景音乐音量"))
	void RequestMusicVolumeUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestQualityDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestQualityUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestResolutionDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestResolutionUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestFrameLimitDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestFrameLimitUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestToggleDisplayMode();

protected:
	virtual void NativeConstruct() override;
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Settings")
	void OnSettingsViewUpdated(const FRogueSettingsMenuViewData& InViewData);

private:
	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};
