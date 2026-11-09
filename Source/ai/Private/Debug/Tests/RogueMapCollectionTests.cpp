#include "World/RogueArenaBoundary.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "World/RogueEnemySpawnRegion.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/StaticMesh.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace RogueMapTests
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMapPoolsTest,"Rogue.World.MapCollection.ValidSeparateRandomPools",RogueMapTests::Flags)
bool FRogueMapPoolsTest::RunTest(const FString&)
{
    const auto* Balance=URogueGameBalanceAsset::GetDefaultGameBalance();
    if(!TestNotNull(TEXT("Authored balance asset"),Balance))return false;
    const auto& Settings=Balance->BattleStage;
    TestEqual(TEXT("Five day themes"),Settings.DayMaps.Num(),5);
    TestEqual(TEXT("Five night themes"),Settings.NightMaps.Num(),5);
    TSet<FString> DaySeen,NightSeen;
    for(int I=0;I<256;++I)
    {
        const FString Day=Settings.ChooseMap(false),Night=Settings.ChooseMap(true);
        TestTrue(TEXT("Day remains inside day directory"),Day.StartsWith(TEXT("/Game/Map/Day/")));
        TestTrue(TEXT("Night remains inside night directory"),Night.StartsWith(TEXT("/Game/Map/Night/")));
        TestTrue(TEXT("Day recognized on arrival"),Settings.ContainsMap(Day,false));
        TestFalse(TEXT("Day never recognized as night"),Settings.ContainsMap(Day,true));
        TestTrue(TEXT("Night recognized on arrival"),Settings.ContainsMap(Night,true));
        DaySeen.Add(Day);NightSeen.Add(Night);
    }
    TestEqual(TEXT("All day themes reachable"),DaySeen.Num(),5);
    TestEqual(TEXT("All night themes reachable"),NightSeen.Num(),5);
    FRogueBattleStageSettings Broken;Broken.DayMaps={TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/Missing.Missing")))};
    TestEqual(TEXT("Missing entry falls back to existing forest"),Broken.ChooseMap(false),Broken.DayMap.ToSoftObjectPath().GetLongPackageName());
    TestTrue(TEXT("Excluded sole fallback yields no unsafe choice"),Broken.ChooseMap(false,Broken.DayMap.ToSoftObjectPath().GetLongPackageName()).IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueArenaWallTest,"Rogue.World.MapCollection.PawnWallsAndWaterHoles",RogueMapTests::Flags)
bool FRogueArenaWallTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;if(!Fixture.CreateTestWorld(EWorldType::Game))return false;
    UWorld* World=Fixture.GetTestWorld();auto* Boundary=World->SpawnActor<ARogueArenaBoundary>();
    FRogueBoundaryLoop Outer;Outer.Points={FVector(-1000,-1000,0),FVector(1000,-1000,0),FVector(1000,1000,0),FVector(-1000,1000,0)};
    FRogueBoundaryLoop Hole;Hole.Points={FVector(-100,-100,0),FVector(100,-100,0),FVector(100,100,0),FVector(-100,100,0)};
    Boundary->Loops={Outer,Hole};Boundary->RebuildWalls();Boundary->RebuildWalls();
    TArray<UBoxComponent*> Boxes;Boundary->GetComponents(Boxes);TestEqual(TEXT("Rebuild replaces, never duplicates eight segments"),Boxes.Num(),8);
    for(auto* Box:Boxes)
    {
        TestEqual(TEXT("Players and enemies blocked"),Box->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
        TestEqual(TEXT("Camera not blocked"),Box->GetCollisionResponseToChannel(ECC_Camera),ECR_Ignore);
        TestEqual(TEXT("Projectiles not blocked"),Box->GetCollisionResponseToChannel(ECC_WorldDynamic),ECR_Ignore);
    }
    FHitResult Hit;
    TestTrue(TEXT("Outer edge stops walking capsule"),World->SweepSingleByChannel(Hit,FVector(500,0,100),FVector(1500,0,100),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(35,88)));
    TestTrue(TEXT("Inner water shore stops walking capsule"),World->SweepSingleByChannel(Hit,FVector(500,0,100),FVector(0,0,100),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(35,88)));
    TestFalse(TEXT("Visibility traces pass air wall"),World->LineTraceSingleByChannel(Hit,FVector(500,0,100),FVector(1500,0,100),ECC_Visibility));
    FRogueBoundaryLoop Railing;
    Railing.bClosed = false;
    Railing.Points = {FVector(-350, -150, 0), FVector(0, -150, 240), FVector(0, 150, 240)};
    Boundary->Loops = {Railing};
    Boundary->BelowGroundDepth = 700;
    Boundary->RebuildWalls();
    Boxes.Reset(); Boundary->GetComponents(Boxes);
    TestEqual(TEXT("Open rail has two segments and leaves the entrance open"), Boxes.Num(), 2);
    TestTrue(TEXT("Rail reaches down into the low side of a slope"), World->SweepSingleByChannel(Hit, FVector(-550,0,-475), FVector(150,0,-475), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(35,88)));
    TestFalse(TEXT("No invisible closing segment across the open side"), World->LineTraceSingleByChannel(Hit, FVector(-500,0,100), FVector(-35,0,100), ECC_Pawn));
    TestFalse(TEXT("WorldStatic projectile queries pass pawn-only walls"), World->LineTraceSingleByObjectType(Hit, FVector(-50,0,100), FVector(50,0,100), FCollisionObjectQueryParams(ECC_WorldStatic)));
    auto* Region=World->SpawnActor<ARogueEnemySpawnRegion>();auto* Spline=Region->GetSpawnOutline();Spline->ClearSplinePoints(false);
    for(const auto& P:Outer.Points)Spline->AddSplinePoint(P,ESplineCoordinateSpace::World,false);
    Spline->SetClosedLoop(true,true);Region->SetSpawnOutlineEnabled(true);
    FRogueSpawnExclusion Exclusion;Exclusion.Points={FVector2D(-100,-100),FVector2D(100,-100),FVector2D(100,100),FVector2D(-100,100)};Region->ExcludedAreas={Exclusion};
    TestTrue(TEXT("Dry land remains available"),Region->IsInsideSpawnOutline(FVector(500,0,0),35));
    TestFalse(TEXT("Water interior excluded"),Region->IsInsideSpawnOutline(FVector(0,0,0),35));
    TestFalse(TEXT("Capsule cannot straddle water boundary"),Region->IsInsideSpawnOutline(FVector(180,0,0),35));
    TestFalse(TEXT("Outer forest excluded"),Region->IsInsideSpawnOutline(FVector(1100,0,0),35));
    Region->SetActorLocation(FVector(3000,0,0));
    TestFalse(TEXT("Moving outline invalidates prior cached world points"),Region->IsInsideSpawnOutline(FVector(500,0,0),35));
    TestTrue(TEXT("Moved valid region remains usable"),Region->IsInsideSpawnOutline(FVector(3500,0,0),35));
    Spline->ClearSplinePoints(false);
    for(const FVector& P:{FVector(2000,-1000,0),FVector(4000,1000,0),FVector(2000,1000,0),FVector(4000,-1000,0)})Spline->AddSplinePoint(P,ESplineCoordinateSpace::World,false);
    Spline->SetClosedLoop(true,true);
    TestFalse(TEXT("An edited crossing outline never reuses a valid cache"),Region->IsInsideSpawnOutline(FVector(3500,0,0),35));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBattleMapAssetWiringTest,"Rogue.World.MapCollection.BattleMapsUseEnemyBlueprintsAndGameplayCollision",RogueMapTests::Flags)
bool FRogueBattleMapAssetWiringTest::RunTest(const FString&)
{
    const auto* Balance=URogueGameBalanceAsset::GetDefaultGameBalance();
    if(!TestNotNull(TEXT("Balance asset"),Balance))return false;
    for(const auto* Pool:{&Balance->BattleStage.DayMaps,&Balance->BattleStage.NightMaps})
    {
        for(const auto& Map:*Pool)
        {
            UWorld* World=Map.LoadSynchronous();
            if(!TestNotNull(*Map.ToString(),World))return false;
            const auto ModeClass=World->GetWorldSettings()->DefaultGameMode;
            auto* Mode=ModeClass?Cast<ARogueGameMode>(ModeClass->GetDefaultObject()):nullptr;
            if(!TestNotNull(TEXT("Battle rules configured"),Mode))return false;
            TestTrue(TEXT("Melee enemies use configured Blueprint classes"),Mode->HasEnemyClassForType(ERogueEnemyType::Juggernaut));
            TestTrue(TEXT("Ranged enemies use configured Blueprint classes"),Mode->HasEnemyClassForType(ERogueEnemyType::Rifleman));
        }
    }
    #if WITH_EDITORONLY_DATA
    for(int32 I=1;I<=4;++I)
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/FourMaps/Map%02d/SM_Map%02d_Environment.SM_Map%02d_Environment"),I,I,I));
        if(!TestNotNull(TEXT("Theme mesh"),Mesh))return false;
        TestNotNull(TEXT("Decorated scene has a separate gameplay collision mesh"),Mesh->ComplexCollisionMesh.Get());
    }
    #endif
    return true;
}

#endif
