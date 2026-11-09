#include "RogueProjectileVisualBuilder.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* SphereMeshPath = TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere");
	const TCHAR* CylinderMeshPath = TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder");
	const TCHAR* BodyMaterialPath = TEXT("/Game/GameAssets/StarterContent/Materials/M_Metal_Copper.M_Metal_Copper");
	const TCHAR* GlowMaterialPath = TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D");
	const TCHAR* BlueGlowMaterialPath = TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst");
	const TCHAR* Hologram10MaterialPath = TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_10.MI_Hologram_10");
	const TCHAR* Hologram12MaterialPath = TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_12.MI_Hologram_12");
	const TCHAR* WedgeMeshPath = TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Wedge_A.Shape_Wedge_A");
	const TCHAR* BladeMaterialPath = TEXT("/Game/GameAssets/StarterContent/Materials/M_Tech_Hex_Tile.M_Tech_Hex_Tile");
	const TCHAR* BladeOverlayMaterialPath = TEXT("/Game/GameAssets/StarterContent/Materials/M_Tech_Hex_Tile_Pulse.M_Tech_Hex_Tile_Pulse");
	const TCHAR* TorusMeshPath = TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus");
	const TCHAR* Hologram8MaterialPath = TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_8.MI_Hologram_8");

	USCS_Node* AddComponentNode(UBlueprint* Blueprint, UClass* ComponentClass, FName Name)
	{
		if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
		{
			return nullptr;
		}

		// 不覆盖用户已经做过的组件；要重跑请先在蓝图里删掉同名组件。
		if (Blueprint->SimpleConstructionScript->FindSCSNode(Name) != nullptr)
		{
			return nullptr;
		}

		USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, Name);
		if (Node == nullptr)
		{
			return nullptr;
		}

		Blueprint->SimpleConstructionScript->AddNode(Node);
		return Node;
	}

	UStaticMeshComponent* ConfigureMesh(USCS_Node* Node, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Scale, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator, int32 SortPriority = 0)
	{
		UStaticMeshComponent* Component = Node != nullptr ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;
		if (Component == nullptr)
		{
			return nullptr;
		}

		Component->SetStaticMesh(Mesh);
		if (Material != nullptr)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCastShadow(false);
		Component->SetTranslucentSortPriority(SortPriority);
		Component->SetRelativeLocation(Location);
		Component->SetRelativeRotation(Rotation);
		Component->SetRelativeScale3D(Scale);
		return Component;
	}
}

bool URogueProjectileVisualBuilder::BuildTemplate(UBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
	{
		return false;
	}

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, SphereMeshPath);
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderMeshPath);
	UMaterialInterface* BodyMaterial = LoadObject<UMaterialInterface>(nullptr, BodyMaterialPath);
	UMaterialInterface* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, GlowMaterialPath);

	Blueprint->Modify();

	ConfigureMesh(AddComponentNode(Blueprint, UStaticMeshComponent::StaticClass(), TEXT("BodyMesh")),
		Sphere, BodyMaterial, FVector(0.58f));

	ConfigureMesh(AddComponentNode(Blueprint, UStaticMeshComponent::StaticClass(), TEXT("CoreMesh")),
		Sphere, GlowMaterial, FVector(0.32f), FVector::ZeroVector, FRotator::ZeroRotator, 4);

	ConfigureMesh(AddComponentNode(Blueprint, UStaticMeshComponent::StaticClass(), TEXT("GlowMesh")),
		Sphere, GlowMaterial, FVector(1.12f), FVector::ZeroVector, FRotator::ZeroRotator, 2);

	ConfigureMesh(AddComponentNode(Blueprint, UStaticMeshComponent::StaticClass(), TEXT("TrailMesh")),
		Cylinder, GlowMaterial, FVector(0.10f, 0.10f, 0.28f), FVector(-26.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 90.0f), 1);

	if (USCS_Node* LightNode = AddComponentNode(Blueprint, UPointLightComponent::StaticClass(), TEXT("GlowLight")))
	{
		if (UPointLightComponent* Light = Cast<UPointLightComponent>(LightNode->ComponentTemplate))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(4200.0f);
			Light->SetLightColor(FLinearColor(1.0f, 0.10f, 0.08f));
			Light->SetAttenuationRadius(260.0f);
			Light->SetSourceRadius(10.0f);
			Light->SetSoftSourceRadius(16.0f);
			Light->SetUseInverseSquaredFalloff(false);
			Light->SetCastShadows(false);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->GetOutermost()->MarkPackageDirty();
	return true;
}

