#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueBeamVFXSetup.generated.h"
class UWidgetBlueprint;
class UBlueprint;
class UMaterialInterface;
class ARogueLaserBeam;

/** 编辑器配置与临时预览；不改武器伤害或地图。 */
UCLASS()
class URogueBeamVFXSetup : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Beam")
    static bool ApplyBeamMaterials(UBlueprint* Blueprint, UMaterialInterface* Core, UMaterialInterface* Glow, UMaterialInterface* Ring, bool bInferno);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview", meta=(WorldContext="Context"))
    static ARogueLaserBeam* CreatePreviewBeam(UObject* Context, bool bInferno, FVector Start, FVector End, float Lifetime);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview")
    static bool RefreshPreviewBeam(ARogueLaserBeam* Beam, bool bInferno, FVector Start, FVector End, float Lifetime);
};
