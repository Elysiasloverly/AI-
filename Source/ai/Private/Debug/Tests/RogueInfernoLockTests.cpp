#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/Visuals/RogueInfernoLockRing.h"
#include "Combat/Visuals/RogueInfernoLockSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Shared/RogueTypes.h"
#include "Tests/AutomationCommon.h"

namespace RogueLockTests
{
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
struct FFixture
{
	FTestWorldWrapper Test;
	UWorld* W = nullptr;
	URogueInfernoLockSubsystem* Rings = nullptr;
	TSubclassOf<ARogueInfernoLockRing> Class;
	bool Init()
	{
		if (!Test.CreateTestWorld(EWorldType::Game))
			return false;
		W = Test.GetTestWorld();
		W->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
		if (!Test.BeginPlayInTestWorld())
			return false;
		W->GetAuthGameMode()->SetActorTickEnabled(false);
		Rings = W->GetSubsystem<URogueInfernoLockSubsystem>();
		Class = LoadClass<ARogueInfernoLockRing>(
			nullptr, TEXT("/Game/BP/Effects/WeaponLock/BP_HellTowerLockRing.BP_HellTowerLockRing_C"));
		return Rings && Class;
	}
	ARogueEnemy* Enemy(float X = 500)
	{
		auto* E = W->SpawnActor<ARogueEnemy>();
		E->ActivatePooledEnemy(nullptr, FVector(X, 0, 150), FRotator::ZeroRotator);
		FRogueEnemyProfile P;
		P.Health = 10000;
		P.Speed = P.Damage = 0;
		E->InitializeEnemy(P);
		E->SetActorTickEnabled(false);
		E->GetCharacterMovement()->SetComponentTickEnabled(false);
		return E;
	}
	void Step(float Time = .05f)
	{
		while (Time > 0)
		{
			const float D = FMath::Min(Time, .05f);
			Test.TickTestWorld(D);
			Rings->Tick(D);
			Time -= D;
		}
	}
};
} // namespace RogueLockTests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInfernoLockTargets,
								 "Rogue.Presentation.InfernoLock.AllTargetsFollowAndShrinkWithoutChangingDamage",
								 RogueLockTests::Flags)
bool FInfernoLockTargets::RunTest(const FString&)
{
	RogueLockTests::FFixture F;
	if (!F.Init())
		return false;
	auto* Source = F.W->SpawnActor<AActor>();
	TArray<ARogueEnemy*> Targets = {F.Enemy(500), F.Enemy(900), F.Enemy(1300)};
	for (auto* E : Targets)
		F.Rings->Touch(Source, E, E->GetPresentationGeneration(), 0, F.Class, 2);
	F.Step();
	TestEqual(TEXT("Every locked target receives a ring"), F.Rings->GetActiveRingCount(), 3);
	const float Before = F.Rings->GetLockProgress(Targets[0]);
	F.Step(.25f);
	TestTrue(TEXT("Continuous lock advances shrink"), F.Rings->GetLockProgress(Targets[0]) > Before);
	Targets[0]->SetActorLocation(FVector(2000, 700, 150));
	F.Step();
	bool Followed = false;
	for (TActorIterator<ARogueInfernoLockRing> It(F.W); It; ++It)
		if (!It->IsAvailable())
		{
			Followed |= FMath::IsNearlyEqual(It->GetActorLocation().X, 2000.) &&
						FMath::IsNearlyEqual(It->GetActorLocation().Y, 700.);
			TestFalse(TEXT("Ring has no collision"), It->GetActorEnableCollision());
			TestFalse(TEXT("Ring adds no actor tick"), It->IsActorTickEnabled());
		}
	TestTrue(TEXT("Circle follows the moving target"), Followed);
	for (auto* E : Targets)
		TestEqual(TEXT("Cosmetic does not change health"), E->GetCurrentHealth(), 10000.f);
	F.Rings->Release(Source, Targets[0], Targets[0]->GetPresentationGeneration());
	TestEqual(TEXT("Explicit unlock clears immediately"), F.Rings->GetActiveRingCount(), 2);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInfernoLockShared,
								 "Rogue.Presentation.InfernoLock.SharedTargetsStaySingleAndNeverExpandOnPartialUnlock",
								 RogueLockTests::Flags)
