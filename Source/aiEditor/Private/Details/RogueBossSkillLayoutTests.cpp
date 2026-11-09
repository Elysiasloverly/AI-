#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/Abilities/RogueBossAttackAbilities.h"
#include "DataTableEditorUtils.h"
#include "Enemies/Data/RogueBossSkillTypes.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"

namespace
{
void CollectAttackFields(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, TSet<FName>& Fields)
{
    for (const TSharedRef<IDetailTreeNode>& Node : Nodes)
    {
        if (const TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle())
        {
            const FProperty* Property = Handle->GetProperty();
            if (Property && Property->GetOwnerStruct() == FRogueBossAttackSettings::StaticStruct())
            {
                Fields.Add(Property->GetFName());
            }
        }

        TArray<TSharedRef<IDetailTreeNode>> Children;
        Node->GetChildren(Children);
        CollectAttackFields(Children, Fields);
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossSkillTableLayoutTest,"Rogue.Editor.BossSkills.SummaryAndSkillSpecificFields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRogueBossSkillTableLayoutTest::RunTest(const FString& Parameters)
{
    const auto* Table = LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_BossSkills.DT_BossSkills"));
    if (!TestNotNull(TEXT("Live skill catalog"),Table)) return false;

    TArray<FDataTableEditorColumnHeaderDataPtr> Columns;
    TArray<FDataTableEditorRowListViewDataPtr> Rows;
    FDataTableEditorUtils::CacheDataTableForEditing(Table,Columns,Rows);
    const TArray<FName> ExpectedColumns={TEXT("DisplayName"),TEXT("bEnabled"),TEXT("ChargeSeconds"),TEXT("Cooldown"),
        TEXT("DurationSeconds"),TEXT("PulseInterval"),TEXT("MaxHealthFraction"),TEXT("MaxUses")};
    TArray<FName> VisibleColumns;
    for (const auto& Column:Columns) VisibleColumns.Add(Column->ColumnId);
    TestTrue(TEXT("The grid contains just the eight ordered summary columns"),VisibleColumns==ExpectedColumns);
    TestEqual(TEXT("The grid contains the nine playable rows"),Rows.Num(),9);

    auto& PropertyEditor=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto CheckFields=[&](const FName RowName, std::initializer_list<const TCHAR*> Required,
        std::initializer_list<const TCHAR*> Excluded, bool bUseCustomAbility=false)
    {
        const auto* Row=Table->FindRow<FRogueBossSkillRow>(RowName,TEXT("Boss skill editor layout test"));
        if (!TestNotNull(RowName.ToString(),Row)) return;
        auto Data=MakeShared<FStructOnScope>(FRogueBossSkillRow::StaticStruct());
        auto* Copy=reinterpret_cast<FRogueBossSkillRow*>(Data->GetStructMemory());
        *Copy=*Row;
        if (bUseCustomAbility) Copy->AbilityClass=URogueBossSkillAbility::StaticClass();

        const auto Generator = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
        Generator->SetStructure(Data);
        TSet<FName> Fields;
        CollectAttackFields(Generator->GetRootTreeNodes(), Fields);
        for (const TCHAR* Field:Required)
            TestTrue(FString::Printf(TEXT("%s exposes %s"),*RowName.ToString(),Field),Fields.Contains(FName(Field)));
        for (const TCHAR* Field:Excluded)
            TestFalse(FString::Printf(TEXT("%s hides %s"),*RowName.ToString(),Field),Fields.Contains(FName(Field)));
    };

    CheckFields(TEXT("JudgmentRay"),{TEXT("BeamHalfWidth")},{TEXT("DamageInterval"),TEXT("BurnInterval"),TEXT("WaveMaxScale"),TEXT("BulletClass")});
    CheckFields(TEXT("ScarletRain"),{TEXT("BombsPerPulse"),TEXT("SafeRadius"),TEXT("FireSeconds")},{TEXT("BeamHalfWidth"),TEXT("WaveMaxScale")});
    CheckFields(TEXT("FlameCrescents"),{TEXT("WaveHeightAboveGround"),TEXT("WaveHalfHeight"),TEXT("WaveGrowthDistance"),TEXT("WaveMaxScale")},{TEXT("SafeRadius"),TEXT("BeamHalfWidth")});
    CheckFields(TEXT("LastBloom"),{TEXT("Radius"),TEXT("EffectClass")},{TEXT("DamageScale"),TEXT("Speed")});
    CheckFields(TEXT("JudgmentRay"),{TEXT("BeamHalfWidth"),TEXT("BurnInterval"),TEXT("WaveMaxScale")},{},true);
    return true;
}
#endif
