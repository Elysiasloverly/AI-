// 镰刀武器 —— 环绕刀片同步逻辑
#include "Combat/RogueWeapon_Scythe.h"

#include "Engine/World.h"

#include "Player/RogueCharacter.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Combat/RogueOrbitingBlade.h"

void ARogueWeapon_Scythe::WeaponTick(float DeltaSeconds)
{
	if (GetEffectiveCount() > 0)
	{
		SharedOrbitAngle = FMath::Fmod(SharedOrbitAngle + Config.RotationSpeed * DeltaSeconds, 360.0f);
		if (SharedOrbitAngle < 0.0f)
		{
			SharedOrbitAngle += 360.0f;
		}
	}

	SyncOrbitingBlades();
}

void ARogueWeapon_Scythe::OnOwnerDied()
{
	for (ARogueOrbitingBlade* Blade : OrbitingBlades)
	{
		if (IsValid(Blade))
		{
			Blade->DeactivateToPool();
		}
	}
	OrbitingBlades.Reset();
}

bool ARogueWeapon_Scythe::OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude)
{
	// 镰刀数量升级
	if (UpgradeType == ERogueUpgradeType::ScytheCount)
	{
		Config.Count = FMath::Max(1, Config.Count + FMath::RoundToInt(Magnitude));
		return true;
	}
	return false;
}

void ARogueWeapon_Scythe::ApplySharedDamageBonus(float Magnitude)
{
	Config.Damage += Magnitude;
}

void ARogueWeapon_Scythe::ApplySharedSpeedBonus(float Magnitude)
{
	Config.RotationSpeed += Magnitude * 280.0f;
}

void ARogueWeapon_Scythe::ApplySharedRangeBonus(float Magnitude)
{
	Config.OrbitRadius += Magnitude * 0.12f;
}

void ARogueWeapon_Scythe::SyncOrbitingBlades()
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

	const int32 TargetBladeCount = GetEffectiveCount();

	// 清理无效引用
	for (int32 Index = OrbitingBlades.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(OrbitingBlades[Index]) || OrbitingBlades[Index]->IsAvailableInPool())
		{
			OrbitingBlades.RemoveAt(Index);
		}
	}

	// 补充不足的刀片
	while (OrbitingBlades.Num() < TargetBladeCount)
	{
		UClass* BladeClassToSpawn = Config.BladeClass ? Config.BladeClass.Get() : ARogueOrbitingBlade::StaticClass();
		URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
		ARogueOrbitingBlade* Blade = Pools != nullptr
			? Pools->AcquireOrbitingBlade(BladeClassToSpawn, OwnerChar, OwnerChar->GetActorLocation(), FRotator::ZeroRotator)
			: GetWorld()->SpawnActor<ARogueOrbitingBlade>(BladeClassToSpawn, OwnerChar->GetActorLocation(), FRotator::ZeroRotator);
		if (Blade != nullptr)
		{
			Blade->ActivatePooledBlade(OwnerChar);
			OrbitingBlades.Add(Blade);
		}
		else
		{
			break;
		}
	}

	// 回收多余的刀片
	while (OrbitingBlades.Num() > TargetBladeCount)
	{
		if (ARogueOrbitingBlade* Blade = OrbitingBlades.Pop())
		{
			Blade->DeactivateToPool();
		}
	}

	// 配置每把刀片的位置
	for (int32 Index = 0; Index < OrbitingBlades.Num(); ++Index)
	{
		if (IsValid(OrbitingBlades[Index]))
		{
			OrbitingBlades[Index]->ConfigureBlade(OwnerChar, Index, OrbitingBlades.Num(), Config.OrbitRadius, Config.RotationSpeed, Config.Damage, SharedOrbitAngle);
		}
	}
}