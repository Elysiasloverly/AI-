#include "AbilitySystem/RogueGameplayCue_Downed.h"

#include "Components/StaticMeshComponent.h"
#include "Core/RogueGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueGameplayCue_Downed::ARogueGameplayCue_Downed()
{
	// 生命周期完全交给 GE：
	//   · GE 移除 → 自动销毁本 Actor，不需要任何一处代码记得来 Destroy；
	//   · 挂到目标身上 → 人走到哪环跟到哪。
	bAutoDestroyOnRemove = true;
	bAutoAttachToOwner = true;

	// 这两个保持 false：一次倒地只该有一个环。
	// 若允许重复 OnActive，GE 被刷新时（例如倒地在即又挨了一下致命伤）会叠出第二个环。
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

	// ---- 视觉资源：全部复用项目里**已有**的资产 ----
	// 与 ARogueCharacterVisualComponent 同一套做法（ConstructorHelpers 按路径加载），
	// 因此本类**不需要新建任何 .uasset**：
	//   · 网格 = StarterContent 的 Torus（角色的 HaloMesh 用的就是它）
	//   · 材质 = 角色外壳同款全息材质（HologramShader 包里的 MI_Hologram_12）
	// 加载失败时不会崩，只是环不可见 —— 与项目里其他视觉组件的处理一致。
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
	// 目标无效、或视觉资源没加载出来（缺网格）时直接返回 false：
	// 让引擎知道这条 cue 没真的生效，而不是留下一个空 Actor 每帧 Tick。
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
	// 只负责"看不见"；**销毁交给 bAutoDestroyOnRemove**。
	// 分开做的理由：即使本 Actor 因为复用/延迟销毁没有立刻消失，也不会留下一个可见的环。
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