bool FInfernoLockShared::RunTest(const FString&)
{
	RogueLockTests::FFixture F;
	if (!F.Init())
		return false;
	auto* A = F.W->SpawnActor<AActor>();
	auto* B = F.W->SpawnActor<AActor>();
	auto* E = F.Enemy();
	const auto G = E->GetPresentationGeneration();
	F.Rings->Touch(A, E, G, 1.4f, F.Class, 2);
	F.Rings->Touch(B, E, G, .2f, F.Class, 2);
	F.Step();
	TestEqual(TEXT("Two players do not draw duplicate circles"), F.Rings->GetActiveRingCount(), 1);
	const float Before = F.Rings->GetLockProgress(E);
	F.Rings->Release(A, E, G);
	F.Step();
	TestTrue(TEXT("Remaining lock preserves the accumulated shrink"), F.Rings->GetLockProgress(E) >= Before);
	F.Rings->Release(B, E, G);
	TestEqual(TEXT("Last holder removes circle"), F.Rings->GetActiveRingCount(), 0);
	F.Rings->Touch(A, E, G, 0, F.Class, 2);
	F.Step();
	TestTrue(TEXT("Reacquiring starts from a large circle"), F.Rings->GetLockProgress(E) < Before);
	F.Rings->RemoveSource(A);
	TestEqual(TEXT("Owner cleanup removes all its circles"), F.Rings->GetActiveRingCount(), 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInfernoLockReuse, "Rogue.Presentation.InfernoLock.PoolGenerationAndMissingRefreshCannotLeaveStaleCircles",
	RogueLockTests::Flags)
bool FInfernoLockReuse::RunTest(const FString&)
{
	RogueLockTests::FFixture F;
	if (!F.Init())
		return false;
	auto* A = F.W->SpawnActor<AActor>();
	auto* E = F.Enemy();
	const auto Old = E->GetPresentationGeneration();
	F.Rings->Touch(A, E, Old, 1, F.Class, 1);
	F.Step();
	E->DeactivateToPool();
	E->ActivatePooledEnemy(nullptr, FVector(900, 0, 150), FRotator::ZeroRotator);
	E->SetActorTickEnabled(false);
	F.Step();
	TestEqual(TEXT("Pooled reincarnation cannot inherit old ring"), F.Rings->GetActiveRingCount(), 0);
	F.Rings->Touch(A, E, Old, 1, F.Class, 1);
	F.Step();
	TestEqual(TEXT("Late old-generation event is ignored"), F.Rings->GetActiveRingCount(), 0);
	F.Rings->Touch(A, E, E->GetPresentationGeneration(), 0, F.Class, .15f);
	F.Step();
	TestEqual(TEXT("New generation can lock normally"), F.Rings->GetActiveRingCount(), 1);
	F.Step(.2f);
	TestEqual(TEXT("Lost stop packet cannot leave a permanent ring"), F.Rings->GetActiveRingCount(), 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInfernoLockReorder,
								 "Rogue.Presentation.InfernoLock.FutureGenerationWaitsForActivation",
								 RogueLockTests::Flags)
bool FInfernoLockReorder::RunTest(const FString&)
{
	RogueLockTests::FFixture F;
	if (!F.Init())
		return false;
	auto* A = F.W->SpawnActor<AActor>();
	auto* E = F.Enemy();
	F.Rings->Touch(A, E, E->GetPresentationGeneration() + 1, .4f, F.Class, 1);
	F.Step();
	TestEqual(TEXT("Early cosmetic does not bind to an old spawn"), F.Rings->GetActiveRingCount(), 0);
	E->DeactivateToPool();
	E->ActivatePooledEnemy(nullptr, FVector(900, 0, 150), FRotator::ZeroRotator);
	E->SetActorTickEnabled(false);
	F.Step();
	TestEqual(TEXT("Activation completes the pending binding"), F.Rings->GetActiveRingCount(), 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInfernoLockPacket,
								 "Rogue.Presentation.InfernoLock.OrdinaryFireAddsOnlyOnePresenceBit",
								 RogueLockTests::Flags)
bool FInfernoLockPacket::RunTest(const FString&)
{
	FRogueBeamLockCosmetic In;
	FBitWriter W(8, true);
	bool Ok = false;
	In.NetSerialize(W, nullptr, Ok);
	TestTrue(TEXT("Empty optional payload serializes"), Ok);
	TestEqual(TEXT("Ordinary attacks use one extra bit"), W.GetNumBits(), int64(1));
	FBitReader R(W.GetData(), W.GetNumBits());
	FRogueBeamLockCosmetic Out;
	Out.Generation = 123;
	Out.LockedSeconds = 8;
	Out.NetSerialize(R, nullptr, Ok);
	TestTrue(TEXT("Empty payload round-trips"), Ok);
	TestNull(TEXT("No stale target"), Out.Target.Get());
	TestEqual(TEXT("No stale age"), Out.LockedSeconds, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInfernoLockVisibility,
    "Rogue.Presentation.InfernoLock.FinalCircleStaysOutsideBodyAndResetsOnReuse",RogueLockTests::Flags)
bool FInfernoLockVisibility::RunTest(const FString&)
{
    RogueLockTests::FFixture F;if(!F.Init())return false;
    auto* Large=F.Enemy();Large->GetCapsuleComponent()->SetCapsuleSize(110,150);
    auto* Small=F.Enemy(900);Small->GetCapsuleComponent()->SetCapsuleSize(28,88);
    auto* Ring=F.W->SpawnActor<ARogueInfernoLockRing>(F.Class);
    Ring->OuterRadius=90;Ring->InnerRadius=45;Ring->BodyClearance=16;Ring->bFitTargetSize=true;
    Ring->ActivateRing(Large,3);
    const float LargeScale=Ring->GetActorScale3D().X;
    const auto& Data=Ring->RingMesh->GetCustomPrimitiveData().Data;
    if(!TestTrue(TEXT("Contraction data reaches material"),Data.Num()>=4))return false;
    const float FinalRadius=LargeScale*40.f*Data[2];
    TestTrue(TEXT("Completed circle stays outside the target with clearance"),FinalRadius>=126.f-.01f);
    TestTrue(TEXT("Outer circle still has visible room to shrink"),Data[2]<.8f);
    Ring->DeactivateRing();Ring->ActivateRing(Small,0);
    TestTrue(TEXT("Reused circle fits the smaller target instead of inheriting the large body"),Ring->GetActorScale3D().X<LargeScale);
    TestFalse(TEXT("Readable ring remains collision-free"),Ring->GetActorEnableCollision());
    return true;
}
#endif
