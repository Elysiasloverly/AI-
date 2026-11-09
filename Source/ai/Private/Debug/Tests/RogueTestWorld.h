#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "Components/BoxComponent.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"

/** 测试公共环境，不代替场景配置：不刷怪、不授予武器、不写本地存档。 */
namespace RogueTestWorld
{
inline bool Begin(FTestWorldWrapper &Fixture, UWorld *&World)
{
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		return false;
	}
	World = Fixture.GetTestWorld();
	World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
	if (!Fixture.BeginPlayInTestWorld())
	{
		return false;
	}
	World->GetAuthGameMode()->SetActorTickEnabled(false);
	return true;
}

// 显式空进度，避免开发者本地存档意外影响测试的开局属性。
inline ARoguePlayerController *SpawnFreshController(UWorld *World)
{
	auto *Controller = World->SpawnActor<ARoguePlayerController>();
	Controller->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun({});
	return Controller;
}

inline UBoxComponent *StaticBox(UWorld *World, FVector Position, FVector Extent,
								FRotator Rotation = FRotator::ZeroRotator,
								ECollisionEnabled::Type Collision = ECollisionEnabled::QueryAndPhysics)
{
	AActor *Actor = World->SpawnActor<AActor>();
	auto *Box = NewObject<UBoxComponent>(Actor);
	Actor->SetRootComponent(Box);
	Box->SetBoxExtent(Extent);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCollisionEnabled(Collision);
	Box->RegisterComponent();
	Actor->SetActorLocationAndRotation(Position, Rotation);
	return Box;
}
} // namespace RogueTestWorld
#endif
