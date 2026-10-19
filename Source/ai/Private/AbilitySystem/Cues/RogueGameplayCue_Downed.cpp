#include "AbilitySystem/Cues/RogueGameplayCue_Downed.h"

#include "Components/StaticMeshComponent.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueGameplayCue_Downed::ARogueGameplayCue_Downed()
{
	// 收到 Removed 后由 Cue 框架结束/回收实例（可进入 Cue 池，不保证立即 Destroy）。
	// 自动附着使环跟随目标；生命周期逻辑仍需显式移除 Infinite 濒死 GE。
	bAutoDestroyOnRemove = true;
	bAutoAttachToOwner = true;

	// 同一 Cue 实例只处理一次 OnActive/WhileActive，避免刷新时重复初始化动画。
	// 这些标志控制回调重入，不是通用的跨源/跨实例重复效果去重规则。
	bAllowMultipleOnActiveEvents = false;
	bAllowMultipleWhileActiveEvents = false;

	// 声明本 cue 处理哪个 Tag。注册走的是 AddCues 的显式映射（见 RogueGameplayCue_Impact.cpp），
	// 设上它是为了让类自描述、也便于在编辑器里按 Tag 检索。
	GameplayCueTag = RogueGameplayTags::GameplayCue_State_Downed;

	// Tick 只在 OnActive 之后才需要（自转 + 呼吸），所以默认关掉。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DownedRing"));
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetGenerateOverlapEvents(false);
	RingMesh->SetCastShadow(false);
	// 不可见是**初始状态**：可见性由 OnActive / OnRemove 控制，见 BeginPlay 的注释。
	RingMesh->SetVisibility(false);

	// 基类未必已经建好根组件，所以这里两种情况都处理。
	if (RootComponent == nullptr)
	{
		RootComponent = RingMesh;
	}
	else
	{
		RingMesh->SetupAttachment(RootComponent);
	}

	// 独立濒死圆环使用以下默认资源，仅设置本 Cue 的 RingMesh。
	// 玩家主体模型和材质由 BP_RogueCharacter 配置，不在此处覆盖。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TorusMeshFinder(
		TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"));
	if (TorusMeshFinder.Succeeded())
	{
		RingMesh->SetStaticMesh(TorusMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HologramMaterialFinder(
		TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_12.MI_Hologram_12"));
	if (HologramMaterialFinder.Succeeded())
	{
		RingMesh->SetMaterial(0, HologramMaterialFinder.Object);
	}
}

void ARogueGameplayCue_Downed::BeginPlay()
{
	Super::BeginPlay();

	// ⚠️ 这里**不打开**可见性。
	// Actor cue 的 BeginPlay 可能早于 OnActive（实例复用/预生成时尤其如此），
	// 在 BeginPlay 里显示会导致"人还没倒地，脚下就有一个环"。
	SetActorTickEnabled(false);
}

bool ARogueGameplayCue_Downed::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 资源或目标无效时不显示、不启动 Tick。
	// 返回 false 不会自动结束/销毁 Cue Actor，生命周期仍由 Cue 框架管理。
	if (!IsValid(MyTarget) || RingMesh == nullptr || RingMesh->GetStaticMesh() == nullptr)
	{
		return false;
	}

	// 兜底附着：bAutoAttachToOwner 正常已经挂好了，但环必须跟着人走，
	// 万一没挂上就会留在原地 —— 这个失败模式（大家看到一个环孤零零飘着）很难归因，所以这里补一次。
	if (GetAttachParentActor() != MyTarget)
	{
		AttachToActor(MyTarget, FAttachmentTransformRules::KeepRelativeTransform);
	}

	ElapsedSeconds = 0.0f;

	// 摆位：目标原点通常在胶囊体中心，所以往下挪到脚边（RingHeightOffset 是负值）。
	// 用**相对**位置而不是世界坐标 —— 这样人走动时环自动跟随。
	SetActorRelativeLocation(FVector(0.0f, 0.0f, RingHeightOffset));
	SetActorRelativeRotation(FRotator::ZeroRotator);
	SetActorRelativeScale3D(FVector(RingBaseScale, RingBaseScale, RingBaseScale));

	RingMesh->SetVisibility(true);
	SetActorTickEnabled(true);

	UE_LOG(LogTemp, Log, TEXT("[Rogue] 倒地表现已激活（Actor 型 GameplayCue）：目标=%s"), *GetNameSafe(MyTarget));
	return true;
}

bool ARogueGameplayCue_Downed::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 先隐藏并停 Tick；bAutoDestroyOnRemove 让框架结束/回收实例。
	// 即使进入复用池而未立即 Destroy，也不应保留可见的环。
	if (RingMesh != nullptr)
	{
		RingMesh->SetVisibility(false);
	}
	SetActorTickEnabled(false);

	UE_LOG(LogTemp, Log, TEXT("[Rogue] 倒地表现已结束（Actor 型 GameplayCue）：目标=%s"), *GetNameSafe(MyTarget));
	return true;
}

void ARogueGameplayCue_Downed::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedSeconds += DeltaSeconds;

	// 自转：绕自身 Z 轴慢慢转
	AddActorLocalRotation(FRotator(0.0f, RotationSpeedDegreesPerSecond * DeltaSeconds, 0.0f));

	// 呼吸：在基础缩放上下做正弦脉动。
	// 用缩放而不是改材质参数，是因为材质是复用的公共资产，
	// 改它的动态实例参数需要知道具体的参数名 —— 那是个只在运行期才会暴露的耦合。
	const float PulseScale = RingBaseScale * (1.0f + FMath::Sin(ElapsedSeconds * PulseSpeed) * PulseAmplitude);
	SetActorRelativeScale3D(FVector(PulseScale, PulseScale, PulseScale));
}
