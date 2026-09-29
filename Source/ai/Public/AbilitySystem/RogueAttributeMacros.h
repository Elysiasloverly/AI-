#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"

/**
 * 属性访问器宏（项目内共用）。
 *
 * 背景：引擎的 `AttributeSet.h` **只提供零件**（`GAMEPLAYATTRIBUTE_PROPERTY_GETTER` /
 * `GAMEPLAYATTRIBUTE_VALUE_GETTER` / `..._SETTER` / `..._INITTER`），并在注释里给了一段
 * "你可以这样定义 ATTRIBUTE_ACCESSORS" 的示例 —— 它自己**并不定义**这个复合宏。
 * 所以每个项目都要自己定义一次；本项目原来把它写在 URogueAttributeSet.h 里，
 * 结果第二套属性集（URogueAttributeSet）一用就报
 * "ATTRIBUTE_ACCESSORS: 缺少返回类型"（宏没被展开，被当成函数声明）。
 *
 * 抽到这个头文件里，谁要用谁包含 —— 每个属性集都要写几十次这个宏，
 * 不该让"能不能用"取决于有没有先包含另一个属性集的头。
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
