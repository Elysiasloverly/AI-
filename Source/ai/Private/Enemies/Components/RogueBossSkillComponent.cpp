#include "Enemies/Components/RogueBossSkillComponent.h"
#include "Combat/Visuals/RogueBossCastSeal.h"
#include "AbilitySystem/Abilities/RogueBossSkillAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "Framework/RogueGameState.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Debug/RogueSelfTest.h"

FRogueBossSkillRow FRogueBossSkillRow::Sanitized() const
{
    auto R=*this;
    auto Safe=[](float V,float Default,float Min,float Max) { return FMath::Clamp(FMath::IsFinite(V)?V:Default,Min,Max); };
    R.InitialDelay=Safe(InitialDelay,3,0,3600); R.Cooldown=Safe(Cooldown,8,0,3600);
    R.Weight=Safe(Weight,1,0,10000);
    R.MinHealthFraction=Safe(MinHealthFraction,0,0,1); R.MaxHealthFraction=Safe(MaxHealthFraction,1,R.MinHealthFraction,1);
    R.ChargeSeconds=Safe(ChargeSeconds,1,0,600); R.RecoverySeconds=Safe(RecoverySeconds,1,0,600);
    R.PulseInterval=Safe(PulseInterval,.5f,.02f,60);
    R.DurationSeconds=Safe(DurationSeconds,10,0,600);
    // 未重存的旧数据仍能按原来的连发长度运行。
    if (PulseCount>0 || ReleaseHoldSeconds>0)
        R.DurationSeconds=Safe((FMath::Max(1,PulseCount)-1)*R.PulseInterval+ReleaseHoldSeconds,10,0,600);
    R.PulseCount=0; R.ReleaseHoldSeconds=0;
    R.MaxUses=FMath::Max(0,MaxUses);
    return R;
}

URogueBossSkillComponent::URogueBossSkillComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=false;
    PrimaryComponentTick.TickInterval=.1f;
}

float URogueBossSkillComponent::ServerTime() const
{
    const UWorld* W=GetWorld();
    const auto* GS=W?W->GetGameState<AGameStateBase>():nullptr;
    return GS?GS->GetServerWorldTimeSeconds():(W?W->GetTimeSeconds():0.f);
}

bool URogueBossSkillComponent::IsOwnerReady() const
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    const auto* GS=GetWorld()?GetWorld()->GetGameState<ARogueGameState>():nullptr;
    return E && E->HasAuthority() && E->GetNetMode()!=NM_Client && E->IsBoss() && !E->IsDead()
        && !E->IsAvailableInPool() && E->GetRogueAttributeSet() && E->GetRogueAttributeSet()->GetHealth()>0
        && (!GS || !GS->IsGameOver());
}

void URogueBossSkillComponent::InitializeRuntime()
{
    if (!GetOwner()->HasAuthority()) { RefreshPresentation(); return; }
    ResetForPool();
    // 普通个体不复制这套技能状态，也不运行额外的组件 Tick。
    SetIsReplicated(CastChecked<ARogueEnemy>(GetOwner())->IsBoss() && (SkillCatalog || !SkillSlots.IsEmpty()));
    if (!IsOwnerReady() || RogueSelfTest::AreEnemyAttackAbilitiesDisabled()) return;
    auto* ASC=CastChecked<ARogueEnemy>(GetOwner())->GetAbilitySystemComponent();
    if (!ASC) return;

    const auto* Mode=GetWorld()->GetAuthGameMode<ARogueGameMode>();
    const bool bAll=Mode && Mode->GetCurrentWave()>=FMath::Max(1,Selection.AllSkillsWave);
    const bool bFixed=!bAll && !SkillSlots.IsEmpty();
    const auto AddCandidate=[this](const FDataTableRowHandle& Row)
    {
        if (!Row.DataTable || Row.RowName.IsNone() || !Row.DataTable->GetRowStruct() || !Row.DataTable->GetRowStruct()->IsChildOf(FRogueBossSkillRow::StaticStruct())) return;
        if (RuntimeSlots.ContainsByPredicate([&](const auto& Slot){ return Slot.Row.DataTable==Row.DataTable && Slot.Row.RowName==Row.RowName; })) return;
        const auto* Data=Row.GetRow<FRogueBossSkillRow>(TEXT("Boss skill slots"));
        if (!Data || !Data->bEnabled || !Data->AbilityClass) return;
        auto& Slot=RuntimeSlots.AddDefaulted_GetRef(); Slot.Row=Row; Slot.Settings=Data->Sanitized();
    };
    if (bFixed || !SkillCatalog) for (const auto& Row:SkillSlots) AddCandidate(Row);
    else for (const FName Name:SkillCatalog->GetRowNames())
    {
        FDataTableRowHandle Row; Row.DataTable=SkillCatalog; Row.RowName=Name;
        AddCandidate(Row);
    }

    if (!bFixed)
    {
        const int32 Count=bAll ? RuntimeSlots.Num() : FMath::Clamp(Mode && Mode->IsNightBattleStage() ? Selection.NightCount : Selection.DayCount,0,RuntimeSlots.Num());
        for (int32 I=0; I<Count && I<RuntimeSlots.Num()-1 && Count<RuntimeSlots.Num(); ++I)
            RuntimeSlots.Swap(I,FMath::RandRange(I,RuntimeSlots.Num()-1));
        RuntimeSlots.SetNum(Count);
        RuntimeSlots.Sort([](const FRuntimeSlot& A,const FRuntimeSlot& B) { return A.Settings.DisplayOrder!=B.Settings.DisplayOrder
            ? A.Settings.DisplayOrder<B.Settings.DisplayOrder : A.Row.RowName.LexicalLess(B.Row.RowName); });
    }
    for (auto& Slot:RuntimeSlots)
    {
        Slot.ReadyAt=ServerTime()+Slot.Settings.InitialDelay;
        Slot.Handle=ASC->GiveAbility(FGameplayAbilitySpec(Slot.Settings.AbilityClass,1,INDEX_NONE,this));
        if (Slot.Handle.IsValid()) SelectedSkills.Add(Slot.Row);
    }
    SetComponentTickEnabled(!SelectedSkills.IsEmpty());
}

