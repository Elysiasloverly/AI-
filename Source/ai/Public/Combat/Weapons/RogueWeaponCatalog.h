#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"

class ARogueCharacter;
class ARogueWeaponBase;
enum class ERogueWeaponUpgradeSource : uint8;
struct FRogueWeaponUpgradeRuleRow;
struct FRogueWeaponTableRow;

/** 稳定的武器身份表。这里只描述“是什么”，伤害和攻击行为仍由数据表、武器类负责。 */
struct FRogueWeaponDescriptor
{
	FName RowName;
	ERogueUpgradeType QuantityType;
	ERogueWeaponUpgradeSource UpgradeSource;
	UClass *(*NativeClass)();
};

namespace RogueWeaponCatalog
{
AI_API TConstArrayView<FRogueWeaponDescriptor> GetAll();
AI_API const FRogueWeaponDescriptor *FindByQuantity(ERogueUpgradeType Type);
AI_API const FRogueWeaponDescriptor *FindBySource(ERogueWeaponUpgradeSource Source);
AI_API bool IsWeaponQuantity(ERogueUpgradeType Type);
/** 将数量卡或专属卡映射到所属武器；一般属性卡没有所属武器。 */
AI_API bool TryGetQuantityForUpgrade(ERogueUpgradeType Upgrade, ERogueUpgradeType &OutQuantity);
AI_API int32 GetOwnedCount(const ARogueCharacter *Character, ERogueUpgradeType Upgrade);
AI_API TConstArrayView<FRogueWeaponUpgradeRuleRow> GetDefaultUpgradeRules();
/** 新表直接读 WeaponClass；旧样本缺少类时，只按稳定行名/类型转换一次，不依赖行顺序。 */
AI_API TSubclassOf<ARogueWeaponBase> ResolveClass(FName RowName, const FRogueWeaponTableRow &Row);
} // namespace RogueWeaponCatalog
