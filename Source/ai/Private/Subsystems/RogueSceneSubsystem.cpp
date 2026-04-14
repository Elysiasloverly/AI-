// 场景优化子系统 —— 实现
#include "Subsystems/RogueSceneSubsystem.h"

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

void URogueSceneSubsystem::OptimizeLevelEnvironment(const FRogueSceneOptimizationSettings& Settings)
{
	if (!Settings.bOptimizeSceneForPerformance || GetWorld() == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();

	ADirectionalLight* PrimaryDirectionalLight = nullptr;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
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
			LightComponent->SetIntensity(Settings.PrimaryDirectionalLightIntensity);
			LightComponent->SetLightColor(FLinearColor(1.00f, 0.95f, 0.88f));
			LightComponent->SetCastShadows(false);
			LightComponent->SetVisibility(true);
		}

		PrimaryDirectionalLight->SetActorRotation(Settings.OptimizedSunRotation);
	}

	ASkyLight* PrimarySkyLight = nullptr;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
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
			LightComponent->SetIntensity(Settings.PrimarySkyLightIntensity);
			LightComponent->SetLightColor(FLinearColor(0.34f, 0.48f, 0.78f));
			LightComponent->SetCastShadows(false);
			LightComponent->SetVisibility(true);
		}
	}

	for (TActorIterator<ALight> It(World); It; ++It)
	{
		ALight* LightActor = *It;
		if (!IsValid(LightActor) || LightActor == PrimaryDirectionalLight)
		{
			continue;
		}

		DisableActorRendering(LightActor);
	}

	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		(*It)->bEnabled = false;
		(*It)->BlendWeight = 0.0f;
		DisableActorRendering(*It);
	}

	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		DisableActorRendering(*It);
	}

	for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
	{
		DisableActorRendering(*It);
	}

	for (TActorIterator<AVolumetricCloud> It(World); It; ++It)
	{
		DisableActorRendering(*It);
	}
}

void URogueSceneSubsystem::DisableActorRendering(AActor* Actor) const
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