#include "World/RogueRangedSpawnPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/World.h"

ARogueRangedSpawnPoint::ARogueRangedSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;
    auto* Marker = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnMarker"));
    SetRootComponent(Marker);
    Marker->InitSphereRadius(85.f);
    Marker->ShapeColor = FColor(40, 210, 255);
    Marker->SetLineThickness(3.f);
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Marker->SetGenerateOverlapEvents(false);
    Marker->SetCanEverAffectNavigation(false);
    Marker->SetHiddenInGame(true);
    auto* Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
    Arrow->SetupAttachment(Marker);
    Arrow->ArrowColor = Marker->ShapeColor;
    Arrow->ArrowSize = 2.f;
    Arrow->SetHiddenInGame(true);
}

void ARogueRangedSpawnPoint::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
        if (auto* Spawner = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
            Spawner->RegisterRangedSpawnPoint(this);
}

void ARogueRangedSpawnPoint::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Spawner = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
        Spawner->UnregisterRangedSpawnPoint(this);
    Super::EndPlay(Reason);
}
