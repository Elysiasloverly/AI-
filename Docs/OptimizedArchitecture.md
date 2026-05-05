# 优化后的系统结构图

这份图按当前重构后的职责划分来画，重点是看清楚：`GameMode` 只做总流程协调，具体能力下放到系统、子系统、数据资产和 UI Widget。

## 总体结构

```mermaid
flowchart TD
    GM["ARogueGameMode\n总流程协调"] --> WorldInit["世界初始化\nArena / ShopTerminal"]
    GM --> Assets["运行时配置加载\nBalance / UpgradeDefinition / UpgradeRule"]
    GM --> RunLoop["局内流程\n时间 / 波次 / 暂停 / 死亡"]
    GM --> RewardFlow["奖励流程\n经验 / 金币 / Boss升级选择"]

    Assets --> Balance["URogueGameBalanceAsset\n数值平衡"]
    Assets --> UpgradeDefs["URogueUpgradeDefinitionAsset\n升级卡文本与数值"]
    Assets --> UpgradeRules["URogueUpgradeRuleAsset\n升级池规则"]

    GM --> SpawnSub["URogueSpawnSubsystem\n刷怪调度"]
    GM --> TrackerSub["URogueEnemyTrackerSubsystem\n敌人/掉落追踪"]
    GM --> PoolSub["URogueCombatPoolSubsystem\n对象池"]
    GM --> SceneSub["URogueSceneSubsystem\n场景优化"]

    SpawnSub --> Balance
    SpawnSub --> EnemyProfile["FRogueEnemyProfile\n敌人运行时数值"]
    EnemyProfile --> Enemy["ARogueEnemy\n敌人行为"]
    Enemy --> EnemyArchetype["FRogueEnemyArchetype\n敌人行为/视觉原型"]
    EnemyArchetype --> EnemyDT["敌人原型 DataTable\n编辑器配置"]

    RewardFlow --> UpgradeSystem["FRogueUpgradeSystem\n升级选择"]
    RewardFlow --> ShopSystem["FRogueShopSystem\n商店报价/刷新/购买"]
    ShopSystem --> RewardOffer["FRogueRewardOffer\n统一奖励报价"]
    UpgradeSystem --> UpgradeOffer["FRogueUpgradeOption\n升级卡"]
    RewardOffer --> UpgradeOffer

    GM --> HUD["ARogueHUD\nHUD桥接层"]
    HUD --> WBP["WBP_*\nShop / Menu / Death / Upgrade"]
```

## 战斗与武器结构

```mermaid
flowchart TD
    Character["ARogueCharacter\n玩家角色"] --> WeaponTable["DT_WeaponConfig\n武器数据表"]
    Character --> Weapons["ARogueWeaponBase 子类\n普通子弹/火箭/迫击炮/激光/地狱塔/镰刀"]

    WeaponTable --> WeaponRow["FRogueWeaponTableRow\n武器统一配置行"]
    WeaponRow --> PrewarmCollect["CollectPoolPrewarmClasses\n收集对象池预热类"]
    PrewarmCollect --> PoolSub["URogueCombatPoolSubsystem"]

    Weapons --> TrackerSub["URogueEnemyTrackerSubsystem\n索敌"]
    Weapons --> PoolSub
    Weapons --> Damage["ApplyDamage / 命中反馈"]

    PoolSub --> PlayerBullet["玩家子弹池"]
    PoolSub --> EnemyBullet["敌人子弹池"]
    PoolSub --> RocketPool["火箭/迫击炮弹池"]
    PoolSub --> LaserPool["激光池"]
    PoolSub --> BladePool["镰刀池"]
    PoolSub --> ImpactPool["命中特效池"]
    PoolSub --> EnemyPool["敌人池"]
    PoolSub --> PickupPool["经验/掉落池"]
```

## 商店、升级、未来道具扩展

```mermaid
flowchart TD
    BossKilled["Boss死亡"] --> GM["ARogueGameMode"]
    LevelUp["玩家升级"] --> GM
    ShopOpen["玩家打开商店"] --> GM

    GM --> UpgradeSystem["FRogueUpgradeSystem"]
    GM --> ShopSystem["FRogueShopSystem"]

    UpgradeSystem --> UpgradeOptions["FRogueUpgradeOption[]\n升级卡候选"]
    ShopSystem --> RewardOffers["FRogueRewardOffer[]\n商店奖励候选"]

    RewardOffers --> UpgradeReward["Upgrade奖励\n当前已接入"]
    RewardOffers -.未来扩展.-> RelicReward["Relic/Item奖励\n未来道具系统"]

    UpgradeReward --> Character["ARogueCharacter.ApplyUpgrade"]
    RelicReward -.未来扩展.-> RelicInventory["玩家道具栏/遗物栏"]

    ShopSystem --> ShopRules["价格规则\n购买同类型永久翻倍\n手动刷新本周期翻倍\n自动刷新重置刷新价"]
```

## 敌人配置结构

```mermaid
flowchart TD
    Balance["URogueGameBalanceAsset"] --> WaveEntries["EnemyWaveEntries\n敌人出现波次/权重"]
    Balance --> EnemyBalance["EnemyBalanceRows\n敌人血量/速度/伤害倍率"]

    SpawnSub["URogueSpawnSubsystem"] --> PickType["按当前波次加权选敌人类型"]
    PickType --> BuildProfile["RogueGameModeRules.BuildEnemyProfile/BuildBossProfile"]
    BuildProfile --> EnemyProfile["FRogueEnemyProfile"]
    EnemyProfile --> SpawnEnemy["对象池生成 ARogueEnemy"]

    SpawnEnemy --> EnemyDT["敌人原型 DataTable"]
    EnemyDT --> ArchetypeRow["FRogueEnemyArchetypeRow.ToArchetype"]
    ArchetypeRow --> DefaultArchetype["先套硬编码默认外观/碰撞"]
    DefaultArchetype --> DataOverride["再应用表格行为参数"]
    DataOverride --> RuntimeEnemy["运行时敌人行为\n移动/远程/地面波/视觉"]
```

## 当前职责边界

```mermaid
flowchart LR
    GameMode["GameMode\n只协调流程"] --> Systems["Core Systems\n升级/商店/规则"]
    GameMode --> Subsystems["World Subsystems\n刷怪/追踪/池/场景"]
    GameMode --> UIBridge["HUD\n只桥接数据到 WBP"]

    Data["DataAssets/DataTables\n编辑器配置"] --> Systems
    Data --> Subsystems
    Data --> WeaponsEnemies["Weapons / Enemies\n具体玩法执行"]

    Systems --> Character["Character\n实际应用升级/金钱/状态"]
    Subsystems --> WorldActors["Enemies / Pickups / Projectiles"]
    UIBridge --> WBP["UserWidget蓝图"]
```

## 后续扩展入口

- 加武器：优先扩展 `FRogueWeaponTableRow` 和对应 `ARogueWeaponBase` 子类，预热类通过 `CollectPoolPrewarmClasses` 收集。
- 加敌人：优先改 `ERogueEnemyType`、敌人平衡资产、敌人原型 DataTable，必要时补 `FRogueEnemyArchetypeRow::ToArchetype`。
- 加升级卡：优先走 `URogueUpgradeDefinitionAsset` 和 `URogueUpgradeRuleAsset`，逻辑落到角色或武器的升级处理函数。
- 加道具/遗物：复用 `FRogueRewardOffer`，新增 `Relic` 类型奖励，再做玩家道具栏和道具效果应用器。
