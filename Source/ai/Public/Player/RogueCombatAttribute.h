#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RogueCombatAttribute.generated.h"

/**
 * 使普通 C++ 类兼容 TArray 的宏
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

/** 脏标记委托 */
DECLARE_MULTICAST_DELEGATE(FOnMarkDirty);

/** 用于顺序遍历，禁止赋值! */
UENUM()
enum class EAttributesOperation : uint8
{
	Additive,
	Multiplier,
	Clamp,
};

/** 基础的属性修饰器数据 */
USTRUCT(BlueprintType)
struct AI_API FAttributeModifier
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttributesOperation Operation = EAttributesOperation::Additive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.0f;
};

/**
 * 属性修饰器集合的抽象接口
 * 
 */
USTRUCT()
struct AI_API FAttributeModifierGroup
{
	GENERATED_BODY()
public:
	virtual ~FAttributeModifierGroup() = default;
	using FModifierPtr = TSharedPtr<FAttributeModifier>;
	
	virtual TArrayView<FModifierPtr> GetModifiers() const
		{ static TArray<FModifierPtr> EmptyArray; return EmptyArray; }
	
	void MarkDirty() { OnMarkDirty.Broadcast(); }
	
private:
	FOnMarkDirty OnMarkDirty;
	friend class FAttributeSystemAbstract;
};

/**
 * 属性计算器接口
 * 每个派生类负责处理一种操作阶段（加性、乘性、钳位）
 */
USTRUCT()
struct AI_API FAttributeCalculator
{
public:
	virtual ~FAttributeCalculator() = default;
	
	GENERATED_BODY()
	using FModifierPtr = TSharedPtr<FAttributeModifier>;
	
	virtual EAttributesOperation GetOperation() const { return EAttributesOperation::Additive; }
	
	/**
	 * 应用所有属于当前阶段的修饰器
	 * @todo 数组出现数据切片问题
	 * @param AttributeProperty 需被修改的属性引用
	 * @param Modifiers 所有修饰器（内部按 Operation 过滤）
	 */
	virtual void ApplyModifier(float& AttributeProperty, const TArray<FAttributeModifier*>& Modifiers) const {}
};

/** 属性区块（占位，可扩展） */
USTRUCT()
struct AI_API FAttributeModifierBlock
{
	GENERATED_BODY()
public:
	FAttributeModifier* Modifier;
};
	
/**
 * 属性状态基类，统一管理计算器和修饰器集，并提供基本计算流程
 */
class AI_API FAttributeSystemAbstract
{
public:
	virtual ~FAttributeSystemAbstract() = default;
	using FModifierPtr = TSharedPtr<FAttributeModifier>;
	
private:
    TArray<FAttributeCalculator*> Calculators;
    TArray<FAttributeModifierGroup*> Groups;

	/** 是否需要重新计算 */
	bool bIsChanged = true;

protected:
    virtual float* GetProperty(FAttributeModifier* Modifier) const;

	void AddModifierSet(FAttributeModifierGroup* ModifierGroup);

	void MarkDirty() { bIsChanged = true; }

	bool GetIsChanged() const { return bIsChanged; }

public:
	void AddCalculator(FAttributeCalculator* Calculator);
	
	void Recalculate();
};