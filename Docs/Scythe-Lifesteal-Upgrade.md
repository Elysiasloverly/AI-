# 吸血收割升级卡

玩家实际持有镰刀时，升级和商店候选池才会加入「吸血收割」。判断使用最终生效的镰刀数量（武器表基础数量 + 角色数量加成）；没有镰刀时，即使规则资产误把这张卡放进基础池，也会被 `BuildUpgradePool()` 的前置规则移除。

## 数值

- 第一次获得：镰刀造成伤害的 **20%** 回复生命。
- 之后每获得一次：额外 **+5%**，即 25%、30%、35%……
- 最终钳制在 **100%**，不会出现回复量超过伤害。
- 回复量按**实际扣除的护甲 + 生命**计算，只补到最大生命；溢出部分丢弃。
- 只在服务端结算，客户端通过属性复制看到 `ScytheLifestealBonus`。

## 配置位置

| 想调整的内容 | 位置 |
|---|---|
| 卡片名称、说明、后续每级幅度 | `/Game/Data/Upgrades/DT_UpgradeDefinitions` 的 `ScytheLifesteal` 行；`Magnitude` 默认 0.05 |
| 首次数值、该卡最大值 | 同一行的 `FirstMagnitude`（默认 0.20）与 `MaxMagnitude`（默认 1.0）；-1 表示回退代码默认 |
| 武器专属出现规则 | `/Game/Data/Upgrades/DA_UpgradeRules` → 武器专属升级规则 → 增加 `镰刀 / 吸血收割` 行 |
| 吸血上限 | `/Game/Data/Player/DA_PlayerBalance` → 06 武器加成上限 → 镰刀吸血上限；-1 表示不额外限制，属性层仍钳制 100% |
| 初始镰刀数量 | `/Game/Data/Weapons/DT_WeaponConfig` → `Scythe` 行 |

> 当前武器表把初始镰刀数量设为 **10**，所以当前配置开局就满足前置。若希望先抽到「回旋镰刀」才出现「吸血收割」，把 Scythe 行的 Count 设为 0，并保证角色初始镰刀数量加成为 0。

## 数据链

`ScytheLifesteal` 升级类型 → `URogueUpgradeEffect_ScytheLifesteal`（映射存在，保证枚举完整）→ `URogueAttributeSet::ScytheLifestealBonus` → `ARogueCharacter::GetScytheLifestealPercent()` → `ARogueOrbitingBlade::ApplyContactDamage()` 按实际伤害调用 `ApplyScytheLifesteal()`。

这张卡的幅度不是恒定的（首次 20%，之后 5%），不能走“同类 GE 叠层”：GAS 叠层只保留一份 SetByCaller 幅度，第二次购买会按同一幅度乘层数。因此 `ARogueCharacter::ApplyUpgrade()` 对这张卡直接写属性集基值，再按属性复制同步；`Magnitude` 只用于后续每级幅度，`FirstMagnitude` / `MaxMagnitude` 控制首次目标与这张卡的上限。

## 与商店的关系

商店报价直接调用同一个 `BuildRandomUpgradeOptions()` / `BuildUpgradePool()`，所以前置条件只维护一份。商店已经生成但尚未刷新的旧报价不会凭空多出这张卡；刷新或自动补货后才会按当前镰刀数量重新抽取。购买接口仍会扣金币并走统一的 `ApplyUpgrade()`。

## 测试

- `Rogue.Gameplay.UpgradePrerequisites.ScytheLifestealUnlocksAndStacks`：未持有镰刀时默认/自定义规则/无角色池都不出现；解锁后出现；首次 20%、第二次 25%、上限钳制。
- `Rogue.Gameplay.ScytheLifesteal.ContactDamageHealsOwner`：真实刀片命中真实敌人后，玩家按实际伤害的 20% 回血。
