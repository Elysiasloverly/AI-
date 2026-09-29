// 镰刀武器 —— 环绕刀片同步逻辑
#include "Combat/RogueWeapon_Scythe.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

#include "Player/RogueCharacter.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Combat/RogueOrbitingBlade.h"

void ARogueWeapon_Scythe::WeaponTick(float /*DeltaSeconds*/)
{
	// 相位用**服务器时间轴上的增量**推进，而不是本地 DeltaSeconds。
	//
	// 为什么要这么绕：这把武器在每台机器上都要 tick（见 NeedsCosmeticTick），
	// 而 AGameStateBase::GetServerWorldTimeSeconds() 在服务端就是世界时间，
	// 在客户端由引擎用"本地时间 + 平滑估计出来的服务器时间差"给出
	// （见 GameStateBase.cpp 里 GetServerWorldTimeSeconds 的实现），
	// 也就是**所有端共享同一条时间轴**。
	// 于是 sum(速度 × 时间增量) 在各端严格相等（速度不变时它就等于 速度 × 总时长，是个恒等式），
	// 队友看到的刀片环相位与本人/服务端完全一致，也不会因帧率差异或卡顿漂移
	// （UE 会钳制本地 DeltaSeconds，旧的逐帧累加正是这样慢慢错开的）。
	//
	// 为什么不直接写"角度 = 服务器时间 × 旋转速度"（更短）：
	// 那样一旦吃到攻速升级（RotationSpeed 变大），历史相位会被整体重新缩放，
	// 表现上就是刀片环"啪"地跳一下。用增量推进只会在升级那一瞬间有极小差异。
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

	// 时间基准每帧都跟住：这样"武器还没解锁"的那段时间不会在解锁瞬间补一大段相位，
	// 各端都是从解锁那一刻开始一起转。
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

// 生效数值：镰刀的换算系数（转速 +加成×280、环绕半径 +加成×0.12）沿用自研版本。
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

bool ARogueWeapon_Scythe::NeedsCosmeticTick() const
{
	// 环绕刀片是**持续**轨道表现，不存在"某一发"这种事件可以让服务端广播，
	// 所以除了服务端和本人，其他客户端也必须自己 tick，否则队友看不到你的刀片。
	// 本地跑只影响画面：刀片的伤害走 RogueAbilitySystem::ApplyDamageToTarget，
	// 那个函数内部有服务端权限闸（TargetASC->IsOwnerActorAuthoritative()），客户端调用无效。
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
			OrbitingBlades[Index]->ConfigureBlade(OwnerChar, Index, OrbitingBlades.Num(), GetEffectiveOrbitRadius(), GetEffectiveRotationSpeed(), GetEffectiveDamage(), SharedOrbitAngle);
		}
	}
}