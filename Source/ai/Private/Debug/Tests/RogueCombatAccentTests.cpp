#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace RogueAccentTests
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
struct FFixture
{
    FTestConsoleVariable Self{TEXT("Rogue.SelfTest.WeaponUpgradeChain")},Stress{TEXT("Rogue.SelfTest.EnemyStress")};
    FTestWorldWrapper TestWorld;UWorld* W=nullptr;URogueCombatPoolSubsystem* Pool=nullptr;
    FRogueCombatAccentSettings Settings;
    bool Init()
    {
        Self.Set(TEXT("0"));Stress.Set(TEXT("0"));
        if(!TestWorld.CreateTestWorld(EWorldType::Game))return false;
        W=TestWorld.GetTestWorld();W->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
        if(!TestWorld.BeginPlayInTestWorld())return false;
        W->GetAuthGameMode()->SetActorTickEnabled(false);Pool=W->GetSubsystem<URogueCombatPoolSubsystem>();
        Settings.EffectClass=LoadClass<ARogueCombatAccentEffect>(nullptr,TEXT("/Game/BP/Effects/BP_CombatAccent.BP_CombatAccent_C"));
        return Pool && Settings.EffectClass;
    }
    int32 Active()const {int N=0;for(TActorIterator<ARogueCombatAccentEffect> It(W);It;++It)if(!It->IsAvailableInPool())++N;return N;}
    ARogueCombatAccentEffect* Spawn(){return Pool->PlayCombatAccent(Settings,FVector(0,0,300),FVector::ForwardVector,true);}
    void Advance(float T){while(T>0){TestWorld.TickTestWorld(.02f);T-=.02f;}}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAccentPoolTest,"Rogue.Presentation.Accents.PoolLifetimeCollisionAndBudget",RogueAccentTests::Flags)
bool FRogueAccentPoolTest::RunTest(const FString&)
{
    using namespace RogueAccentTests;FFixture F;if(!F.Init())return false;
    auto* A=F.Spawn();if(!TestNotNull(TEXT("Configured accent spawns"),A))return false;
    TestFalse(TEXT("Cosmetic actor is not replicated"),A->GetIsReplicated());
    TestFalse(TEXT("Cosmetic actor has no collision"),A->GetActorEnableCollision());
    TArray<UPrimitiveComponent*> Meshes;A->GetComponents(Meshes);
    for(auto* C:Meshes)TestTrue(TEXT("Every accent mesh is collision-free"),C->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    auto* Material=A->Core->GetMaterial(0);TestNotNull(TEXT("Saved procedural glow material is applied"),Cast<UMaterialInstanceDynamic>(Material));
    F.Advance(.3f);TestTrue(TEXT("Expired actor returns to pool"),A->IsAvailableInPool());TestFalse(TEXT("Pooled actor stops ticking"),A->IsActorTickEnabled());
    auto* B=F.Spawn();TestTrue(TEXT("Next activation reuses actor"),A==B);TestTrue(TEXT("Next activation reuses dynamic materials"),B && B->Core->GetMaterial(0)==Material);
    for(int I=0;I<60;++I)F.Spawn();TestEqual(TEXT("Local effects have a finite simultaneous budget"),F.Active(),48);
    F.Advance(.3f);TestEqual(TEXT("Whole burst budget is reclaimed"),F.Active(),0);
    F.Settings.bEnabled=false;TestNull(TEXT("Explicitly disabled accent does not spawn"),F.Spawn());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAccentDamageTest,"Rogue.Presentation.Accents.ActualGASHitsThrottleAndPoolGeneration",RogueAccentTests::Flags)
bool FRogueAccentDamageTest::RunTest(const FString&)
{
    using namespace RogueAccentTests;FFixture F;if(!F.Init())return false;
    auto* Class=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Rifleman/BP_EnemyRifleman.BP_EnemyRifleman_C"));
    auto* E=F.W->SpawnActor<ARogueEnemy>(Class);if(!E)return false;
    E->ActivatePooledEnemy(nullptr,FVector(0,0,300),FRotator::ZeroRotator);FRogueEnemyProfile P;P.Health=100;E->InitializeEnemy(P);E->SetActorTickEnabled(false);
    TestEqual(TEXT("Initialization produces no hit spark"),F.Active(),0);
    auto* Source=F.W->SpawnActor<AActor>();
    TestTrue(TEXT("Real damage accepted"),RogueAbilitySystem::ApplyDamageToTarget(Source,E,5,RogueGameplayTags::Damage_Source_Projectile));
    TestEqual(TEXT("Real damage creates hit spark"),F.Active(),1);TestEqual(TEXT("Visual effect does not add damage"),E->GetCurrentHealth(),95.f);
    RogueAbilitySystem::ApplyDamageToTarget(Source,E,5,RogueGameplayTags::Damage_Source_Projectile);
    TestEqual(TEXT("Rapid hits coalesce visual bursts"),F.Active(),1);TestEqual(TEXT("Throttling does not remove actual hits"),E->GetCurrentHealth(),90.f);
    E->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),95);
    TestEqual(TEXT("Healing is not a hit spark"),F.Active(),1);
    E->DeactivateToPool();E->ActivatePooledEnemy(nullptr,FVector(0,0,300),FRotator::ZeroRotator);E->InitializeEnemy(P);E->SetActorTickEnabled(false);
    RogueAbilitySystem::ApplyDamageToTarget(Source,E,5,RogueGameplayTags::Damage_Source_Projectile);
    TestEqual(TEXT("New pooled life does not inherit old visual cooldown"),F.Active(),2);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAccentFireTest,"Rogue.Presentation.Accents.FireIndependentOfAudioAndNoServerReplay",RogueAccentTests::Flags)
bool FRogueAccentFireTest::RunTest(const FString&)
{
    using namespace RogueAccentTests;FFixture F;if(!F.Init())return false;
    auto* PC=F.W->SpawnActor<ARoguePlayerController>();PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun({});
    FTransform Transform(FVector(0,0,300));auto* P=F.W->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    auto* Table=NewObject<UDataTable>(F.W);Table->RowStruct=FRogueWeaponTableRow::StaticStruct();FRogueWeaponTableRow Row;Row.WeaponClass=ARogueWeapon_Projectile::StaticClass();Row.Count=1;Row.FireAccent=F.Settings;Table->AddRow(TEXT("Projectile"),Row);
    FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(P,Table);
    P->FinishSpawning(Transform);PC->Possess(P);P->SetActorTickEnabled(false);P->GetCharacterMovement()->SetComponentTickEnabled(false);P->GetAbilitySystemComponent()->CancelAllAbilities();
    auto* Weapon=P->FindWeapon<ARogueWeapon_Projectile>();if(!Weapon)return false;
    FRogueWeaponCosmeticFire Fire;Fire.Start=FVector(100,0,325);Fire.bPlayFireAccent=true;Fire.bPlayAttackSound=false;
    Weapon->BroadcastCosmeticFire(Fire);TestEqual(TEXT("Silent attack still shows flash"),F.Active(),1);
    P->Tick(.01f);TestEqual(TEXT("Server multicast does not replay local flash"),F.Active(),1);
    Weapon->PlayFireAccent(Fire);TestEqual(TEXT("Duplicate trigger in same frame is throttled"),F.Active(),1);
    F.Advance(.3f);TestEqual(TEXT("Flash naturally expires"),F.Active(),0);
    Weapon->GetConfig().FireAccent.bEnabled=false;Weapon->PlayFireAccent(Fire);TestEqual(TEXT("Weapon config can disable flash without disabling fire"),F.Active(),0);return true;
}
#endif
