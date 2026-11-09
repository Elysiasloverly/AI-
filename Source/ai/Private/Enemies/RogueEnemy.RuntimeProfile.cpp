#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/Components/RogueEnemyStationaryHazardComponent.h"
void ARogueEnemy::RefreshUtilityComponents()
{
    if (BossSkills) BossSkills->RefreshPresentation();
    if (EnemyPresentation) EnemyPresentation->RefreshRuntime();
    if (EnemySupport) EnemySupport->InitializeRuntime();
    if (StationaryHazard) StationaryHazard->InitializeRuntime();
}
void ARogueEnemy::OnRep_RuntimeProfile()
{
    if (HasAuthority()) return;
    if (EnemyPresentation) EnemyPresentation->ResetForPool();
    const auto& P=RuntimeProfile.Profile;
    EnemyType=P.EnemyType;bIsBoss=P.bIsBoss;MaxHealth=P.Health;
    MoveSpeed=P.Speed;ContactDamage=P.Damage;ExperienceReward=P.ExperienceReward;FlightGroundZ=RuntimeProfile.GroundZ;
    CurrentArchetype=EnemyArchetypeDataTable?RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(EnemyArchetypeDataTable,EnemyType,bIsBoss)
        :RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType,bIsBoss);
    ApplyEnemyStyle();ConfigureGroundedCollision();RefreshUtilityComponents();
}
void ARogueEnemy::SetSpawnGroundReference(const FVector& Location)
{
    FlightGroundZ=Location.Z;
    FVector P=Location;
    if (RogueEnemyArchetypes::IsFlyingType(EnemyType)) P.Z+=CurrentArchetype.Flying.Sanitized().HoverHeight;
    SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
    if (HasAuthority()) { RuntimeProfile.GroundZ=FlightGroundZ;ForceNetUpdate(); }
}
