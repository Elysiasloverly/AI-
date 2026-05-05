// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/RogueCombatAttribute.h"


/** 临时计算项，用于平铺和排序 */
// USTRUCT()
struct FModifierFlatItem
{
	// GENERATED_BODY()
	
	FAttributeModifier* Modifier;
	float* TargetProperty;

	// 排序逻辑：先按 Operation 升序，再按内存地址升序
	auto operator<=>(const FModifierFlatItem& Other) const
	{
		auto Cmp = Modifier->Operation <=> Other.Modifier->Operation;
		if (Cmp == 0)
		{
			Cmp = &TargetProperty <=> &Other.TargetProperty;
		}
		return Cmp;
	}
};

float* FAttributeSystemAbstract::GetProperty(FAttributeModifier* Modifier) const
{
	static float Tmp;
	return &Tmp;
}

void FAttributeSystemAbstract::AddModifierSet(FAttributeModifierGroup* ModifierGroup)
{
	if (ModifierGroup)
	{
		Groups.Add(ModifierGroup);
		MarkDirty();
	}
}

void FAttributeSystemAbstract::MarkDirty()
{
	bIsChanged = true;
}

void FAttributeSystemAbstract::AddCalculator(FAttributeCalculator* Calculator)
{
	Calculators.Add(Calculator);
}


void FAttributeSystemAbstract::Recalculate()
{
    if (!bIsChanged) return;

    // ==========================================
    // 步骤 1：收集并铺平
    // ==========================================
    TArray<FModifierFlatItem> FlatModifiers;
    FlatModifiers.Reserve(Groups.Num() * 10);

    for (const FAttributeModifierGroup* Group : Groups)
    {
        if (!Group)
        {
        	continue;
        }
        for (const auto Modifier : Group->GetModifiers())
        {
	        const auto ModifierPtr = Modifier.Get();
            FlatModifiers.Add({ ModifierPtr, GetProperty(ModifierPtr) });
        }
    }

    if (FlatModifiers.Num() == 0)
    {
        bIsChanged = false;
        return;
    }

    // ==========================================
    // 步骤 2：核心排序 (保证阶段连续性和地址连续性)
    // ==========================================
    FlatModifiers.Sort();

    // ==========================================
    // 步骤3：以 FlatModifiers 为驱动的“控制断点”循环
    // ==========================================
    
    // 提取在循环外，全过程复用同一块内存，避免反复 new/delete 数组
    TArray<FAttributeModifier*> ReusableBatch;
	auto CurrentCalculator = Calculators.CreateIterator();
    ReusableBatch.Reserve(10); 

    // 初始化游标状态（以第一个元素为准）
    EAttributesOperation CurrentPhase = FlatModifiers[0].Modifier->Operation;
    float* CurrentProperty = FlatModifiers[0].TargetProperty;

    // 单次遍历打天下
    for (int32 i = 0; i < FlatModifiers.Num() && CurrentCalculator; ++i)
    {
        const FModifierFlatItem& Item = FlatModifiers[i];

        // 【断点判定】：当计算阶段改变，或者目标属性改变时 -> 触发结算！
        if (Item.Modifier->Operation != CurrentPhase || Item.TargetProperty != CurrentProperty)
        {
            if (ReusableBatch.Num() > 0)
            {
                // 提交给对应的计算器
                if (const FAttributeCalculator* Calc = *CurrentCalculator)
                {
                    Calc->ApplyModifier(*CurrentProperty, ReusableBatch);
                }
                // 清空数组元素，但不释放 Capacity
                ReusableBatch.Reset(); 
            }

            // 推进游标状态
            CurrentProperty = Item.TargetProperty;
        	while (CurrentCalculator && (*CurrentCalculator)->GetOperation() != Item.Modifier->Operation)
        	{
        		++CurrentCalculator;
        	}
            CurrentPhase = Item.Modifier->Operation;
        }

        // 将当前修饰符压入批处理队列
        ReusableBatch.Add(Item.Modifier);
    }

    // 【收尾判定】：循环结束后，提交最后残留的一个 Batch
    if (ReusableBatch.Num() > 0 && CurrentCalculator && (*CurrentCalculator)->GetOperation() == CurrentPhase)
    {
        (*CurrentCalculator)->ApplyModifier(*CurrentProperty, ReusableBatch);
    }

    bIsChanged = false;
}
