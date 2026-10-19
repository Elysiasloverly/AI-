#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"

/** 组合引擎的属性/数值访问器宏。各属性集显式包含本头文件，避免依赖包含顺序。
 * 引擎 AttributeSet.h 提供四个组成宏，ATTRIBUTE_ACCESSORS 由项目定义。 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
