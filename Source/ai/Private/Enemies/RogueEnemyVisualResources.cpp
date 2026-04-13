#include "Enemies/RogueEnemyVisualResources.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

void RogueEnemyVisualResources::LoadDefaultResources(FRogueEnemyVisualResourceLibrary& OutLibrary)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HunterMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (HunterMesh.Succeeded())
	{
		OutLibrary.HunterMesh = HunterMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ChargerMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cone.Shape_Cone"));
	if (ChargerMesh.Succeeded())
	{
		OutLibrary.ChargerMesh = ChargerMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TankMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cube.Shape_Cube"));
	if (TankMesh.Succeeded())
	{
		OutLibrary.TankMesh = TankMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> OrbiterMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (OrbiterMesh.Succeeded())
	{
		OutLibrary.OrbiterMesh = OrbiterMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShooterMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Wedge_B.Shape_Wedge_B"));
	if (ShooterMesh.Succeeded())
	{
		OutLibrary.ShooterMesh = ShooterMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SkirmisherMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"));
	if (SkirmisherMesh.Succeeded())
	{
		OutLibrary.SkirmisherMesh = SkirmisherMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArtilleryMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (ArtilleryMesh.Succeeded())
	{
		OutLibrary.ArtilleryMesh = ArtilleryMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SpitterMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (SpitterMesh.Succeeded())
	{
		OutLibrary.SpitterMesh = SpitterMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HunterMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Shared/Materials/MI_Opaque_Dark_01.MI_Opaque_Dark_01"));
	if (HunterMaterial.Succeeded())
	{
		OutLibrary.HunterMaterial = HunterMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ChargerMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MI_Vefects_Grid_01.MI_Vefects_Grid_01"));
	if (ChargerMaterial.Succeeded())
	{
		OutLibrary.ChargerMaterial = ChargerMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TankMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MI_Vefects_Grid_02.MI_Vefects_Grid_02"));
	if (TankMaterial.Succeeded())
	{
		OutLibrary.TankMaterial = TankMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OrbiterMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_B.M_Glow_B"));
	if (OrbiterMaterial.Succeeded())
	{
		OutLibrary.OrbiterMaterial = OrbiterMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShooterMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_5.MI_Hologram_5"));
	if (ShooterMaterial.Succeeded())
	{
		OutLibrary.ShooterMaterial = ShooterMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkirmisherMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_A.M_Glow_A"));
	if (SkirmisherMaterial.Succeeded())
	{
		OutLibrary.SkirmisherMaterial = SkirmisherMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ArtilleryMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_21.MI_Hologram_21"));
	if (ArtilleryMaterial.Succeeded())
	{
		OutLibrary.ArtilleryMaterial = ArtilleryMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpitterMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MI_Vefects_Grid_02.MI_Vefects_Grid_02"));
	if (SpitterMaterial.Succeeded())
	{
		OutLibrary.SpitterMaterial = SpitterMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BossMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_03.MI_VFX_Lush_Galaxy_Shader_03"));
	if (BossMaterial.Succeeded())
	{
		OutLibrary.BossMaterial = BossMaterial.Object;
	}
}

FRogueEnemyVisualResourceSelection RogueEnemyVisualResources::ResolveSelection(const FRogueEnemyVisualResourceLibrary& Library, ERogueEnemyVisualKey VisualKey)
{
	FRogueEnemyVisualResourceSelection Selection;

	switch (VisualKey)
	{
	case ERogueEnemyVisualKey::Charger:
		Selection.Mesh = Library.ChargerMesh;
		Selection.Material = Library.ChargerMaterial;
		break;
	case ERogueEnemyVisualKey::Tank:
		Selection.Mesh = Library.TankMesh;
		Selection.Material = Library.TankMaterial;
		break;
	case ERogueEnemyVisualKey::Orbiter:
		Selection.Mesh = Library.OrbiterMesh;
		Selection.Material = Library.OrbiterMaterial;
		break;
	case ERogueEnemyVisualKey::Shooter:
		Selection.Mesh = Library.ShooterMesh;
		Selection.Material = Library.ShooterMaterial;
		break;
	case ERogueEnemyVisualKey::Skirmisher:
		Selection.Mesh = Library.SkirmisherMesh != nullptr ? Library.SkirmisherMesh : Library.OrbiterMesh;
		Selection.Material = Library.SkirmisherMaterial != nullptr ? Library.SkirmisherMaterial : Library.ChargerMaterial;
		break;
	case ERogueEnemyVisualKey::Artillery:
		Selection.Mesh = Library.ArtilleryMesh != nullptr ? Library.ArtilleryMesh : Library.HunterMesh;
		Selection.Material = Library.ArtilleryMaterial != nullptr ? Library.ArtilleryMaterial : Library.TankMaterial;
		break;
	case ERogueEnemyVisualKey::Spitter:
		Selection.Mesh = Library.SpitterMesh != nullptr ? Library.SpitterMesh : Library.OrbiterMesh;
		Selection.Material = Library.SpitterMaterial != nullptr ? Library.SpitterMaterial : Library.ShooterMaterial;
		break;
	case ERogueEnemyVisualKey::Hunter:
	default:
		Selection.Mesh = Library.HunterMesh;
		Selection.Material = Library.HunterMaterial;
		break;
	}

	return Selection;
}
