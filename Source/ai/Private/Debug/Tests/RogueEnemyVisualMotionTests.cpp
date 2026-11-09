#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/RogueGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueEnemyPresentationComponent.h"
namespace RogueMotionTests
{
 constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
 struct FFixture
 {
  FTestWorldWrapper Test;UWorld* World=nullptr;ARogueEnemy* Enemy=nullptr;URogueEnemyPresentationComponent* Motion=nullptr;
  bool Init()
  {
   if(!Test.CreateTestWorld(EWorldType::Game))return false;World=Test.GetTestWorld();
   World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
   if(!Test.BeginPlayInTestWorld())return false;
   World->GetAuthGameMode()->SetActorTickEnabled(false);
   Enemy=World->SpawnActor<ARogueEnemy>();Enemy->ActivatePooledEnemy(nullptr,FVector(200,100,300),FRotator(0,35,0));
   FRogueEnemyProfile P;P.Health=1000;P.Speed=240;Enemy->InitializeEnemy(P);
   Enemy->SetActorTickEnabled(false);Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
   Motion=Enemy->FindComponentByClass<URogueEnemyPresentationComponent>();Motion->ResetForPool();return true;
  }
  UStaticMeshComponent* Mesh(USceneComponent* Parent,FName Tag=NAME_None)
  {
   auto* M=NewObject<UStaticMeshComponent>(Enemy);M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   M->SetupAttachment(Parent);M->SetRelativeLocation(FVector(8,14,-30));M->SetRelativeRotation(FRotator(0,-90,0));
   M->SetRelativeScale3D(FVector(.4,.6,.9));M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   if(!Tag.IsNone())M->ComponentTags.Add(Tag);M->RegisterComponent();return M;
  }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotionPoolReset,"Rogue.Presentation.EnemyMotion.AuthoredPoseAndCollisionSurvivePool",RogueMotionTests::Flags)
bool FMotionPoolReset::RunTest(const FString&)
{
 RogueMotionTests::FFixture F;if(!F.Init())return false;
 auto* M=F.Mesh(F.Enemy->GetRootComponent());auto* Area=F.Mesh(F.Enemy->GetRootComponent(),TEXT("Rogue.DamageArea"));
 const FTransform Base=M->GetRelativeTransform(),AreaBase=Area->GetRelativeTransform(),Actor=F.Enemy->GetActorTransform();
 const float Radius=F.Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
 F.Motion->RefreshRuntime();F.Enemy->GetCharacterMovement()->Velocity=F.Enemy->GetActorForwardVector()*240;
 F.Motion->PlayAttackCue();F.Motion->PlayHitCue();F.Motion->TickComponent(.05f,LEVELTICK_All,nullptr);
 TestFalse(TEXT("A moving, attacking static body gains a visual pose"),M->GetRelativeTransform().Equals(Base,.001f));
 TestTrue(TEXT("Visual pose never moves the enemy actor"),F.Enemy->GetActorTransform().Equals(Actor));
 TestEqual(TEXT("Collision radius is untouched"),F.Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius(),Radius);
 TestTrue(TEXT("Blueprint model scale is untouched"),M->GetRelativeScale3D().Equals(Base.GetScale3D()));
 TestTrue(TEXT("Damage area mesh stays fixed"),Area->GetRelativeTransform().Equals(AreaBase));
 F.Enemy->DeactivateToPool();
 TestTrue(TEXT("Pool recycle restores the authored body pose"),M->GetRelativeTransform().Equals(Base,.001f));
 TestFalse(TEXT("Pooled presentation does not tick"),F.Motion->IsComponentTickEnabled());
 F.Motion->PlayAttackCue();F.Motion->PlayHitCue();
 TestTrue(TEXT("Late feedback cannot displace a pooled body"),M->GetRelativeTransform().Equals(Base,.001f));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotionAttachment,"Rogue.Presentation.EnemyMotion.ChildrenFollowParentOnce",RogueMotionTests::Flags)
bool FMotionAttachment::RunTest(const FString&)
{
 RogueMotionTests::FFixture F;if(!F.Init())return false;
 auto* Parent=F.Mesh(F.Enemy->GetRootComponent());auto* Child=F.Mesh(Parent);
 const FTransform ChildBase=Child->GetRelativeTransform(),ParentBase=Parent->GetRelativeTransform();
 F.Motion->RefreshRuntime();F.Motion->PlayAttackCue();F.Motion->TickComponent(.1f,LEVELTICK_All,nullptr);
 TestFalse(TEXT("Parent receives the recoil"),Parent->GetRelativeTransform().Equals(ParentBase,.001f));
 TestTrue(TEXT("Attached accessory receives no second local recoil"),Child->GetRelativeTransform().Equals(ChildBase));
 F.Motion->bEnableVisualMotion=false;F.Motion->TickComponent(.05f,LEVELTICK_All,nullptr);
 TestTrue(TEXT("Disabling motion restores original parent pose"),Parent->GetRelativeTransform().Equals(ParentBase,.001f));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotionSustainedHits,"Rogue.Presentation.EnemyMotion.RepeatedCuesStayBounded",RogueMotionTests::Flags)
bool FMotionSustainedHits::RunTest(const FString&)
{
 RogueMotionTests::FFixture F;if(!F.Init())return false;
 auto* M=F.Mesh(F.Enemy->GetRootComponent());const FTransform Base=M->GetRelativeTransform();F.Motion->RefreshRuntime();
 for(int I=0;I<100;++I)
 {
  F.Motion->PlayHitCue();F.Motion->PlayAttackCue();F.Motion->TickComponent(.05f,LEVELTICK_All,nullptr);
  TestTrue(TEXT("Continuous hits never accumulate translation"),FVector::Dist(M->GetRelativeLocation(),Base.GetLocation())<10.f);
 }
 for(int I=0;I<12;++I)F.Motion->TickComponent(.05f,LEVELTICK_All,nullptr);
 TestTrue(TEXT("Finite cues settle back to authored pose"),M->GetRelativeTransform().Equals(Base,.001f));
 return true;
}
#endif
