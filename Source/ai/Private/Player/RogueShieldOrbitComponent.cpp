#include "Player/RogueShieldOrbitComponent.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Player/RogueCharacter.h"

URogueShieldOrbitComponent::URogueShieldOrbitComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	bAutoActivate = true;
}

void URogueShieldOrbitComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ARogueCharacter>(GetOwner());
	if (!Character.IsValid() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FName Name = MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(),
			FName(*FString::Printf(TEXT("ShieldOrbitMesh_%d"), Index)));
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner(), Name);
		Mesh->SetupAttachment(Character->GetRootComponent());
		// 世界坐标控制轨道，避免角色转身或胶囊缩放额外改变配置的半径和转速。
		Mesh->SetAbsolute(true, true, true);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->CanCharacterStepUpOn = ECB_No;
		Mesh->SetCastShadow(false);
		Mesh->SetVisibility(false);
		GetOwner()->AddInstanceComponent(Mesh);
		Mesh->RegisterComponent();
		OrbitMeshes.Add(Mesh);
	}
	RefreshShieldVisuals();
}

void URogueShieldOrbitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsActive() && bOrbitEnabled && FMath::IsFinite(DeltaTime) && FMath::IsFinite(OrbitSpeed))
	{
		OrbitAngleDegrees = FMath::Fmod(OrbitAngleDegrees + double(OrbitSpeed) * FMath::Max(0.0f, DeltaTime), 360.0);
	}
	RefreshShieldVisuals();
}

void URogueShieldOrbitComponent::RefreshShieldVisuals()
{
	const ARogueCharacter* OwnerCharacter = Character.Get();
	const URogueAttributeSet* Attributes = OwnerCharacter ? OwnerCharacter->GetRogueAttributeSet() : nullptr;
	// 直接读本玩家的 GAS 当前值，不依赖可能晚一帧的显示镜像或客户端属性委托。
	// 客户端拿到 PlayerState/属性复制前保持隐藏；无额外逐帧 RPC。
	const bool bHasShield = Attributes && FMath::IsFinite(Attributes->GetMaxArmor()) && Attributes->GetMaxArmor() > 0.0f
		&& FMath::IsFinite(Attributes->GetArmor()) && Attributes->GetArmor() > 0.0f;
	const bool bValidLayout = FMath::IsFinite(OrbitRadius) && FMath::IsFinite(HeightOffset)
		&& !MeshScale.ContainsNaN() && !MeshRotation.ContainsNaN();
	const bool bShow = IsActive() && bOrbitEnabled && bHasShield && IsValid(ShieldMesh) && bValidLayout
		&& OwnerCharacter->IsAlive() && !OwnerCharacter->IsHidden() && OrbitMeshes.Num() == 3;
	SetShieldVisible(bShow);
	if (!bShow) { return; }

	const FVector Center = OwnerCharacter->GetActorLocation() + FVector(0, 0, HeightOffset);
	for (int32 Index = 0; Index < OrbitMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Mesh = OrbitMeshes[Index];
		if (!IsValid(Mesh)) { continue; }
		if (Mesh->GetStaticMesh() != ShieldMesh) { Mesh->SetStaticMesh(ShieldMesh); }
		const double Angle = OrbitAngleDegrees + Index * 120.0;
		const double Radians = FMath::DegreesToRadians(Angle);
		const double Radius = FMath::Max(0.0f, OrbitRadius);
		const FVector Position = Center + FVector(FMath::Cos(Radians) * Radius, FMath::Sin(Radians) * Radius, 0);
		const FQuat Rotation = FRotator(0, Angle, 0).Quaternion() * MeshRotation.Quaternion();
		Mesh->SetWorldTransform(FTransform(Rotation, Position, MeshScale));
	}
}

void URogueShieldOrbitComponent::SetShieldVisible(bool bVisible)
{
	if (bShieldVisible == bVisible) { return; }
	bShieldVisible = bVisible;
	for (UStaticMeshComponent* Mesh : OrbitMeshes)
	{
		if (IsValid(Mesh)) { Mesh->SetVisibility(bVisible); }
	}
}

void URogueShieldOrbitComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	RefreshShieldVisuals();
}

void URogueShieldOrbitComponent::Deactivate()
{
	SetShieldVisible(false);
	Super::Deactivate();
}

void URogueShieldOrbitComponent::DestroyOrbitMeshes()
{
	bShieldVisible = false;
	for (UStaticMeshComponent* Mesh : OrbitMeshes)
	{
		if (IsValid(Mesh))
		{
			if (AActor* Owner = GetOwner()) { Owner->RemoveInstanceComponent(Mesh); }
			Mesh->DestroyComponent();
		}
	}
	OrbitMeshes.Reset();
}

void URogueShieldOrbitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyOrbitMeshes();
	Super::EndPlay(EndPlayReason);
}

void URogueShieldOrbitComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DestroyOrbitMeshes();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
