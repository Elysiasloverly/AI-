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
#include "RogueCombatAttribute.generated.h"

/**
 * 使普通 C++ 类兼容 TArray 的宏
 * @param ClassName 类名
 * @param ComparisonLogic 两个对象相等的逻辑（使用 Other 引用）
 */
#define IMPLEMENT_TARRAY_COMPATIBLE(ClassName, ComparisonLogic) \
public: \
ClassName() = default; \
ClassName(const ClassName& Other) = default; \
ClassName& operator=(const ClassName& Other) = default; \
bool operator==(const ClassName& Other) const \
{ \
return ComparisonLogic; \
} \
bool operator!=(const ClassName& Other) const \
{ \
return !(*this == Other); \
}

/**
 * 分阶段运算
 */
enum class EAttributesOperation
{
	Additive,
	Multiplier,
	Clamp,
};



/**
 * 战斗的属性修饰器，描述本属性的规则和数值
 * Todo 通过继承FAttributesModifier实现FAttributesModifier<T>，完成成员选择
 */
struct AI_API FAttributesModifier
{
	EAttributesOperation Operation;

	float Value;

	// bool operator<=>(const FAttributesModifier& Other) const = default;
};


USTRUCT()
struct AI_API FAttributesModifierSet
{
	GENERATED_BODY()
	virtual ~FAttributesModifierSet() = default;
	virtual const TArray<FAttributesModifier>& GetModifiers() const
		{ static TArray<FAttributesModifier> EmptyArray; return EmptyArray; }
};


/**
 * 属性计算缓存
 */
USTRUCT()
struct AI_API FAttributeCache
{
	GENERATED_BODY()
};


/**
 * 属性计算规则的接口
 */
struct AI_API FAttributesCalculator
{
	virtual EAttributesOperation GetOperation() const { return EAttributesOperation::Additive; }
	virtual void ApplyModifier(const FAttributesModifier& Modifier) {}
};

/**
 * 通过继承FAttributesModifier实现FAttributesModifier<T>，完成成员选择
 */
template<typename TAttributes>
class AI_API TAttributesStat
{
public:
	TArray<FAttributesCalculator> Calculators;
	TArray<FAttributesModifierSet> Modifiers;

	TAttributes& Get()
	{
		// 步骤1：根据Operation和 指定的指针 对Modifier进行整理
		// 步骤2：将Modifier分别丢进Calculator计算应用
		// 步骤3：输出结果
		// 优化：储存中间结果，需要时再进行局部更新？需要思考
		// 难点：对Modifier进行高效的分类
	}
};