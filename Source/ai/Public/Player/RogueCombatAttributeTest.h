// Fill out your copyright notice in the Description page of Project Settings.

/**
 * 这个文件目前存储战斗伤害相关的数值，其他属性暂时没考虑要不要加入
 * FAttributesCalculator不需要与找到对应属性，只需要接受一个float&进行运算
 * 因此FAttributesModifier不再需要携带成员信息
 * 最终类型的选择只需要继承FAttributesModifier和FAttributesStat实现泛型即可
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RogueCombatAttributeTemplate.h"
#include "RogueCombatAttributeTest.generated.h"


// 1. 实现具体的加法计算器
struct FAdditiveCalculator : public FAttributeCalculator
{
	virtual EAttributesOperation GetOperation() const override { return EAttributesOperation::Additive; }

	virtual void ApplyModifier(float& AttributeProperty, const TArray<FAttributeModifier*>& Modifiers) const override;
};

// 2. 实现具体的乘法计算器
struct FMultiplierCalculator : public FAttributeCalculator
{
	virtual EAttributesOperation GetOperation() const override { return EAttributesOperation::Multiplier; }

	virtual void ApplyModifier(float& AttributeProperty, const TArray<FAttributeModifier*>& Modifiers) const override;
};

class AI_API FRogueCombatAttribute
{
public:
	float Damage;
	float Speed;

	float& GetSpeed()
	{
		return Speed;
	}
};

// 3. 实现一个用于测试的修饰器集合
struct FTestModifierGroup : public TAttributeModifierGroup<FRogueCombatAttribute>
{
	TArray<TAttributeModifierPtr> ModifiersArray;
	
protected:
	virtual TArrayView<TAttributeModifierPtr> GetTModifiers() const override
	{
		return const_cast<TArray<TAttributeModifierPtr>&>(ModifiersArray);
	}
};

UCLASS(Blueprintable)
class AI_API URogueCombatAttributeTest : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	static void Test();
};
