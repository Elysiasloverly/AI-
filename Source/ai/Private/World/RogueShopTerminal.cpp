#include "World/RogueShopTerminal.h"

#include "Player/RogueCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double ShopFallTime = 1.12;
	constexpr double ShopSettleTime = 0.20;
	constexpr double ShopChargeTime = 0.50;
	constexpr double ShopRiseTime = 1.05;
	constexpr double ShopDropHeight = 2000.0;
	constexpr double ShopRiseHeight = 2400.0;
	constexpr double ShopDrift = 220.0;
	constexpr double ShopSideSwing = 64.0;

	FVector ShopDropOffset(const FVector& Direction, double Progress)
	{
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction);
		return -Direction * (ShopDrift * FMath::Square(1.0 - Progress)) +
			Right * (ShopSideSwing * FMath::Sin(Progress * PI)) +
			FVector(0, 0, ShopDropHeight * (1.0 - FMath::Square(Progress)));
	}
}

ARogueShopTerminal::ARogueShopTerminal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(SceneRoot);
	BaseMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BaseMesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(SceneRoot);
	GlowMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GlowMesh->SetCastShadow(false);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(SceneRoot);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionBox->SetGenerateOverlapEvents(true);
	InteractionBox->SetBoxExtent(FVector(150.0));

	PromptAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("PromptAnchor"));
	PromptAnchor->SetupAttachment(SceneRoot);
	PromptAnchor->SetRelativeLocation(FVector(0.0, 0.0, 220.0));

	static ConstructorHelpers::FObjectFinder<UParticleSystem> Dust(
		TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Particles/Smoke/P_Smoke_F.P_Smoke_F"));
	LandingDust = Dust.Object;
}

void ARogueShopTerminal::BeginPlay()
{
	Super::BeginPlay();
	AuthoredRotation = GetActorRotation();
	AuthoredScale = GetActorScale3D();
	if (BaseMesh->GetStaticMesh())
	{
		RootHeightFromGround = GetActorLocation().Z - BaseMesh->Bounds.GetBox().Min.Z + 4.0;
	}
	SetReplicatingMovement(false);
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ARogueShopTerminal::HandleOverlapBegin);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ARogueShopTerminal::HandleOverlapEnd);
	bFlightPresentationInitialized = true;
	OnRep_Flight();
}

void ARogueShopTerminal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueShopTerminal, Flight);
}

bool ARogueShopTerminal::TryDropNear(const ARogueCharacter& Player)
{
	if (!HasAuthority() || !IsWaitingInSky()) { return false; }

	UWorld* World = GetWorld();
	const FVector PlayerLocation = Player.GetActorLocation();
	const double FootZ = PlayerLocation.Z - Player.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Forward = Player.GetActorForwardVector().GetSafeNormal2D();
	const double Radius = BaseMesh->GetStaticMesh() ? FMath::Clamp(
		FMath::Max(BaseMesh->Bounds.BoxExtent.X, BaseMesh->Bounds.BoxExtent.Y), 55.0, 150.0) : 75.0;
	const double HalfHeight = BaseMesh->GetStaticMesh() ? FMath::Max(Radius, BaseMesh->Bounds.BoxExtent.Z) : 90.0;
	const FCollisionShape Clearance = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(ShopLanding), false, this);
	const FCollisionObjectQueryParams GroundQuery(ECC_WorldStatic);
	FCollisionObjectQueryParams Obstacles(ECC_WorldStatic);
	Obstacles.AddObjectTypesToQuery(ECC_GameTraceChannel2); // RogueArenaWall
	constexpr double Angles[] = {45.0, -45.0, 90.0, -90.0, 0.0, 135.0, -135.0, 180.0};
	for (double Distance : {360.0, 530.0, 700.0})
	{
		for (double Angle : Angles)
		{
			const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);
			const FVector Candidate(PlayerLocation.X + Direction.X * Distance, PlayerLocation.Y + Direction.Y * Distance, FootZ);
			FHitResult Floor;
			if (!World->LineTraceSingleByObjectType(Floor, Candidate + FVector(0, 0, 110),
				Candidate - FVector(0, 0, 135), GroundQuery, Query) || Floor.ImpactNormal.Z < .88f ||
				FMath::Abs(Floor.ImpactPoint.Z - FootZ) > 105.0)
			{
				continue;
			}

			const FVector LandingCenter = Floor.ImpactPoint + FVector(0, 0, HalfHeight + 16.0);
			const FVector WalkStart(PlayerLocation.X, PlayerLocation.Y, FootZ + 125.0);
			const FVector WalkEnd = Floor.ImpactPoint + FVector(0, 0, 125.0);
			if (World->SweepTestByObjectType(WalkStart, WalkEnd, FQuat::Identity, Obstacles,
				FCollisionShape::MakeCapsule(35.0, 72.0), Query))
			{
				continue;
			}

			const FVector AirStart = LandingCenter + ShopDropOffset(Direction, 0.0);
			const FVector AirMid = LandingCenter + ShopDropOffset(Direction, .5);
			if (World->SweepTestByObjectType(AirStart, AirMid, FQuat::Identity, Obstacles, Clearance, Query) ||
				World->SweepTestByObjectType(AirMid, LandingCenter, FQuat::Identity, Obstacles, Clearance, Query))
			{
				continue;
			}

			Flight.Ground = Floor.ImpactPoint;
			Flight.Direction = Direction;
			SetPhase(ERogueShopFlightPhase::Falling);
			return true;
		}
	}
	return false;
}

