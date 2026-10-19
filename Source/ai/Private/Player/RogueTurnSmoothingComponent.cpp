#include "Player/RogueTurnSmoothingComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
URogueTurnSmoothingComponent::URogueTurnSmoothingComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=true;
    PrimaryComponentTick.bAllowTickOnDedicatedServer=false;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
    bAutoActivate=true;
    SetIsReplicatedByDefault(false);
}
USceneComponent* URogueTurnSmoothingComponent::GetTargetComponent() const
{
    if (!GetOwner() || (VisualTarget.ComponentProperty.IsNone() && VisualTarget.PathToComponent.IsEmpty() && !VisualTarget.OverrideComponent.IsValid())) { return nullptr; }
    USceneComponent* Target=Cast<USceneComponent>(VisualTarget.GetComponent(GetOwner()));
    return Target && Target->GetOwner()==GetOwner() && !(GetOwner()->IsA<ACharacter>() && Target==GetOwner()->GetRootComponent()) ? Target : nullptr;
}
void URogueTurnSmoothingComponent::SetTargetComponent(USceneComponent* Target)
{
    ResetSmoothing(); VisualTarget=FComponentReference();
    if (Target && Target->GetOwner()==GetOwner()) { VisualTarget.PathToComponent=Target->GetPathName(GetOwner()); }
}
void URogueTurnSmoothingComponent::InitializeTarget()
{
    USceneComponent* Target=GetTargetComponent();
    if (!IsValid(Target)) { return; }
    SmoothedTarget=Target;
    AuthoredRelativeRotation=Target->GetRelativeRotation().Quaternion();
    SmoothedWorldRotation=Target->GetComponentQuat();
}
void URogueTurnSmoothingComponent::BeginPlay() { Super::BeginPlay(); InitializeTarget(); }
void URogueTurnSmoothingComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    USceneComponent* Target=GetTargetComponent();
    if (!IsActive() || !bSmoothTurning || !IsValid(Target) || !FMath::IsFinite(FollowSpeed) || FollowSpeed<=0) { ResetSmoothing(); return; }
    if (SmoothedTarget.Get()!=Target) { ResetSmoothing(); InitializeTarget(); }
    const USceneComponent* Parent=Target->GetAttachParent();
    const FQuat Desired=(Parent ? Parent->GetComponentQuat() : FQuat::Identity)*AuthoredRelativeRotation;
    const float Alpha=1.0f-FMath::Exp(-FollowSpeed*FMath::Max(0.0f,DeltaTime));
    SmoothedWorldRotation=FQuat::Slerp(SmoothedWorldRotation,Desired,Alpha).GetNormalized();
    Target->SetWorldRotation(SmoothedWorldRotation,false,nullptr,ETeleportType::TeleportPhysics);
}
void URogueTurnSmoothingComponent::ResetSmoothing()
{
    if (USceneComponent* Target=SmoothedTarget.Get()) { Target->SetRelativeRotation(AuthoredRelativeRotation,false,nullptr,ETeleportType::TeleportPhysics); }
    SmoothedTarget.Reset();
}
void URogueTurnSmoothingComponent::Deactivate() { ResetSmoothing(); Super::Deactivate(); }
void URogueTurnSmoothingComponent::EndPlay(const EEndPlayReason::Type Reason) { ResetSmoothing(); Super::EndPlay(Reason); }
void URogueTurnSmoothingComponent::OnUnregister() { ResetSmoothing(); Super::OnUnregister(); }
