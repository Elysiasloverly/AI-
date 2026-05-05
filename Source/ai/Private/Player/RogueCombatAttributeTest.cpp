#include "Player/RogueCombatAttributeTest.h"


class FAttributeModifierFactory
{
public:
    using TModifierPtr = TSharedPtr<TAttributeModifier<FRogueCombatAttribute>>;
    
private:
    static TModifierPtr CreateModifier(EAttributesOperation Operation, float Bonus)
    {
        auto Modifier = MakeShared<TAttributeModifier<FRogueCombatAttribute>>();
        Modifier->Operation = Operation;
        Modifier->Value = Bonus;
        return Modifier;
    }
    
public:
    static TModifierPtr AttackBonus(float Bonus)
    {
        auto Modifier = CreateModifier(EAttributesOperation::Additive, Bonus);
        Modifier->SetPropertyField(&FRogueCombatAttribute::Damage);
        return Modifier;
    }
    
    static TModifierPtr AttackMult(float Mult)
    {
        auto Modifier = CreateModifier(EAttributesOperation::Multiplier, Mult);
        Modifier->SetPropertyField(&FRogueCombatAttribute::Damage);
        return Modifier;
    }
    
    static TModifierPtr SpeedBonus(float Bonus)
    {
        auto Modifier = CreateModifier(EAttributesOperation::Additive, Bonus);
        Modifier->SetPropertyGetter(&FRogueCombatAttribute::GetSpeed);
        return Modifier;
    }
    
    static TModifierPtr SpeedMult(float Mult)
    {
        auto Modifier = CreateModifier(EAttributesOperation::Multiplier, Mult);
        Modifier->SetPropertyGetter(&FRogueCombatAttribute::GetSpeed);
        return Modifier;
    }
    
};


void FAdditiveCalculator::ApplyModifier(float& AttributeProperty, const TArray<FAttributeModifier*>& Modifiers) const
{
    for (const FAttributeModifier* Mod : Modifiers)
    {
        AttributeProperty += Mod->Value;
    }
}

void FMultiplierCalculator::ApplyModifier(float& AttributeProperty, const TArray<FAttributeModifier*>& Modifiers) const
{
    float Multiplier = 0;
    for (const FAttributeModifier* Mod : Modifiers)
    {
        Multiplier += Mod->Value;
    }
    AttributeProperty *= Multiplier; 
}

void URogueCombatAttributeTest::Test()
{
    // ==========================================
    // 阶段 1：初始化基础数据与系统
    // ==========================================

    // 实例化模板化的状态管理器
    // 内部会调用初始化自动构建FRogueCombatAttribute
    // FRogueCombatAttribute的值一定要初始化成0，否则后面计算会出问题
    TAttributeSystem<FRogueCombatAttribute> StatSystem;
    auto PlayerStats = StatSystem.GetAttributes();
    

    // 注册计算器流水线 (假设你已将 Calculators 设为 public 或提供了 Add 方法)
    StatSystem.AddCalculator(new FAdditiveCalculator());
    StatSystem.AddCalculator(new FMultiplierCalculator());

    // ==========================================
    // 阶段 3：装载并计算
    // ==========================================
    
    // 将基础面板的修饰器打包进集合
    // Attack: 10; 100%
    // Speed:  5;  100%
    TSharedPtr<FTestModifierGroup> Basic = MakeShared<FTestModifierGroup>();
    Basic->ModifiersArray.Add(FAttributeModifierFactory::AttackBonus(10));
    Basic->ModifiersArray.Add(FAttributeModifierFactory::AttackMult(1));
    Basic->ModifiersArray.Add(FAttributeModifierFactory::SpeedBonus(5));
    Basic->ModifiersArray.Add(FAttributeModifierFactory::SpeedMult(1));

    // 注册集合，这会触发 StatSystem 内部的 MarkDirty()
    StatSystem.AddModifierSet(Basic.Get());
    
    // 可以手动通过FModifierGroup::MarkDirty，通知StatSystem需要重新计算
    Basic->MarkDirty();
    
    // 将修饰器打包进集合1
    // Attack: +20; +50%
    // Speed:  +15; -50%
    TSharedPtr<FTestModifierGroup> EquipSet1 = MakeShared<FTestModifierGroup>();
    EquipSet1->ModifiersArray.Add(FAttributeModifierFactory::AttackBonus(20));
    EquipSet1->ModifiersArray.Add(FAttributeModifierFactory::AttackMult(0.5));
    EquipSet1->ModifiersArray.Add(FAttributeModifierFactory::SpeedBonus(15));
    EquipSet1->ModifiersArray.Add(FAttributeModifierFactory::SpeedMult(-0.5));
    StatSystem.AddModifierSet(EquipSet1.Get());
    
    // 通过FModifierGroup::MarkDirty，通知StatSystem需要重新计算
    EquipSet1->MarkDirty();
    
    // 手动检测MarkDirty+自动重新计算，true时强制重新计算
    // 触发流水线运算 (假设 CalculateAttribute 已公开)
    // 此时它会执行你的 FlatModifiers 排序算法，依次处理 加法 -> 乘法
    StatSystem.CalculateAttribute(false);
    
    // 使用GetAttributes也可以自动计算
    // StatSystem.GetAttributesConst();
    // StatSystem.GetAttributes();

    UE_LOG(LogTemp, Display, TEXT("测试通过！当前攻击力: %f, 当前速度: %f"), PlayerStats->Damage, PlayerStats->Speed);

    // 将修饰器打包进集合2
    // Attack: +20+15; +0%
    // Speed:  +0;     +50%
    TSharedPtr<FTestModifierGroup> EquipSet2 = MakeShared<FTestModifierGroup>();
    EquipSet2->ModifiersArray.Add(FAttributeModifierFactory::AttackBonus(20));
    EquipSet2->ModifiersArray.Add(FAttributeModifierFactory::AttackBonus(15));
    EquipSet2->ModifiersArray.Add(FAttributeModifierFactory::SpeedMult(0.5));
    
    // 注册集合2，再次触发 StatSystem 内部的 MarkDirty()
    StatSystem.AddModifierSet(EquipSet2.Get());
    StatSystem.CalculateAttribute();
    UE_LOG(LogTemp, Display, TEXT("测试通过！当前攻击力: %f, 当前速度: %f"), PlayerStats->Damage, PlayerStats->Speed);
    
    // 清空集合2，重新计算
    EquipSet2->ModifiersArray.Empty();
    EquipSet2->MarkDirty();
    StatSystem.CalculateAttribute();
    UE_LOG(LogTemp, Display, TEXT("测试通过！当前攻击力: %f, 当前速度: %f"), PlayerStats->Damage, PlayerStats->Speed);
}
