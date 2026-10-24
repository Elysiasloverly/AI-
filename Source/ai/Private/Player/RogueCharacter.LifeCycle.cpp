#include "Player/RogueCharacter.h"

#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Player/RoguePlayerState.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

void ARogueCharacter::PreInitializeComponents()
{
	// GameMode 负责玩家分配；禁用 Player0 自动接管，避免新 Pawn 额外抢占首个控制器。
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	Super::PreInitializeComponents();
}

void ARogueCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReviveTimerHandle);
		World->GetTimerManager().ClearTimer(PendingDeathTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ARogueCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	ApplyDownedState();
}

bool ARogueCharacter::IsAlive() const
{
	if (LifeState.Phase != ERoguePlayerLifePhase::Alive || bPendingDeath)
	{
		return false;
	}
	// 服务端致死 GE 的下一帧善后尚未执行时，也必须立即退出索敌/团灭的存活集合。
	if (IsAuthoritySide())
	{
		const ARoguePlayerState* State = GetPlayerState<ARoguePlayerState>();
		// 档案握手期间还没有本局属性，暂时退出索敌/伤害集合；不是一次濒死或团灭。
		if (State != nullptr && !State->IsBaseAttributesInitialized())
		{
			return false;
		}
		const URogueAttributeSet* Attributes = GetRogueAttributeSet();
		if (State != nullptr && State->IsBaseAttributesInitialized() && Attributes != nullptr && Attributes->GetHealth() <= 0.0f)
		{
			return false;
		}
	}
	return true;
}

float ARogueCharacter::GetLifeServerTime() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0.0f;
	}
	if (World->GetNetMode() == NM_Client)
	{
		if (const AGameStateBase* State = World->GetGameState())
		{
			return State->GetServerWorldTimeSeconds();
		}
	}
	return World->GetTimeSeconds();
}

float ARogueCharacter::GetReviveSecondsRemaining() const
{
	return IsRevivePending()
		? FMath::Max(0.0f, LifeState.ReviveServerTime - GetLifeServerTime())
		: 0.0f;
}

bool ARogueCharacter::IsRevivePending() const
{
	return IsDowned() && LifeState.ReviveServerTime > 0.0f;
}

void ARogueCharacter::ProcessDeath()
{
	if (IsAuthoritySide() && LifeState.Phase == ERoguePlayerLifePhase::Alive)
	{
		Die();
	}
	bPendingDeath = false;
}

void ARogueCharacter::Die()
{
	UWorld* World = GetWorld();
	if (!IsAuthoritySide() || World == nullptr || LifeState.Phase != ERoguePlayerLifePhase::Alive)
	{
		return;
	}
	const ARoguePlayerState* State = GetPlayerState<ARoguePlayerState>();
	if (State != nullptr && !State->IsBaseAttributesInitialized())
	{
		return;
	}
	World->GetTimerManager().ClearTimer(PendingDeathTimerHandle);
	bPendingDeath = false;
	LifeState.Phase = ERoguePlayerLifePhase::Downed;
	const FRoguePlayerRevivalConfig Revival = LoadedPlayerBalanceAsset
		? LoadedPlayerBalanceAsset->Revival : FRoguePlayerRevivalConfig();
	const float WaitSeconds = FMath::IsFinite(Revival.DownedDuration)
		? FMath::Max(0.01f, Revival.DownedDuration) : RoguePlayerLifeCycle::DownedDurationSeconds;
	LifeState.ReviveServerTime = World->GetTimeSeconds() + WaitSeconds;
	RemoveInvulnerableEffect();
	ApplyLifeStateEffects();
	ApplyDownedState();
	StartReviveTimer();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] %s 濒死：幽灵状态，%.2f 秒后复活（服务端截止 %.3f）"), *GetName(), WaitSeconds, LifeState.ReviveServerTime);
	if (ARogueGameMode* Mode = World->GetAuthGameMode<ARogueGameMode>())
	{
		Mode->NotifyPlayerDied(this);
	}
}

void ARogueCharacter::StartReviveTimer()
{
	if (!IsAuthoritySide() || !IsRevivePending() || GetWorld() == nullptr)
	{
		return;
	}
	const float Remaining = FMath::Max(0.01f, LifeState.ReviveServerTime - GetWorld()->GetTimeSeconds());
	GetWorldTimerManager().SetTimer(ReviveTimerHandle, this, &ARogueCharacter::TryReviveIfExpired, Remaining, false);
}