void ARogueShopTerminal::BeginDeparture()
{
	if (HasAuthority() && IsReady()) { SetPhase(ERogueShopFlightPhase::Charging); }
}

void ARogueShopTerminal::SetPhase(ERogueShopFlightPhase Phase)
{
	Flight.Phase = Phase;
	Flight.ServerStartTime = GetServerTime();
	if (Phase == ERogueShopFlightPhase::Settling) { Flight.LastLandingTime = Flight.ServerStartTime; }
	OnRep_Flight();
	ForceNetUpdate();
}

double ARogueShopTerminal::GetServerTime() const
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void ARogueShopTerminal::OnRep_Flight()
{
	if (!bFlightPresentationInitialized) { return; }
	const bool bHideTerminal = IsWaitingInSky();
	const bool bReady = IsReady();
	if (!bReady) { InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	SetActorHiddenInGame(bHideTerminal);
	const double Now = GetServerTime();
	const double Age = FMath::Max(0.0, Now - Flight.ServerStartTime);
	if (bHideTerminal)
	{
		SetActorRotation(AuthoredRotation);
		SetActorScale3D(AuthoredScale);
	}
	else { UpdateFlightPose(Age); }
	if (bReady) { InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
	SetActorTickEnabled(!bHideTerminal && !bReady);
	if (!bHideTerminal && Flight.LastLandingTime >= 0.0 && LastDustStartTime != Flight.LastLandingTime)
	{
		LastDustStartTime = Flight.LastLandingTime;
		if (Now - Flight.LastLandingTime < .4) { PlayLandingDust(); }
	}
}

void ARogueShopTerminal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Age = FMath::Max(0.0, GetServerTime() - Flight.ServerStartTime);
	if (HasAuthority())
	{
		switch (Flight.Phase)
		{
		case ERogueShopFlightPhase::Falling:
			if (Age >= ShopFallTime) { SetPhase(ERogueShopFlightPhase::Settling); return; }
			break;
		case ERogueShopFlightPhase::Settling:
			if (Age >= ShopSettleTime) { SetPhase(ERogueShopFlightPhase::Ready); return; }
			break;
		case ERogueShopFlightPhase::Charging:
			if (Age >= ShopChargeTime) { SetPhase(ERogueShopFlightPhase::Rising); return; }
			break;
		case ERogueShopFlightPhase::Rising:
			if (Age >= ShopRiseTime) { SetPhase(ERogueShopFlightPhase::Hidden); return; }
			break;
		default: return;
		}
	}
	UpdateFlightPose(Age);
}

void ARogueShopTerminal::UpdateFlightPose(double Age)
{
	FVector Position = FVector(Flight.Ground) + FVector(0, 0, RootHeightFromGround);
	FRotator Rotation = AuthoredRotation;
	FVector Scale = AuthoredScale;
	const FVector Direction = Flight.Direction;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction);
	switch (Flight.Phase)
	{
	case ERogueShopFlightPhase::Falling:
	{
		const double T = FMath::Clamp(Age / ShopFallTime, 0.0, 1.0);
		const double Swing = FMath::Sin(T * PI);
		Position += ShopDropOffset(Direction, T);
		Rotation += FRotator(16.0 * Swing, -32.0 * (1.0 - T), -12.0 * Swing);
		Scale *= FVector(1.0 - .05 * Swing, 1.0 - .05 * Swing, 1.0 + .09 * Swing);
		break;
	}
	case ERogueShopFlightPhase::Settling:
	{
		const double T = FMath::Clamp(Age / ShopSettleTime, 0.0, 1.0);
		const double Squash = FMath::Square(1.0 - T);
		Position.Z += 17.0 * FMath::Sin(T * PI);
		Scale *= FVector(1.0 + .12 * Squash, 1.0 + .12 * Squash, 1.0 - .16 * Squash);
		break;
	}
	case ERogueShopFlightPhase::Charging:
	{
		const double T = FMath::SmoothStep(0.0, 1.0, FMath::Clamp(Age / ShopChargeTime, 0.0, 1.0));
		Rotation.Roll += 5.0 * FMath::Sin(T * PI * 3.0) * T;
		Scale *= FVector(1.0 + .15 * T, 1.0 + .15 * T, 1.0 - .27 * T);
		break;
	}
	case ERogueShopFlightPhase::Rising:
	{
		const double T = FMath::Clamp(Age / ShopRiseTime, 0.0, 1.0);
		Position += -Direction * (ShopDrift * T * T) + Right * (ShopSideSwing * FMath::Sin(T * PI));
		Position.Z += ShopRiseHeight * (1.0 - FMath::Pow(1.0 - T, 3.0));
		Rotation += FRotator(-20.0 * T, 70.0 * T, 10.0 * T);
		const double Stretch = FMath::Sin(T * PI) * .22;
		Scale *= FVector(1.0 - Stretch, 1.0 - Stretch, 1.0 + Stretch);
		Scale *= FMath::Lerp(FVector(1.15, 1.15, .73), FVector::OneVector, FMath::Min(1.0, T * 9.0));
		break;
	}
	default: break;
	}
	// 以地面为缩放支点，避免带居中枢轴的商店在下蹲时反而抬高底部。
	if (!FMath::IsNearlyZero(AuthoredScale.Z))
	{
		Position.Z += (RootHeightFromGround - 4.0) * (Scale.Z / AuthoredScale.Z - 1.0);
	}
	SetActorTransform(FTransform(Rotation, Position, Scale), false, nullptr, ETeleportType::TeleportPhysics);
}

