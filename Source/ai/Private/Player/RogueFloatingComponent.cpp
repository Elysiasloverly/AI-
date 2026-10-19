#include "Player/RogueFloatingComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"

URogueFloatingComponent::URogueFloatingComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=true;
    PrimaryComponentTick.bAllowTickOnDedicatedServer=false;
    bAutoActivate=true;
    SetIsReplicatedByDefault(false);
}
USceneComponent* URogueFloatingComponent::GetTargetComponent() const
{
    if (!GetOwner() || (FloatingTarget.ComponentProperty.IsNone() && FloatingTarget.PathToComponent.IsEmpty() && !FloatingTarget.OverrideComponent.IsValid())) { return nullptr; }
    USceneComponent* Target=Cast<USceneComponent>(FloatingTarget.GetComponent(GetOwner()));
    // 角色根碰撞不能作为外观漂浮目标。
    return Target && Target->GetOwner()==GetOwner() && !(GetOwner()->IsA<ACharacter>() && Target==GetOwner()->GetRootComponent()) ? Target : nullptr;
}
void URogueFloatingComponent::SetTargetComponent(USceneComponent* Target)
{
    ResetOffset(); FloatingTarget=FComponentReference();
    if (Target && Target->GetOwner()==GetOwner()) { FloatingTarget.PathToComponent=Target->GetPathName(GetOwner()); }
}
void URogueFloatingComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    USceneComponent* Target=GetTargetComponent();
    if (!IsActive() || !bFloatingEnabled || !IsValid(Target) || !FMath::IsFinite(FloatAmplitude) || FloatAmplitude<=0
        || !FMath::IsFinite(FloatPeriod) || FloatPeriod<0.1f) { ResetOffset(); return; }
    if (AppliedTarget.Get()!=Target) { ResetOffset(); AppliedTarget=Target; }
    Elapsed=FMath::Fmod(Elapsed+FMath::Max(0.0f,DeltaTime),static_cast<double>(FloatPeriod));
    const FVector Offset(0,0,FloatAmplitude*FMath::Sin(2.0*PI*Elapsed/FloatPeriod));
    Target->SetRelativeLocation(Target->GetRelativeLocation()-AppliedOffset+Offset,false,nullptr,ETeleportType::TeleportPhysics);
    AppliedOffset=Offset;
}
void URogueFloatingComponent::ResetOffset()
{
    if (USceneComponent* Target=AppliedTarget.Get())
    {
        if (!AppliedOffset.IsZero()) { Target->SetRelativeLocation(Target->GetRelativeLocation()-AppliedOffset,false,nullptr,ETeleportType::TeleportPhysics); }
    }
    AppliedTarget.Reset(); AppliedOffset=FVector::ZeroVector; Elapsed=0;
}
void URogueFloatingComponent::SetFloatingEnabled(bool bEnabled) { bFloatingEnabled=bEnabled; if (!bEnabled) { ResetOffset(); } }
void URogueFloatingComponent::Deactivate() { ResetOffset(); Super::Deactivate(); }
void URogueFloatingComponent::EndPlay(const EEndPlayReason::Type Reason) { ResetOffset(); Super::EndPlay(Reason); }
void URogueFloatingComponent::OnUnregister() { ResetOffset(); Super::OnUnregister(); }
