// 敌人移动：选择目标和期望方向，交给绕障组件及 CharacterMovement；不处理伤害。
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/Components/RogueEnemyObstacleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Player/RogueCharacter.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Player/RoguePlayerQuery.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Debug/RoguePerfStats.h"

FVector ARogueEnemy::SampleCrowdSteering()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextCrowdSampleTime)
		return CachedCrowdSteering;

	CSV_SCOPED_TIMING_STAT(RoguePerf, EnemyCrowdSeparation);
	const uint32 MyId = GetUniqueID();
	// 错开同批敌人的后续采样，避免在同一帧集中查询。
	NextCrowdSampleTime = Now + .08 + (MyId % 7) * .005;
	CachedCrowdSteering = FVector::ZeroVector;
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	constexpr float Spacing = 58.f;
	const FVector Origin = GetActorLocation();
	TArray<FOverlapResult> Neighbors;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(RogueEnemyCrowd), false, this);
	GetWorld()->OverlapMultiByObjectType(Neighbors, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeCapsule(Radius + Spacing, HalfHeight + Spacing), Query);

	for (const FOverlapResult& Result : Neighbors)
	{
		const ARogueEnemy* Other = Cast<ARogueEnemy>(Result.GetActor());
		if (!IsValid(Other) || Other->IsDead() || Other->IsAvailableInPool() ||
			Result.GetComponent() != Other->GetCapsuleComponent())
			continue;

		const UCapsuleComponent* OtherCapsule = Other->GetCapsuleComponent();
		FVector Away = Origin - Other->GetActorLocation();
		if (FMath::Abs(Away.Z) >= HalfHeight + OtherCapsule->GetScaledCapsuleHalfHeight())
			continue;
		Away.Z = 0;
		const float Distance = Away.Size2D();
		const float DesiredDistance = Radius + OtherCapsule->GetScaledCapsuleRadius() + Spacing;
		const float Pressure = FMath::Clamp((DesiredDistance - Distance) / Spacing, 0.f, 1.f);
		if (Distance > 1.f)
		{
			Away /= Distance;
		}
		else
		{
			// 完全重合时，用同一对 ID 得到一组反向的分离方向。
			const uint32 OtherId = Other->GetUniqueID();
			const uint32 Pair = HashCombineFast(FMath::Min(MyId, OtherId), FMath::Max(MyId, OtherId));
			Away = FRotator(0, Pair % 360, 0).Vector() * (MyId < OtherId ? 1.f : -1.f);
		}
		CachedCrowdSteering += Away * Pressure;
	}
	CachedCrowdSteering = (CachedCrowdSteering * 2.2f).GetClampedToMaxSize2D(1.65f);
	if (Neighbors.IsEmpty())
		NextCrowdSampleTime += .055;
	return CachedCrowdSteering;
}

ARogueCharacter *ARogueEnemy::GetPlayerCharacter() const
{
	// 缓存目标失去存活状态后立即失效，蓝图行为也不能在下一次重选前继续追踪他。
	ARogueCharacter *PlayerCharacter = CachedPlayerCharacter.Get();
	return IsValid(PlayerCharacter) && PlayerCharacter->IsAlive() ? PlayerCharacter : nullptr;
}

void ARogueEnemy::GatherAlivePlayers(TArray<ARogueCharacter *, TInlineAllocator<4>> &OutPlayers) const
{
	// 实现统一在 RoguePlayerQuery 里：全项目只有一份"谁算活着"的定义
	// （曾经四处各写一遍，口径分叉就会出现"怪物还在打倒地的人"这类 bug）。
	RoguePlayerQuery::CollectAlivePlayers(GetWorld(), OutPlayers);
}

void ARogueEnemy::RefreshTargetPlayer()
{
	// 联机的目标策略：活着的玩家里最近的那一个。
	// 换了策略（仇恨/威胁/固定目标）也只改这里，调用方不用动。
	TArray<ARogueCharacter *, TInlineAllocator<4>> Players;
	GatherAlivePlayers(Players);

	// 刻意**不**再"退回尸体"当目标：按设计，倒下的人处于无法控制状态，
	// 也不应该被怪物索敌（否则会出现"围着尸体打"的观感）。
	// 场上没有存活玩家时不再索敌；团灭与待复活取消由 GameMode 处理。

	ARogueCharacter *Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (ARogueCharacter *Candidate : Players)
	{
		const float DistanceSquared =
			FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = Candidate;
		}
	}

	CachedPlayerCharacter = Nearest;
}

