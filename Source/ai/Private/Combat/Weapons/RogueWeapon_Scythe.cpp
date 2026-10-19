// 镰刀武器 —— 环绕刀片同步逻辑
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

#include "Player/RogueCharacter.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"

void ARogueWeapon_Scythe::WeaponTick(float /*DeltaSeconds*/)
{
	// 使用服务器世界时间的估计增量推进，避免直接用新转速乘绝对时间而跳相位。
	// 客户端时间偏移估计、首次 Tick、升级到达时机与增量钳制均可能造成相位差；
	// SharedOrbitAngle 不复制，因此这里不保证各端严格同步或晚加入时相位一致。
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	const double NowSeconds = GameState != nullptr
		? GameState->GetServerWorldTimeSeconds()
		: (World != nullptr ? World->GetTimeSeconds() : 0.0);

	if (GetEffectiveCount() > 0 && LastOrbitServerTimeSeconds >= 0.0)
	{
		// 上限 0.25s：长时间没 tick（池回收、卡顿）之后不要让相位一次跳一大段
		const double ServerDelta = FMath::Clamp(NowSeconds - LastOrbitServerTimeSeconds, 0.0, 0.25);
		SharedOrbitAngle = FMath::Fmod(SharedOrbitAngle + GetEffectiveRotationSpeed() * static_cast<float>(ServerDelta), 360.0f);
		if (SharedOrbitAngle < 0.0f)
		{
			SharedOrbitAngle += 360.0f;
		}
	}

	// 未解锁时仍更新时间基准，解锁后不追补停转期间的时长。
	LastOrbitServerTimeSeconds = NowSeconds;

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

// 生效数值：镰刀的换算系数（转速 +加成×280、环绕半径 +加成×0.12）由本武器定义。
// 刀片是持续轨道表现，"攻速"在这里表现为转速而不是冷却。

float ARogueWeapon_Scythe::GetEffectiveRotationSpeed() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackSpeed;
	return GetConfig().RotationSpeed + Bonus * 280.0f;
}

float ARogueWeapon_Scythe::GetEffectiveOrbitRadius() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackRange;
	return GetConfig().OrbitRadius + Bonus * 0.12f;
}

float ARogueWeapon_Scythe::GetEffectiveSizeMultiplier() const
{
	return 1.0f + FMath::Max(0.0f, GetWeaponBonusValues().ScytheSize);
}

bool ARogueWeapon_Scythe::NeedsCosmeticTick() const
{
	// 远端角色副本也需更新连续轨道。接触检测可在本地运行，
	// 但公共伤害入口拒绝非权威目标 ASC；本地命中特效不代表服务器命中。
	return true;
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
			OrbitingBlades[Index]->ConfigureBlade(OwnerChar, Index, OrbitingBlades.Num(), GetEffectiveOrbitRadius(), GetEffectiveRotationSpeed(), GetEffectiveDamage(), SharedOrbitAngle, GetEffectiveSizeMultiplier());
		}
	}
}