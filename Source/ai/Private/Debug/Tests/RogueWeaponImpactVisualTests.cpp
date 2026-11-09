#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/RogueGameMode.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/RogueEnemyHitFeedback.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Combat/Visuals/RogueGroundEffectFade.h"
#include "Enemies/Components/RogueEnemyStationaryHazardComponent.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
namespace RogueWeaponImpactTests
{
 constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
 FRogueEnemyHitFeedback& Snapshot(ARogueEnemy* E)
 {return *FindFProperty<FStructProperty>(ARogueEnemy::StaticClass(),TEXT("HitFeedback"))->ContainerPtrToValuePtr<FRogueEnemyHitFeedback>(E);}
 struct FFixture
 {
  FTestWorldWrapper Test;UWorld* W=nullptr;
  bool Init(){if(!Test.CreateTestWorld(EWorldType::Game))return false;W=Test.GetTestWorld();W->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();if(!Test.BeginPlayInTestWorld())return false;W->GetAuthGameMode()->SetActorTickEnabled(false);return true;}
  ARogueEnemy* Enemy()
  {
   auto* C=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Rifleman/BP_EnemyRifleman.BP_EnemyRifleman_C"));
   auto* E=W->SpawnActor<ARogueEnemy>(C);E->ActivatePooledEnemy(nullptr,FVector(0,0,300),FRotator::ZeroRotator);
   FRogueEnemyProfile P;P.Health=1000;E->InitializeEnemy(P);E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);return E;
  }
  int32 Active(){int32 N=0;for(TActorIterator<ARogueCombatAccentEffect> It(W);It;++It)if(!It->IsAvailableInPool())++N;return N;}
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponImpactGAS,"Rogue.Presentation.WeaponImpact.RealDamageCarriesStyleWithoutChangingDamage",RogueWeaponImpactTests::Flags)
bool FWeaponImpactGAS::RunTest(const FString&)
{
 using namespace RogueWeaponImpactTests;FFixture F;if(!F.Init())return false;auto* Source=F.W->SpawnActor<AActor>();
 for(uint8 Style=1;Style<=6;++Style)
 {
  auto* E=F.Enemy();float Actual=0;
  TestTrue(TEXT("Styled damage succeeds"),RogueAbilitySystem::ApplyDamageToTarget(Source,E,7,RogueGameplayTags::Damage_Source_Projectile,&Actual,Style));
  TestEqual(TEXT("Cosmetic metadata does not change actual damage"),Actual,7.f);
  TestEqual(TEXT("Health follows the original damage amount"),E->GetCurrentHealth(),993.f);
  TestEqual(TEXT("Real GAS callback records the style"),Snapshot(E).VisualStyle,Style);
  const auto* Presets=FindFProperty<FMapProperty>(ARogueEnemy::StaticClass(),TEXT("WeaponHitAccents"))->ContainerPtrToValuePtr<TMap<ERogueWeaponImpactStyle,FRogueCombatAccentSettings>>(E);
  TestTrue(TEXT("Enemy inherits the corresponding effect preset"),Presets->Contains(static_cast<ERogueWeaponImpactStyle>(Style)));
  E->DeactivateToPool();
 }
 auto* E=F.Enemy();RogueAbilitySystem::ApplyDamageToTarget(Source,E,3,RogueGameplayTags::Damage_Source_Projectile,nullptr,255);
 TestEqual(TEXT("Invalid style falls back to generic feedback"),Snapshot(E).VisualStyle,uint8(0));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponImpactSnapshot,"Rogue.Presentation.WeaponImpact.CompactStyleSnapshotRoundtrip",RogueWeaponImpactTests::Flags)
