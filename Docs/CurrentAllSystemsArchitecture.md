# 当前全系统架构图

这份文档画的是项目当前已经存在的全部主要系统。重点不是“每个类都列出来”，而是把系统边界、数据流、谁调用谁、后续扩展入口画清楚。

## 1. 总览图

```mermaid
flowchart TD
    Entry["UE 关卡 / GameMode 启动"] --> GM["ARogueGameMode\n局内总流程协调"]

    GM --> World["World 系统\nArena / ShopTerminal / ExperiencePickup"]
    GM --> Player["Player 系统\nARogueCharacter"]
    GM --> Core["Core 规则系统\n商店 / 升级 / 平衡 / 奖励"]
    GM --> Subsystems["WorldSubsystem 系统\n刷怪 / 追踪 / 对象池 / 场景优化"]
    GM --> UI["UI 系统\nHUD / WBP"]

    Player --> Combat["Combat 武器系统\n普通子弹 / 火箭 / 迫击炮 / 激光 / 地狱塔 / 镰刀"]
    Player --> PlayerBalance["URoguePlayerBalanceAsset\n玩家基础数值"]
    Player --> Visuals["URogueCharacterVisualComponent\n玩家视觉表现"]

    Combat --> Pools["URogueCombatPoolSubsystem\n对象池"]
    Combat --> Tracker["URogueEnemyTrackerSubsystem\n索敌 / 空间查询"]
    Combat --> Enemies["Enemies 系统\nARogueEnemy"]

    Subsystems --> Spawn["URogueSpawnSubsystem\n刷怪与 Boss 生成"]
    Subsystems --> Tracker
    Subsystems --> Pools
    Subsystems --> Scene["URogueSceneSubsystem\n场景性能优化"]

    Spawn --> Enemies
    Enemies --> EnemyData["敌人配置\nEnemyProfile / Archetype / DataTable"]
    Enemies --> EnemyProjectile["敌人弹体\nARogueEnemyProjectile"]
    Enemies --> Feedback["RogueCombatFeedback\n伤害数字 / 命中反馈"]

    Core --> Balance["URogueGameBalanceAsset\n全局平衡"]
    Core --> UpgradeSystem["FRogueUpgradeSystem\n升级选择"]
    Core --> ShopSystem["FRogueShopSystem\n商店"]
    Core --> RewardOffer["FRogueRewardOffer\n统一奖励报价"]

    UI --> HUD["ARogueHUD\nC++ HUD 桥接"]
    UI --> Widgets["WBP_*\n商店 / 升级 / 菜单 / 死亡界面"]

    Experimental["RogueCombatAttribute\n实验数值系统"] -. "当前已有代码，未完全接入主流程" .-> Player
```

## 2. 主循环与运行时流程

```mermaid
flowchart TD
    BeginPlay["ARogueGameMode.BeginPlay"] --> SpawnWorld["生成 Arena 与 ShopTerminal"]
    SpawnWorld --> LoadAssets["加载 GameBalance / UpgradeDefinition / UpgradeRule"]
    LoadAssets --> ConfigureSystems["配置 ShopSystem / SpawnSubsystem / TrackerSubsystem"]
    ConfigureSystems --> PrewarmPools["预热对象池"]
    PrewarmPools --> Tick["ARogueGameMode.Tick"]

    Tick --> GameOverCheck["是否 GameOver"]
    GameOverCheck -->|否| ShopTimer["商店自动刷新倒计时"]
    ShopTimer --> PauseCheck["是否暂停 / 商店打开 / 升级选择中"]
    PauseCheck -->|否| RunTime["推进局内时间"]
    RunTime --> SpawnTick["SpawnSubsystem.TickSpawning"]
    SpawnTick --> Wave["计算当前波次"]
    Wave --> BossCheck["检查 Boss 波次"]
    Wave --> NormalSpawn["按刷怪节奏生成普通敌人"]

    BossCheck --> BossSpawn["生成 Boss"]
    NormalSpawn --> EnemyPool["从对象池取敌人"]
    BossSpawn --> EnemyPool
```

## 3. 玩家系统

