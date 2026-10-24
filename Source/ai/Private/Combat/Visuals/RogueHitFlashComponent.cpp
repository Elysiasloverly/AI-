#include "Combat/Visuals/RogueHitFlashComponent.h"

#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

URogueHitFlashComponent::URogueHitFlashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URogueHitFlashComponent::PlayHitFlash()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || Owner->GetNetMode() == NM_DedicatedServer) { return false; }
	if (!bEnableHitFlash || !FlashMaterial)
	{
		ClearHitFlash();
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (Now < NextAllowedFlashTime) { return false; }
	if (!FlashMID || FlashMID->Parent != FlashMaterial)
	{
		ClearHitFlash();
		FlashMID = UMaterialInstanceDynamic::Create(FlashMaterial, this);
	}
	if (!FlashMID) { return false; }

	const float Hold = FMath::IsFinite(HoldSeconds) ? FMath::Clamp(HoldSeconds, 0.0f, 1.0f) : 0.02f;
	const float Fade = FMath::IsFinite(FadeOutSeconds) ? FMath::Clamp(FadeOutSeconds, 0.001f, 1.0f) : 0.06f;
	const float Interval = FMath::IsFinite(MinFlashInterval) ? FMath::Clamp(MinFlashInterval, 0.0f, 2.0f) : 0.12f;
	FlashMID->SetScalarParameterValue(TEXT("FlashStartTime"), float(Now));
	FlashMID->SetScalarParameterValue(TEXT("FlashHold"), Hold);
	FlashMID->SetScalarParameterValue(TEXT("FlashFade"), Fade);
	FlashMID->SetScalarParameterValue(TEXT("FlashOpacity"), FMath::Clamp(FlashOpacity, 0.0f, 1.0f));
	FlashMID->SetScalarParameterValue(TEXT("FlashIntensity"), FMath::Clamp(FlashIntensity, 0.0f, 10.0f));
	FlashMID->SetVectorParameterValue(TEXT("FlashColor"), FlashColor);

	// Static/skeletal body meshes and accessories share one cached MID. Widget components are excluded.
	TInlineComponentArray<UMeshComponent*> Meshes(Owner);
	for (UMeshComponent* Mesh : Meshes)
	{
		UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Mesh);
		const USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(Mesh);
		if ((!Static || !Static->GetStaticMesh()) && (!Skeletal || !Skeletal->GetSkeletalMeshAsset())) { continue; }
		if (Mesh->ComponentHasTag(TEXT("NoHitFlash"))) { continue; }
		FRogueHitFlashMeshSnapshot* Snapshot = FlashedMeshes.FindByPredicate(
			[Mesh](const FRogueHitFlashMeshSnapshot& Entry) { return Entry.Mesh == Mesh; });
		if (!Snapshot)
		{
			Snapshot = &FlashedMeshes.AddDefaulted_GetRef();
			Snapshot->Mesh = Mesh;
			Snapshot->PreviousOverlay = Mesh->GetOverlayMaterial();
			Snapshot->PreviousDrawDistance = Mesh->OverlayMaterialMaxDrawDistance;
			Snapshot->bPreviousForceDisableNanite = Static && Static->bForceDisableNanite;
		}
		else if (Mesh->GetOverlayMaterial() != FlashMID)
		{
			// Another effect changed the slot since the last pulse; retain that newer baseline.
			Snapshot->PreviousOverlay = Mesh->GetOverlayMaterial();
			Snapshot->PreviousDrawDistance = Mesh->OverlayMaterialMaxDrawDistance;
		}
		Mesh->SetOverlayMaterial(FlashMID);
		Mesh->SetOverlayMaterialMaxDrawDistance(0.0f);
		// UE 5.5 Nanite does not render this overlay path; use its existing fallback only during the pulse.
		if (Static && !Static->bForceDisableNanite) { Static->SetForceDisableNanite(true); }
	}
	if (FlashedMeshes.IsEmpty()) { return false; }
	NextAllowedFlashTime = Now + Interval;
	World->GetTimerManager().SetTimer(RestoreTimer, this, &URogueHitFlashComponent::RestoreOverlays, Hold + Fade, false);
	return true;
}

void URogueHitFlashComponent::RestoreOverlays()
{
	for (const FRogueHitFlashMeshSnapshot& Snapshot : FlashedMeshes)
	{
		UMeshComponent* Mesh = Snapshot.Mesh.Get();
		if (Mesh && Mesh->GetOverlayMaterial() == FlashMID)
		{
			Mesh->SetOverlayMaterial(Snapshot.PreviousOverlay);
			Mesh->SetOverlayMaterialMaxDrawDistance(Snapshot.PreviousDrawDistance);
		}
		if (UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Mesh))
		{
			if (Static->bForceDisableNanite != Snapshot.bPreviousForceDisableNanite)
			{
				Static->SetForceDisableNanite(Snapshot.bPreviousForceDisableNanite);
			}
		}
	}
	FlashedMeshes.Reset();
}

void URogueHitFlashComponent::ClearHitFlash()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(RestoreTimer); }
	RestoreOverlays();
	NextAllowedFlashTime = -1.0;
}

void URogueHitFlashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearHitFlash();
	Super::EndPlay(EndPlayReason);
}
