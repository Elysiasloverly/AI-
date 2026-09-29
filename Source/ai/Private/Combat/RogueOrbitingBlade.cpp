#include "Combat/RogueOrbitingBlade.h"

#include "Combat/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "Core/RogueGameplayTags.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

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

	// 发光改成 Mesh 的 Overlay Material：同一份网格再画一遍、叠一层半透明材质。
	// 对比原来的做法（再建一个略大的静态网格套在外面）：
	//   · 少一个组件、少一个 draw call；
	//   · 不会出现"外层不透明把本体整块挡住"的问题（本体永远在下面画）；
	//   · 大纲/辉光形状由材质自己决定（要靠 WPO 外扩就在材质里外扩）。
	// 引擎对 OverlayMaterial 的定义就是"Translucent material to blend on top of this mesh"，
	// 所以这个材质必须是半透明/叠加的。
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/GameAssets/StarterContent/Materials/M_Tech_Hex_Tile_Pulse.M_Tech_Hex_Tile_Pulse"));
	if (GlowMaterial.Succeeded())
	{
		VisualConfig.GlowMaterial = GlowMaterial.Object;
	}

	// OverlayMaterialMaxDrawDistance = 0 → 不额外裁剪，跟随本体的可见距离
	Mesh->SetOverlayMaterial(VisualConfig.GlowMaterial);
	Mesh->SetOverlayMaterialMaxDrawDistance(0.0f);

	// 注意：这里**不再**绑定 OnComponentBeginOverlap。
	// 原因：BeginOverlap 只在"进入的那一帧"触发一次，一旦刀片和敌人持续贴在一起就再也不会触发；
	// 而 HitCooldown 这个字段的语义是"贴身每 N 秒结算一次"。两者冲突会让实际伤害
	// 变成"每次进入才算一次"（一圈才一次），冷却表形同虚设。
	// 现在统一改成 Tick 里按重叠列表结算（见 ApplyContactDamage）。
}

void ARogueOrbitingBlade::BeginPlay()
{
	Super::BeginPlay();

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

	// 每次激活都重建 MID 会持续泄漏 UObject，所以只在没有实例时建一次；
	// 但 GlowMaterial 被换掉的情况要能重新建。
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

	// 启动时解析一次参数是否存在：参数名写错时只提示一次，不在每帧刷警告。
	// （SetScalarParameterValue 对不存在的参数会走引擎的 "Failed to find parameter" 警告路径）
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
		// 每局只提示一次（不是每把刀一次）。
		// 实测这条警告在一次会话里刷了 20+ 行 —— 每把环绕刀都会解析一次材质，
		// 而它反映的是**材质资源本身的缺失**（M_Tech_Hex_Tile_Pulse 里没有 GlowPulse 参数），
		// 与刀的数量无关，刷屏只会淹没真正有用的日志。
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

	if (!OwnerActor.IsValid())
	{
		DeactivateToPool();
		return;
	}

	// 位置由武器每帧通过 ConfigureBlade → UpdateTransform 推送（瞬移），
	// 瞬移会调用 UpdateOverlaps，所以这里的重叠列表就是"当前帧位置"的结果。
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
}

void ARogueOrbitingBlade::ConfigureBlade(AActor* InOwnerActor, int32 InBladeIndex, int32 InBladeCount, float InOrbitRadius, float InRotationSpeed, float InDamage, float InSharedOrbitAngle)
{
	OwnerActor = InOwnerActor;
	BladeIndex = InBladeIndex;
	BladeCount = FMath::Max(1, InBladeCount);
	OrbitRadius = InOrbitRadius;
	RotationSpeed = InRotationSpeed;
	Damage = InDamage;
	SharedOrbitAngle = InSharedOrbitAngle;

	if (OwnerActor.IsValid())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor.Get(), true);
	}

	UpdateTransform();
}

void ARogueOrbitingBlade::ApplyContactDamage()
{
	if (Collision == nullptr || Damage <= 0.0f)
	{
		return;
	}

	// 用"已缓存的重叠列表"而不是每帧做一次球体查询：
	// 刀片每帧都会被 SetActorLocation 瞬移，瞬移内部会调用 UpdateOverlaps，
	// 所以这份列表就是当前帧位置的准确结果，读它不产生任何物理查询开销。
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
		// 每把都独立结算，伤害随刀片数量线性叠加。
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
