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
#include "RogueCombatAttribute.h"
#include "RogueCombatAttributeTemplate.generated.h"

template<typename TAttributes>
class TAttributesModifier : FAttributesModifier
{
	enum class EMember
	{
		Method,
		Field,
	};

protected:
	EMember MemberMode = EMember::Field;

    typedef float& (TAttributes::*FPropertyMethod)();
	using FPropertyField = float TAttributes::*;

	/** 选择返回属性的函数 */
	FPropertyMethod PropertyGetter = nullptr;
	
	/** 选择属性字段 */
	FPropertyField PropertyField = nullptr;

public:
	FPropertyField GetMemberMode() const
	{
		return MemberMode;
	}

	void SetPropertyField(FPropertyField InPropertyField)
	{
		PropertyField = InPropertyField;
		MemberMode = EMember::Field;
	}

	void SetPropertyGetter(FPropertyMethod InPropertyMethod)
	{
		PropertyGetter = InPropertyMethod;
		MemberMode = EMember::Method;
	}

	float& GetMember(TAttributes* InAttributes)
	{
		switch (MemberMode)
		{
			case EMember::Method:
				return (InAttributes->*PropertyGetter)();
			case EMember::Field:
				return &(InAttributes.*PropertyField);
			default:
				return &(InAttributes.*PropertyField);
		}
	}
};


template<typename TAttributes>
class TAttributeModifierSet : public FAttributeModifierSet
{
public:
	/** 禁止重写FAttributeModifierSet::GetModifiers */
	virtual const TArray<TAttributesModifier<TAttributes>>& FAttributeModifierSet::GetModifiers() const override final 
		{ return TAttributeModifierSet::GetModifiers(); }
	
protected:
	virtual const TArray<TAttributesModifier<TAttributes>>& GetModifiers() 
		{ static TArray<FAttributesModifier> EmptyArray; return EmptyArray; }
};


/**
 * 通过继承FAttributesModifier实现FAttributesModifier<T>，完成成员选择
 */
template<typename TAttributes>
class AI_API TAttributeStat : public FAttributeStat
{
private:
	bool IsChanged = false;

	TAttributes Attributes;
	
public:
	const TAttributes& Get()
	{
		// 步骤1：根据Operation和 指定的指针 对Modifier进行整理
		// 步骤2：将Modifier分别丢进Calculator计算应用
		// 步骤3：输出结果
		// 优化：储存中间结果，需要时再进行局部更新？需要思考
		// 目前：懒得搞，每次更新先全部重算
		return Attributes;
	}
};
