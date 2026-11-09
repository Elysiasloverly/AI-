#include "Combat/Visuals/RogueDefeatFeedbackSubsystem.h"
#include "Combat/Visuals/RogueEnemyDefeatEffect.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
int32 URogueDefeatFeedbackSubsystem::GetActiveCount()const
{int32 N=0;for(const auto& A:Pool)if(IsValid(A)&&!A->IsAvailableInPool())++N;return N;}
ARogueEnemyDefeatEffect* URogueDefeatFeedbackSubsystem::Play(TSubclassOf<ARogueEnemyDefeatEffect> Class,FVector Location,bool bBoss,float Size)
{
    UWorld* W=GetWorld();if(!W || W->GetNetMode()==NM_DedicatedServer || !Class || Location.ContainsNaN())return nullptr;
    bool HasCamera=false,NearCamera=false;
    for(auto It=W->GetPlayerControllerIterator();It;++It)
        if(auto* PC=It->Get();PC&&PC->IsLocalController()&&PC->PlayerCameraManager)
        {HasCamera=true;NearCamera|=FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(),Location)<FMath::Square(6000.f);}
    if(HasCamera&&!NearCamera)return nullptr;
    Pool.RemoveAll([](const auto& A){return !IsValid(A);});const auto* Tracker=W->GetSubsystem<URogueEnemyTrackerSubsystem>();
    const int32 Limit=(Tracker&&Tracker->ShouldCullCombatEffects()?10:24)+(bBoss?4:0);if(GetActiveCount()>=Limit)return nullptr;
    ARogueEnemyDefeatEffect* Effect=nullptr;
    for(const auto& A:Pool)if(A->IsAvailableInPool()&&A->GetClass()==Class){Effect=A;break;}
    if(!Effect)
    {
        if(Pool.Num()>=32)return nullptr;
        FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Effect=W->SpawnActor<ARogueEnemyDefeatEffect>(Class,Location,FRotator::ZeroRotator,P);if(!Effect)return nullptr;Pool.Add(Effect);
    }
    Effect->ActivateDefeat(Location,bBoss,Size);if(!Effect->IsAvailableInPool())++PlayedCount;return Effect;
}
