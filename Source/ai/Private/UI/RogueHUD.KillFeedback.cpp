#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "UI/Widgets/RogueKillFeedbackWidgetBase.h"
#include "GameFramework/PlayerController.h"
#include "Audio/RogueMusicSubsystem.h"
void ARogueHUD::NotifyEnemyDefeated(int32 Count,bool bBoss)
{
    if(bMainMenuMode || Count<=0)return;
    auto* PC=GetOwningPlayerController();if(!PC||!PC->IsLocalController())return;
    if(!KillFeedbackWidget && !KillFeedbackWidgetClassPath.IsEmpty())
        if(auto* Class=LoadClass<URogueKillFeedbackWidgetBase>(nullptr,*KillFeedbackWidgetClassPath))
        {KillFeedbackWidget=CreateWidget<URogueKillFeedbackWidgetBase>(PC,Class);if(KillFeedbackWidget)KillFeedbackWidget->AddToViewport(7);}
    if(KillFeedbackWidget){KillFeedbackWidget->SetBottomInset(OwnedItemsWidget?OwnedItemsWidget->GetOccupiedHeight():0);KillFeedbackWidget->RegisterKills(Count,bBoss);}
    if(auto* Audio=URogueMusicSubsystem::GetAudioSystem(this))Audio->PlayDefeatFeedback(KillFeedbackWidget?KillFeedbackWidget->GetStreakCount():Count,bBoss);
}
