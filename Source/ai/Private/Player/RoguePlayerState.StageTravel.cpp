#include "Player/RoguePlayerState.h"
#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

void ARoguePlayerState::PrepareStageTravel()
{
    if (!HasAuthority() || !IsBaseAttributesInitialized()) { return; }
    if (const ARogueCharacter* Character = Cast<ARogueCharacter>(GetPawn()))
    {
        Character->CaptureStageGrowth(StageGrowth);
        bStageTravelPrepared = true;
    }
}

void ARoguePlayerState::CancelStageTravel()
{
    bStageTravelPrepared = false;
    bStageGrowthPending = false;
    StageGrowth = FRogueStagePlayerSnapshot();
}

void ARoguePlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);
    ARoguePlayerState* New = Cast<ARoguePlayerState>(NewPlayerState);
    if (!HasAuthority() || !New || (!bStageTravelPrepared && !bStageGrowthPending)) { return; }
    New->StageGrowth = StageGrowth;
    New->bStageGrowthPending = true;
    New->ProgressionSnapshot = ProgressionSnapshot;
    New->bProgressionReady = bProgressionReady;
    New->bProgressionSubmitted = true;
    New->BossIceBricksEarnedThisRun = BossIceBricksEarnedThisRun;
    New->GrantedBossRewardIds = GrantedBossRewardIds;
    New->PendingUpgrades = PendingUpgrades;
    New->bAwaitingUpgradeChoice = bAwaitingUpgradeChoice;
    New->QueuedUpgradeSelections = QueuedUpgradeSelections;
    New->ShopRuntime = ShopRuntime;
    New->ShopRuntime.CloseShop();
    New->ShopOffers = ShopOffers;
    New->ShopRefreshCost = ShopRefreshCost;
    // 基础属性、能力及回复效果在新世界正常初始化；随后只恢复一次已结算成长。
    New->bBaseAttributesInitialized = false;
}

void ARoguePlayerState::RestoreStageTravelGrowth(ARogueCharacter* Character)
{
    if (!HasAuthority() || !bStageGrowthPending || !bBaseAttributesInitialized || !Character) { return; }
    bStageGrowthPending = false;
    Character->RestoreStageGrowth(StageGrowth);
    StageGrowth = FRogueStagePlayerSnapshot();
    ForceNetUpdate();
}

void ARogueCharacter::CaptureStageGrowth(FRogueStagePlayerSnapshot& Out) const
{
    Out = FRogueStagePlayerSnapshot();
    const URogueAttributeSet* Set = GetRogueAttributeSet();
    if (!Set) { return; }
    for (TFieldIterator<FStructProperty> It(URogueAttributeSet::StaticClass()); It; ++It)
    {
        if (It->Struct == FGameplayAttributeData::StaticStruct() && It->GetFName() != TEXT("Damage"))
        {
            const FGameplayAttribute Attribute(*It);
            Out.Attributes.Add(It->GetFName(), Attribute.GetNumericValue(Set));
        }
    }
    Out.Level = PlayerLevel;
    Out.Experience = CurrentExperience;
    Out.NextLevelExperience = ExperienceToNextLevel;
    Out.Money = Money;
    Out.ExperienceRemainder = ExperienceRemainder;
    Out.bDowned = IsDowned();
    Out.ReviveRemaining = GetReviveSecondsRemaining();
}

void ARogueCharacter::RestoreStageGrowth(const FRogueStagePlayerSnapshot& Snapshot)
{
    URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
    if (!IsAuthoritySide() || !ASC) { return; }
    // 先恢复上限及其他属性，最后恢复生命/护甲，避免上限变化的补血委托污染保存值。
    auto Restore = [ASC, &Snapshot](FName Name)
    {
        FProperty* Property = FindFProperty<FProperty>(URogueAttributeSet::StaticClass(), Name);
        const float* Value = Snapshot.Attributes.Find(Name);
        if (Property && Value && FMath::IsFinite(*Value)) { ASC->SetNumericAttributeBase(FGameplayAttribute(Property), *Value); }
    };
    if (Snapshot.bDowned)
    {
        LifeState.Phase = ERoguePlayerLifePhase::Downed;
        LifeState.ReviveServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(.01f, Snapshot.ReviveRemaining);
    }
    for (const auto& Pair : Snapshot.Attributes)
    {
        if (Pair.Key != TEXT("Health") && Pair.Key != TEXT("Armor")) { Restore(Pair.Key); }
    }
    Restore(TEXT("Health"));
    Restore(TEXT("Armor"));
    PlayerLevel = Snapshot.Level;
    CurrentExperience = Snapshot.Experience;
    ExperienceToNextLevel = Snapshot.NextLevelExperience;
    Money = Snapshot.Money;
    ExperienceRemainder = Snapshot.ExperienceRemainder;
    bPendingDeath = false;
    GetWorldTimerManager().ClearTimer(PendingDeathTimerHandle);
    if (Snapshot.bDowned) { StartReviveTimer(); }
    RefreshWeaponBonusSnapshotFromAttributes();
    ForceNetUpdate();
    UE_LOG(LogTemp, Log, TEXT("[RogueStage] Restored %s Level=%d Money=%d Attributes=%d"),
        *GetNameSafe(GetPlayerState()), PlayerLevel, Money, Snapshot.Attributes.Num());
}