bool FWeaponImpactSnapshot::RunTest(const FString&)
{
 for(uint8 Style=0;Style<=6;++Style)
 {
  FRogueEnemyHitFeedback A;A.Generation=4;A.Sequence=3;A.TotalDamage=11;A.LatestDamage=7;A.ServerTime=12;A.bActive=true;A.VisualStyle=Style;
  TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);bool Ok=false;A.NetSerialize(Writer,nullptr,Ok);
  FRogueEnemyHitFeedback B;FMemoryReader Reader(Bytes);B.NetSerialize(Reader,nullptr,Ok);
  TestTrue(TEXT("Hit identity and cosmetic style remain coherent"),Ok&&A==B);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponImpactClient,"Rogue.Presentation.WeaponImpact.ClientConsumesLatestStyleOnce",RogueWeaponImpactTests::Flags)
bool FWeaponImpactClient::RunTest(const FString&)
{
 using namespace RogueWeaponImpactTests;FFixture F;if(!F.Init())return false;auto* E=F.Enemy();
 auto* Role=FindFProperty<FByteProperty>(AActor::StaticClass(),TEXT("Role"));if(!Role)return false;
 Role->SetPropertyValue_InContainer(E,ROLE_SimulatedProxy);auto& S=Snapshot(E);S.Sequence=1;S.TotalDamage=S.LatestDamage=5;S.ServerTime=F.W->GetTimeSeconds();S.VisualStyle=5;S.bActive=true;
 E->ProcessEvent(E->FindFunctionChecked(TEXT("OnRep_HitFeedback")),nullptr);
 TestEqual(TEXT("Client reconstructs a local hit effect"),F.Active(),1);
 E->ProcessEvent(E->FindFunctionChecked(TEXT("OnRep_HitFeedback")),nullptr);
 TestEqual(TEXT("Duplicate replication does not replay effect"),F.Active(),1);
 S.Sequence++;S.ServerTime=-10;S.VisualStyle=1;E->ProcessEvent(E->FindFunctionChecked(TEXT("OnRep_HitFeedback")),nullptr);
 TestEqual(TEXT("Stale relevant snapshot does not show an old impact"),F.Active(),1);
 Role->SetPropertyValue_InContainer(E,ROLE_Authority);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponImpactMixedPool,"Rogue.Presentation.WeaponImpact.MixedClassesNeverStarveBehindIdlePoolEntries",RogueWeaponImpactTests::Flags)
bool FWeaponImpactMixedPool::RunTest(const FString&)
{
 using namespace RogueWeaponImpactTests;FFixture F;if(!F.Init())return false;
 auto* Pool=F.W->GetSubsystem<URogueCombatPoolSubsystem>();FRogueCombatAccentSettings S;S.Duration=.05f;
 auto Fill=[&](const TCHAR* Name,int Count)
 {
  S.EffectClass=LoadClass<ARogueCombatAccentEffect>(nullptr,Name);
  for(int I=0;I<Count;++I)if(!Pool->PlayCombatAccent(S,FVector(0,0,300),FVector::UpVector,true))return false;
  for(int I=0;I<8;++I)F.Test.TickTestWorld(.02f);return true;
 };
 TestTrue(TEXT("First class fills 48 entries"),Fill(TEXT("/Game/BP/Effects/WeaponImpact/BP_ImpactFrost.BP_ImpactFrost_C"),48));
 TestTrue(TEXT("Second class fills remaining idle capacity"),Fill(TEXT("/Game/BP/Effects/WeaponImpact/BP_ImpactEmber.BP_ImpactEmber_C"),16));
 TestTrue(TEXT("A third class can reclaim idle entries"),Fill(TEXT("/Game/BP/Effects/WeaponImpact/BP_ImpactSilver.BP_ImpactSilver_C"),12));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastBlastMaterialReuse,"Rogue.Presentation.WeaponImpact.CastBlastProgressResetsOnPoolReuse",RogueWeaponImpactTests::Flags)
