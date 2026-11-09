#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/Visuals/RogueEnemyDefeatEffect.h"
#include "Framework/RogueGameState.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
// Explicit opt-in profiling load: cosmetic snapshots only, never kills actors or changes saves.
static FAutoConsoleCommand DefeatLoadProbe(TEXT("Rogue.Test.DefeatLoad"),TEXT("Emit 16 cosmetic kill snapshots every 150ms for 30 seconds for rendered VFX profiling."),FConsoleCommandDelegate::CreateLambda([]
{
    auto Last=MakeShared<double>(-1);auto Started=MakeShared<double>(FPlatformTime::Seconds());auto Cycle=MakeShared<int32>(0);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Last,Started,Cycle](float)
    {
        if(FPlatformTime::Seconds()-*Started>30)return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()){W=C.World();break;}
        if(!W||W->GetNetMode()==NM_Client)return true;auto* S=W->GetGameState<ARogueGameState>();auto* PC=W->GetFirstPlayerController();auto* P=PC?PC->GetPawn():nullptr;if(!S||!P)return true;
        const double Now=W->GetTimeSeconds();if(Now-*Last<.15)return true;*Last=Now;
        auto* Class=LoadClass<ARogueEnemyDefeatEffect>(nullptr,TEXT("/Game/BP/Effects/BP_EnemyDefeat.BP_EnemyDefeat_C"));
        for(int I=0;I<16;++I)
        {
            const float A=(I+*Cycle*.21f)*UE_TWO_PI/16;FRogueDefeatVisualEvent V;V.EffectClass=Class;V.CreditedPlayer=PC->PlayerState;
            V.Location=P->GetActorLocation()+FVector(FMath::Cos(A)*650,FMath::Sin(A)*650,30);V.bBoss=(I==0&&*Cycle%8==0);S->QueueDefeatFeedback(V);
        }
        ++*Cycle;return true;
    }),.02f);
}));
#endif
