#include "Core/RogueGameMode.h"

#include "World/RogueArena.h"
#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/RogueEnemyProjectile.h"
#include "World/RogueExperiencePickup.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Combat/RogueImpactEffect.h"
#include "Combat/RogueLaserBeam.h"
#include "Combat/RogueOrbitingBlade.h"
#include "Combat/RogueProjectile.h"
#include "Combat/RogueRocketProjectile.h"
#include "Core/RogueCombatPools.h"
#include "Core/RogueEnemyTracker.h"
#include "Core/RogueGameBalanceAsset.h"
#include "Core/RogueUpgradeDefinitionAsset.h"
#include "Core/RogueUpgradeRuleAsset.h"
#include "Core/RogueGameModeRules.h"
#include "Core/RogueUpgradeSystem.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ARogueGameMode::ARogueGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ARogueCharacter::StaticClass();
	HUDClass = ARogueHUD::StaticClass();
	ArenaClass = ARogueArena::StaticClass();
	ShopTerminalClass = ARogueShopTerminal::StaticClass();
}

void ARogueGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!SpawnedArena.IsValid())
	{
		UClass* ArenaToSpawn = ArenaClass ? ArenaClass.Get() : ARogueArena::StaticClass();
		SpawnedArena = GetWorld()->SpawnActor<ARogueArena>(ArenaToSpawn, FVector::ZeroVector, FRotator::ZeroRotator);
	}

	if (!SpawnedShopTerminal.IsValid())
	{
		UClass* ShopTerminalToSpawn = ShopTerminalClass ? ShopTerminalClass.Get() : ARogueShopTerminal::StaticClass();
		const float TerminalOffset = SpawnedArena.IsValid() ? FMath::Min(SpawnedArena->GetArenaHalfExtent() * 0.42f, 2400.0f) : 1800.0f;
		SpawnedShopTerminal = GetWorld()->SpawnActor<ARogueShopTerminal>(ShopTerminalToSpawn, FVector(0.0f, -TerminalOffset, 0.0f), FRotator::ZeroRotator);
	}

	OptimizeLevelEnvironment();

	CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (CachedCharacter.IsValid())
	{
		CachedCharacter->SetActorLocation(FVector(0.0f, 0.0f, 120.0f));
	}

	LoadedGameBalanceAsset = GameBalanceAsset.IsNull() ? nullptr : GameBalanceAsset.LoadSynchronous();
	UpgradeSystem.SetDefinitionAsset(UpgradeDefinitionAsset.IsNull() ? nullptr : UpgradeDefinitionAsset.LoadSynchronous());
	UpgradeSystem.SetRuleAsset(UpgradeRuleAsset.IsNull() ? nullptr : UpgradeRuleAsset.LoadSynchronous());
	ShopSystem.Configure(
		RogueGameModeRules::GetShopOfferCount(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopOfferCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopRefreshCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopAutoRefreshInterval(LoadedGameBalanceAsset));

	PrewarmCombatPools();
}

void ARogueGameMode::OptimizeLevelEnvironment()
{
	if (!SceneOptimizationSettings.bOptimizeSceneForPerformance || GetWorld() == nullptr)
	{
		return;
	}

	ADirectionalLight* PrimaryDirectionalLight = nullptr;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		if (PrimaryDirectionalLight == nullptr)
		{
			PrimaryDirectionalLight = *It;
			continue;
		}

		DisableActorRendering(*It);
	}

	if (PrimaryDirectionalLight != nullptr)
	{
		if (ULightComponent* LightComponent = PrimaryDirectionalLight->GetLightComponent())
		{
			LightComponent->SetIntensity(SceneOptimizationSettings.PrimaryDirectionalLightIntensity);
			LightComponent->SetLightColor(FLinearColor(1.00f, 0.95f, 0.88f));
			LightComponent->SetCastShadows(false);
			LightComponent->SetVisibility(true);
		}

		PrimaryDirectionalLight->SetActorRotation(SceneOptimizationSettings.OptimizedSunRotation);
	}

	ASkyLight* PrimarySkyLight = nullptr;
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		if (PrimarySkyLight == nullptr)
		{
			PrimarySkyLight = *It;
			continue;
		}

		DisableActorRendering(*It);
	}

	if (PrimarySkyLight != nullptr)
	{
		if (USkyLightComponent* LightComponent = PrimarySkyLight->GetLightComponent())
		{
			LightComponent->SetIntensity(SceneOptimizationSettings.PrimarySkyLightIntensity);
			LightComponent->SetLightColor(FLinearColor(0.34f, 0.48f, 0.78f));
			LightComponent->SetCastShadows(false);
			LightComponent->SetVisibility(true);
		}
	}

	for (TActorIterator<ALight> It(GetWorld()); It; ++It)
	{
		ALight* LightActor = *It;
		if (!IsValid(LightActor) || LightActor == PrimaryDirectionalLight)
		{
			continue;
		}

		DisableActorRendering(LightActor);
	}

	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		(*It)->bEnabled = false;
		(*It)->BlendWeight = 0.0f;
		DisableActorRendering(*It);
	}

	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		DisableActorRendering(*It);
	}

	for (TActorIterator<ASkyAtmosphere> It(GetWorld()); It; ++It)
	{
		DisableActorRendering(*It);
	}

	for (TActorIterator<AVolumetricCloud> It(GetWorld()); It; ++It)
	{
		DisableActorRendering(*It);
	}
}