void ARogueCharacter::TryReviveIfExpired()
{
	if (!IsAuthoritySide() || !IsRevivePending() || GetWorld() == nullptr)
	{
		return;
	}
	if (ARogueGameMode* Mode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		// 先处理同帧致死/团灭，避免刚到期的濒死玩家抢先解除团灭。
		if (Mode->IsGameOver() || Mode->EvaluateTeamWipe())
		{
			FinalizeForGameOver();
			return;
		}
	}
	if (LifeState.ReviveServerTime - GetWorld()->GetTimeSeconds() > KINDA_SMALL_NUMBER)
	{
		StartReviveTimer();
		return;
	}
	Revive();
}

void ARogueCharacter::Revive()
{
	if (!IsAuthoritySide() || !IsRevivePending() || GetWorld() == nullptr
		|| LifeState.ReviveServerTime - GetWorld()->GetTimeSeconds() > KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (ARogueGameMode* Mode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		if (Mode->IsGameOver() || Mode->EvaluateTeamWipe())
		{
			FinalizeForGameOver();
			return;
		}
	}
	GetWorldTimerManager().ClearTimer(ReviveTimerHandle);
	bPendingDeath = false;
	LifeState.Phase = ERoguePlayerLifePhase::Alive;
	LifeState.ReviveServerTime = 0.0f;
	ApplyLifeStateEffects();
	const FRoguePlayerRevivalConfig Revival = LoadedPlayerBalanceAsset
		? LoadedPlayerBalanceAsset->Revival : FRoguePlayerRevivalConfig();
	if (const URogueAttributeSet* Attributes = GetRogueAttributeSet())
	{
		SetCurrentHealth(FMath::Min(Attributes->GetMaxHealth(), FMath::Max(1.0f,
			Attributes->GetMaxHealth() * FMath::Clamp(Revival.HealthRatio, 0.0f, 1.0f))));
	}
	ClearRecentlyDamagedState();
	ClearDashCooldown();
	SetActorTransform(ReviveSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetUpgradeSelectionInput(false);
	ApplyDownedState();
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		RemoveInvulnerableEffect();
		if (FMath::IsFinite(Revival.InvulnerableSeconds) && Revival.InvulnerableSeconds > 0.0f)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(URogueInvulnerableEffect::StaticClass(), 1.0f, Context);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Duration, Revival.InvulnerableSeconds);
				InvulnerableEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}
	SyncAttributeMirrors();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] %s 已复活：碰撞/移动恢复，等待界面由 Alive 快照收起"), *GetName());
}

void ARogueCharacter::FinalizeForGameOver()
{
	if (!IsAuthoritySide() || GetWorld() == nullptr)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(ReviveTimerHandle);
	GetWorldTimerManager().ClearTimer(PendingDeathTimerHandle);
	bPendingDeath = false;
	LifeState.Phase = ERoguePlayerLifePhase::Eliminated;
	LifeState.ReviveServerTime = 0.0f;
	RemoveInvulnerableEffect();
	ApplyLifeStateEffects();
	ApplyDownedState();
	ForceNetUpdate();
}

void ARogueCharacter::RemoveDownedEffect()
{
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		FGameplayTagContainer StateTags;
		StateTags.AddTag(RogueGameplayTags::State_Downed);
		ASC->RemoveActiveEffectsWithGrantedTags(StateTags);
	}
}

void ARogueCharacter::RemoveInvulnerableEffect()
{
	InvulnerableEffectHandle = FActiveGameplayEffectHandle();
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		FGameplayTagContainer StateTags;
		StateTags.AddTag(RogueGameplayTags::State_Invulnerable);
		ASC->RemoveActiveEffectsWithGrantedTags(StateTags);
	}
}

