#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueAudioSetup.generated.h"
class UWidgetBlueprint;
class UUserWidget;
class UWidget;
class USoundBase;

/** 编辑器一次性创建可编辑音频配置及设置行；不导入或生成实际音乐。 */
UCLASS()
class URogueAudioSetup : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Audio") static bool EnsureAudioAssets();
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Audio") static bool AddMusicControls(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview") static USoundBase* CreateSilentPreviewSound();
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview") static UWidget* FindPreviewWidget(UUserWidget* Widget, FName Name);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview", meta=(WorldContext="Context"))
    static bool SetPreviewBattleState(UObject* Context, bool bGameOver, bool bBossAlive, bool bNight);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview", meta=(WorldContext="Context")) static UUserWidget* OpenPreviewSettings(UObject* Context);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview") static bool ClickPreviewButton(UUserWidget* Widget, FName Name);
};