void ARogueGameMode::DisableActorRendering(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			SceneComponent->SetVisibility(false, true);
			SceneComponent->SetComponentTickEnabled(false);
		}

		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			PrimitiveComponent->SetCastShadow(false);
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ARogueGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (RunState.bGameOver || UpgradeSystem.IsAwaitingChoice() || ShopSystem.IsOpen())
	{
		return;
	}

	RunState.RunTimeSeconds += DeltaSeconds;
	RunState.SpawnTimer -= DeltaSeconds;
	EnemyTracker.TickMaintenance(DeltaSeconds, RuntimePerformanceSettings);
	ShopSystem.AdvanceAutoRefresh(DeltaSeconds, CachedCharacter.Get(), UpgradeSystem);

	const int32 TargetWave = FMath::FloorToInt(RunState.RunTimeSeconds / SpawnSettings.WaveDuration) + 1;
	if (TargetWave != RunState.CurrentWave)
	{
		RunState.CurrentWave = TargetWave;
	}

	if (BossSettings.BossWaveInterval > 0 && RunState.CurrentWave > 0 && RunState.CurrentWave % BossSettings.BossWaveInterval == 0 && !RunState.bBossAlive && RunState.LastBossWaveSpawned != RunState.CurrentWave)
	{
		SpawnBoss();
	}

	int32 AliveEnemies = EnemyTracker.GetActiveEnemyCount();
	const FRogueSpawnCadence SpawnCadence = RogueGameModeRules::BuildSpawnCadence(RunState.CurrentWave, AliveEnemies, SpawnSettings.MaxAliveEnemies, RunState.bBossAlive, SpawnSettings.SpawnInterval, LoadedGameBalanceAsset);
	if (RunState.SpawnTimer <= 0.0f && AliveEnemies < SpawnCadence.EffectiveMaxAliveEnemies)
	{
		for (int32 SpawnIndex = 0; SpawnIndex < SpawnCadence.SpawnBatchSize && AliveEnemies < SpawnCadence.EffectiveMaxAliveEnemies; ++SpawnIndex)
		{
			SpawnEnemy();
			++AliveEnemies;
		}

		RunState.SpawnTimer = SpawnCadence.NextSpawnDelay;
	}
}

void ARogueGameMode::HandleEnemyKilled(ARogueEnemy* Enemy, AActor* Killer)
{
	++RunState.EnemiesDefeated;
	UnregisterEnemy(Enemy);

	if (IsValid(Enemy))
	{
		ARogueCharacter* RewardCharacter = Cast<ARogueCharacter>(Killer);
		if (RewardCharacter == nullptr && IsValid(Killer))
		{
			RewardCharacter = Cast<ARogueCharacter>(Killer->GetOwner());
			if (RewardCharacter == nullptr)
			{
				RewardCharacter = Cast<ARogueCharacter>(Killer->GetInstigator());
			}
		}
		if (RewardCharacter == nullptr)
		{
			RewardCharacter = CachedCharacter.Get();
		}
		if (RewardCharacter != nullptr)
		{
			RewardCharacter->AddMoney(RogueGameModeRules::BuildCurrencyReward(RunState.CurrentWave, Enemy->IsBoss(), LoadedGameBalanceAsset));
		}

		ARogueExperiencePickup* MergeTargetPickup = EnemyTracker.FindPickupMergeTarget(Enemy->GetActorLocation(), 170.0f);

		if (MergeTargetPickup != nullptr)
		{
			MergeTargetPickup->AddExperienceValue(Enemy->GetExperienceReward());
		}
		else
		{
			ARogueExperiencePickup* Pickup = AcquireExperiencePickup(this, Enemy->GetActorLocation());
			if (Pickup != nullptr)
			{
				Pickup->ActivatePooledPickup(this, Enemy->GetActorLocation(), Enemy->GetExperienceReward());
				RegisterExperiencePickup(Pickup);
			}
		}

		if (Enemy->IsBoss())
		{
			RunState.bBossAlive = false;
			if (!CachedCharacter.IsValid())
			{
				CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			}

			QueueUpgradeSelections(BossSettings.BossBonusSelections, CachedCharacter.Get());
		}
	}
}

