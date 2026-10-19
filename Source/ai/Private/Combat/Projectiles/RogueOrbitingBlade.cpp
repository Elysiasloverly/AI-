#include "Combat/Projectiles/RogueOrbitingBlade.h"

#include "Combat/Visuals/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Player/RogueCharacter.h"

ARogueOrbitingBlade::ARogueOrbitingBlade()
{
	PrimaryActorTick.bCanEverTick = true;
	// 每帧 Tick。原因见 Tick() 里的说明：伤害判定现在是"贴身持续判定"，
	// 必须和武器每帧推送的位置更新同频，不能再靠 0.03s 的粗粒度间隔。
	PrimaryActorTick.TickInterval = 0.0f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(VisualConfig.CollisionRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);   // Tick 里的贴身判定依赖重叠列表，必须产生重叠事件
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeRotation(VisualConfig.MeshRotation);
	Mesh->SetRelativeScale3D(VisualConfig.MeshScale);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BladeMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Wedge_A.Shape_Wedge_A"));
	if (BladeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(BladeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BladeMaterial(TEXT("/Game/GameAssets/StarterContent/Materials/M_Tech_Hex_Tile.M_Tech_Hex_Tile"));
	if (BladeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BladeMaterial.Object);
	}

	// Overlay 在现有网格上追加材质绘制；材质应采用适合的混合方式。
	// 这省去独立 GlowMesh 组件，但不能据此断言减少 draw call。
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/GameAssets/StarterContent/Materials/M_Tech_Hex_Tile_Pulse.M_Tech_Hex_Tile_Pulse"));
	if (GlowMaterial.Succeeded())
	{
		VisualConfig.GlowMaterial = GlowMaterial.Object;
	}

	// OverlayMaterialMaxDrawDistance = 0 → 不额外裁剪，跟随本体的可见距离
	Mesh->SetOverlayMaterial(VisualConfig.GlowMaterial);
	Mesh->SetOverlayMaterialMaxDrawDistance(0.0f);

	// 持续接触伤害读取重叠列表并使用 HitCooldown；BeginOverlap 只报告开始重叠，
	// 不会为持续重叠自动周期触发。
}

void ARogueOrbitingBlade::BeginPlay()
{
	Super::BeginPlay();
	BaseMeshScale = Mesh->GetRelativeScale3D();
	BaseCollisionRadius = Collision->GetUnscaledSphereRadius();

	// 在这里（而不是构造函数里）建 MID：BP 可能覆盖 VisualConfig.GlowMaterial，
	// 属性值是在构造之后才加载的，所以要以 BeginPlay 时读到的为准。
	SetupGlowMaterial();

	DeactivateToPool();
}

void ARogueOrbitingBlade::SetupGlowMaterial()
{
	if (Mesh == nullptr)
	{
		return;
	}

	// 尝试复用已缓存的 MID；仅在此初始化入口创建实例。
	// 反复创建会增加 UObject/GC 负担，但不等同于不可回收的内存泄漏。
	UMaterialInterface* CurrentOverlay = Mesh->GetOverlayMaterial();
	if (CurrentOverlay == nullptr)
	{
		return;
	}

	if (GlowMaterialInstance != nullptr && GlowMaterialInstance->Parent == CurrentOverlay)
	{
		return;
	}

	GlowMaterialInstance = UMaterialInstanceDynamic::Create(CurrentOverlay, this);
	if (GlowMaterialInstance == nullptr)
	{
		return;
	}

	Mesh->SetOverlayMaterial(GlowMaterialInstance);

	// 解析材质参数，缺失时跳过后续无效写入并使用下面的限次诊断。
	TArray<FMaterialParameterInfo> ScalarParams;
	TArray<FGuid> ScalarParamIds;
	GlowMaterialInstance->GetAllScalarParameterInfo(ScalarParams, ScalarParamIds);

	bHasGlowPulseParameter = false;
	for (const FMaterialParameterInfo& ParamInfo : ScalarParams)
	{
		if (ParamInfo.Name == VisualConfig.GlowPulseParameterName)
		{
			bHasGlowPulseParameter = true;
			break;
		}
	}

	if (!bHasGlowPulseParameter)
	{
		// 函数静态开关在此进程/模块生命周期内共享，不按刀片、World 或对局重置。
		static bool bWarnedAboutMissingGlowPulse = false;
		if (!bWarnedAboutMissingGlowPulse)
		{
			bWarnedAboutMissingGlowPulse = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[RogueOrbitingBlade] 发光材质 %s 里没有标量参数 '%s'，刀片不会脉冲。")
				TEXT("请在材质里加一个同名 Scalar Parameter，或把 VisualConfig.GlowPulseParameterName 改成材质里已有的参数名。"),
				*GetNameSafe(CurrentOverlay), *VisualConfig.GlowPulseParameterName.ToString());
		}
	}

	UpdateGlowPulse(false);
}