void ARogueCharacter::ApplyLifeStateEffects()
{
	if (!IsAuthoritySide())
	{
		return;
	}
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}
	if (LifeState.Phase == ERoguePlayerLifePhase::Alive)
	{
		FGameplayTagContainer StateTags;
		StateTags.AddTag(RogueGameplayTags::State_Dead);
		StateTags.AddTag(RogueGameplayTags::State_Downed);
		ASC->RemoveActiveEffectsWithGrantedTags(StateTags);
	}
	else
	{
		if (!ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead))
		{
			ASC->ApplyGameplayEffectToSelf(GetDefault<URogueDeathEffect>(), 1.0f, ASC->MakeEffectContext());
		}
		if (IsDowned() && !ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Downed))
		{
			ASC->ApplyGameplayEffectToSelf(GetDefault<URogueDownedEffect>(), 1.0f, ASC->MakeEffectContext());
		}
		else if (!IsDowned())
		{
			RemoveDownedEffect();
		}
	}
	if (APlayerState* State = GetPlayerState())
	{
		State->ForceNetUpdate();
	}
}

void ARogueCharacter::ApplyDownedState()
{
	if (!IsAlive())
	{
		if (!bGhostStateApplied)
		{
			bGhostStateApplied = true;
			bActorCollisionBeforeDowned = GetActorEnableCollision();
			PendingWeaponCosmetics.Reset();
			ForwardInputValue = 0.0f;
			RightInputValue = 0.0f;
			StopJumping();
			for (ARogueWeaponBase* Weapon : Weapons)
			{
				if (IsValid(Weapon))
				{
					Weapon->OnOwnerDied();
				}
			}
			if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
			{
				FGameplayTagContainer DashTags;
				DashTags.AddTag(RogueGameplayTags::Ability_Dash);
				ASC->CancelAbilities(&DashTags);
			}
		}
		SetActorEnableCollision(false);
		TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
		for (UPrimitiveComponent* Component : Primitives)
		{
			if (!DownedCollisionSnapshots.ContainsByPredicate([Component](const FRoguePlayerCollisionSnapshot& Snapshot) { return Snapshot.Component.Get() == Component; }))
			{
				FRoguePlayerCollisionSnapshot& Snapshot = DownedCollisionSnapshots.AddDefaulted_GetRef();
				Snapshot.Component = Component;
				// 读取底层配置，忽略 Actor 的碰撞总开关；否则会把原配置错误保存成 NoCollision。
				const FBodyInstance* Body = Component->GetBodyInstance();
				Snapshot.Collision = Body != nullptr ? Body->GetCollisionEnabled(false) : Component->BodyInstance.GetCollisionEnabled(false);
				Snapshot.bGenerateOverlapEvents = Component->GetGenerateOverlapEvents();
				Snapshot.bSimulatePhysics = Component->IsSimulatingPhysics();
			}
			Component->SetGenerateOverlapEvents(false);
			Component->SetSimulatePhysics(false);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		ConsumeMovementInputVector();
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		DisableInput(Cast<APlayerController>(GetController()));
		return;
	}
	if (!bGhostStateApplied)
	{
		return;
	}
	SetActorEnableCollision(bActorCollisionBeforeDowned);
	for (const FRoguePlayerCollisionSnapshot& Snapshot : DownedCollisionSnapshots)
	{
		if (UPrimitiveComponent* Component = Snapshot.Component.Get())
		{
			Component->SetCollisionEnabled(Snapshot.Collision);
			Component->SetGenerateOverlapEvents(Snapshot.bGenerateOverlapEvents);
			Component->SetSimulatePhysics(Snapshot.bSimulatePhysics);
		}
	}
	DownedCollisionSnapshots.Reset();
	bGhostStateApplied = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	// APawn::EnableInput 在参数等于 Controller 或为空时置输入标志；
	// Controller 可能晚于 Alive 快照到达。这里只重新启用输入，不恢复先前禁用原因。
	EnableInput(Cast<APlayerController>(GetController()));
}

void ARogueCharacter::OnRep_LifeState()
{
	// 客户端仅应用幽灵/正常表现，不改血、不排定时器，也不依据迟到标签改变状态。
	ApplyDownedState();
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] NET Player=%s Pawn=%s Local=%d Controller=%s Phase=%d Pending=%d Remaining=%.2f Collision=%d ServerTime=%.3f"),
		GetPlayerState() != nullptr ? *GetPlayerState()->GetPlayerName() : TEXT("NoPlayerState"), *GetName(),
		IsLocallyControlled() ? 1 : 0, *GetNameSafe(GetController()), static_cast<int32>(LifeState.Phase),
		IsRevivePending() ? 1 : 0, GetReviveSecondsRemaining(), GetActorEnableCollision() ? 1 : 0, GetLifeServerTime());
}
