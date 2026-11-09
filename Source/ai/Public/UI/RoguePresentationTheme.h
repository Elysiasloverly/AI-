#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shared/RogueTypes.h"
#include "RoguePresentationTheme.generated.h"
class UMaterialInterface;
UCLASS(BlueprintType)
class AI_API URoguePresentationTheme : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="符卡图标",meta=(DisplayName="升级类型与图标"))
    TMap<ERogueUpgradeType,TObjectPtr<UMaterialInterface>> CardIcons;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="符卡图标",meta=(DisplayName="武器图标（数量升级类型）"))
    TMap<ERogueUpgradeType,TObjectPtr<UMaterialInterface>> WeaponIcons;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Boss演出",meta=(DisplayName="Boss名称")) TMap<ERogueEnemyType,FText> BossNames;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Boss演出",meta=(DisplayName="符卡展示名")) TMap<ERogueEnemyType,FText> BossSpellTitles;
    UFUNCTION(BlueprintPure,Category="符卡图标") UMaterialInterface* GetCardIcon(ERogueUpgradeType Type) const
    {const auto* Icon=CardIcons.Find(Type);return Icon?Icon->Get():nullptr;}
    UFUNCTION(BlueprintPure,Category="符卡图标") UMaterialInterface* GetWeaponIcon(ERogueUpgradeType Type) const
    {const auto* Icon=WeaponIcons.Find(Type);return Icon?Icon->Get():GetCardIcon(Type);}
};
