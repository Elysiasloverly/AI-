#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "RogueCombatCameraModifier.generated.h"

/** A small, local boss-finish recoil. Never writes control rotation or game time. */
UCLASS()
class AI_API URogueCombatCameraModifier : public UCameraModifier
{
    GENERATED_BODY()
public:
    URogueCombatCameraModifier();
    static void PlayLocal(UWorld* World, const FVector& Location, float Strength);
    void AddImpulse(float Strength);
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
private:
    float Age = 1.f;
    float KickStrength = 0.f;
};