void ARogueOrbitingBlade::UpdateGlowPulse(bool bHighLoad)
{
	if (GlowMaterialInstance == nullptr || !bHasGlowPulseParameter)
	{
		return;
	}

	float PulseValue = VisualConfig.GlowPulseHighLoadValue;
	if (!bHighLoad)
	{
		const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(
			GetWorld()->GetTimeSeconds() * VisualConfig.PulseSpeed
			+ static_cast<float>(BladeIndex) * VisualConfig.PulsePhaseOffsetPerBlade);
		PulseValue = FMath::Lerp(VisualConfig.GlowPulseMin, VisualConfig.GlowPulseMax, PulseAlpha);
	}

	GlowMaterialInstance->SetScalarParameterValue(VisualConfig.GlowPulseParameterName, PulseValue);
}

void ARogueOrbitingBlade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPoolAvailable)
	{
		return;
	}

	UpdateCooldowns(DeltaSeconds);

	const ARogueCharacter* OwnerCharacter = Cast<ARogueCharacter>(OwnerActor.Get());
	if (!OwnerActor.IsValid() || (OwnerCharacter != nullptr && !OwnerCharacter->IsAlive()))
	{
		DeactivateToPool();
		return;
	}

	// ConfigureBlade 推送位置后读取当前缓存的重叠状态。
	// 角色与刀片 Tick 没有显式前置关系，此处不保证已经收到同帧的新位置。
	ApplyContactDamage();

	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	const bool bCullCombatEffects = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	if (bCullCombatEffects)
	{
		// 高负载：发光固定在低值，跳过 sin 与参数插值（原来是把外层网格缩放冻结）
		if (!bVisualsSimplified)
		{
			bVisualsSimplified = true;
			UpdateGlowPulse(true);
		}
		return;
	}

	bVisualsSimplified = false;
	UpdateGlowPulse(false);
}

void ARogueOrbitingBlade::ActivatePooledBlade(AActor* InOwnerActor)
{
	ApplySizeMultiplier(1.0f);
	OwnerActor = InOwnerActor;
	SetOwner(InOwnerActor);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);
	bPoolAvailable = false;
	bVisualsSimplified = false;
	HitCooldowns.Reset();

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->ClearMoveIgnoreActors();
		if (InOwnerActor != nullptr)
		{
			Collision->IgnoreActorWhenMoving(InOwnerActor, true);
		}
	}
}

void ARogueOrbitingBlade::DeactivateToPool()
{
	bPoolAvailable = true;
	bVisualsSimplified = false;
	HitCooldowns.Reset();
	OwnerActor.Reset();
	SetOwner(nullptr);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Collision->ClearMoveIgnoreActors();
	}
	ApplySizeMultiplier(1.0f);
}

void ARogueOrbitingBlade::ConfigureBlade(AActor* InOwnerActor, int32 InBladeIndex, int32 InBladeCount, float InOrbitRadius, float InRotationSpeed, float InDamage, float InSharedOrbitAngle, float InSizeMultiplier)
{
	OwnerActor = InOwnerActor;
	BladeIndex = InBladeIndex;
	BladeCount = FMath::Max(1, InBladeCount);
	OrbitRadius = InOrbitRadius;
	RotationSpeed = InRotationSpeed;
	Damage = InDamage;
	SharedOrbitAngle = InSharedOrbitAngle;
	ApplySizeMultiplier(InSizeMultiplier);

	if (OwnerActor.IsValid())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor.Get(), true);
	}

	UpdateTransform();
}