void ARogueShopTerminal::PlayLandingDust()
{
	if (!LandingDust || GetNetMode() == NM_DedicatedServer) { return; }
	const FVector Right = FVector::CrossProduct(FVector::UpVector, FVector(Flight.Direction));
	for (double Offset : {-44.0, 0.0, 44.0})
	{
		const FVector Position = FVector(Flight.Ground) + Right * Offset + FVector(0, 0, 12.0);
		if (auto* Dust = UGameplayStatics::SpawnEmitterAtLocation(this, LandingDust, Position,
			FRotator::ZeroRotator, FVector(.65, .65, .30)))
		{
			FTimerHandle StopDust;
			GetWorldTimerManager().SetTimer(StopDust, FTimerDelegate::CreateWeakLambda(Dust,
				[Dust] { Dust->DeactivateSystem(); }), .20f, false);
		}
	}
}

bool ARogueShopTerminal::CanInteract(const ARogueCharacter& Player) const
{
	return IsReady() && InteractionBox->IsOverlappingActor(&Player);
}

FVector ARogueShopTerminal::GetPromptWorldLocation() const
{
	return PromptAnchor->GetComponentLocation();
}

void ARogueShopTerminal::HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ARogueCharacter* Player = Cast<ARogueCharacter>(OtherActor); Player && IsReady())
	{
		Player->SetNearbyShopTerminal(this);
	}
}

void ARogueShopTerminal::HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ARogueCharacter* Player = Cast<ARogueCharacter>(OtherActor))
	{
		Player->ClearNearbyShopTerminal(this);
	}
}