bool URogueProjectileVisualBuilder::BuildPlayerProjectileVisuals(UBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
	{
		return false;
	}

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, SphereMeshPath);
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderMeshPath);
	UMaterialInterface* CoreMaterial = LoadObject<UMaterialInterface>(nullptr, BlueGlowMaterialPath);
	UMaterialInterface* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, Hologram10MaterialPath);
	UMaterialInterface* RingMaterial = LoadObject<UMaterialInterface>(nullptr, Hologram12MaterialPath);
	UMaterialInterface* TrailMaterial = LoadObject<UMaterialInterface>(nullptr, BlueGlowMaterialPath);

	Blueprint->Modify();

	auto AddTaggedMesh = [&](FName Name, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Scale,
		const FVector& Location, const FRotator& Rotation, int32 SortPriority, FName Tag)
	{
		UStaticMeshComponent* Component = ConfigureMesh(
			AddComponentNode(Blueprint, UStaticMeshComponent::StaticClass(), Name),
			Mesh, Material, Scale, Location, Rotation, SortPriority);
		if (Component != nullptr)
		{
			Component->ComponentTags.Add(Tag);
		}
		return Component;
	};

	AddTaggedMesh(TEXT("Mesh"), Sphere, CoreMaterial, FVector(0.14f),
		FVector::ZeroVector, FRotator::ZeroRotator, 4, TEXT("RogueVisual.Mesh"));
	AddTaggedMesh(TEXT("GlowMesh"), Sphere, GlowMaterial, FVector(0.34f),
		FVector::ZeroVector, FRotator::ZeroRotator, 2, TEXT("RogueVisual.Glow"));
	AddTaggedMesh(TEXT("RingMesh"), Sphere, RingMaterial, FVector(0.48f),
		FVector::ZeroVector, FRotator::ZeroRotator, 3, TEXT("RogueVisual.Ring"));
	AddTaggedMesh(TEXT("TrailMesh"), Cylinder, TrailMaterial, FVector(0.08f, 0.08f, 0.22f),
		FVector(-18.0f, 0.0f, 0.0f), FRotator(90.0f, 0.0f, 0.0f), 1, TEXT("RogueVisual.Trail"));

	if (USCS_Node* LightNode = AddComponentNode(Blueprint, UPointLightComponent::StaticClass(), TEXT("GlowLight")))
	{
		if (UPointLightComponent* Light = Cast<UPointLightComponent>(LightNode->ComponentTemplate))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(700.0f);
			Light->SetLightColor(FLinearColor(0.20f, 0.55f, 1.0f));
			Light->SetAttenuationRadius(78.0f);
			Light->SetSourceRadius(6.0f);
			Light->SetSoftSourceRadius(10.0f);
			Light->SetUseInverseSquaredFalloff(false);
			Light->SetCastShadows(false);
			Light->SetVisibility(false);
			Light->ComponentTags.Add(TEXT("RogueVisual.Light"));
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->GetOutermost()->MarkPackageDirty();
	return true;
}

bool URogueProjectileVisualBuilder::BuildOrbitingBladeVisuals(UBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
	{
		return false;
	}

	UStaticMesh* Wedge = LoadObject<UStaticMesh>(nullptr, WedgeMeshPath);
	UMaterialInterface* BladeMaterial = LoadObject<UMaterialInterface>(nullptr, BladeMaterialPath);
	UMaterialInterface* OverlayMaterial = LoadObject<UMaterialInterface>(nullptr, BladeOverlayMaterialPath);

	Blueprint->Modify();
	UStaticMeshComponent* Mesh = ConfigureMesh(
		AddComponentNode(Blueprint, UStaticMeshComponent::StaticClass(), TEXT("Mesh")),
		Wedge, BladeMaterial, FVector(0.75f, 0.14f, 1.1f), FVector::ZeroVector, FRotator(0.0f, 45.0f, 90.0f), 2);
	if (Mesh != nullptr)
	{
		Mesh->ComponentTags.Add(TEXT("RogueVisual.Mesh"));
		Mesh->SetOverlayMaterial(OverlayMaterial);
		Mesh->SetOverlayMaterialMaxDrawDistance(0.0f);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->GetOutermost()->MarkPackageDirty();
	return true;
}

bool URogueProjectileVisualBuilder::BuildMortarIndicator(UBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
	{
		return false;
	}

	UStaticMesh* Torus = LoadObject<UStaticMesh>(nullptr, TorusMeshPath);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Hologram8MaterialPath);

	Blueprint->Modify();
	UStaticMeshComponent* Indicator = ConfigureMesh(
		AddComponentNode(Blueprint, UStaticMeshComponent::StaticClass(), TEXT("LandingIndicatorMesh")),
		Torus, Material, FVector(1.0f, 1.0f, 0.10f));
	if (Indicator != nullptr)
	{
		Indicator->ComponentTags.Add(TEXT("RogueVisual.LandingIndicator"));
		Indicator->SetUsingAbsoluteLocation(true);
		Indicator->SetUsingAbsoluteRotation(true);
		Indicator->SetUsingAbsoluteScale(true);
		Indicator->SetHiddenInGame(true);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->GetOutermost()->MarkPackageDirty();
	return true;
}

bool URogueProjectileVisualBuilder::RemoveComponent(UBlueprint* Blueprint, FName ComponentName)
{
	if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
	{
		return false;
	}

	USCS_Node* Node = Blueprint->SimpleConstructionScript->FindSCSNode(ComponentName);
	if (Node == nullptr)
	{
		return false;
	}

	Blueprint->Modify();
	Blueprint->SimpleConstructionScript->RemoveNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->GetOutermost()->MarkPackageDirty();
	return true;
}

FString URogueProjectileVisualBuilder::GetParentClassPath(UBlueprint* Blueprint)
{
	return Blueprint != nullptr && Blueprint->ParentClass != nullptr
		? Blueprint->ParentClass->GetPathName()
		: TEXT("<none>");
}

FString URogueProjectileVisualBuilder::DescribeMeshComponents(UBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
	{
		return TEXT("<no construction script>");
	}

	FString Result;
	for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		const UStaticMeshComponent* MeshComponent = Node != nullptr
			? Cast<UStaticMeshComponent>(Node->ComponentTemplate)
			: nullptr;
		if (MeshComponent != nullptr)
		{
			Result += FString::Printf(TEXT("%s -> %s\n"),
				*Node->GetVariableName().ToString(),
				MeshComponent->GetStaticMesh() != nullptr ? *MeshComponent->GetStaticMesh()->GetPathName() : TEXT("<none>"));
		}
	}
	return Result;
}

FString URogueProjectileVisualBuilder::DescribeComponents(UBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
	{
		return TEXT("<no construction script>");
	}

	FString Result;
	for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node != nullptr)
		{
			Result += FString::Printf(TEXT("%s:%s\n"),
				*GetNameSafe(Node->ComponentClass), *Node->GetVariableName().ToString());
		}
	}
	return Result;
}
