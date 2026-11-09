#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/Abilities/RogueBossSkillAbility.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/RogueCharacter.h"
#include "World/RogueEnemySpawnRegion.h"

struct FRogueBossSkillPreviewAccess
{
    static void MakeReady(URogueBossSkillComponent* Skills)
    {
        if (!Skills->RuntimeSlots.IsEmpty()) Skills->RuntimeSlots[0].ReadyAt = 0;
    }
};

namespace RogueBossSkillPreview
{
const FName PreviewTag(TEXT("Rogue.BossSkillPreview"));
const FName SkillNames[] = {
    TEXT("Octagram"), TEXT("RollingSuns"), TEXT("JudgmentRay"),
    TEXT("LastBloom"), TEXT("ScarletRain"), TEXT("FlameCrescents"),
    TEXT("EndlessRing"), TEXT("DiamondReversal"), TEXT("FrostPursuit")
};

void Message(const FString& Text)
{
    UE_LOG(LogTemp, Display, TEXT("[BossSkillPreview] %s"), *Text);
    if (GEngine) GEngine->AddOnScreenDebugMessage(1190601, 12.f, FColor::Cyan, Text);
}

bool FindLocation(UWorld* World, ARogueCharacter* Player, const ARogueEnemy* Defaults, FVector& Location)
{
    const auto* Capsule = Defaults->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    ARogueEnemySpawnRegion* Region = nullptr;
    for (TActorIterator<ARogueEnemySpawnRegion> It(World); It; ++It) { Region = *It; break; }
    const FVector Feet = Player->GetActorLocation() - FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BossPreviewPlacement), false, Player);
    for (float Distance : {650.f, 450.f, 300.f})
    {
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const FVector Direction = Player->GetActorForwardVector().GetSafeNormal2D().RotateAngleAxis(Index * 22.5f, FVector::UpVector);
            FVector Ground;
            if (!ARogueBossAttackActor::FindGround(World, Feet + Direction * Distance, Player, Ground)) continue;
            const FVector Candidate = Ground + FVector(0, 0, HalfHeight + 3.f);
            if (Region && !Region->ContainsCapsule(Candidate, Radius, HalfHeight)) continue;
            if (World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) continue;
            Location = Candidate;
            return true;
        }
    }
    return false;
}

