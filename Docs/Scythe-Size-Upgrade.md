# 镰刀变大升级卡

玩家实际持有镰刀时，升级和商店候选池才会加入「镰刀变大」。判断使用最终生效的镰刀数量，包含武器表基础数量和角色数量加成；没有镰刀时，即使规则资产误把这张卡放进基础池，也会被前置规则移除。解锁镰刀后的同一帧即可参与下一轮候选抽取，参与候选不保证每次都抽到。

默认每张卡增加基础尺寸的 20%。一次后为 1.2 倍，两次后为 1.4 倍，使用加法叠加；镰刀网格尺寸和实际伤害碰撞球半径同步放大。卡片不改变环绕距离、刀片数量、单次伤害和命中冷却。

| 想调整的内容 | 位置 |
|---|---|
| 卡片名称、说明、每次增长值 | `/Game/Data/Upgrades/DT_UpgradeDefinitions` 的 `ScytheSize` 行；Magnitude 默认 0.2 |
| 开局自带的大小加成 | `/Game/Data/Player/DA_PlayerBalance` → 05 初始武器加成 → 镰刀大小加成；默认 0 |
| 大小加成上限 | 同资产 → 06 武器加成上限 → 镰刀大小加成上限；-1 表示不限，例如 1 表示最终尺寸最多为基础的 2 倍 |
| 初始镰刀数量与环绕距离 | `/Game/Data/Weapons/DT_WeaponConfig` → `Scythe` 行 |
| 基础外观与碰撞球 | `/Game/BP/Projectile/BP_RogueOrbitingBlade` 的网格与碰撞组件 |

当前项目武器表把初始镰刀数量设为 **10**，因此当前配置开局已经满足前置。若希望先抽到「回旋镰刀」才能抽「镰刀变大」，把 Scythe 行的 Count 设为 0，并保证角色初始镰刀数量加成为 0。此次添加卡片保留了原有武器表配置。

数据链：`ScytheSize` 升级类型 → `URogueUpgradeEffect_ScytheSize` → PlayerState 的 ASC / `ScytheSizeBonus` → Character 的复制武器加成快照 `ScytheSize` → `ARogueWeapon_Scythe::GetEffectiveSizeMultiplier` → `ConfigureBlade` 同步外观与碰撞。服务器判定伤害，各端根据快照维护本地轨道表现。

刀片在 BeginPlay 缓存蓝图组件的基础尺寸。变大使用该基准乘倍率；回池和再次激活恢复基础尺寸，避免大镰刀被下一个玩家复用时继承大小。升级扩大范围后更新真实重叠列表，不能只改变可见网格。

同系列的吸血卡见 [Scythe-Lifesteal-Upgrade.md](Scythe-Lifesteal-Upgrade.md)。