float ARogueEnemy::GetDistanceToPlayer() const
{
	const ARogueCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FLT_MAX;
	}
	return FVector::Dist2D(GetActorLocation(), PlayerCharacter->GetActorLocation());
}

FVector ARogueEnemy::GetDirectionToPlayer() const
{
	const ARogueCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FVector::ZeroVector;
	}
	return (PlayerCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
}

FVector ARogueEnemy::GetVectorToPlayer() const
{
	const ARogueCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FVector::ZeroVector;
	}
	return PlayerCharacter->GetActorLocation() - GetActorLocation();
}

bool ARogueEnemy::IsPlayerInRange(float Range) const
{
	return GetDistanceToPlayer() <= Range;
}

bool ARogueEnemy::IsInRangedAttackRange() const
{
	// 攻击距离限制已取消：有远程攻击的原型在任何距离都视为“在射程内”。
	// 保留该函数是为了兼容蓝图/调试调用，不再返回距离判定结果。
	return CurrentArchetype.Ranged.bUsesRangedAttack;
}

void ARogueEnemy::MoveInDirection(const FVector &Direction, float SpeedMultiplier, float DeltaSeconds)
{
	FVector Destination = GetActorLocation() + Direction.GetSafeNormal2D() * 1500;
	if (const auto *Player = GetPlayerCharacter();
		Player &&
		FVector::DotProduct(Direction.GetSafeNormal2D(), GetDirectionToPlayer().GetSafeNormal2D()) > .85f)
		Destination = Player->GetActorLocation();
	if (RogueEnemyArchetypes::IsHealerType(EnemyType) && EnemySupport)
		EnemySupport->GetDesiredMovementDestination(Destination);
	ApplyObstacleMovement(Direction, Destination, SpeedMultiplier, DeltaSeconds);
}

void ARogueEnemy::MoveTowardsLocation(const FVector &Destination, float SpeedMultiplier, float DeltaSeconds)
{
	ApplyObstacleMovement(Destination - GetActorLocation(), Destination, SpeedMultiplier, DeltaSeconds);
}

