#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Visuals/RogueHitFlashComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/AutomationCommon.h"

struct FRogueEnemyHitFlashTestAccess
{
	static void SetMaterial(ARogueEnemy* E, UMaterialInterface* M) { E->HitFlashMaterial = M; }
	static UMaterialInterface* Material(const ARogueEnemy* E) { return E->HitFlashMaterial; }
	static FRogueEnemyHitFeedback Snapshot(const ARogueEnemy* E) { return E->HitFeedback; }
	static void Receive(ARogueEnemy* E, FRogueEnemyHitFeedback Value) { E->HitFeedback = Value; E->ConsumeReplicatedHitFeedback(); }
	static void SetActive(ARogueEnemy* E, bool Active) { E->bReplicatedActive = Active; E->bPoolAvailable = !Active; E->bDead = !Active; }
	static void AllowNext(URogueHitFlashComponent* C) { C->NextAllowedFlashTime = -1; }
	static UMaterialInstanceDynamic* MID(URogueHitFlashComponent* C) { return C->FlashMID; }
};
namespace RogueEnemyHitFlashTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* MaterialPath = TEXT("/Game/GameAssets/Combat/Materials/M_EnemyHitFlash.M_EnemyHitFlash");
	struct FFixture
	{
		FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		bool Initialize()
		{
			SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			World->GetAuthGameMode()->SetActorTickEnabled(false);
			return true;
		}
		ARogueEnemy* Enemy()
		{
			ARogueEnemy* E = World->SpawnActor<ARogueEnemy>();
			E->ActivatePooledEnemy(nullptr, FVector(0,0,500), FRotator::ZeroRotator);
			FRogueEnemyProfile P; P.Health = 100; E->InitializeEnemy(P);
			E->SetActorTickEnabled(false);
			FRogueEnemyHitFlashTestAccess::SetMaterial(E, LoadObject<UMaterialInterface>(nullptr, MaterialPath));
			return E;
		}
		UStaticMeshComponent* Mesh(ARogueEnemy* E)
		{
			auto* M = NewObject<UStaticMeshComponent>(E);
			M->SetupAttachment(E->GetRootComponent());
			M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->RegisterComponent();
			return M;
		}
		void Step() { for (int32 I=0; I<12; ++I) { TestWorld.TickTestWorld(.02f); } }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyRealDamageFlashTest,
	"Rogue.Gameplay.EnemyHitFlash.RealGASDamageAndPoolReset", RogueEnemyHitFlashTests::Flags)
bool FRogueEnemyRealDamageFlashTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyHitFlashTests;
	FFixture F; if (!F.Initialize()) { return false; }
	auto* E = F.Enemy(); auto* M = F.Mesh(E); auto* C = E->GetHitFlashComponent();
	TestFalse(TEXT("Initialization never flashes"), C->IsFlashing());
	AActor* Source = F.World->SpawnActor<AActor>();
	TestTrue(TEXT("Real GAS hit accepted"), RogueAbilitySystem::ApplyDamageToTarget(Source, E, 5, RogueGameplayTags::Damage_Source_Projectile));
	TestEqual(TEXT("Damage reaches authoritative health"), E->GetRogueAttributeSet()->GetHealth(), 95.f);
	TestTrue(TEXT("Health loss flashes"), C->IsFlashing());
	TestNotNull(TEXT("Overlay is installed"), M->GetOverlayMaterial());
	const auto Generation = FRogueEnemyHitFlashTestAccess::Snapshot(E).Generation;
	E->DeactivateToPool();
	TestFalse(TEXT("Pool clears active effect immediately"), C->IsFlashing());
	TestNull(TEXT("Pool restores overlay"), M->GetOverlayMaterial());
	E->ActivatePooledEnemy(nullptr, FVector(0,0,500), FRotator::ZeroRotator);
	FRogueEnemyProfile P; P.Health=200; E->InitializeEnemy(P); E->SetActorTickEnabled(false);
	TestTrue(TEXT("Reused actor receives a new presentation generation"), FRogueEnemyHitFlashTestAccess::Snapshot(E).Generation != Generation);
	TestFalse(TEXT("Healing/resetting health does not flash"), C->IsFlashing());
	E->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(), 200);
	TestFalse(TEXT("Equal health does not flash"), C->IsFlashing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyOverlayOwnershipTest,
	"Rogue.Gameplay.EnemyHitFlash.PreservesAppearanceAndRestoresOverlay", RogueEnemyHitFlashTests::Flags)
bool FRogueEnemyOverlayOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyHitFlashTests;
	FFixture F; if (!F.Initialize()) { return false; }
	auto* E = F.Enemy(); auto* M = F.Mesh(E); auto* Excluded = F.Mesh(E); auto* C = E->GetHitFlashComponent();
	Excluded->ComponentTags.Add(TEXT("NoHitFlash"));
	UMaterialInterface* Base = M->GetMaterial(0);
	UMaterialInterface* Previous = UMaterial::GetDefaultMaterial(MD_Surface);
	M->SetOverlayMaterial(Previous); M->SetOverlayMaterialMaxDrawDistance(1234);
	M->SetRelativeScale3D(FVector(2,3,4)); const FTransform Transform=M->GetRelativeTransform();
	E->OnEnemyDamaged(5);
	TestTrue(TEXT("First pulse active"), C->IsFlashing());
	TestTrue(TEXT("Overlay temporarily uses raster fallback for Nanite compatibility"), bool(M->bForceDisableNanite));
	TestNull(TEXT("Opted-out accessory unaffected"), Excluded->GetOverlayMaterial());
	TestTrue(TEXT("Base slot retained"), Base == M->GetMaterial(0));
	TestTrue(TEXT("Authored transform retained"), Transform.Equals(M->GetRelativeTransform()));
	auto* MID=FRogueEnemyHitFlashTestAccess::MID(C);
	E->OnEnemyDamaged(5); TestTrue(TEXT("Rapid hits reuse MID"), MID == FRogueEnemyHitFlashTestAccess::MID(C));
	F.Step();
	TestFalse(TEXT("Timer ends pulse"), C->IsFlashing());
	TestFalse(TEXT("Original Nanite mode restored"), bool(M->bForceDisableNanite));
	TestTrue(TEXT("Previous overlay restored"), M->GetOverlayMaterial()==Previous);
	TestEqual(TEXT("Previous draw distance restored"), M->OverlayMaterialMaxDrawDistance,1234.f);
	FRogueEnemyHitFlashTestAccess::AllowNext(C); E->OnEnemyDamaged(5);
	auto* Other = UMaterialInstanceDynamic::Create(Previous,E); M->SetOverlayMaterial(Other);
	C->ClearHitFlash(); TestTrue(TEXT("Later external effect not overwritten"), M->GetOverlayMaterial()==Other);
	M->SetForceDisableNanite(true);
	E->OnEnemyDamaged(5); C->ClearHitFlash();
	TestTrue(TEXT("Pre-existing forced fallback remains enabled"), bool(M->bForceDisableNanite));
	FRogueEnemyHitFlashTestAccess::SetMaterial(E,nullptr); E->OnEnemyDamaged(5);
	TestFalse(TEXT("Clearing base material disables hit flash"), C->IsFlashing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyReplicatedFlashTest,
	"Rogue.Gameplay.EnemyHitFlash.ReplicatedSnapshotActivationAndReuse", RogueEnemyHitFlashTests::Flags)
bool FRogueEnemyReplicatedFlashTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyHitFlashTests;
	FFixture F; if (!F.Initialize()) { return false; }
	auto* E=F.Enemy(); F.Mesh(E); auto* C=E->GetHitFlashComponent();
	FRogueEnemyHitFeedback Value; Value.Generation=10; Value.Sequence=2; Value.TotalDamage=10; Value.LatestDamage=5;
	Value.bActive=true; Value.ServerTime=F.World->GetTimeSeconds();
	FRogueEnemyHitFlashTestAccess::SetActive(E,false);
	FRogueEnemyHitFlashTestAccess::Receive(E,Value);
	TestFalse(TEXT("Snapshot before activation waits"), C->IsFlashing());
	FRogueEnemyHitFlashTestAccess::SetActive(E,true); FRogueEnemyHitFlashTestAccess::Receive(E,Value);
	TestTrue(TEXT("Activation consumes waiting hit"), C->IsFlashing());
	C->ClearHitFlash(); FRogueEnemyHitFlashTestAccess::Receive(E,Value);
	TestFalse(TEXT("Duplicate snapshot cannot flash twice"), C->IsFlashing());
	Value.Sequence++; Value.TotalDamage+=5; Value.ServerTime=-10;
	FRogueEnemyHitFlashTestAccess::Receive(E,Value);
	TestFalse(TEXT("Late relevance skips old hit"), C->IsFlashing());
	Value.Sequence++; Value.TotalDamage+=5; Value.ServerTime=F.World->GetTimeSeconds();
	FRogueEnemyHitFlashTestAccess::Receive(E,Value); TestTrue(TEXT("Fresh next hit flashes"),C->IsFlashing());
	Value.Generation++; Value.Sequence=0; Value.TotalDamage=0;
	FRogueEnemyHitFlashTestAccess::Receive(E,Value);
	TestFalse(TEXT("Coalesced pool reuse clears previous occupant flash"), C->IsFlashing());
	TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); bool Success=false;
	Value.NetSerialize(Writer,nullptr,Success); TestTrue(TEXT("Serialize snapshot"), Success);
	FRogueEnemyHitFeedback Decoded; FMemoryReader Reader(Bytes); Decoded.NetSerialize(Reader,nullptr,Success);
	TestTrue(TEXT("Coherent snapshot roundtrips"), Success && Value==Decoded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyBaseFlashInheritanceTest,
	"Rogue.Gameplay.EnemyHitFlash.ActualBlueprintsInheritBaseMaterial", RogueEnemyHitFlashTests::Flags)
bool FRogueEnemyBaseFlashInheritanceTest::RunTest(const FString& Parameters)
{
	UClass* Base=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/BP_EnemyBase.BP_EnemyBase_C"));
	if (!TestNotNull(TEXT("Base BP loads"),Base)) { return false; }
	const auto* Material=FRogueEnemyHitFlashTestAccess::Material(Base->GetDefaultObject<ARogueEnemy>());
	if (!TestNotNull(TEXT("Base BP owns configured hit material"),Material)) { return false; }
	const TCHAR* Names[]={TEXT("Rifleman"),TEXT("Assault"),TEXT("Juggernaut"),TEXT("Orbiter"),TEXT("Marksman"),TEXT("Scatterer"),TEXT("Skirmisher"),TEXT("Cannoneer"),TEXT("ShockPillar"),TEXT("Sniper"),TEXT("Shotgunner"),TEXT("Repeater"),TEXT("Ringcaster")};
	for (const TCHAR* Name:Names)
	{
		UClass* Class=LoadClass<ARogueEnemy>(nullptr,*FString::Printf(TEXT("/Game/BP/Enemy/BP_Enemy%s.BP_Enemy%s_C"),Name,Name));
		if (!TestNotNull(Name,Class)) { return false; }
		TestTrue(*FString::Printf(TEXT("%s inherits BP_EnemyBase"),Name),Class->GetSuperClass()==Base);
		TestTrue(*FString::Printf(TEXT("%s inherits shared hit material"),Name),FRogueEnemyHitFlashTestAccess::Material(Class->GetDefaultObject<ARogueEnemy>())==Material);
	}
	return true;
}
#endif
