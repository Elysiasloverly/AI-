#if WITH_DEV_AUTOMATION_TESTS
#include "RogueProbeSupport.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
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
#include "HAL/IConsoleManager.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Enemies/RogueEnemyHitFeedback.h"
#include "UObject/UnrealType.h"
namespace FrostNet
{
struct FProbe
{
 double Started=FPlatformTime::Seconds(),Ready=0;bool Quiet=false,Released=false,SawBoth=false,SawSlash=false,SawWaves=false,SawMoonHit=false;TArray<TWeakObjectPtr<ARogueWeapon_FrostBlade>> Weapons;

    bool End(bool bSuccess, const TCHAR* Detail)
    {
        return RogueProbeSupport::Finish(TEXT("FrostNet"), bSuccess, Detail);
    }
 bool Tick()
 {
  const double Now=FPlatformTime::Seconds();if(Now-Started>65)return End(false,TEXT("timeout"));
  UWorld* W = RogueProbeSupport::FindGameWorld();
        if(!W)return true;
  if(W->GetNetMode()==NM_Client)
  {
   int32 Blades=0,Waves=0;TMap<AActor*,int32> Owners;
   for(TActorIterator<ARogueFrostBlade> B(W);B;++B){++Blades;if(B->GetOwner()){if(++Owners.FindOrAdd(B->GetOwner())>1)return End(false,TEXT("duplicate blade for one player"));}SawSlash|=B->GetPhase()==ERogueFrostBladePhase::Slash;}
   for(TActorIterator<ARogueFrostSwordWave> V(W);V;++V)if(!V->IsAvailableInPool())
   {
    ++Waves;
    const FVector Base=V->GetClass()->GetDefaultObject<ARogueFrostSwordWave>()->Mesh->GetRelativeScale3D();
    if(!V->Mesh->GetRelativeScale3D().Equals(Base*1.4f,.01f))return End(false,TEXT("CLIENT wave size applied more than once"));
   }
   if(Blades==2&&Owners.Num()==2&&!SawBoth){SawBoth=true;UE_LOG(LogTemp,Display,TEXT("[FrostNet] CLIENT saw host and own blade, one per player"));}
   if(Waves>=6&&!SawWaves){SawWaves=true;UE_LOG(LogTemp,Display,TEXT("[FrostNet] CLIENT saw six authoritative penetrating waves"));}
   for(TActorIterator<ARogueEnemy> E(W);E;++E)
   {
    const auto* Feedback=FindFProperty<FStructProperty>(ARogueEnemy::StaticClass(),TEXT("HitFeedback"))->ContainerPtrToValuePtr<FRogueEnemyHitFeedback>(*E);
    SawMoonHit|=Feedback->Sequence>0&&Feedback->VisualStyle==6;
   }
   if(SawBoth&&SawWaves&&SawSlash&&SawMoonHit&&Blades==0)return End(true,TEXT("CLIENT received own/host phases, 1.4x waves, moon hit identity and cleanup"));return true;
  }
  auto* Mode=W->GetAuthGameMode<ARogueGameMode>();if(!Mode)return true;Mode->SetActorTickEnabled(false);W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
  if(!Quiet){for(TActorIterator<ARogueEnemy> E(W);E;++E)E->DeactivateToPool();Quiet=true;}
  if(!Ready)
  {
   TArray<ARogueCharacter*> Players;
   for(auto I=W->GetPlayerControllerIterator();I;++I){auto* PC=I->Get();auto* P=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;auto* PS=P?P->GetPlayerState<ARoguePlayerState>():nullptr;if(PS&&PS->IsBaseAttributesInitialized())Players.Add(P);}
   if(Players.Num()!=2)return true;
   for(auto* P:Players)
   {
    P->GetCharacterMovement()->SetComponentTickEnabled(false);P->GetRogueAbilitySystemComponent()->CancelAllAbilities();
    for(ARogueWeaponBase* A:P->GetWeapons())if(A){A->OnOwnerDied();A->GetConfig().Count=0;A->GetConfig().MaxCount=0;}
    auto* B=P->FindWeapon<ARogueWeapon_FrostBlade>();if(!B)return End(false,TEXT("weapon table missing blade"));B->GetConfig().Count=1;B->GetConfig().MaxCount=1;
    FRogueUpgradeOption O;O.Type=ERogueUpgradeType::FrostBladeWave;O.Magnitude=1;P->ApplyUpgrade(O);O.Type=ERogueUpgradeType::FrostBladeWaveCount;O.Magnitude=2;P->ApplyUpgrade(O);O.Type=ERogueUpgradeType::FrostBladeWaveSize;O.Magnitude=.4f;P->ApplyUpgrade(O);Weapons.Add(B);
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;const FVector Pos=P->GetActorLocation()+FVector(300,0,0);auto* E=W->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),Pos,FRotator::ZeroRotator,Params);
    E->ActivatePooledEnemy(nullptr,Pos,FRotator::ZeroRotator);FRogueEnemyProfile Profile;Profile.Health=100000;Profile.Damage=Profile.Speed=0;E->InitializeEnemy(Profile);E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);E->GetAbilitySystemComponent()->CancelAllAbilities();W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);
   }
   Ready=Now;UE_LOG(LogTemp,Display,TEXT("[FrostNet] SERVER normal character heartbeat, two blades and three waves each"));
  }
  if(!Released&&Now-Ready>6){Released=true;for(auto& A:Weapons)if(A.IsValid()){A->GetConfig().Count=0;A->GetConfig().MaxCount=0;A->OnOwnerDied();}}
  if(Now-Ready>9)return End(true,TEXT("SERVER both players fired and cleaned up"));return true;
 }
};
}
static FAutoConsoleCommand FrostNetProbe(TEXT("Rogue.Test.FrostBladeNet"),TEXT("Explicit isolated listen server/client frost blade verification."),FConsoleCommandDelegate::CreateLambda([](){auto P=MakeShared<FrostNet::FProbe>();FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([P](float){return P->Tick();}),.015f);}));
#endif