void Run(const TArray<FString>& Args, UWorld* World)
{
    auto* Mode = World ? World->GetAuthGameMode<ARogueGameMode>() : nullptr;
    if (!Mode || !World->IsGameWorld() || Mode->IsGameOver())
    {
        Message(TEXT("请先进入战斗地图，并在单人或房主窗口执行命令。"));
        return;
    }
    auto* Catalog = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Enemies/DT_BossSkills.DT_BossSkills"));
    if (!Catalog) return;
    if (Args.IsEmpty() || Args[0].Equals(TEXT("list"), ESearchCase::IgnoreCase))
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(SkillNames); ++Index)
        {
            const auto* Row = Catalog->FindRow<FRogueBossSkillRow>(SkillNames[Index], TEXT("Boss skill preview list"));
            if (Row) UE_LOG(LogTemp, Display, TEXT("[BossSkillPreview] %d  %s  %s"), Index + 1, *SkillNames[Index].ToString(), *Row->DisplayName.ToString());
        }
        Message(TEXT("输入 Rogue.Test.BossSkill 1 到 9，或直接填写表中的行名。"));
        return;
    }
    if (Args[0].Equals(TEXT("stop"), ESearchCase::IgnoreCase))
    {
        for (TActorIterator<ARogueEnemy> It(World); It; ++It)
            if (It->ActorHasTag(PreviewTag)) { It->DeactivateToPool(); It->Destroy(); }
        for (TActorIterator<ARogueEnemyProjectile> It(World); It; ++It)
            if (It->GetOwner() && It->GetOwner()->ActorHasTag(PreviewTag)) It->DeactivateToPool();
        Message(TEXT("技能预览已停止。停止 PIE 后重新运行，即可恢复正常对局。"));
        return;
    }
    const int32 Number = Args[0].IsNumeric() ? FCString::Atoi(*Args[0]) : 0;
    const FName Name = Number >= 1 && Number <= UE_ARRAY_COUNT(SkillNames) ? SkillNames[Number - 1] : FName(Args[0]);
    const auto* Definition = Catalog->FindRow<FRogueBossSkillRow>(Name, TEXT("Boss skill preview"), false);
    auto* Player = Cast<ARogueCharacter>(World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr);
    auto* EnemyClass = LoadClass<ARogueEnemy>(nullptr, *RogueEnemyAssetPaths::EnemyClassPath(TEXT("Rifleman")));
    if (!Definition || !Definition->bEnabled || !Definition->AbilityClass || !Player || !EnemyClass)
    {
        Message(TEXT("技能或玩家尚未就绪。输入 Rogue.Test.BossSkill list 查看技能行名。"));
        return;
    }
    FVector Location;
    if (!FindLocation(World, Player, EnemyClass->GetDefaultObject<ARogueEnemy>(), Location))
    {
        Message(TEXT("附近没有可放置 Boss 的空地，请移动到地图中央后重试。"));
        return;
    }

    // 仅本次 PIE 的隔离预览。退出 PIE 后正常恢复，不改地图、配置或本地解锁存档。
    Mode->SetActorTickEnabled(false);
    if (auto* Spawns = World->GetSubsystem<URogueSpawnSubsystem>()) Spawns->SetSpawningSuspended(true);
    for (TActorIterator<ARogueEnemy> It(World); It; ++It) It->DeactivateToPool();
    for (TActorIterator<ARogueEnemyProjectile> It(World); It; ++It) It->DeactivateToPool();
    for (TActorIterator<ARogueBossAttackActor> It(World); It; ++It) It->Destroy();
    for (TActorIterator<ARogueWeaponBase> It(World); It; ++It) It->Destroy();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Boss = World->SpawnActor<ARogueEnemy>(EnemyClass, Location, FRotator::ZeroRotator, Params);
    if (!Boss) return;
    Boss->Tags.Add(PreviewTag);
    auto* Skills = Boss->BossSkills.Get();
    auto Row = *Definition;
    FDataTableRowHandle Handle;
    Handle.DataTable = Catalog;
    Handle.RowName = Name;
    Skills->SkillCatalog = nullptr;
    Skills->SkillSlots = {Handle};
    Skills->bAutoUseSkills = false;
    Boss->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
    FRogueEnemyProfile Profile;
    Profile.bIsBoss = true;
    Profile.Health = 1000000;
    Profile.Speed = 0;
    Profile.Damage = Args.Num() > 1 ? FMath::Max(0.f, FCString::Atof(*Args[1])) : 0.f;
    Boss->InitializeEnemy(Profile);
    World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(Boss);
    Boss->SetSpawnGroundReference(Location);
    Boss->SetActorTickEnabled(false);
    Boss->GetCharacterMovement()->StopMovementImmediately();
    Boss->GetCharacterMovement()->SetComponentTickEnabled(false);
    if (Name == TEXT("LastBloom") || Name == TEXT("DiamondReversal"))
        Boss->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(), Profile.Health * .2f);
    FRogueBossSkillPreviewAccess::MakeReady(Skills);
    const bool Started = Skills->TryStartSkill(0);
    Message(FString::Printf(TEXT("%s %s。再次输入命令可换招或重测，stop 可停止。"),
        *Row.DisplayName.ToString(), Started ? TEXT("开始预览") : TEXT("启动失败")));
}
}

static FAutoConsoleCommandWithWorldAndArgs BossSkillPreviewCommand(
    TEXT("Rogue.Test.BossSkill"),
    TEXT("Preview one real Boss skill in a battle map: <1..9|RowName|list|stop> [baseDamage=0]. Restart PIE to resume normal play."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RogueBossSkillPreview::Run));
#endif