void URogueBossSkillComponent::ResetForPool()
{
    if (bResetting) return;
    TGuardValue<bool> Guard(bResetting,true);
    SetComponentTickEnabled(false);
    if (IsValid(CastSeal)) CastSeal->HideSeal();
    if (auto* E=Cast<ARogueEnemy>(GetOwner()); E && E->HasAuthority())
    {
        auto* ASC=E->GetAbilitySystemComponent();
        // 先移走调度状态，再取消 GAS；EndAbility 的回调不能重入旧的插槽数组。
        auto OldSlots=MoveTemp(RuntimeSlots);
        SelectedSkills.Reset();
        auto PreviousAbility=ActiveAbility;
        ActiveSlot=INDEX_NONE; ActiveAbility.Reset();
        if (PreviousAbility.IsValid()) PreviousAbility->StopExecution();
        if (ASC) for (const auto& Slot:OldSlots) if (Slot.Handle.IsValid())
        { ASC->CancelAbilityHandle(Slot.Handle); ASC->ClearAbility(Slot.Handle); }
        State.Phase=ERogueBossSkillPhase::Idle; State.bCancelled=true;
        State.CastId=NextCastId=NextCastId==MAX_int32?1:NextCastId+1;
        State.Generation=E->GetPresentationGeneration();
        State.PhaseStartServerTime=State.PhaseEndServerTime=ServerTime();
        if (E->IsBoss() || !OldSlots.IsEmpty()) E->ForceNetUpdate();
    }
    if (!GetOwner()->HasAuthority())
    {
        auto Idle=State;Idle.Phase=ERogueBossSkillPhase::Idle;Idle.bCancelled=true;
        LastPresented=Idle;bHadPresentation=true;
        if (GetNetMode()!=NM_DedicatedServer) OnPresentationChanged.Broadcast(Idle);
    }
    else RefreshPresentation();
}

bool URogueBossSkillComponent::IsSlotReady(int32 I) const
{
    return ActiveSlot==INDEX_NONE && MeetsSlotConditions(I);
}

bool URogueBossSkillComponent::MeetsSlotConditions(int32 I) const
{
    if (!IsOwnerReady() || bResetting || !RuntimeSlots.IsValidIndex(I)) return false;
    const auto& Slot=RuntimeSlots[I]; const auto* E=CastChecked<ARogueEnemy>(GetOwner());
    const float Health=E->GetHealthPercent();
    return Slot.Handle.IsValid() && Slot.ReadyAt<=ServerTime() && Slot.Settings.Weight>0
        && (Slot.Settings.MaxUses==0 || Slot.Uses<Slot.Settings.MaxUses)
        && Health>=Slot.Settings.MinHealthFraction
        && (Slot.Settings.MaxHealthFraction>=1.f || Health<Slot.Settings.MaxHealthFraction);
}