void ARogueOrbitingBlade::ApplySizeMultiplier(float InSizeMultiplier)
{
	const float NewMultiplier = FMath::IsFinite(InSizeMultiplier) ? FMath::Max(1.0f, InSizeMultiplier) : 1.0f;
	if (FMath::IsNearlyEqual(SizeMultiplier, NewMultiplier)) { return; }
	SizeMultiplier = NewMultiplier;
	Mesh->SetRelativeScale3D(BaseMeshScale * SizeMultiplier);
	// 显式扩大实际查询球；只放大网格不会改变持续接触伤害覆盖面。
	// 尺寸改变时更新重叠列表，下一次伤害 Tick 即可读到新进入范围的敌人。
	Collision->SetSphereRadius(BaseCollisionRadius * SizeMultiplier, !bPoolAvailable);
}

void ARogueOrbitingBlade::ApplyContactDamage()
{
	if (Collision == nullptr || Damage <= 0.0f)
	{
		return;
	}

	// 读取组件缓存的重叠对象，避免在此额外发起球形范围查询。
	// 移动/启停碰撞仍会维护重叠并可能发生物理查询；结果受更新与 Tick 时序影响。
	TArray<AActor*> OverlappingActors;
	Collision->GetOverlappingActors(OverlappingActors, ARogueEnemy::StaticClass());

	for (AActor* OtherActor : OverlappingActors)
	{
		ARogueEnemy* Enemy = Cast<ARogueEnemy>(OtherActor);
		if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsAvailableInPool())
		{
			continue;
		}

		// 冷却表是"每把刀片 × 每个敌人"各一份，所以多把刀片贴同一个敌人时
		// 每把都独立提交伤害请求；实际总伤害还受接触时长、轨道与权威结算条件影响。
		if (HitCooldowns.Contains(Enemy))
		{
			continue;
		}

		HitCooldowns.Add(Enemy, VisualConfig.HitCooldown);
		ARogueImpactEffect::SpawnImpactEffect(GetWorld(), Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f), GetActorRotation(), ERogueImpactVisualStyle::Spark, VisualConfig.ImpactEffectScale, VisualConfig.ImpactEffectLifetime, this);
		// 环绕刀片是近身伤害，Tag 用 Damage.Source.Melee
		RogueAbilitySystem::ApplyDamageToTarget(OwnerActor.Get(), Enemy, Damage, RogueGameplayTags::Damage_Source_Melee);
	}
}

void ARogueOrbitingBlade::UpdateCooldowns(float DeltaSeconds)
{
	TArray<TObjectPtr<AActor>> KeysToRemove;
	for (TPair<TObjectPtr<AActor>, float>& Pair : HitCooldowns)
	{
		Pair.Value -= DeltaSeconds;
		if (Pair.Value <= 0.0f || !IsValid(Pair.Key))
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (AActor* Actor : KeysToRemove)
	{
		HitCooldowns.Remove(Actor);
	}
}

void ARogueOrbitingBlade::UpdateTransform()
{
	if (!OwnerActor.IsValid())
	{
		return;
	}

	const float SlotOffset = BladeCount > 0 ? (360.0f / static_cast<float>(BladeCount)) * static_cast<float>(BladeIndex) : 0.0f;
	const float TotalAngle = SharedOrbitAngle + SlotOffset;
	const FVector OrbitOffset = FVector(FMath::Cos(FMath::DegreesToRadians(TotalAngle)), FMath::Sin(FMath::DegreesToRadians(TotalAngle)), 0.0f) * OrbitRadius;
	const FVector OwnerLocation = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);

	SetActorLocation(OwnerLocation + OrbitOffset);
	SetActorRotation((OwnerLocation - GetActorLocation()).Rotation());
}
