#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Algo/Transform.h"
#include "RogueCombatAttribute.h"
// #include "RogueCombatAttributeTemplate.generated.h"


// 选择属性的方式
enum class EAttributesModifierMember
{
	Field,   // 成员变量指针
	Method,  // 返回 float& 的成员函数指针
};


/**
 * 模板化属性修饰器：在基础修饰器上绑定属性成员指针，
 * 使其能直接定位到 TAttributes 的某个 float 字段或方法。
 */
template<typename TAttributes>
class TAttributeModifier : public FAttributeModifier
{
public:
	/** 成员函数类型：返回 float& */
	typedef float& (TAttributes::*FPropertyMethod)();
	/** 成员变量类型：float 成员指针 */
	using FPropertyField = float TAttributes::*;

	EAttributesModifierMember GetMemberMode() const
	{
		return MemberMode;
	}

	FPropertyField GetPropertyField()
	{
		return PropertyField;
	}

	FPropertyMethod GetPropertyMethod()
	{
		return PropertyGetter;
	}
	void SetPropertyField(FPropertyField InPropertyField)
	{
		PropertyField = InPropertyField;
		MemberMode = EAttributesModifierMember::Field;
	}

	void SetPropertyGetter(FPropertyMethod InPropertyMethod)
	{
		PropertyGetter = InPropertyMethod;
		MemberMode = EAttributesModifierMember::Method;
	}

public:
	EAttributesModifierMember MemberMode = EAttributesModifierMember::Field;
	FPropertyMethod PropertyGetter = nullptr;
	FPropertyField PropertyField = nullptr;
};

/**
 * 模板化修饰器集合，持有具体类型的修饰器列表
 */
template<typename TAttributes>
class TAttributeModifierGroup : public FAttributeModifierGroup
{
public:
using TAttributeModifierPtr = TSharedPtr<TAttributeModifier<TAttributes>>;

protected:
	virtual TArrayView<TAttributeModifierPtr> GetTModifiers() const
	{ static TArray<TAttributeModifierPtr> EmptyArray; return EmptyArray; }

public:
	/** 禁止重写FAttributeModifierSet::GetModifiers */
	virtual TArrayView<FModifierPtr> GetModifiers() const override final
	{
		TArrayView<TAttributeModifierPtr> TModifiers = GetTModifiers();
		return TArrayView<FModifierPtr>(
			reinterpret_cast<FModifierPtr*>(TModifiers.GetData()), 
			TModifiers.Num()
		);
	}

};


/**
 * 模板化属性状态：管理一个 TAttributes 实例的最终属性计算
 */
template<typename TAttributes>
class TAttributeSystem : public FAttributeSystemAbstract
{
public:
	using TAttributeModifier = TAttributeModifier<TAttributes>;
	using TAttributeModifierSet = TAttributeModifierGroup<TAttributes>;
	using TAttributesPtr = TUniquePtr<TAttributes>;
	
	TAttributes GetAttributesCopy()
	{
		CalculateAttribute();
		return *Attributes;
	}

	TWeakPtr<TAttributes> GetAttributesWeak()
	{
		CalculateAttribute();
		return Attributes;
	}

	const TAttributes* GetAttributes()
	{
		CalculateAttribute();
		return Attributes.Get();
	}

	void AddModifierSet(TAttributeModifierSet* ModifierSet)
	{
		FAttributeSystemAbstract::AddModifierSet(ModifierSet);
	}
	
protected:
	TAttributesPtr Attributes = MakeUnique<TAttributes>();

	virtual void OnRecalculate() override
	{
		*Attributes = {};
	}
	
	virtual float* GetProperty(FAttributeModifier* InModifier) const override
	{
		TAttributeModifier* Modifier = static_cast<TAttributeModifier*>(InModifier);
		switch (Modifier->GetMemberMode())
		{
			case EAttributesModifierMember::Field:
				return &(Attributes.Get()->*Modifier->GetPropertyField());
			case EAttributesModifierMember::Method:
				return &(Attributes.Get()->*Modifier->GetPropertyMethod())();
		}
		return FAttributeSystemAbstract::GetProperty(InModifier);
	}

};