bool URogueBossSkillComponent::TryStartSkill(int32 I)
{
    if (!IsSlotReady(I)) return false;
    auto* E=CastChecked<ARogueEnemy>(GetOwner());
    ARogueCharacter* Target=E->GetPlayerCharacter();
    if (!Target)
    {
        TArray<ARogueCharacter*,TInlineAllocator<4>> Players;
        RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
        float Best=MAX_flt;
        for (auto* P:Players)
        {
            const float D=FVector::DistSquared2D(E->GetActorLocation(),P->GetActorLocation());
            if (D<Best) { Best=D; Target=P; }
        }
    }
    const auto& Slot=RuntimeSlots[I];
    if (Slot.Settings.bRequiresTarget && (!IsValid(Target) || !Target->IsAlive())) return false;
    ActiveSlot=I; ActiveSettings=Slot.Settings;
    State=FRogueBossSkillState(); State.Skill=Slot.Row; State.DisplayName=Slot.Settings.DisplayName;
    State.CastId=NextCastId=NextCastId==MAX_int32?1:NextCastId+1;
    State.Generation=E->GetPresentationGeneration(); State.Target=Target;
    // 默认从蓄力开始就固定落点；后续具体招式可另做追踪预警规则。
    State.TargetLocation=Target?Target->GetActorLocation():E->GetActorLocation();
    const bool bActivated=E->GetAbilitySystemComponent()->TryActivateAbility(Slot.Handle);
    if (!bActivated || (!ActiveAbility.IsValid() && ActiveSlot==I))
    {
        ActiveSlot=INDEX_NONE; State.Phase=ERogueBossSkillPhase::Idle;
        if (RuntimeSlots.IsValidIndex(I)) RuntimeSlots[I].ReadyAt=ServerTime()+.5;
        RefreshPresentation(); return false;
    }
    return true;
}

bool URogueBossSkillComponent::IsActivationAllowed(FGameplayAbilitySpecHandle Handle) const
{
    return IsOwnerReady() && !bResetting && RuntimeSlots.IsValidIndex(ActiveSlot) && RuntimeSlots[ActiveSlot].Handle==Handle;
}

bool URogueBossSkillComponent::AttachExecution(URogueBossSkillAbility* Ability,FGameplayAbilitySpecHandle Handle)
{
    if (!IsActivationAllowed(Handle) || ActiveAbility.IsValid()) return false;
    ActiveAbility=Ability;
    ++RuntimeSlots[ActiveSlot].Uses;
    if (ActiveSettings.bPauseMovement)
    {
        auto* Enemy=CastChecked<ARogueEnemy>(GetOwner());
        Enemy->ConsumeMovementInputVector();
        Enemy->GetCharacterMovement()->StopMovementImmediately();
    }
    return true;
}

bool URogueBossSkillComponent::IsExecutionCurrent(const URogueBossSkillAbility* Ability,int32 Id) const
{
    return IsOwnerReady() && !bResetting && ActiveAbility.Get()==Ability && State.CastId==Id
        && State.Generation==CastChecked<ARogueEnemy>(GetOwner())->GetPresentationGeneration()
        && (!ActiveSettings.bRequiresTarget || (IsValid(State.Target) && State.Target->IsAlive())
            || (ActiveSettings.bKeepCastingAfterRelease && State.PulseIndex>0));
}

void URogueBossSkillComponent::PublishPhase(ERogueBossSkillPhase Phase,float Duration,int32 PulseIndex)
{
    State.Phase=Phase; State.PulseIndex=PulseIndex;
    State.PhaseStartServerTime=ServerTime(); State.PhaseEndServerTime=State.PhaseStartServerTime+Duration;
    GetOwner()->ForceNetUpdate(); RefreshPresentation();
    if (PulseIndex>0 && (Phase==ERogueBossSkillPhase::Firing || Phase==ERogueBossSkillPhase::Charging) && ActiveAbility.IsValid()) Multicast_Pulse(State);
}

void URogueBossSkillComponent::NotifyExecutionEnded(URogueBossSkillAbility* Ability,bool bCancelled)
{
    if (ActiveAbility.Get()!=Ability || bResetting) return;
    if (RuntimeSlots.IsValidIndex(ActiveSlot)) RuntimeSlots[ActiveSlot].ReadyAt=ServerTime()+FMath::Max(.1f,ActiveSettings.Cooldown);
    ActiveAbility.Reset(); ActiveSlot=INDEX_NONE;
    State.bCancelled=bCancelled;
    PublishPhase(ERogueBossSkillPhase::Idle,0,State.PulseIndex);
}

void URogueBossSkillComponent::CancelCurrentSkill()
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if (E && E->HasAuthority() && ActiveAbility.IsValid()) ActiveAbility->StopExecution();
}

void URogueBossSkillComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Delta,Type,Tick);
    if (!IsOwnerReady()) { CancelCurrentSkill(); SetComponentTickEnabled(false); return; }
    if (ActiveSlot!=INDEX_NONE)
    {
        if (!IsExecutionCurrent(ActiveAbility.Get(),State.CastId)) { CancelCurrentSkill(); return; }
        if (!bAutoUseSkills) return;
        // 紧急符卡有更高优先级，及时抢占普通攻击，不叠加两个持续技能。
        int32 Emergency=INDEX_NONE;
        int32 Priority=FMath::Max(0,ActiveSettings.Priority);
        for (int32 I=0;I<RuntimeSlots.Num();++I)
            if (I!=ActiveSlot && RuntimeSlots[I].Settings.Priority>Priority && MeetsSlotConditions(I))
            { Emergency=I; Priority=RuntimeSlots[I].Settings.Priority; }
        if (Emergency!=INDEX_NONE) { CancelCurrentSkill(); TryStartSkill(Emergency); }
        return;
    }
    if (!bAutoUseSkills) return;
    int32 Highest=MIN_int32;
    float Total=0;
    for (int32 I=0;I<RuntimeSlots.Num();++I) if (IsSlotReady(I))
    {
        const auto& R=RuntimeSlots[I].Settings;
        if (R.Priority>Highest) { Highest=R.Priority; Total=0; }
        if (R.Priority==Highest) Total+=R.Weight;
    }
    if (Total<=0) return;
    float Pick=FMath::FRand()*Total;
    for (int32 I=0;I<RuntimeSlots.Num();++I) if (RuntimeSlots[I].Settings.Priority==Highest && IsSlotReady(I))
    { Pick-=RuntimeSlots[I].Settings.Weight; if (Pick<=0) { TryStartSkill(I); break; } }
}

bool URogueBossSkillComponent::SuppressesNormalAttacks() const { return ActiveSlot!=INDEX_NONE && ActiveSettings.bPauseNormalAttacks; }
bool URogueBossSkillComponent::SuppressesMovement() const { return ActiveSlot!=INDEX_NONE && ActiveSettings.bPauseMovement; }
float URogueBossSkillComponent::GetSkillCooldownRemaining(int32 I) const
{ return RuntimeSlots.IsValidIndex(I)?FMath::Max(0.f,float(RuntimeSlots[I].ReadyAt-ServerTime())):0.f; }
float URogueBossSkillComponent::GetPhaseRemaining() const { return FMath::Max(0.f,State.PhaseEndServerTime-ServerTime()); }

void URogueBossSkillComponent::RefreshPresentation()
{
    if (GetNetMode()==NM_DedicatedServer) return;
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    auto Visible=State;
    if (!E || !E->IsBoss() || E->IsDead() || E->IsAvailableInPool() || State.Generation!=E->GetPresentationGeneration())
        Visible.Phase=ERogueBossSkillPhase::Idle;
    if (!bHadPresentation || LastPresented.CastId!=Visible.CastId || LastPresented.Generation!=Visible.Generation
        || LastPresented.Phase!=Visible.Phase || LastPresented.PulseIndex!=Visible.PulseIndex || LastPresented.bCancelled!=Visible.bCancelled)
    {
        bHadPresentation=true;
        LastPresented=Visible;
        UpdateCastSeal(Visible);
        OnPresentationChanged.Broadcast(Visible);
    }
}

void URogueBossSkillComponent::UpdateCastSeal(const FRogueBossSkillState& Visible)
{
    if (!CastSealMaterial) return;
    if (Visible.Phase != ERogueBossSkillPhase::Idle && !IsValid(CastSeal))
    {
        FActorSpawnParameters Params;
        Params.Owner = GetOwner();
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        CastSeal = GetWorld()->SpawnActor<ARogueBossCastSeal>(GetOwner()->GetActorLocation(), FRotator::ZeroRotator, Params);
    }
    if (IsValid(CastSeal)) CastSeal->ApplyState(Visible, CastSealMaterial);
}

void URogueBossSkillComponent::OnRep_State() { RefreshPresentation(); }
void URogueBossSkillComponent::Multicast_Pulse_Implementation(const FRogueBossSkillState& Pulse)
{
    if (GetNetMode()==NM_DedicatedServer) return;
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E || !E->IsBoss() || E->IsDead() || E->IsAvailableInPool() || Pulse.Generation!=E->GetPresentationGeneration()
        || Pulse.CastId<State.CastId || (LastPulseGeneration==Pulse.Generation && (Pulse.CastId<LastPulseCast
        || (LastPulseCast==Pulse.CastId && Pulse.PulseIndex<=LastPulseIndex)))) return;
    LastPulseGeneration=Pulse.Generation; LastPulseCast=Pulse.CastId; LastPulseIndex=Pulse.PulseIndex;
    OnPulseCosmetic.Broadcast(Pulse);
}

void URogueBossSkillComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ResetForPool();
    if (IsValid(CastSeal)) CastSeal->Destroy();
    Super::EndPlay(Reason);
}
void URogueBossSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(URogueBossSkillComponent,State); }
