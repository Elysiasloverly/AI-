#include "Player/RogueCombatAttributeTest.h"


class FAttributeModifierFatory
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
    FRogueCombatAttribute PlayerStats;
    PlayerStats.Damage = 0;
    PlayerStats.Speed = 0;

    // 实例化模板化的状态管理器
    TAttributeSystem<FRogueCombatAttribute> StatSystem;
    StatSystem.Attributes = &PlayerStats;

    // 注册计算器流水线 (假设你已将 Calculators 设为 public 或提供了 Add 方法)
    StatSystem.AddCalculator(new FAdditiveCalculator());
    StatSystem.AddCalculator(new FMultiplierCalculator());

    // ==========================================
    // 阶段 3：装载并计算
    // ==========================================
    
    // 将修饰器打包进集合
    FTestModifierGroup* EquipSet1 = new FTestModifierGroup();
    EquipSet1->ModifiersArray.Add(FAttributeModifierFatory::AttackBonus(20));
    EquipSet1->ModifiersArray.Add(FAttributeModifierFatory::AttackMult(15));
    EquipSet1->ModifiersArray.Add(FAttributeModifierFatory::SpeedBonus(15));
    EquipSet1->ModifiersArray.Add(FAttributeModifierFatory::SpeedMult(0.5));

    // 注册集合，这会触发 StatSystem 内部的 MarkDirty()
    StatSystem.AddModifierSet(EquipSet1);

    // 触发流水线运算 (假设 Recalculate 已公开)
    // 此时它会执行你的 FlatModifiers 排序算法，依次处理 加法 -> 乘法
    StatSystem.Recalculate();
    
    UE_LOG(LogTemp, Display, TEXT("测试通过！当前攻击力: %f, 当前速度: %f"), PlayerStats.Damage, PlayerStats.Speed);

    // 将修饰器打包进集合
    FTestModifierGroup* EquipSet2 = new FTestModifierGroup();
    EquipSet2->ModifiersArray.Add(FAttributeModifierFatory::AttackBonus(20));
    EquipSet2->ModifiersArray.Add(FAttributeModifierFatory::AttackBonus(15));
    EquipSet2->ModifiersArray.Add(FAttributeModifierFatory::SpeedBonus(15));
    EquipSet2->ModifiersArray.Add(FAttributeModifierFatory::SpeedMult(0.5));
    
    // 注册集合，再次触发 StatSystem 内部的 MarkDirty()
    StatSystem.AddModifierSet(EquipSet2);
    StatSystem.Recalculate();

    UE_LOG(LogTemp, Display, TEXT("测试通过！当前攻击力: %f, 当前速度: %f"), PlayerStats.Damage, PlayerStats.Speed);
}