```mermaid
flowchart TD
    Character["ARogueCharacter"] --> Input["输入\n移动 / 镜头 / 冲刺 / 跳跃 / 交互 / 菜单 / 选卡"]
    Character --> Stats["玩家状态\n生命 / 护甲 / 经验 / 金钱 / 等级"]
    Character --> BalanceAsset["URoguePlayerBalanceAsset\n基础属性配置"]
    Character --> Weapons["武器实例数组\nTArray<ARogueWeaponBase>"]
    Character --> VisualComp["URogueCharacterVisualComponent\n移动/冲刺视觉"]
    Character --> ShopDetect["商店交互检测\nNearbyShopTerminal"]

    BalanceAsset --> ApplyBalance["ApplyBalanceAsset\n开局套基础数值"]
    Stats --> LevelUp["经验满后 LevelUp"]
    LevelUp --> GM["ARogueGameMode.StartUpgradeSelection"]

    ShopDetect --> Interact["按 E 交互"]
    Interact --> GMShop["ARogueGameMode.TryOpenShop"]

    Character --> Damage["TakeDamage\n护甲抵扣 / 生命扣减 / 死亡"]
    Damage --> Death["Die"]
    Death --> GMDeath["ARogueGameMode.NotifyPlayerDied"]
```

## 4. 武器与战斗系统

```mermaid
flowchart TD
    Character["ARogueCharacter"] --> WeaponTable["DT_WeaponConfig\n武器配置表"]
    WeaponTable --> WeaponRow["FRogueWeaponTableRow\n统一武器配置"]
    Character --> WeaponInit["InitializeWeapons\n生成武器 Actor"]
    WeaponInit --> WeaponBase["ARogueWeaponBase"]

    WeaponBase --> ProjectileWeapon["ARogueWeapon_Projectile\n普通子弹"]
    WeaponBase --> RocketWeapon["ARogueWeapon_Rocket\n火箭"]
    WeaponBase --> MortarWeapon["ARogueWeapon_Mortar\n迫击炮"]
    WeaponBase --> LaserWeapon["ARogueWeapon_Laser\n激光"]
    WeaponBase --> HellTower["ARogueWeapon_HellTower\n地狱塔"]
    WeaponBase --> Scythe["ARogueWeapon_Scythe\n镰刀"]

    WeaponBase --> Targeting["索敌辅助\nFindNearest / CollectEnemies"]
    Targeting --> Tracker["URogueEnemyTrackerSubsystem"]

    ProjectileWeapon --> PlayerProjectile["ARogueProjectile"]
    RocketWeapon --> RocketProjectile["ARogueRocketProjectile"]
    MortarWeapon --> MortarProjectile["ARogueMortarProjectile"]
    LaserWeapon --> LaserBeam["ARogueLaserBeam"]
    HellTower --> LaserBeam
    Scythe --> OrbitingBlade["ARogueOrbitingBlade"]

    PlayerProjectile --> Pools["对象池"]
    RocketProjectile --> Pools
    MortarProjectile --> Pools
    LaserBeam --> Pools
    OrbitingBlade --> Pools

    PlayerProjectile --> DamageEnemy["ApplyDamage 到敌人"]
    RocketProjectile --> DamageEnemy
    MortarProjectile --> DamageEnemy
    LaserBeam --> DamageEnemy
    OrbitingBlade --> DamageEnemy
    DamageEnemy --> Enemy["ARogueEnemy.TakeDamage"]
```

## 5. 敌人与刷怪系统

