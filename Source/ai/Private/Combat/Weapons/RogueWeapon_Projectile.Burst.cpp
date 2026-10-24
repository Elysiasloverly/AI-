#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Engine/World.h"
#include "TimerManager.h"

int32 ARogueWeapon_Projectile::GetEffectiveBurstCount() const
{
	const float Bonus = GetWeaponBonusValues().ProjectileBurst;
	const double SafeBonus = FMath::IsFinite(Bonus) ? FMath::Clamp(double(Bonus), 0.0, double(MAX_int32 - 1)) : 0.0;
	int32 Count = 1 + FMath::RoundToInt(SafeBonus);
	if (Config.MaxProjectileBurstCount >= 0) { Count = FMath::Min(Count, FMath::Max(1, Config.MaxProjectileBurstCount)); }
	return Count;
}

float ARogueWeapon_Projectile::GetBurstShotInterval() const
{
	return FMath::IsFinite(Config.ProjectileBurstInterval) ? FMath::Max(.02f, Config.ProjectileBurstInterval) : .12f;
}

float ARogueWeapon_Projectile::GetAbilityCooldown() const
{
	// Finish the complete burst, then wait the normal weapon cooldown. Bursts never overlap.
	return GetEffectiveCooldown() + (GetEffectiveBurstCount() - 1) * GetBurstShotInterval();
}

bool ARogueWeapon_Projectile::CanFireNow() const
{
	return RemainingBurstShots == 0 && Super::CanFireNow();
}

void ARogueWeapon_Projectile::FireNextBurstShot()
{
	ARogueCharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character) || !Character->IsAlive() || RemainingBurstShots <= 0 || GetEffectiveProjectileCount() <= 0
		|| (!Character->IsAuthoritySide() && !Character->IsLocallyControlled()))
	{
		CancelBurst();
		return;
	}
	ARogueEnemy* Target = BurstTarget.Get();
	if (!IsValid(Target) || Target->IsDead() || Target->IsAvailableInPool() || Target->GetSpawnCycleId() != BurstTargetSpawnId
		|| FVector::DistSquared2D(Character->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(GetEffectiveRange()))
	{
		Target = FindNearestEnemy(GetEffectiveRange());
		if (!IsValid(Target)) { CancelBurst(); return; }
		BurstTarget = Target;
		BurstTargetSpawnId = Target->GetSpawnCycleId();
	}
	--RemainingBurstShots;
	// Reuse the existing volley path, including spread, authoritative damage and per-projectile cosmetic replication.
	FireAtTarget(Target);
	if (RemainingBurstShots > 0 && IsValid(Character) && Character->IsAlive())
	{
		GetWorld()->GetTimerManager().SetTimer(BurstTimer, this, &ARogueWeapon_Projectile::FireNextBurstShot, GetBurstShotInterval(), false);
	}
	else { CancelBurst(); }
}

void ARogueWeapon_Projectile::CancelBurst()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(BurstTimer); }
	RemainingBurstShots = 0;
	BurstTarget.Reset();
	BurstTargetSpawnId.Invalidate();
}

void ARogueWeapon_Projectile::OnOwnerDied()
{
	CancelBurst();
	Super::OnOwnerDied();
}

void ARogueWeapon_Projectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelBurst();
	Super::EndPlay(EndPlayReason);
}