void ARogueGameMode::RegisterEnemy(ARogueEnemy* Enemy)
{
	EnemyTracker.RegisterEnemy(Enemy);
}

void ARogueGameMode::UnregisterEnemy(ARogueEnemy* Enemy)
{
	EnemyTracker.UnregisterEnemy(Enemy);
}

void ARogueGameMode::RegisterExperiencePickup(ARogueExperiencePickup* Pickup)
{
	EnemyTracker.RegisterPickup(Pickup);
}

void ARogueGameMode::UnregisterExperiencePickup(ARogueExperiencePickup* Pickup)
{
	EnemyTracker.UnregisterPickup(Pickup);
}

ARogueProjectile* ARogueGameMode::AcquirePlayerProjectile(TSubclassOf<ARogueProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquirePlayerProjectile(GetWorld(), ProjectileClass, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueEnemy* ARogueGameMode::AcquireEnemy(TSubclassOf<ARogueEnemy> EnemyClassToUse, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireEnemy(GetWorld(), EnemyClassToUse, OwnerActor, SpawnLocation, SpawnRotation);
}

ARogueEnemyProjectile* ARogueGameMode::AcquireEnemyProjectile(TSubclassOf<ARogueEnemyProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireEnemyProjectile(GetWorld(), ProjectileClass, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueRocketProjectile* ARogueGameMode::AcquireRocketProjectile(TSubclassOf<ARogueRocketProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireRocketProjectile(GetWorld(), ProjectileClass, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueExperiencePickup* ARogueGameMode::AcquireExperiencePickup(AActor* OwnerActor, const FVector& SpawnLocation)
{
	return CombatPools.AcquireExperiencePickup(GetWorld(), ExperiencePickupClass, OwnerActor, SpawnLocation);
}

ARogueImpactEffect* ARogueGameMode::AcquireImpactEffect(const FVector& SpawnLocation, const FRotator& SpawnRotation, AActor* OwnerActor)
{
	return CombatPools.AcquireImpactEffect(GetWorld(), OwnerActor, SpawnLocation, SpawnRotation);
}

ARogueLaserBeam* ARogueGameMode::AcquireLaserBeam(TSubclassOf<ARogueLaserBeam> BeamClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireLaserBeam(GetWorld(), BeamClass, OwnerActor, SpawnLocation, SpawnRotation);
}

ARogueOrbitingBlade* ARogueGameMode::AcquireOrbitingBlade(TSubclassOf<ARogueOrbitingBlade> BladeClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireOrbitingBlade(GetWorld(), BladeClass, OwnerActor, SpawnLocation, SpawnRotation);
}

ARogueEnemy* ARogueGameMode::FindNearestEnemyInRange(const FVector& Origin, float MaxRange, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies)
{
	return EnemyTracker.FindNearestEnemyInRange(Origin, MaxRange, IgnoredEnemies, RuntimePerformanceSettings);
}

void ARogueGameMode::CollectEnemiesInRange(const FVector& Origin, float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults, bool bSortByDistance)
{
	EnemyTracker.CollectEnemiesInRange(Origin, Range, OutEnemies, MaxResults, bSortByDistance, RuntimePerformanceSettings);
}

bool ARogueGameMode::GetShopPromptWorldLocation(FVector& OutLocation) const
{
	const ARogueShopTerminal* ShopTerminal = SpawnedShopTerminal.Get();
	if (!IsValid(ShopTerminal))
	{
		return false;
	}

	OutLocation = ShopTerminal->GetPromptWorldLocation();
	return true;
}

void ARogueGameMode::StartUpgradeSelection(ARogueCharacter* Character)
{
	QueueUpgradeSelections(1, Character);
}

void ARogueGameMode::TryOpenShop(ARogueCharacter* Character)
{
	if (RunState.bGameOver || UpgradeSystem.IsAwaitingChoice() || ShopSystem.IsOpen() || !IsValid(Character) || Character->IsDead())
	{
		return;
	}

	CachedCharacter = Character;
	ShopSystem.Configure(
		RogueGameModeRules::GetShopOfferCount(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopOfferCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopRefreshCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopAutoRefreshInterval(LoadedGameBalanceAsset));

	if (!ShopSystem.OpenShop(Character, UpgradeSystem))
	{
		return;
	}

	Character->SetMenuInteractionInput(true);
	UGameplayStatics::SetGamePaused(this, true);
}

void ARogueGameMode::CloseShop()
{
	if (!ShopSystem.IsOpen())
	{
		return;
	}

	ShopSystem.CloseShop();
	if (CachedCharacter.IsValid() && !UpgradeSystem.IsAwaitingChoice() && !RunState.bGameOver)
	{
		CachedCharacter->SetMenuInteractionInput(false);
	}
	UGameplayStatics::SetGamePaused(this, false);
}

void ARogueGameMode::TryBuyShopOffer(int32 OfferIndex)
{
	if (!ShopSystem.IsOpen() || !CachedCharacter.IsValid())
	{
		return;
	}

	FRogueUpgradeOption PurchasedUpgrade;
	if (ShopSystem.TryBuyOffer(OfferIndex, CachedCharacter.Get(), PurchasedUpgrade))
	{
		CachedCharacter->ApplyUpgrade(PurchasedUpgrade);
	}
}

void ARogueGameMode::TryRefreshShop()
{
	if (!ShopSystem.IsOpen() || !CachedCharacter.IsValid())
	{
		return;
	}

	ShopSystem.TryRefresh(CachedCharacter.Get(), UpgradeSystem);
}

void ARogueGameMode::TrySelectUpgrade(int32 UpgradeIndex)
{
	if (!CachedCharacter.IsValid())
	{
		return;
	}

	FRogueUpgradeOption SelectedUpgrade;
	if (!UpgradeSystem.TryConsumeChoice(UpgradeIndex, SelectedUpgrade))
	{
		return;
	}

	CachedCharacter->ApplyUpgrade(SelectedUpgrade);

	if (UpgradeSystem.GetQueuedSelections() > 0)
	{
		OpenNextUpgradeSelection();
		return;
	}

	CachedCharacter->SetUpgradeSelectionInput(false);
	UGameplayStatics::SetGamePaused(this, false);
}

void ARogueGameMode::NotifyPlayerDied()
{
	RunState.bGameOver = true;
	RunState.bBossAlive = false;
	UpgradeSystem.Reset();
	ShopSystem.CloseShop();
	UGameplayStatics::SetGamePaused(this, false);

	if (CachedCharacter.IsValid())
	{
		CachedCharacter->SetMenuInteractionInput(true);
	}
}

void ARogueGameMode::SpawnEnemy()
{
	if (!CachedCharacter.IsValid())
	{
		CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	if (!CachedCharacter.IsValid() || CachedCharacter->IsDead())
	{
		return;
	}

	SpawnConfiguredEnemy(RogueGameModeRules::BuildEnemyProfile(RunState.CurrentWave, PickEnemyTypeForCurrentWave(), LoadedGameBalanceAsset), SpawnSettings.SpawnRadius, SpawnSettings.EnemySpawnMinimumDistance);
}

void ARogueGameMode::SpawnBoss()
{
	if (!CachedCharacter.IsValid())
	{
		CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	if (!CachedCharacter.IsValid() || CachedCharacter->IsDead())
	{
		return;
	}

	if (SpawnConfiguredEnemy(RogueGameModeRules::BuildBossProfile(RunState.CurrentWave, PickEnemyTypeForCurrentWave(), LoadedGameBalanceAsset), BossSettings.BossSpawnDistance, BossSettings.BossSpawnMinimumDistance) != nullptr)
	{
		RunState.bBossAlive = true;
		RunState.LastBossWaveSpawned = RunState.CurrentWave;
	}
}

ARogueEnemy* ARogueGameMode::SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance)
{
	const FVector SpawnLocation = FindSpawnLocation(DesiredDistance, MinimumDistance);
	UClass* EnemyToSpawn = EnemyClass ? EnemyClass.Get() : ARogueEnemy::StaticClass();
	ARogueEnemy* Enemy = AcquireEnemy(EnemyToSpawn, this, SpawnLocation, FRotator::ZeroRotator);
	if (Enemy == nullptr)
	{
		return nullptr;
	}

	Enemy->ActivatePooledEnemy(this, SpawnLocation, FRotator::ZeroRotator);
	Enemy->InitializeEnemy(EnemyProfile);
	RegisterEnemy(Enemy);
	return Enemy;
}

void ARogueGameMode::QueueUpgradeSelections(int32 Count, ARogueCharacter* Character)
{
	if (Count <= 0 || RunState.bGameOver)
	{
		return;
	}

	if (IsValid(Character))
	{
		CachedCharacter = Character;
	}

	if (!CachedCharacter.IsValid() || CachedCharacter->IsDead())
	{
		return;
	}

	UpgradeSystem.QueueSelections(Count);
	if (!UpgradeSystem.IsAwaitingChoice())
	{
		OpenNextUpgradeSelection();
	}
}

void ARogueGameMode::OpenNextUpgradeSelection()
{
	if (!CachedCharacter.IsValid() || CachedCharacter->IsDead())
	{
		return;
	}

	if (!UpgradeSystem.OpenNextSelection(CachedCharacter.Get()))
	{
		CachedCharacter->SetUpgradeSelectionInput(false);
		UGameplayStatics::SetGamePaused(this, false);
		return;
	}

	CachedCharacter->SetUpgradeSelectionInput(true);
	UGameplayStatics::SetGamePaused(this, true);
}

FVector ARogueGameMode::FindSpawnLocation(float DesiredDistance, float MinimumDistance) const
{
	if (!CachedCharacter.IsValid())
	{
		return FVector(0.0f, 0.0f, 60.0f);
	}

	const FVector PlayerLocation = CachedCharacter->GetActorLocation();
	const float ArenaHalfExtent = SpawnedArena.IsValid() ? SpawnedArena->GetArenaHalfExtent() - SpawnSettings.ArenaSpawnPadding : FMath::Max(SpawnSettings.SpawnRadius, DesiredDistance) + 500.0f;

	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		const FVector2D SpawnDirection = FVector2D(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f)).GetSafeNormal();
		const float DistanceScale = FMath::FRandRange(0.82f, 1.15f);
		const FVector CandidateLocation = PlayerLocation + FVector(SpawnDirection.X * DesiredDistance * DistanceScale, SpawnDirection.Y * DesiredDistance * DistanceScale, 60.0f);
		if (FMath::Abs(CandidateLocation.X) <= ArenaHalfExtent && FMath::Abs(CandidateLocation.Y) <= ArenaHalfExtent && FVector::DistSquared2D(CandidateLocation, PlayerLocation) >= FMath::Square(MinimumDistance))
		{
			return CandidateLocation;
		}
	}

	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		const FVector CandidateLocation(
			FMath::FRandRange(-ArenaHalfExtent, ArenaHalfExtent),
			FMath::FRandRange(-ArenaHalfExtent, ArenaHalfExtent),
			60.0f);

		if (FVector::DistSquared2D(CandidateLocation, PlayerLocation) >= FMath::Square(MinimumDistance))
		{
			return CandidateLocation;
		}
	}

	return FVector(
		FMath::Clamp(PlayerLocation.X + DesiredDistance * 0.5f, -ArenaHalfExtent, ArenaHalfExtent),
		FMath::Clamp(PlayerLocation.Y + DesiredDistance * 0.5f, -ArenaHalfExtent, ArenaHalfExtent),
		60.0f);
}

ERogueEnemyType ARogueGameMode::PickEnemyTypeForCurrentWave() const
{
	return RogueGameModeRules::PickWeightedEnemyTypeForWave(RunState.CurrentWave, LoadedGameBalanceAsset);
}

void ARogueGameMode::PrewarmCombatPools()
{
	CombatPools.Prewarm(GetWorld(), this, PoolSettings, EnemyClass, ExperiencePickupClass);
}