```mermaid
flowchart TD
    SpawnSub["URogueSpawnSubsystem"] --> Balance["URogueGameBalanceAsset"]
    Balance --> WaveEntries["EnemyWaveEntries\n敌人出现波次与权重"]
    Balance --> EnemyBalance["EnemyBalanceRows\n敌人倍率"]

    SpawnSub --> PickType["PickWeightedEnemyTypeForWave"]
    PickType --> Profile["FRogueEnemyProfile\n类型 / 血量 / 速度 / 伤害 / 经验 / Boss"]
    Profile --> ResolveClass["按 EnemyClassMap 解析蓝图类"]
    ResolveClass --> PoolEnemy["CombatPool.AcquireEnemy"]
    PoolEnemy --> Enemy["ARogueEnemy"]

    Enemy --> Archetype["FRogueEnemyArchetype\n行为原型"]
    Archetype --> HardcodedFallback["硬编码默认原型\n兜底外观与行为"]
    Archetype --> EnemyDT["敌人原型 DataTable\n编辑器覆盖"]

    Enemy --> Movement["移动行为\n直追 / 冲锋 / 环绕 / 远程风筝 / Z字 / 固定"]
    Enemy --> Ranged["远程攻击\n单发 / 散射"]
    Enemy --> GroundWave["地面波攻击\n震荡柱"]
    Enemy --> Contact["接触伤害"]
    Enemy --> Death["死亡回收"]

    Ranged --> EnemyProjectile["ARogueEnemyProjectile"]
    Death --> GMKilled["ARogueGameMode.HandleEnemyKilled"]
    GMKilled --> Currency["金币奖励"]
    GMKilled --> ExpPickup["经验球掉落或合并"]
    GMKilled --> BossReward["Boss 奖励升级选择"]
```

## 6. 对象池与性能系统

```mermaid
flowchart TD
    GM["ARogueGameMode"] --> Prewarm["PrewarmCombatPools"]
    Prewarm --> WeaponCollect["FRogueWeaponTableRow.CollectPoolPrewarmClasses"]
    WeaponCollect --> PoolSub["URogueCombatPoolSubsystem"]

    PoolSub --> CombatPools["FRogueCombatPools"]
    CombatPools --> EnemyPool["EnemyPool"]
    CombatPools --> PlayerProjectilePool["PlayerProjectilePool"]
    CombatPools --> EnemyProjectilePool["EnemyProjectilePool"]
    CombatPools --> RocketProjectilePool["Rocket/Mortar Projectile Pool"]
    CombatPools --> PickupPool["ExperiencePickupPool"]
    CombatPools --> ImpactPool["ImpactEffectPool"]
    CombatPools --> LaserPool["LaserBeamPool"]
    CombatPools --> BladePool["OrbitingBladePool"]

    TrackerSub["URogueEnemyTrackerSubsystem"] --> TrackerCore["FRogueEnemyTracker"]
    TrackerCore --> ActiveEnemies["ActiveEnemies"]
    TrackerCore --> ActivePickups["ActivePickups"]
    TrackerCore --> SpatialHash["敌人空间哈希\n加速索敌"]
    TrackerCore --> EffectCull["高负载特效裁剪判断"]

    SceneSub["URogueSceneSubsystem"] --> LevelOptimize["场景灯光/阴影/组件优化"]
```

## 7. 升级、商店与奖励系统

```mermaid
flowchart TD
    LevelUp["玩家升级"] --> GM["ARogueGameMode"]
    BossKilled["Boss 击杀"] --> GM
    ShopInteract["玩家按 E 打开商店"] --> GM

    GM --> UpgradeSystem["FRogueUpgradeSystem"]
    GM --> ShopSystem["FRogueShopSystem"]

    UpgradeSystem --> UpgradeRules["URogueUpgradeRuleAsset\n升级池规则"]
    UpgradeSystem --> UpgradeDefs["URogueUpgradeDefinitionAsset\n升级文本与数值"]
    UpgradeSystem --> UpgradeOptions["FRogueUpgradeOption[]"]

    ShopSystem --> ShopOffers["FRogueShopOffer[]"]
    ShopOffers --> RewardOffer["FRogueRewardOffer\n统一奖励报价"]
    RewardOffer --> UpgradeReward["当前接入 Upgrade 奖励"]
    RewardOffer -. "预留" .-> RelicReward["未来 Relic / Item 奖励"]

    UpgradeOptions --> ApplyUpgrade["ARogueCharacter.ApplyUpgrade"]
    UpgradeReward --> ApplyUpgrade
    ApplyUpgrade --> EffectApplier["FRogueUpgradeEffectApplier"]
    EffectApplier --> PlayerStats["玩家属性升级"]
    EffectApplier --> WeaponUpgrade["武器专属升级分发"]

    ShopSystem --> PriceRules["商店价格规则\n同类型购买永久翻倍\n手动刷新本周期翻倍\n自动补货重置刷新价"]
```

## 8. UI 系统

