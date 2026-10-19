#include "World/RogueArena.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "UObject/ConstructorHelpers.h"

ARogueArena::ARogueArena()
{
	PrimaryActorTick.bCanEverTick = false;

	// 场地随关卡加载并参与复制；GameMode 复用已放置的 BP_RogueArena。
	// Actor 复制不表示当前构造函数会创建地板、墙壁或灯光。
	bReplicates = true;
	// 范围载体设为始终相关，避免距离相关性裁剪；原生实现不提供场景几何。
	bAlwaysRelevant = true;
	// 保留范围载体，几何与灯光由当前关卡/蓝图提供。
}

void ARogueArena::BeginPlay()
{
	Super::BeginPlay();
	// 当前原生 BeginPlay 不额外生成环境粒子。
}
