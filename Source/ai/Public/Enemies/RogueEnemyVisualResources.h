#pragma once

#include "CoreMinimal.h"
#include "Enemies/RogueEnemyArchetypes.h"

class UMaterialInterface;
class UStaticMesh;

struct AI_API FRogueEnemyVisualResourceLibrary
{
	UStaticMesh* HunterMesh = nullptr;
	UStaticMesh* ChargerMesh = nullptr;
	UStaticMesh* TankMesh = nullptr;
	UStaticMesh* OrbiterMesh = nullptr;
	UStaticMesh* ShooterMesh = nullptr;
	UStaticMesh* SkirmisherMesh = nullptr;
	UStaticMesh* ArtilleryMesh = nullptr;
	UStaticMesh* SpitterMesh = nullptr;

	UMaterialInterface* HunterMaterial = nullptr;
	UMaterialInterface* ChargerMaterial = nullptr;
	UMaterialInterface* TankMaterial = nullptr;
	UMaterialInterface* OrbiterMaterial = nullptr;
	UMaterialInterface* ShooterMaterial = nullptr;
	UMaterialInterface* SkirmisherMaterial = nullptr;
	UMaterialInterface* ArtilleryMaterial = nullptr;
	UMaterialInterface* SpitterMaterial = nullptr;
	UMaterialInterface* BossMaterial = nullptr;
};

struct AI_API FRogueEnemyVisualResourceSelection
{
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
};

namespace RogueEnemyVisualResources
{
	AI_API void LoadDefaultResources(FRogueEnemyVisualResourceLibrary& OutLibrary);
	AI_API FRogueEnemyVisualResourceSelection ResolveSelection(const FRogueEnemyVisualResourceLibrary& Library, ERogueEnemyVisualKey VisualKey);
}
