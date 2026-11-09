#include "Combat/Visuals/RogueCombatCameraModifier.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarCombatCameraKick(TEXT("Rogue.VFX.CameraKick"),1,TEXT("Enable the brief local boss-finish camera recoil. 0 disables immediately."));

URogueCombatCameraModifier::URogueCombatCameraModifier()
{
    Priority=190;AlphaInTime=AlphaOutTime=0;bExclusive=false;bDisabled=true;
}
void URogueCombatCameraModifier::PlayLocal(UWorld* World,const FVector& Location,float Strength)
{
    if(!World || World->GetNetMode()==NM_DedicatedServer || !CVarCombatCameraKick.GetValueOnGameThread() || Location.ContainsNaN() || !FMath::IsFinite(Strength) || Strength<=0)return;
    for(auto It=World->GetPlayerControllerIterator();It;++It)
    {
        auto* PC=It->Get();if(!PC||!PC->IsLocalController()||!PC->PlayerCameraManager)continue;
        const FVector Origin=PC->GetPawn()?PC->GetPawn()->GetActorLocation():PC->PlayerCameraManager->GetCameraLocation();
        const float Weight=1-FMath::SmoothStep(400.f,2200.f,float(FVector::Distance(Origin,Location)));
        if(Weight<=0)continue;
        APlayerCameraManager* Manager=PC->PlayerCameraManager.Get();
        auto* Modifier=Cast<URogueCombatCameraModifier>(Manager->FindCameraModifierByClass(StaticClass()));
        if(!Modifier)Modifier=Cast<URogueCombatCameraModifier>(Manager->AddNewCameraModifier(StaticClass()));
        if(Modifier)Modifier->AddImpulse(Strength*Weight);
    }
}
void URogueCombatCameraModifier::AddImpulse(float Strength)
{
    if(!FMath::IsFinite(Strength)||Strength<=0)return;
    Strength=FMath::Clamp(Strength,0.f,1.f);
    // Concurrent finish events share one bounded pulse instead of stacking shakes.
    if(Age<.32f){KickStrength=FMath::Max(KickStrength,Strength);return;}
    Age=0;KickStrength=Strength;EnableModifier();
}
bool URogueCombatCameraModifier::ModifyCamera(float DeltaTime,FMinimalViewInfo& POV)
{
    if(!CVarCombatCameraKick.GetValueOnGameThread()){Age=1;KickStrength=0;DisableModifier(true);return false;}
    Age+=FMath::IsFinite(DeltaTime)?FMath::Max(0.f,DeltaTime):0;
    if(Age>=.32f){KickStrength=0;DisableModifier(true);return false;}
    const float T=Age/.32f,Envelope=FMath::SmoothStep(0.f,.12f,T)*FMath::Square(1-T)*KickStrength;
    POV.FOV-=.85f*Envelope;
    POV.Rotation.Roll+=.20f*Envelope*FMath::Sin(T*2*PI);
    POV.Rotation.Pitch+=.06f*Envelope*FMath::Sin(T*PI);
    return false;
}
