# 武器专属卡配置

新增三张专属卡均在永久升级树花费 5 冰砖解锁。解锁只开放抽卡资格；本局拥有对应武器后，才会出现在该玩家的升级与商店卡池。每次在本局选择卡片，才获得一次下表中的加成。

| 武器 | 卡片 / 升级类型 | 当前前置节点 | 每次选择的加成 |
| --- | --- | --- | --- |
| 御札 | 灵符连诵 / ProjectileBurst | Card.ProjectileCount | 每轮额外连射一次完整弹幕 |
| 八卦炉 | 炉心扩散 / RocketBlastRadius | Card.RocketCount | 爆炸半径 +90 厘米 |
| 御柱 | 神威增幅 / HellTowerDamageRamp | Card.HellTowerCount | 每跳伤害递增值 +0.1 |

御札原本单发，选 1、2、3 次灵符连诵后分别是双连发、三连发、四连发。数量卡增加每次连射的弹数：数量为 2 且三连发时，每轮共发出 6 个御札。连射结束后等待正常武器冷却，期间不会重叠启动下一轮。目标死亡或回池时重新索敌，玩家濒死、无可用目标或武器销毁时取消剩余连射。

御柱的神威增幅改变持续锁定时的伤害增长，不直接增加基础伤害。例如原本基础伤害 D、每跳递增 R，选择一次后前几跳为 D、D+(R+0.1)、D+2×(R+0.1)。

## 在编辑器中调整

- `/Game/Data/Progression/DA_UpgradeTree`：配置节点名称、永久解锁说明、费用和前置节点。三个新节点分别为 `Card.ProjectileBurst`、`Card.RocketBlastRadius`、`Card.HellTowerDamageRamp`，都接在本次修改前对应列的末尾。WBP 沿前置关系生成后续节点，不能跨前置购买。
- `/Game/Data/Upgrades/DT_UpgradeDefinitions`：三行同名升级类型配置局内卡片的标题、说明和 `Magnitude`。修改加成数值时同步修改说明文字；升级卡和商店共用该表。
- `/Game/Data/Upgrades/DA_UpgradeDefinitions`：继续引用上面的共享数据表。
- `/Game/Data/Upgrades/DA_UpgradeRules`：三个新类型已加入对应武器规则。永久解锁和本局持有武器两项条件都会过滤卡池。
- `/Game/Data/Weapons/DT_WeaponConfig` 的 `Projectile` 行：`ProjectileBurstInterval` 控制两次连射间隔，默认 0.12 秒；`MaxProjectileBurstCount` 包括第一发，默认 -1 不限制。八卦炉基础半径及御柱基础递增仍由对应原有武器行配置。
- `/Game/Data/Player/DA_PlayerBalance` 的武器加成上限：新增三个对应上限，默认 -1 不限制。新一局从零专属加成开始，由局内选卡增加。

永久解锁沿用每个玩家的本地存档与开局快照；新卡的 GAS 属性会复制到客户端。节点继续使用 `WBP_UpgradeTreeNode` 样式，保留每页最多四列与翻页逻辑。