bool FCastBlastMaterialReuse::RunTest(const FString&)
{
 using namespace RogueWeaponImpactTests;FFixture F;if(!F.Init())return false;
 auto* Pool=F.W->GetSubsystem<URogueCombatPoolSubsystem>();
 for(const TCHAR* Name:{TEXT("CastFrost"),TEXT("CastEmber"),TEXT("CastGlacier"),TEXT("CastStar"),TEXT("CastFlame"),TEXT("CastSilver"),TEXT("CastOfuda"),TEXT("BlastEmber"),TEXT("BlastGlacier")})
 {
  FRogueCombatAccentSettings S;S.Duration=.2f;
  const FString Path=FString::Printf(TEXT("/Game/BP/Effects/WeaponCast/BP_%s.BP_%s_C"),Name,Name);
  S.EffectClass=LoadClass<ARogueCombatAccentEffect>(nullptr,*Path);
  if(!TestNotNull(*Path,S.EffectClass.Get()))return false;
  auto* A=Pool->PlayCombatAccent(S,FVector(0,0,300),FVector::UpVector,FCString::Strncmp(Name,TEXT("Blast"),5)==0);
  if(!TestNotNull(TEXT("Saved visual class acquires a real pooled effect"),A))return false;
  auto* Material=Cast<UMaterialInstanceDynamic>(A->Core->GetMaterial(0));
  if(!TestNotNull(TEXT("Saved material creates its pooled instance"),Material))return false;
  float Progress=-1;
  TestTrue(TEXT("Procedural material accepts the shared animation clock"),Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Progress")),Progress));
  TestEqual(TEXT("First activation starts at zero"),Progress,0.f);
  A->Tick(.08f);Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Progress")),Progress);
  TestTrue(*FString::Printf(TEXT("Visible animation advances before expiry (%.6f / 0.400000)"),Progress),FMath::IsNearlyEqual(Progress,.4f,1.e-4f));
  A->Tick(.2f);TestTrue(TEXT("Completed animation returns to pool"),A->IsAvailableInPool());
  auto* Reused=Pool->PlayCombatAccent(S,FVector(0,0,300),FVector::UpVector,false);
  TestTrue(TEXT("Reuse preserves the actor and its material allocation"),Reused==A&&Reused->Core->GetMaterial(0)==Material);
  Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Progress")),Progress);
  TestEqual(TEXT("Pooled animation does not resume the previous fade"),Progress,0.f);
  TestFalse(TEXT("Visual does not add collision"),Reused->GetActorEnableCollision());
  Reused->Tick(.3f);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundFadeSnapshotTest,"Rogue.Gameplay.Visuals.GroundFadeSnapshotSurvivesPoolReuse",RogueWeaponImpactTests::Flags)
bool FGroundFadeSnapshotTest::RunTest(const FString&)
{
    RogueWeaponImpactTests::FFixture F;
    if (!F.Init()) return false;
    auto* Source=F.W->SpawnActor<AActor>();
    auto* Mesh=NewObject<UStaticMeshComponent>(Source);
    Source->SetRootComponent(Mesh);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GameAssets/ArtDirection/BossSkills/Materials/M_BossFX_Ground.M_BossFX_Ground"));
    if (!TestNotNull(TEXT("The real ground material is available"),Parent)) return false;
    auto* SourceMaterial=UMaterialInstanceDynamic::Create(Parent,Source);
    SourceMaterial->SetScalarParameterValue(TEXT("Opacity"),.8f);
    SourceMaterial->SetScalarParameterValue(TEXT("EffectAge"),.4f);
    Mesh->SetMaterial(0,SourceMaterial);
    Mesh->SetCustomPrimitiveDataFloat(3,.37f);
    Mesh->RegisterComponent();
    Source->SetActorLocation(FVector(1200,350,9));
    Source->SetActorScale3D(FVector(3,2,1));
    const FTransform OriginalTransform=Mesh->GetComponentTransform();
    auto* Fade=ARogueGroundEffectFade::CaptureActor(Source);
    if (!TestNotNull(TEXT("An ending effect has a local fade snapshot"),Fade)) return false;
    TestFalse(TEXT("The snapshot adds no network actor"),Fade->GetIsReplicated());
    TestFalse(TEXT("The snapshot adds no collision or damage"),Fade->GetActorEnableCollision());
    auto* SnapshotMesh=Fade->FindComponentByClass<UStaticMeshComponent>();
    if (!TestNotNull(TEXT("The ground mesh was captured"),SnapshotMesh)) return false;
    auto* SnapshotMaterial=Cast<UMaterialInstanceDynamic>(SnapshotMesh->GetMaterial(0));
    if (!TestNotNull(TEXT("The snapshot owns independent material parameters"),SnapshotMaterial)) return false;
    TestNearlyEqual(TEXT("Primitive data survives capture"),SnapshotMesh->GetCustomPrimitiveData().Data[3],.37f,.001f);

    Source->SetActorHiddenInGame(true);
    Source->SetActorLocation(FVector(0,0,-20000));
    SourceMaterial->SetScalarParameterValue(TEXT("Opacity"),.05f);
    SourceMaterial->SetScalarParameterValue(TEXT("EffectAge"),99.f);
    float Opacity=0,Age=0,FadeValue=0;
    SnapshotMaterial->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity")),Opacity);
    SnapshotMaterial->GetScalarParameterValue(FMaterialParameterInfo(TEXT("EffectAge")),Age);
    TestNearlyEqual(TEXT("Pool reuse cannot change the previous snapshot opacity"),Opacity,.8f,.001f);
    TestNearlyEqual(TEXT("Pool reuse cannot restart the previous animation"),Age,.4f,.001f);
    TestTrue(TEXT("The snapshot stays at the original ground position"),SnapshotMesh->GetComponentTransform().Equals(OriginalTransform,.001f));
    TestNull(TEXT("Repeated hiding does not create duplicate fade actors"),ARogueGroundEffectFade::CaptureActor(Source));
    Fade->Tick(.3f);
    SnapshotMaterial->GetScalarParameterValue(FMaterialParameterInfo(TEXT("GroundFade")),FadeValue);
    TestTrue(TEXT("The midpoint fades gradually instead of disappearing"),FadeValue>.3f && FadeValue<.7f);
    Fade->Tick(.31f);
    TestTrue(TEXT("The finished snapshot is removed"),Fade->IsActorBeingDestroyed());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundFadeHiddenEnemyTest,"Rogue.Gameplay.Visuals.GroundFadeBeforeEnemyHidden",RogueWeaponImpactTests::Flags)
bool FGroundFadeHiddenEnemyTest::RunTest(const FString&)
{
    RogueWeaponImpactTests::FFixture F;
    if (!F.Init()) return false;
    auto* Enemy=F.W->SpawnActor<ARogueEnemy>();
    Enemy->ActivatePooledEnemy(nullptr,FVector(500,0,90),FRotator::ZeroRotator);
    auto* Hazard=Enemy->FindComponentByClass<URogueEnemyStationaryHazardComponent>();
    if (!TestNotNull(TEXT("The enemy has its ground hazard component"),Hazard)) return false;
    auto* Mesh=NewObject<UStaticMeshComponent>(Enemy);
    Mesh->SetupAttachment(Enemy->GetRootComponent());
    Mesh->ComponentTags.Add(Hazard->AreaMeshTag);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GameAssets/ArtDirection/BossSkills/Materials/M_BossFX_Ground.M_BossFX_Ground")));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    Hazard->InitializeRuntime();
    Enemy->SetActorHiddenInGame(true); // 和客户端 PostNetReceive 相同的先隐藏顺序。
    Hazard->ResetForPool();           // 后到的在场位通知不能再生成第二份残影。
    int32 Count=0;
    for (TActorIterator<ARogueGroundEffectFade> It(F.W);It;++It)
    {
        ++Count;
        auto* Copy=It->FindComponentByClass<UStaticMeshComponent>();
        TestTrue(TEXT("The ground fade keeps its pre-pool position"),Copy && Copy->GetComponentLocation().Equals(Mesh->GetComponentLocation(),.01f));
    }
    TestEqual(TEXT("Hiding before the pool notification creates exactly one fade"),Count,1);
    return true;
}
#endif
