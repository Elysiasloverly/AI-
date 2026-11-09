#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Combat/Weapons/RogueWeapon_FrostBlade.h"
#include "Combat/Projectiles/RogueFrostBlade.h"
#include "Combat/Projectiles/RogueFrostSwordWave.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
namespace FrostVisual
{
struct FProbe
{
 FString Folder;double Started=FPlatformTime::Seconds(),Ready=0,SlashSeen=0;TWeakObjectPtr<ARogueCharacter> Player;TWeakObjectPtr<ARogueWeapon_FrostBlade> Weapon;
 bool HoverShot=false,SlashShot=false,WaveShot=false,Spawned=false,CloseReady=false,CloseShot=false;int32 PeakWaves=0;
 void Shot(UWorld* W,const TCHAR* Name){W->GetFirstPlayerController()->ConsoleCommand(FString::Printf(TEXT("Shot SHOWUI filename=%s -nosuffix"),*FPaths::Combine(Folder,Name)),false);}
 bool Tick()
 {
  double Now=FPlatformTime::Seconds();if(Now-Started>55){UE_LOG(LogTemp,Error,TEXT("[FrostVisual] FAIL timeout"));FPlatformMisc::RequestExit(false);return false;}
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()&&C.World()->GetAuthGameMode<ARogueGameMode>()){W=C.World();break;}if(!W)return true;
  if(!Ready)
  {
   auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;auto* PS=P?P->GetPlayerState<ARoguePlayerState>():nullptr;if(!PS||!PS->IsBaseAttributesInitialized())return true;
   auto* B=P->FindWeapon<ARogueWeapon_FrostBlade>();if(!B)return true;
   W->GetAuthGameMode()->SetActorTickEnabled(false);W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
   for(TActorIterator<ARogueEnemy> E(W);E;++E)E->DeactivateToPool();
   for(ARogueWeaponBase* A:P->GetWeapons())if(A){A->OnOwnerDied();A->GetConfig().Count=0;A->GetConfig().MaxCount=0;}
   B->GetConfig().Count=1;B->GetConfig().MaxCount=1;B->GetConfig().Range=0;
   FRogueUpgradeOption O;O.Type=ERogueUpgradeType::FrostBladeWave;O.Magnitude=1;P->ApplyUpgrade(O);O.Type=ERogueUpgradeType::FrostBladeWaveCount;O.Magnitude=2;P->ApplyUpgrade(O);
   Player=P;Weapon=B;Ready=Now;UE_LOG(LogTemp,Display,TEXT("[FrostVisual] READY class=%s wave=%s count=%d"),*GetNameSafe(B->GetConfig().FrostBladeClass),*GetNameSafe(B->GetConfig().FrostWaveClass),B->GetEffectiveWaveCount());
  }
  const double Age=Now-Ready;auto* B=Weapon->GetBlade();if(!B)return true;
  if(!HoverShot&&Age>5){Shot(W,TEXT("01-hover.png"));HoverShot=true;UE_LOG(LogTemp,Display,TEXT("[FrostVisual] HOVER tipZ=%.2f location=%s mesh=%s"),B->GetActorForwardVector().Z,*B->GetActorLocation().ToString(),*GetNameSafe(B->Mesh->GetStaticMesh()));}
  if(!Spawned&&Age>2.2)
  {
   Spawned=true;UClass* Cl=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Assault/BP_EnemyAssault.BP_EnemyAssault_C"));
   for(int32 I=0;I<3;++I)
   {
    const FVector Pos=Player->GetActorLocation()+FVector(300+I*45,(I-1)*90,0);FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* E=W->SpawnActor<ARogueEnemy>(Cl,Pos,FRotator::ZeroRotator,Params);E->ActivatePooledEnemy(nullptr,Pos,FRotator::ZeroRotator);FRogueEnemyProfile Profile;Profile.Health=100000;Profile.Speed=0;Profile.Damage=0;Profile.ExperienceReward=0;
    E->InitializeEnemy(Profile);E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);E->GetAbilitySystemComponent()->CancelAllAbilities();W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);
   }
  }
  if(Age>6&&!CloseReady)Weapon->GetConfig().Range=450;
  if(Age>7.7&&B->GetPhase()==ERogueFrostBladePhase::Slash&&!SlashSeen)SlashSeen=Now;
  if(SlashSeen&&!SlashShot&&Now-SlashSeen>.12){Shot(W,TEXT("02-slash.png"));SlashShot=true;}
  int32 Waves=0;for(TActorIterator<ARogueFrostSwordWave> I(W);I;++I)if(!I->IsAvailableInPool())++Waves;PeakWaves=FMath::Max(PeakWaves,Waves);
  if(SlashSeen&&!WaveShot&&Now-SlashSeen>.65){Shot(W,TEXT("03-wave.png"));WaveShot=true;}
  if(Age>12&&!CloseReady)
  {
   CloseReady=true;Weapon->GetConfig().Range=0;
   for(TActorIterator<ARogueEnemy> E(W);E;++E)E->DeactivateToPool();
   for(TActorIterator<ARogueFrostSwordWave> V(W);V;++V)V->Deactivate();
   auto* Camera=W->SpawnActor<ACameraActor>();const FVector Center=Player->GetActorLocation()+FVector(-105,30,60);
   const FVector Position=Center+FVector(350,500,380);Camera->SetActorLocation(Position);Camera->SetActorRotation((Center-Position).Rotation());Camera->GetCameraComponent()->FieldOfView=38;
   W->GetFirstPlayerController()->SetViewTarget(Camera);
  }
  if(Age>13.5&&!CloseShot){Shot(W,TEXT("04-model-detail.png"));CloseShot=true;}
  if(Age>15)
  {
   UE_LOG(LogTemp,Display,TEXT("[FrostVisual] %s actual character heartbeat; peak waves=%d"),PeakWaves>=3&&HoverShot&&SlashShot&&WaveShot?TEXT("PASS"):TEXT("FAIL"),PeakWaves);FPlatformMisc::RequestExit(false);return false;
  }
  return true;
 }
};
}
static FAutoConsoleCommand FrostVisualProbe(TEXT("Rogue.Test.FrostBladeVisual"),TEXT("Explicit frost blade screenshot capture; output folder."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args){if(Args.Num()!=1)return;auto P=MakeShared<FrostVisual::FProbe>();P->Folder=Args[0];FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([P](float){return P->Tick();}),.01f);}));
#endif