```mermaid
flowchart TD
    HUD["ARogueHUD"] --> GameModeData["读取 GameMode\n波次 / 时间 / 商店 / 升级"]
    HUD --> PlayerData["读取 Character\n血量 / 护甲 / 经验 / 金钱 / 冷却"]
    HUD --> TrackerData["读取 Tracker\n敌人血条 / 伤害数字"]

    HUD --> DrawHUD["C++ DrawHUD\n战斗信息 / 血条 / 伤害数字 / SHOP提示"]
    HUD --> WidgetBridge["UserWidget 桥接"]

    WidgetBridge --> ShopWBP["WBP_Shop\n商店界面"]
    WidgetBridge --> OfferCardWBP["WBP_ShopOfferCard\n商店卡牌"]
    WidgetBridge --> UpgradeWBP["WBP_UpgradeSelection\n升级选择"]
    WidgetBridge --> MenuWBP["WBP_Menu / Settings\n菜单与设置"]
    WidgetBridge --> DeathWBP["WBP_DeathScreen\n死亡界面"]

    ShopWBP --> ShopCallbacks["RequestBuyOffer / RequestRefresh / RequestClose"]
    UpgradeWBP --> UpgradeCallbacks["RequestSelectUpgrade"]
    MenuWBP --> MenuCallbacks["继续 / 重开 / 退出 / 设置"]

    ShopCallbacks --> GM["ARogueGameMode"]
    UpgradeCallbacks --> GM
    MenuCallbacks --> GM
```

## 9. 世界交互系统

```mermaid
flowchart TD
    Arena["ARogueArena\n战斗场地"] --> SpawnBounds["刷怪边界 / ArenaHalfExtent"]
    ShopTerminal["ARogueShopTerminal\n商店方块"] --> Prompt["SHOP 常驻提示 / 靠近交互"]
    ExperiencePickup["ARogueExperiencePickup\n经验球"] --> PickupMove["靠近吸附 / 合并 / 拾取"]

    ShopTerminal --> Character["ARogueCharacter\nNearbyShopTerminal"]
    Character --> OpenShop["Interact 打开商店"]
    OpenShop --> GM["ARogueGameMode.TryOpenShop"]

    ExperiencePickup --> Tracker["URogueEnemyTrackerSubsystem\n注册 / 注销 / 合并查找"]
    ExperiencePickup --> CharacterExp["ARogueCharacter.AddExperience"]
```

## 10. 数据资产与配置入口

```mermaid
flowchart LR
    GameBalance["AS_RogueGameBalanceAsse\n全局平衡"] --> SpawnRules["刷怪 / Boss / 金币 / 商店"]
    PlayerBalance["AS_RoguePlayerBalanceAsset\n玩家基础数值"] --> Player["ARogueCharacter"]
    WeaponConfig["DT_WeaponConfig\n武器表"] --> Weapons["武器生成与配置"]
    UpgradeDefs["AS_UpgradeDefinitionAsset\n升级定义"] --> UpgradeSystem["升级系统"]
    UpgradeRules["AS_RogueUpgradeRuleAsset\n升级规则"] --> UpgradeSystem
    EnemyDT["敌人原型 DataTable\n敌人行为与视觉"] --> Enemies["ARogueEnemy"]
    WBPAssets["WBP_* 资产\nUI 蓝图"] --> HUD["ARogueHUD"]
```

## 11. 当前已有但需要注意的边界

- `ARogueGameMode` 现在主要负责总流程协调，但它仍然是运行时入口，所有“开局初始化、暂停状态、Boss奖励、商店打开关闭”都从这里过。
- `FRogueRewardOffer` 已经作为统一奖励报价结构接入商店，但目前真正落地的奖励类型仍是升级卡，未来道具/遗物可以接在这里。
- `RogueCombatAttribute` 数值系统代码已经存在，但当前还没有完全替换主流程中的玩家属性、武器属性和升级效果。
- `RogueWeaponConfig.h` 是旧武器配置结构，当前主流程已转向 `DT_WeaponConfig + FRogueWeaponTableRow`，旧头文件建议先保留作为资产兼容缓冲。
- UI 当前是混合结构：战斗信息仍由 C++ HUD 绘制，商店/升级/菜单/死亡界面走 WBP。
