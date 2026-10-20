# 显示名与代码标识符对照

玩家看到的名称已统一为东方 Project 风格，但**代码标识符、资产名、DataTable 行键保持原英文不变**。
改标识符会牵连蓝图类引用、已保存的配置与历史文档，收益为零；所以两套名字并存，这份表负责对齐。

新增或改名时先查这里，避免在代码里按"银刃"去搜 `Scythe` 却搜不到。

## 武器

| 显示名 | 表行键（`DT_WeaponConfig.Name`） | 武器类 | 弹体 / 表现类 |
|---|---|---|---|
| 御札 | `Projectile` | `ARogueWeapon_Projectile` | `ARogueProjectile` |
| 银刃 | `Scythe` | `ARogueWeapon_Scythe` | `ARogueOrbitingBlade` |
| 八卦炉 | `Rocket` | `ARogueWeapon_Rocket` | `ARogueRocketProjectile` |
| 极限火花 | `Laser` | `ARogueWeapon_Laser` | `ARogueLaserBeam` |
| 御柱 | `HellTower` | `ARogueWeapon_HellTower` | `ARogueLaserBeam`（`BP_RogueHellTowerBeam`） |
| 阴阳玉 | `Mortar` | `ARogueWeapon_Mortar` | `ARogueMortarProjectile` |

## 升级卡

表行键就是 `ERogueUpgradeType` 的枚举名，写在 `DT_UpgradeDefinitions.Type`。

| 显示名 | 枚举 | 显示名 | 枚举 |
|---|---|---|---|
| 蓬莱之药 | `MaxHealth` | 妖怪退治 | `ExperienceGain` |
| 天狗疾风 | `MoveSpeed` | 银刃乱舞 | `ScytheCount` |
| 结界护壁 | `ArmorCapacity` | 八卦炉·改 | `RocketCount` |
| 隙间跳跃 | `DashCooldown` | 双重魔炮 | `LaserCount` |
| 灵力增幅 | `AttackPower` | 御柱林立 | `HellTowerCount` |
| 咏唱加速 | `AttackSpeed` | 星尘折射 | `LaserRefraction` |
| 赛钱箱 | `PickupRadius` | 阴阳玉连投 | `MortarCount` |
| 月之药 | `Recovery` | 阴阳玉·扩散 | `MortarBlastRadius` |
| 远隔操纵 | `AttackRange` | 银刃巨化 | `ScytheSize` |
| 流星弹 | `ProjectileSpeed` | 红雾吸血 | `ScytheLifesteal` |
| 弹幕增殖 | `ProjectileCount` | | |
| 弹幕偏转 | `Armor` | | |

## 敌人

表行键是 `DT_EnemyArchetypes.Name`，与 `ERogueEnemyType` 的枚举名一致。

| 显示名 | 枚举 | 定位 |
|---|---|---|
| 妖精 | `Rifleman` | 基础单发 |
| 夜雀 | `Assault` | 冲锋 + 散射 |
| 鬼 | `Juggernaut` | 缓慢重单发 |
| 人魂 | `Orbiter` | 环绕 + 单发 |
| 鸦天狗 | `Marksman` | 标准单发 |
| 妖狐 | `Skirmisher` | Z 字 + 散射 |
| 山蜘蛛 | `Cannoneer` | 远距离重单发 |
| 唐伞 | `Scatterer` | 三发散射 |
| 地藏 | `ShockPillar` | 固定环形弹幕 |
| 月兔 | `Sniper` | 超远距离单发 |
| 毒茸 | `Shotgunner` | 五发宽散射 |
| 血蝠 | `Repeater` | 高频单发 |
| 亡灵 | `Ringcaster` | 移动环形弹幕 |

弹道与波次参数见 [Enemy-Ranged-Variants.md](Enemy-Ranged-Variants.md)。

## 改一个名字要动哪些地方

漏掉任何一处都会出现"编辑器里是这个名、游戏里是那个名"的分裂。

| 位置 | 改什么 |
|---|---|
| `Content/Data/Weapons/DT_WeaponConfig` | `DisplayName` 列 |
| `Content/Data/Upgrades/DT_UpgradeDefinitions` | `Title`、`Description` 两列 |
| `Source/ai/Public/Shared/RogueTypes.h` | `ERogueUpgradeType` / `ERogueEnemyType` 的 `UMETA(DisplayName=...)` |
| `Source/ai/Public/Progression/Upgrades/RogueUpgradeRuleAsset.h` | 武器来源枚举的 `UMETA(DisplayName=...)` |
| `Source/ai/Private/Progression/Upgrades/RogueUpgradeSystem.cpp` | 仅 4 个类型有硬编码兜底文案（`ScytheSize` / `ScytheLifesteal` / `MortarCount` / `MortarBlastRadius`），其余缺表时显示"未配置升级" |
| `Source/ai/Private/Player/RogueCharacter.Weapons.cpp` | 阴阳玉的表缺失时的兜底 `DisplayName` |

商店卡片没有独立文案：它直接读 `Offer.GetUpgrade().Title`，改升级表即同时生效。

自动化用例里有对兜底文案的断言（`RogueUpgradePrerequisiteTests.cpp`），改文案时一起改，否则 `Rogue.Gameplay` 会红。

## 校验

查是否还有旧称呼漏网：

```bash
grep -rnE "镰刀|火箭|激光|地狱塔|迫击炮|子弹|震荡柱|步枪手|突击手|重装兵" Docs README.md
```

导出两张表对照实际内容：

```bash
UnrealEditor-Cmd.exe ai.uproject -run=pythonscript -script="Tools/dump_data_assets.py" -unattended -nullrhi -nosplash
# 输出在 Saved/DataDump/*.json
```

## 关于文件名

`Hell-Tower-Beam.md`、`Shock-Pillar-Attack.md`、`Scythe-*.md` 的文件名沿用旧英文：
它们被其它文档链接引用，改名要同步改链接，收益不抵风险。正文已使用新的显示名。
