#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
namespace RogueOwnedItemsNet
{
struct FProbe
{
    double Start=FPlatformTime::Seconds(),PickedAt=0,ReadyAt=0;bool bQuiet=false,bPicked=false;
    TWeakObjectPtr<ARogueCharacter> Host,Client;
    bool End(bool bOK,const TCHAR* Why)
    {
        if(bOK){UE_LOG(LogTemp,Display,TEXT("[OwnedItemsNet] PASS %s"),Why);}
        else{UE_LOG(LogTemp,Error,TEXT("[OwnedItemsNet] FAIL %s"),Why);}
        FPlatformMisc::RequestExitWithStatus(false,bOK?0:1);return false;
    }
    void Pick(ARogueCharacter* P,ERogueUpgradeType Type,int32 Times)
    {
        for(int32 I=0;I<Times;++I){FRogueUpgradeOption U;U.Type=Type;U.Magnitude=1;P->ApplyUpgrade(U);}
        P->RefreshWeaponBonusSnapshotFromAttributes();
    }
    bool ViewIsLocal(APlayerController* PC,bool bHost)
    {
        auto* HUD=PC?Cast<ARogueHUD>(PC->GetHUD()):nullptr;auto* Bar=HUD?HUD->GetOwnedItemsWidget():nullptr;
        if(!Bar)return false;
        const auto& Items=Bar->GetItems();
        auto Has=[&](ERogueUpgradeType T,int32 N){return Items.ContainsByPredicate([&](const auto& V){return V.Type==T&&!V.bWeapon&&V.Count==N;});};
        auto Any=[&](ERogueUpgradeType T){return Items.ContainsByPredicate([&](const auto& V){return V.Type==T;});};
        return bHost?(Has(ERogueUpgradeType::AttackPower,2)&&Has(ERogueUpgradeType::MoveSpeed,3)&&!Any(ERogueUpgradeType::Recovery)&&!Any(ERogueUpgradeType::AttackRange))
                    :(Has(ERogueUpgradeType::Recovery,2)&&Has(ERogueUpgradeType::AttackRange,3)&&!Any(ERogueUpgradeType::AttackPower)&&!Any(ERogueUpgradeType::MoveSpeed));
    }
    bool Tick()
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>65)return End(false,TEXT("timeout waiting for owner-only inventory and HUD"));
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()){W=C.World();break;}if(!W)return true;
        if(W->GetNetMode()==NM_Client)
        {
            auto* PC=W->GetFirstPlayerController();auto* PS=PC?PC->GetPlayerState<ARoguePlayerState>():nullptr;
            if(PS&&PS->GetUpgradePickCount(ERogueUpgradeType::Recovery)==2&&PS->GetUpgradePickCount(ERogueUpgradeType::AttackRange)==3&&ViewIsLocal(PC,false))
            {
                if(PS->GetUpgradePickCount(ERogueUpgradeType::AttackPower)||PS->GetUpgradePickCount(ERogueUpgradeType::MoveSpeed))return End(false,TEXT("client inherited host picks"));
                for(TActorIterator<ARoguePlayerState> It(W);It;++It)if(*It!=PS&&!It->GetUpgradePickViews().IsEmpty())return End(false,TEXT("remote pick mirror leaked to client"));
                if(!ReadyAt)ReadyAt=Now;
                if(Now-ReadyAt>4)return End(true,TEXT("CLIENT real replicated counts 2/3, own HUD only, remote history remains empty"));
            }
            return true;
        }
        auto* Mode=W->GetAuthGameMode<ARogueGameMode>();if(!Mode)return true;Mode->SetActorTickEnabled(false);
        if(auto* Spawns=W->GetSubsystem<URogueSpawnSubsystem>())Spawns->SetSpawningSuspended(true);
        if(!bQuiet){for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();bQuiet=true;}
        if(!bPicked)
        {
            int32 Players=0;
            for(auto It=W->GetPlayerControllerIterator();It;++It)if(auto* PC=It->Get())if(auto* P=Cast<ARogueCharacter>(PC->GetPawn()))
                if(auto* PS=P->GetPlayerState<ARoguePlayerState>();PS&&PS->IsBaseAttributesInitialized())
                {++Players;if(PC->IsLocalController())Host=P;else Client=P;}
            if(Players<2||!Host.IsValid()||!Client.IsValid())return true;
            for(auto P:{Host,Client})
            {
                P->SetActorTickEnabled(false);P->GetPlayerState<ARoguePlayerState>()->ResetUpgradeState();
                for(const auto& Weapon:P->GetWeapons())if(Weapon){Weapon->OnOwnerDied();Weapon->SetActorTickEnabled(false);}
            }
            Pick(Host.Get(),ERogueUpgradeType::AttackPower,2);Pick(Host.Get(),ERogueUpgradeType::MoveSpeed,3);Pick(Host.Get(),ERogueUpgradeType::RocketCount,1);
            Pick(Client.Get(),ERogueUpgradeType::Recovery,2);Pick(Client.Get(),ERogueUpgradeType::AttackRange,3);Pick(Client.Get(),ERogueUpgradeType::ScytheCount,1);
            bPicked=true;PickedAt=Now;
        }
        if(bPicked&&Now-PickedAt>10)
        {
            auto* PC=Cast<APlayerController>(Host->GetController());
            if(!ViewIsLocal(PC,true))return End(false,TEXT("host HUD does not match host picks"));
            return End(true,TEXT("HOST two independent player histories, local HUD 2/3, quantity cards merged"));
        }
        return true;
    }
};
}
static FAutoConsoleCommand OwnedItemsNetProbe(TEXT("Rogue.Test.OwnedItemsNet"),TEXT("Explicit isolated inventory replication/UI check; no asset writes; exits processes."),FConsoleCommandDelegate::CreateLambda([]
{auto Probe=MakeShared<RogueOwnedItemsNet::FProbe>();FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float){return Probe->Tick();}),.02f);}));
#endif