void ARogueEnemy::ApplyObstacleMovement(const FVector &Direction, const FVector &Destination,
										float SpeedMultiplier, float DeltaSeconds)
{
    if (IsBossSkillBlockingMovement())
    { GetCharacterMovement()->StopMovementImmediately(); return; }
	if (!HasAuthority() || bDead || bPoolAvailable || Direction.ContainsNaN() || Destination.ContainsNaN() ||
		!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0 || !FMath::IsFinite(SpeedMultiplier) ||
		SpeedMultiplier <= 0 || Direction.IsNearlyZero())
		return;
	const FVector Desired = Direction.GetSafeNormal2D();
	const FVector NormalizedDirection =
		ObstacleMovement ? ObstacleMovement->ResolveDirection(Desired, Destination) : Desired;
	if (NormalizedDirection.IsNearlyZero())
	{
		GetCharacterMovement()->StopMovementImmediately();
		return; // A pending/blocked route must not turn the actor toward zero-vector yaw.
	}
	float MaxStep = GetMoveSpeed() * SpeedMultiplier * DeltaSeconds;
	if (ObstacleMovement)
		MaxStep = ObstacleMovement->LimitMovementStep(MaxStep);
	const bool bDetouring = ObstacleMovement && ObstacleMovement->IsDetouring();
	const bool bHealer = RogueEnemyArchetypes::IsHealerType(EnemyType);
	const FVector ToDestination = Destination - GetActorLocation();
	float RouteStep = MaxStep;
	const FVector TowardDestination = ToDestination.GetSafeNormal2D();
	const ARogueCharacter* Player = GetPlayerCharacter();
	const bool bFollowGoal = !bDetouring && FVector::DotProduct(NormalizedDirection, TowardDestination) > .99f;
	const bool bStopAtPlayer = bFollowGoal && !bHealer && !CurrentArchetype.Ranged.bUsesRangedAttack && Player &&
		FVector::DistSquared2D(Destination, Player->GetActorLocation()) < 1.f;
	if (bFollowGoal)
	{
		float Remaining = ToDestination.Size2D();
		if (bStopAtPlayer)
		{
			// 接触伤害半径还有 8 cm 余量，近战到达玩家身边就停。
			Remaining -= GetCapsuleComponent()->GetScaledCapsuleRadius() +
				Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 2.f;
		}
		RouteStep = FMath::Clamp(Remaining, bStopAtPlayer ? -MaxStep : 0.f, MaxStep);
	}

	const FVector RouteDelta = NormalizedDirection * RouteStep;
	FVector Delta = RouteDelta;
	FHitResult Edge;
	if (!bDetouring && MaxStep > 0.f)
	{
		const FVector Steering = SampleCrowdSteering();
		if (!Steering.IsNearlyZero())
		{
			FVector Candidate = RouteDelta + Steering * MaxStep;
			if (bStopAtPlayer)
			{
				// 侧让也遵守贴身距离，已经重叠时优先退出玩家胶囊。
				const float Inward = FVector::DotProduct(Candidate, TowardDestination);
				Candidate -= TowardDestination * FMath::Max(0.f, Inward - RouteStep);
			}
			Candidate = Candidate.GetClampedToMaxSize2D(MaxStep);
			if (!ObstacleMovement || Candidate.IsNearlyZero() || ObstacleMovement->CanAdvanceOnGround(Candidate, Edge))
				Delta = Candidate;
		}
	}
	if (Delta.IsNearlyZero())
	{
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	const bool bCrowdStep = !Delta.Equals(RouteDelta, KINDA_SMALL_NUMBER);
	if (!bCrowdStep && ObstacleMovement && !ObstacleMovement->CanAdvanceOnGround(Delta, Edge))
	{
		ObstacleMovement->NotifyBlocked(Edge, Desired);
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	if (!bHealer)
		SetActorRotation(
			FMath::RInterpTo(GetActorRotation(), NormalizedDirection.Rotation(), DeltaSeconds, 8.f));
	if (bHealer)
	{
		float WalkingSpeed = bCrowdStep ? Delta.Size2D() / DeltaSeconds : GetMoveSpeed() * SpeedMultiplier;
		if (ObstacleMovement && ObstacleMovement->IsDetouring())
		{
			const float Remaining = ObstacleMovement->LimitMovementStep(TNumericLimits<float>::Max());
			const float Braking = FMath::Max(1.f, GetCharacterMovement()->BrakingDecelerationWalking);
			WalkingSpeed =
				FMath::Min(WalkingSpeed, FMath::Min(Remaining / FMath::Max(DeltaSeconds, .001f),
													FMath::Sqrt(2 * Braking * FMath::Max(Remaining, 1.f))));
			if (FVector::DotProduct(GetCharacterMovement()->Velocity.GetSafeNormal2D(), NormalizedDirection) <
				.7f)
				GetCharacterMovement()->StopMovementImmediately();
		}
		GetCharacterMovement()->MaxWalkSpeed = WalkingSpeed;
		AddMovementInput(Delta.GetSafeNormal2D(), 1.f, true);
		return;
	}
	FHitResult Hit;
	// SafeMove resolves initial overlaps before retrying the same swept planar move.
	GetCharacterMovement()->SafeMoveUpdatedComponent(Delta, GetActorQuat(), true, Hit);
	if (bCrowdStep && Hit.bBlockingHit)
	{
		// 侧让撞到墙时，剩余步长沿原路线走，避免把短暂侧让误判成路线被堵。
		Delta = RouteDelta * (1.f - Hit.Time);
		Hit = FHitResult();
		if (!Delta.IsNearlyZero())
		{
			if (!ObstacleMovement || ObstacleMovement->CanAdvanceOnGround(Delta, Hit))
				GetCharacterMovement()->SafeMoveUpdatedComponent(Delta, GetActorQuat(), true, Hit);
			else
				Delta = FVector::ZeroVector;
		}
	}
    if (Hit.bBlockingHit && !RogueEnemyArchetypes::IsFlyingType(EnemyType))
    {
        auto* Movement=GetCharacterMovement();
        // 普通怪的手动平移也要执行 UE 的台阶处理；整个过程仍使用胶囊扫掠，不瞬移穿墙。
        if (Movement->IsMovingOnGround() &&
            Hit.ImpactPoint.Z <= GetActorLocation().Z-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+Movement->MaxStepHeight+2.f &&
            Movement->CanStepUp(Hit) &&
            Movement->StepUp(FVector(0,0,-1),Delta*(1.f-Hit.Time),Hit))
            Hit=FHitResult();
    }
	if (Hit.bBlockingHit && ObstacleMovement)
	{
		ObstacleMovement->NotifyBlocked(Hit, Desired);
		// Slide only in XY. Slope/floor resolution and falling remain CharacterMovement's job.
		const FVector N = Hit.ImpactNormal.GetSafeNormal2D();
		if (!N.IsNearlyZero())
			AddActorWorldOffset(FVector::VectorPlaneProject(Delta * (1 - Hit.Time), N), true);
	}
	// 手动平移绕过了 CharacterMovement 的位移，不能继续沿用旧位置的地面缓存。
	GetCharacterMovement()->bForceNextFloorCheck = true;
}

FVector ARogueEnemy::CalculateArchetypeMovement(float DeltaSeconds)
{
	// 治疗者寻找友军不依赖已缓存的玩家索敌目标，行为树也走同一入口。
	if (RogueEnemyArchetypes::IsHealerType(EnemyType))
		return GetMovementDirection(FVector::ZeroVector, 0.f, DeltaSeconds);
	const ARogueCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FVector::ZeroVector;
	}

	const FVector ToPlayer = PlayerCharacter->GetActorLocation() - GetActorLocation();
	const float DistanceToPlayer = ToPlayer.Size2D();
	return GetMovementDirection(ToPlayer, DistanceToPlayer, DeltaSeconds);
}

void ARogueEnemy::UpdateBurstTimers(float DeltaSeconds)
{
	const FRogueEnemyMovementArchetype &MovementArchetype = CurrentArchetype.Movement;
	BurstCooldown -= DeltaSeconds;
	BurstTimeRemaining -= DeltaSeconds;

	if (BurstCooldown <= 0.0f)
	{
		BurstCooldown = MovementArchetype.BurstCycleTime;
		BurstTimeRemaining = MovementArchetype.BurstDuration;
	}
}

FVector ARogueEnemy::GetMovementDirection(const FVector &ToPlayer, float DistanceToPlayer, float DeltaSeconds)
{
	const FRogueEnemyMovementArchetype &MovementArchetype = CurrentArchetype.Movement;
	if (MovementArchetype.Model == ERogueEnemyMovementModel::Stationary)
		return FVector::ZeroVector;
	if (RogueEnemyArchetypes::IsHealerType(EnemyType) && EnemySupport)
		return EnemySupport->CalculateSupportMovement(DeltaSeconds);
	const FVector TowardPlayer = ToPlayer.GetSafeNormal2D();
	if (TowardPlayer.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	switch (MovementArchetype.Model)
	{
	case ERogueEnemyMovementModel::Stationary:
		return FVector::ZeroVector;
	case ERogueEnemyMovementModel::Direct:
		return TowardPlayer * MovementArchetype.BaseMoveMultiplier;
	case ERogueEnemyMovementModel::BurstCharge:
		BurstCooldown -= DeltaSeconds;
		BurstTimeRemaining -= DeltaSeconds;

		if (BurstCooldown <= 0.0f)
		{
			BurstCooldown = MovementArchetype.BurstCycleTime;
			BurstTimeRemaining = MovementArchetype.BurstDuration;
		}

		return TowardPlayer * (BurstTimeRemaining > 0.0f ? MovementArchetype.BurstActiveMultiplier
														 : MovementArchetype.BurstIdleMultiplier);
	case ERogueEnemyMovementModel::Orbit: {
		const FVector Tangent = FVector(-TowardPlayer.Y, TowardPlayer.X, 0.0f) * OrbitDirection;
		if (DistanceToPlayer > MovementArchetype.FarDistance)
		{
			return (TowardPlayer * MovementArchetype.FarTowardWeight +
					Tangent * MovementArchetype.FarTangentWeight)
				.GetSafeNormal2D();
		}

		if (DistanceToPlayer < MovementArchetype.CloseDistance)
		{
			return (TowardPlayer * MovementArchetype.CloseTowardWeight +
					Tangent * MovementArchetype.CloseTangentWeight)
				.GetSafeNormal2D();
		}

		return (Tangent * MovementArchetype.MidTangentWeight +
				TowardPlayer * MovementArchetype.MidTowardWeight)
			.GetSafeNormal2D();
	}
	case ERogueEnemyMovementModel::RangedKite: {
		const FVector Tangent = FVector(-TowardPlayer.Y, TowardPlayer.X, 0.0f) * OrbitDirection;
		if (DistanceToPlayer > MovementArchetype.FarDistance)
		{
			return (TowardPlayer * MovementArchetype.FarTowardWeight +
					Tangent * MovementArchetype.FarTangentWeight)
				.GetSafeNormal2D();
		}

		if (DistanceToPlayer < MovementArchetype.CloseDistance)
		{
			return (TowardPlayer * MovementArchetype.CloseTowardWeight +
					Tangent * MovementArchetype.CloseTangentWeight)
				.GetSafeNormal2D();
		}

		const float MidTowardWeight =
			MovementArchetype.bUseRangeBiasForMidToward
				? FMath::Clamp((MovementArchetype.PreferredRange - DistanceToPlayer) /
								   FMath::Max(MovementArchetype.PreferredRange, 1.0f),
							   -1.0f, 1.0f) *
					  MovementArchetype.MidTowardWeight
				: MovementArchetype.MidTowardWeight;
		return (Tangent * MovementArchetype.MidTangentWeight + TowardPlayer * MidTowardWeight)
			.GetSafeNormal2D();
	}
	case ERogueEnemyMovementModel::ZigZagChase: {
		const FVector Tangent = FVector(-TowardPlayer.Y, TowardPlayer.X, 0.0f) * OrbitDirection;
		const float ZigZagAlpha = FMath::Sin(GetGameTimeSinceCreation() * MovementArchetype.ZigZagFrequency +
											 OrbitDirection * 1.6f);
		if (DistanceToPlayer < MovementArchetype.CloseDistance)
		{
			return (TowardPlayer * MovementArchetype.CloseTowardWeight +
					Tangent * MovementArchetype.CloseTangentWeight * FMath::Sign(ZigZagAlpha))
				.GetSafeNormal2D();
		}

		return (TowardPlayer * MovementArchetype.ZigZagTowardWeight +
				Tangent * ZigZagAlpha * MovementArchetype.ZigZagTangentWeight)
			.GetSafeNormal2D();
	}
	default:
		return TowardPlayer * MovementArchetype.BaseMoveMultiplier;
	}
}

float ARogueEnemy::GetMoveSpeed() const
{
    const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0;float Slow=0;
    for(const auto& Pair:MovementSlows)
        if(Pair.Key.IsValid() && Pair.Value.Until>Now)Slow=FMath::Max(Slow,Pair.Value.Fraction);
    return MoveSpeed*(1-Slow);
}
void ARogueEnemy::RefreshMovementSlow(AActor* Source,float Fraction,float Duration)
{
    if(!HasAuthority() || IsDead() || !IsValid(Source))return;
    const float Now=GetWorld()->GetTimeSeconds();
    for(auto It=MovementSlows.CreateIterator();It;++It)if(!It.Key().IsValid() || It.Value().Until<=Now)It.RemoveCurrent();
    auto& Slow=MovementSlows.FindOrAdd(Source);Slow.Fraction=FMath::Clamp(Fraction,0.f,.95f);Slow.Until=Now+FMath::Max(0.f,Duration);
}
void ARogueEnemy::ClearMovementSlow(AActor* Source){MovementSlows.Remove(Source);}
