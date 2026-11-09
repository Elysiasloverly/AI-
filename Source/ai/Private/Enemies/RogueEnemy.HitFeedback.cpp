#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Combat/Visuals/RogueHitFlashComponent.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"

void ARogueEnemy::OnEnemyDamaged_Implementation(float ActualDamage)
{
    if(ActualDamage<=0 || !FMath::IsFinite(ActualDamage) || !GetWorld() || GetNetMode()==NM_DedicatedServer)return;
    if(EnemyPresentation)EnemyPresentation->PlayHitCue();
    if(LastHitAccentGeneration!=HitFeedback.Generation)
    {LastHitAccentGeneration=HitFeedback.Generation;LastHitAccentTime=-1.e30;}
    const double Now=GetWorld()->GetTimeSeconds();
    const FRogueCombatAccentSettings* Accent=WeaponHitAccents.Find(static_cast<ERogueWeaponImpactStyle>(HitFeedback.VisualStyle));
    if(!Accent)Accent=&HitAccent;
    if(Now-LastHitAccentTime>=FMath::Max(.02f,Accent->MinimumInterval))
    {
        FVector SparkLocation=GetActorLocation()+FVector(0,0,25);
        FVector Extent(GetCapsuleComponent()->GetScaledCapsuleRadius());float Volume=0;
        // Follow the authored body, rather than assuming all models share a pivot.
        // Ignore role badges and damage-area meshes; evaluate only accepted visual hits.
        for(auto* M:TInlineComponentArray<UMeshComponent*>(this))
            if(M->IsVisible()&&!M->bHiddenInGame&&!M->ComponentHasTag(TEXT("Rogue.RoleAccessory"))&&!M->ComponentHasTag(TEXT("Rogue.DamageArea")))
            {const FVector E=M->Bounds.BoxExtent;const float V=E.X*E.Y*E.Z;if(V>Volume){Volume=V;SparkLocation=M->Bounds.Origin;Extent=E;}}
        if(const auto* PC=GetWorld()->GetFirstPlayerController();PC && PC->PlayerCameraManager)
        {
            const FVector View=(PC->PlayerCameraManager->GetCameraLocation()-SparkLocation).GetSafeNormal();
            SparkLocation+=View*(FVector::DotProduct(Extent,View.GetAbs())+8.f);
        }
        if(auto* Pool=GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>())
            if(Pool->PlayCombatAccent(*Accent,SparkLocation,FVector::UpVector,true))LastHitAccentTime=Now;
    }

	if (HitFlashComponent)
	{
		HitFlashComponent->FlashMaterial = HitFlashMaterial;
		HitFlashComponent->PlayHitFlash();
	}
}

void ARogueEnemy::RecordHitFeedback(float ActualDamage,uint8 VisualStyle)
{
	if (!HasAuthority() || bPoolAvailable || bDead || ActualDamage <= 0.0f || !FMath::IsFinite(ActualDamage)) { return; }
	++HitFeedback.Sequence;
	HitFeedback.TotalDamage += ActualDamage;
	HitFeedback.LatestDamage = ActualDamage;
    HitFeedback.VisualStyle=VisualStyle<=6?VisualStyle:0;
	HitFeedback.ServerTime = GetWorld()->GetTimeSeconds();
	// Existing replication budget coalesces rapid hits; do not force a packet/RPC for every pellet.
	if (GetNetMode() != NM_DedicatedServer) { OnEnemyDamaged(ActualDamage); }
}

void ARogueEnemy::OnRep_HitFeedback()
{
	ConsumeReplicatedHitFeedback();
    if (BossSkills) BossSkills->RefreshPresentation();
}

void ARogueEnemy::ConsumeReplicatedHitFeedback()
{
	const bool bNewGeneration = ObservedHitGeneration != HitFeedback.Generation;
	if (bNewGeneration || !HitFeedback.bActive)
	{
		if (HitFlashComponent) { HitFlashComponent->ClearHitFlash(); }
	}
	if (!HitFeedback.bActive || HitFeedback.Sequence == 0)
	{
		ObservedHitGeneration = HitFeedback.Generation;
		ObservedHitSequence = HitFeedback.Sequence;
		ObservedHitTotalDamage = HitFeedback.TotalDamage;
		return;
	}
	// Actor activation and this snapshot may be notified in either order. Consume after activation.
	if (!bReplicatedActive || bPoolAvailable || bDead) { return; }
	if (!bNewGeneration && HitFeedback.Sequence == ObservedHitSequence) { return; }
	const float Damage = bNewGeneration ? HitFeedback.LatestDamage : FMath::Max(0.0f, HitFeedback.TotalDamage - ObservedHitTotalDamage);
	ObservedHitGeneration = HitFeedback.Generation;
	ObservedHitSequence = HitFeedback.Sequence;
	ObservedHitTotalDamage = HitFeedback.TotalDamage;
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World ? World->GetGameState() : nullptr;
	const float Now = State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
	// Do not flash old damage when an already-injured enemy first becomes relevant.
	if (Now - HitFeedback.ServerTime > 0.5f || Now - HitFeedback.ServerTime < -1.0f) { return; }
	if (Damage > 0.0f && GetNetMode() != NM_DedicatedServer) { OnEnemyDamaged(Damage); }
}
