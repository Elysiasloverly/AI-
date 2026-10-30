#include "RogueEditorAssetUtils.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"

#include "EditorAssetLibrary.h"
#include "Engine/CurveTable.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Framework/RogueGameMode.h"

FString URogueEditorAssetUtils::InspectEnemyPreview(ARogueEnemy* Enemy)
{
    if (!IsValid(Enemy)) { return TEXT("{}"); }
    const auto* Member = Enemy->GetRogueAttributeSet();
    const auto* Default = Enemy->GetDefaultSubobjectByName(TEXT("AttributeSet"));
    const auto* ASC = Enemy->GetAbilitySystemComponent();
    const auto* Set = ASC ? ASC->GetSet<URogueAttributeSet>() : nullptr;
    return FString::Printf(TEXT("{\"member_present\":%s,\"default_present\":%s,\"asc_present\":%s,\"gas_health\":%.6f,\"gas_max_health\":%.6f,\"mirror_health\":%.6f,\"boss\":%s,\"dead\":%s,\"pooled\":%s,\"spawn_id\":\"%s\",\"world_type\":%d,\"member_name\":\"%s\",\"default_name\":\"%s\",\"asc_attribute_name\":\"%s\"}"),
        Member ? TEXT("true") : TEXT("false"), Default ? TEXT("true") : TEXT("false"), Set ? TEXT("true") : TEXT("false"),
        Set ? Set->GetHealth() : -1.f, Set ? Set->GetMaxHealth() : -1.f, Enemy->GetCurrentHealth(),
        Enemy->IsBoss() ? TEXT("true") : TEXT("false"), Enemy->IsDead() ? TEXT("true") : TEXT("false"),
        Enemy->IsAvailableInPool() ? TEXT("true") : TEXT("false"), *Enemy->GetSpawnCycleId().ToString(),
        Enemy->GetWorld() ? int32(Enemy->GetWorld()->WorldType) : -1,
        *GetNameSafe(Member), *GetNameSafe(Default), *GetNameSafe(Set));
}

bool URogueEditorAssetUtils::ApplyPreviewDamage(AActor* Source, AActor* Target, float Damage)
{
    return IsValid(Target) && Target->GetWorld() && Target->GetWorld()->IsGameWorld()
        && RogueAbilitySystem::ApplyDamageToTarget(Source, Target, Damage, FGameplayTag());
}

bool URogueEditorAssetUtils::OpenPreviewUpgradeSelection(ARogueCharacter* Player)
{
    if (!IsValid(Player) || !Player->GetWorld() || !Player->GetWorld()->IsGameWorld()) { return false; }
    auto* Mode = Player->GetWorld()->GetAuthGameMode<ARogueGameMode>();
    if (!Mode) { return false; }
    Mode->StartUpgradeSelection(Player);
    const auto* State = Player->GetPlayerState<ARoguePlayerState>();
    return State && State->IsAwaitingUpgradeChoice();
}

bool URogueEditorAssetUtils::SaveAsset(UObject* Asset)
{
	if (Asset == nullptr)
	{
		return false;
	}

	Asset->MarkPackageDirty();
	if (UPackage* Package = Asset->GetOutermost())
	{
		Package->SetDirtyFlag(true);
	}

	return UEditorAssetLibrary::SaveAsset(Asset->GetPathName(), false);
}

AActor* URogueEditorAssetUtils::SpawnPreviewActor(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FTransform Transform)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !ActorClass) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass, Transform, Params);
}


bool URogueEditorAssetUtils::ConnectWorldPositionOffset(UMaterial* Material, UMaterialExpression* Expression)
{
    if (!IsValid(Material) || !IsValid(Expression)) { return false; }
    Material->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Expression);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return true;
}

bool URogueEditorAssetUtils::ImportGrowthCurveTable(UCurveTable* Table, const FString& Json, bool bMergeRows)
{
    if (!IsValid(Table)) { return false; }
    TStrongObjectPtr<UCurveTable> Imported(bMergeRows ? NewObject<UCurveTable>() : nullptr);
    UCurveTable* Source = bMergeRows ? Imported.Get() : Table;
    const TArray<FString> Problems = Source->CreateTableFromJSONString(Json, RCIM_Cubic);
    if (!Problems.IsEmpty())
    {
        for (const FString& Problem : Problems) { UE_LOG(LogTemp, Error, TEXT("[GrowthCurve] %s"), *Problem); }
        return false;
    }
    // 保留Rich模式，用户可以直接切换关键帧插值/切线；初始线性取值避免自动曲线过冲。
    for (const auto& Row : Source->GetRichCurveRowMap())
    {
        FRichCurve& Curve = *Row.Value;
        Curve.PreInfinityExtrap = RCCE_Constant;
        Curve.PostInfinityExtrap = RCCE_Linear;
        for (auto Key = Curve.GetKeyHandleIterator(); Key; ++Key) { Curve.SetKeyInterpMode(*Key, RCIM_Linear); }
        if (bMergeRows)
        {
            // 只替换传入的行，保留其它行的关键帧、切线和外推设置。
            Table->RemoveRow(Row.Key);
            if (Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
            {
                FSimpleCurve& Destination = Table->AddSimpleCurve(Row.Key);
                Destination.SetKeyInterpMode(RCIM_Linear);
                Destination.PreInfinityExtrap = Curve.PreInfinityExtrap;
                Destination.PostInfinityExtrap = Curve.PostInfinityExtrap;
                for (auto Key = Curve.GetKeyHandleIterator(); Key; ++Key)
                    Destination.AddKey(Curve.GetKeyTime(*Key), Curve.GetKeyValue(*Key));
            }
            else { Table->AddRichCurve(Row.Key) = Curve; }
        }
    }
    Table->MarkPackageDirty();
    return true;
}

void URogueEditorAssetUtils::StartWidgetPreviewWindow(int32 Width,int32 Height)
{
    if(!GEditor || GEditor->PlayWorld)return;
    auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->NewWindowWidth=FMath::Clamp(Width,640,3840);
    Settings->NewWindowHeight=FMath::Clamp(Height,360,2160);
    FRequestPlaySessionParams Params;
    Params.WorldType=EPlaySessionWorldType::PlayInEditor;
    GEditor->RequestPlaySession(Params);
}
