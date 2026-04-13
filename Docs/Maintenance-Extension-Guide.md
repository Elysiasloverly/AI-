# 维护与扩展指南

## 1. 文档目标

这份文档是给后续维护者和扩展者用的工程手册，核心目标有两件事：

1. 让你快速判断“一个新需求应该改哪一层”。
2. 尽量避免把新逻辑继续堆进 `GameMode`、`Character`、`HUD` 这种本来就容易变胖的类里。

这份项目已经做过一轮结构整理，目前整体思路是：

- **流程调度放在 Core**
- **玩家战斗放在 Player**
- **敌人行为和视觉拆到 Enemies**
- **短命战斗对象拆到 Combat**
- **地图、掉落、商店场景物放在 World**
- **界面和交互绘制放在 UI**
- **适合调参的内容尽量走 Data Asset**
- **运行时状态、对象池、追踪器、流程控制继续放代码**

---

## 2. 目录约定

当前源码目录按职责分层：

### `Source/ai/Public/Core` / `Source/ai/Private/Core`

负责“导演层”和基础服务：

- `RogueGameMode.*`
- `RogueGameModeRules.*`
- `RogueGameBalanceAsset.*`
- `RogueUpgradeSystem.*`
- `RogueUpgradeSystemRules.*`
- `RogueUpgradeDefinitionAsset.*`
- `RogueUpgradeRuleAsset.*`
- `RogueUpgradeEffectApplier.*`
- `RogueShopSystem.*`
- `RogueCombatPools.*`
- `RogueEnemyTracker.*`
- `RogueCombatFeedback.*`
- `RogueTypes.h`

### `Source/ai/Public/Player` / `Source/ai/Private/Player`

负责玩家和玩家侧战斗调度：

- `RogueCharacter.*`
- `RogueCharacterVisualComponent.*`
- `RogueWeaponConfig.h`
- `RoguePlayerBalanceAsset.*`

### `Source/ai/Public/Enemies` / `Source/ai/Private/Enemies`

负责敌人、敌方弹体、敌人行为原型、敌人视觉资源：

- `RogueEnemy.*`
- `RogueEnemyProjectile.*`
- `RogueEnemyArchetypes.*`
- `RogueEnemyVisualResources.*`

### `Source/ai/Public/Combat` / `Source/ai/Private/Combat`

负责战斗对象和表现：

- `RogueProjectile.*`
- `RogueRocketProjectile.*`
- `RogueLaserBeam.*`
- `RogueOrbitingBlade.*`
- `RogueImpactEffect.*`

### `Source/ai/Public/World` / `Source/ai/Private/World`

负责世界实体：

- `RogueArena.*`
- `RogueExperiencePickup.*`
- `RogueShopTerminal.*`

### `Source/ai/Public/UI` / `Source/ai/Private/UI`

负责 HUD、菜单、卡牌和商店绘制：

- `RogueHUD.*`

---

## 3. 先记住的扩展原则

### 3.1 什么时候该改 Data Asset

以下内容适合资产化：

- 数值
- 文案
- 出现规则
- 波次权重
- 商店基础价格
- 玩家基础属性
- 武器基础参数

当前已经支持资产化或半资产化的内容：

- `URogueGameBalanceAsset`
- `URoguePlayerBalanceAsset`
- `URogueUpgradeDefinitionAsset`
- `URogueUpgradeRuleAsset`

### 3.2 什么时候不要硬做资产化

以下内容继续放代码更合适：

- 对象池
- 运行时状态
- 战斗 Tick 流程
- 空间哈希与敌人查询
- HUD 点击命令流
- 伤害结算
- 投射物生命周期

原因很简单：这些东西不是“内容数据”，而是“执行机制”。

### 3.3 优先改哪一层

如果你拿到一个新需求，先按下面判断：

1. **只是调数值**  
   优先改 Data Asset。

2. **只是新增升级文案或升级数值**  
   优先改升级定义资产，必要时补规则。

3. **只是加一个敌人变体，行为模式现有逻辑能表达**  
   先改敌人枚举、原型、视觉资源、波次权重和数值表。

4. **只是加一个新武器，但本质还是“自动攻击 + 短命战斗对象”**  
   先沿现有武器模板扩。

5. **是新的运行机制**  
   才进入 `Character`、`Enemy`、`GameMode` 主流程。

---

### 3.4 武器玩法参数和表现参数怎么分

这是后续最容易重新写乱的一条边界，所以单独拿出来说。

当前项目里，武器相关参数应该分成两类：

#### A. 玩法参数

这些值决定“武器怎么玩”，应该放在：

- `RogueWeaponConfig.h`
- `URoguePlayerBalanceAsset`
- 必要时再配合升级系统和效果应用器

典型例子：

- `Damage`
- `Cooldown / Interval`
- `Range`
- `Count`
- `RefractionCount`
- `ExplosionRadius`
- `ProjectileSpeed`
- `DamageTickInterval`

判断标准很简单：

- 会影响平衡
- 会影响伤害曲线
- 会影响构筑路线
- 会被升级卡修改
- 会被玩家平衡资产覆盖

这种值不要塞进 `Projectile.cpp`、`RocketProjectile.cpp`、`LaserBeam.cpp` 的实现常量里。

#### B. 表现参数

这些值决定“武器看起来是什么样”，应该放在对应表现类自己的头文件配置里。

当前已经按这个思路整理过的类包括：

- `RogueProjectile`
- `RogueEnemyProjectile`
- `RogueRocketProjectile`
- `RogueLaserBeam`
- `RogueOrbitingBlade`
- `RogueImpactEffect`

典型例子：

- Mesh 缩放
- Glow 缩放
- Ring 尺寸
- 拖尾长度
- 灯光强度和范围
- 脉冲速度和脉冲幅度
- 命中特效缩放
- 最短生命周期
- 简化模式下的显示尺寸

判断标准也很直接：

- 改了不会影响平衡
- 只是视觉阅读性变化
- 只是某个 Actor 自己的显示细节
- 不该让升级系统或玩家数值层来关心

#### C. 为什么要这么分

如果混在一起，后面会出几种很常见的问题：

1. 调手感时找不到值到底在哪一层
2. 改视觉结果把玩法平衡一起带偏
3. `Character` 和 `Projectile` 同时持有相似概念，边界越来越糊
4. 同类武器很难形成统一扩展规范

#### D. 当前项目里的真实例子

##### 激光

- 激光武器玩法参数放在 `FRogueLaserWeaponConfig`
- 激光 Beam 的视觉厚度、环大小、最短显示时间放在 `ARogueLaserBeam`

##### 普通子弹

- 子弹速度、伤害、数量是玩法参数
- 子弹外层 glow、ring 尺寸、trail 脉冲是表现参数

##### 火箭

- 火箭伤害、爆炸半径、速度是玩法参数
- 火箭尾焰、光晕、尾迹、灯光脉冲是表现参数

#### E. 什么时候可以不分这么细

只有一种情况可以不严格拆：

- 这个对象是一次性的纯视觉 Actor，本身根本没有玩法地位

例如某些只负责展示的瞬时 UI 或极轻的环境效果。

但只要它是“武器的一部分”，而且后面可能继续做平衡、升级或配置化，就最好一开始就按这条边界分。

---

## 4. 如何添加一个新武器

这一节是最重要的，因为“新武器”通常会横跨多个系统。

当前武器族如下：

- 普通子弹：`Projectile`
- 镰刀：`Scythe`
- 火箭：`Rocket`
- 激光：`Laser`
- 地狱塔：`HellTower`

它们共同特点是：

- 武器参数由 `RogueWeaponConfig.h` 定义
- 玩家运行时由 `ARogueCharacter` 调度
- 武器基础数值可由 `URoguePlayerBalanceAsset` 覆盖
- 升级入口由 `ERogueUpgradeType`、升级规则、升级定义共同驱动

---

### 4.1 先决定：这是“现有武器族扩展”，还是“新武器族”

#### 情况 A：只是给现有武器加一个分支能力

例子：

- 激光增加“穿透”
- 火箭增加“多段爆炸”
- 地狱塔增加“锁定时间越久越快”

这种情况通常不需要新武器族，只需要：

1. 新增 `ERogueUpgradeType`
2. 新增升级定义
3. 新增升级出现规则
4. 在 `FRogueUpgradeEffectApplier` 里处理效果
5. 在该武器对应的运行逻辑里消耗这个新参数

#### 情况 B：这是一个全新的武器族

例子：

- 追踪雷球
- 扇形喷火
- 召唤无人机
- 连锁闪电塔

这种情况建议按下面完整路径走。

---

### 4.2 新武器族扩展清单

#### 第一步：定义武器配置结构

文件：

- `Source/ai/Public/Player/RogueWeaponConfig.h`

做法：

1. 新增一个 `USTRUCT(BlueprintType)` 配置结构。
2. 把这把武器的基础参数都放进去。

例如：

- `Count`
- `Cooldown`
- `Damage`
- `Range`
- `ProjectileClass`
- `ExplosionRadius`
- `TickInterval`

建议：

- 把“可调数值”都放这里
- 不要把运行时状态放这里
- 不要把单纯的表现参数也塞进来

运行时状态应该继续放在 `ARogueCharacter` 里，例如：

- 计时器
- 当前锁定目标
- 当前伤害层数
- 当前 Beam 刷新累积器

表现参数则应该放到对应武器 Actor 自己的头文件里，例如：

- `FRogueProjectileVisualConfig`
- `FRogueRocketVisualConfig`
- `FRogueLaserBeamVisualStyle`
- `FRogueEnemyProjectileVisualState`
- `FRogueOrbitingBladeVisualConfig`

---

#### 第二步：把配置接入玩家数值资产

文件：

- `Source/ai/Public/Player/RoguePlayerBalanceAsset.h`

做法：

1. 在 `URoguePlayerBalanceAsset` 里新增这个武器配置字段。
2. 在 `ARogueCharacter::ApplyBalanceAsset()` 中应用这份配置。

原则：

- **所有“开局默认数值”尽量都能从 `PlayerBalanceAsset` 覆盖**
- 不要把基础数值散落在 `Character.cpp` 里

---

#### 第三步：决定武器表现形态

你需要先判断它属于哪一类：

1. **飞行弹体型**
   - 参考：`ARogueProjectile`
   - 参考：`ARogueRocketProjectile`

2. **瞬发 Beam 型**
   - 参考：`ARogueLaserBeam`

3. **持续环绕/附着型**
   - 参考：`ARogueOrbitingBlade`

4. **纯逻辑武器，无需独立 Actor**
   - 参考：地狱塔的一部分逻辑仍然在 `ARogueCharacter`

如果是高频、短命、数量多的对象，建议**一开始就做进对象池**。

同时建议一开始就把“表现参数”收成独立配置，不要等 `cpp` 里写死常量越来越多之后再返工。

---

#### 第四步：新增战斗 Actor（如需要）

文件位置建议：

- `Source/ai/Public/Combat`
- `Source/ai/Private/Combat`

如果是玩家武器，一般放在 `Combat`。

建议接口风格和现有对象池一致：

- `ActivatePooledXxx(...)`
- `DeactivateToPool()`
- `IsAvailableInPool()`

这样才能直接接入 `FRogueCombatPools`。

如果你不确定怎么写，直接照着以下对象的接口风格抄：

- `RogueProjectile`
- `RogueRocketProjectile`
- `RogueLaserBeam`
- `RogueOrbitingBlade`

---

### 4.3 是否需要接入对象池

推荐标准：

#### 强烈建议池化

- 高频生成
- 生命周期短
- 同屏数量多
- 命中就销毁

例如：

- 子弹
- 火箭
- 命中特效
- 激光 Beam

#### 可以不池化

- 同屏数量少
- 生命周期长
- 每局创建次数很低

例如：

- 极少量召唤物
- 单个大范围持续技能

如果决定池化，需要改：

1. `FRoguePoolSettings`
2. `FRogueCombatPools`
3. `ARogueGameMode` 的 `Acquire...` 包装
4. 该 Actor 的激活/回池逻辑

---

### 4.4 把武器接入玩家调度

文件：

- `Source/ai/Public/Player/RogueCharacter.h`
- `Source/ai/Private/Player/RogueCharacter.cpp`

你通常需要做这些事情：

1. 增加一个配置字段
2. 增加必要的运行时状态
3. 增加一个 `HandleXxx(float DeltaSeconds)` 方法
4. 在 `Tick()` 中接入调度
5. 如果有 HUD 需要显示，增加 getter

当前 `ARogueCharacter` 的武器调度已经是分方法的，所以请继续沿用这个结构，不要重新把逻辑塞回 `Tick()` 大块里。

推荐模式：

1. `Tick()` 里只做调度调用
2. `HandleXxx()` 里完成计时、目标选择、开火
3. 具体飞行/爆炸/碰撞交给武器 Actor

---

### 4.5 把武器接入升级系统

如果你希望它能通过升级解锁或强化，需要改以下几层：

#### A. 新增升级类型

文件：

- `Source/ai/Public/Core/RogueTypes.h`

通常要新增至少一项：

- `XxxCount`：负责解锁/数量增加

如果还有专属能力，再继续加：

- `XxxRange`
- `XxxPierce`
- `XxxBounce`
- `XxxDuration`

#### B. 新增升级定义

文件：

- 默认代码兜底：`Source/ai/Private/Core/RogueUpgradeSystemRules.cpp`
- 资产化入口：`URogueUpgradeDefinitionAsset`

你至少要提供：

- `Title`
- `Description`
- `Magnitude`

#### C. 新增升级出现规则

文件：

- 默认代码兜底：`Source/ai/Private/Core/RogueUpgradeSystemRules.cpp`
- 资产化入口：`URogueUpgradeRuleAsset`

如果武器是“未持有时解锁，已持有时继续加数量”，建议保持现有模式：

- `XxxCount` 同时承担“解锁”和“数量 +1”

#### D. 在升级效果应用器里落地

文件：

- `Source/ai/Private/Core/RogueUpgradeEffectApplier.cpp`

这里是升级真正生效的地方。

建议保持现有边界：

- `UpgradeSystem` 负责发牌
- `UpgradeEffectApplier` 负责解释效果
- `Character` 负责提供被修改的运行时状态

不要重新把升级逻辑写回 `ARogueCharacter::ApplyUpgrade()` 里。

---

### 4.6 把武器接入 HUD

如果需要在 HUD 显示数量、层数、弹药或状态，需要改：

- `Source/ai/Private/UI/RogueHUD.cpp`

原则：

- HUD 只读角色状态，不自己算玩法逻辑
- 角色提供 getter，HUD 只负责显示

如果武器是复杂状态，例如：

- 当前锁定数
- 当前充能层
- 当前持续时间

建议给 `ARogueCharacter` 补 getter，而不是让 HUD 直接读内部字段。

---

### 4.7 新武器最小改动路径

如果你想最省事地加一把新武器，推荐顺序是：

1. `RogueWeaponConfig.h` 新增配置结构
2. `RoguePlayerBalanceAsset.h` 挂接配置
3. 新建 `Combat/XxxWeaponActor`
4. `ARogueCharacter` 中增加配置、计时器、处理函数
5. `RogueTypes.h` 增加升级类型
6. `RogueUpgradeSystemRules.cpp` 增加定义和出现规则
7. `RogueUpgradeEffectApplier.cpp` 增加应用逻辑
8. `RogueHUD.cpp` 增加显示
9. 如有必要，把它接进 `RogueCombatPools`

---

### 4.8 新武器完成后的验证清单

最少验证这些：

1. 开局不崩
2. 解锁卡能刷出来
3. 购买后能真正启用
4. 专属升级不会在未解锁前提前出现
5. 同类武器数量升级只影响该武器
6. 对象池复用后不会残留状态
7. 玩家死亡、重开一局后状态能正确重置
8. 商店能正确出售这类卡牌
9. HUD 数值能正确变化
10. 中后期高压场景不会爆性能

---

## 5. 如何添加一个技能卡

这里的“技能卡”指升级卡、商店卡、奖励卡，本质都走同一套 `FRogueUpgradeOption`。

### 5.1 最小流程

#### 第一步：新增枚举

文件：

- `Source/ai/Public/Core/RogueTypes.h`

新增一个 `ERogueUpgradeType`。

#### 第二步：新增定义

文件：

- `Source/ai/Private/Core/RogueUpgradeSystemRules.cpp`

在 `FindUpgradeOptionDefinition()` 的静态定义表里加一条。

如果你已经建立了资产化定义，则同步把它加到 `URogueUpgradeDefinitionAsset` 资产中。

#### 第三步：新增出现规则

文件：

- `Source/ai/Private/Core/RogueUpgradeSystemRules.cpp`

如果它是基础通用卡，就进 `BaseUpgradeTypes`。

如果它是某个武器专属卡，就进武器规则部分。

#### 第四步：让它真正生效

文件：

- `Source/ai/Private/Core/RogueUpgradeEffectApplier.cpp`

这里必须加，否则卡牌会“能抽到但没效果”。

---

### 5.2 什么时候只改资产就够了

如果升级类型已经存在，你只是想调整：

- 标题
- 描述
- 数值

那优先改：

- `URogueUpgradeDefinitionAsset`

如果只是想改“哪些卡默认会出、哪些武器解锁卡会出”，优先改：

- `URogueUpgradeRuleAsset`

---

### 5.3 技能卡常见错误

1. **只加定义，不加效果**
2. **只加效果，不加出现规则**
3. **专属卡在未解锁前就进池**
4. **HUD/商店文字还在显示旧描述**
5. **Magnitude 改了，但逻辑里没有按这个 Magnitude 生效**

---

## 6. 如何添加一个敌人

敌人系统已经被刻意整理成“原型 + 视觉 + 单类运行时”的结构，所以加敌人通常不需要新建一套完整类。

当前推荐做法是：

- **尽量复用 `ARogueEnemy`**
- 通过枚举、原型和视觉资源来扩新敌型

---

### 6.1 最低成本加一个敌人

如果你的新敌人能被现有行为模型表达，步骤如下：

#### 第一步：增加敌人类型枚举

文件：

- `Source/ai/Public/Core/RogueTypes.h`

新增一个 `ERogueEnemyType`。

#### 第二步：增加行为原型

文件：

- `Source/ai/Public/Enemies/RogueEnemyArchetypes.h`
- `Source/ai/Private/Enemies/RogueEnemyArchetypes.cpp`

这里决定：

- 移动模型
- 射击模式
- 各种距离参数
- 视觉 key

如果它能用现有模型表达，例如：

- 直冲：`Direct`
- 冲锋：`BurstCharge`
- 环绕：`Orbit`
- 远程拉扯：`RangedKite`
- 之字追击：`ZigZagChase`

那只需要补参数，不需要动 `ARogueEnemy` 主逻辑。

#### 第三步：增加视觉映射

文件：

- `Source/ai/Public/Enemies/RogueEnemyVisualResources.h`
- `Source/ai/Private/Enemies/RogueEnemyVisualResources.cpp`

这里负责把 `VisualKey` 映射到：

- Mesh
- Material

#### 第四步：增加数值和平衡

文件：

- 默认兜底：`Source/ai/Private/Core/RogueGameModeRules.cpp`
- 资产化入口：`URogueGameBalanceAsset`

你要补：

- 波次权重
- 血量倍率
- 速度倍率
- 伤害倍率
- 经验奖励修正
- Boss 倍率

#### 第五步：确认出生路径可选到它

`RogueGameMode` 不需要专门为每种怪写分支，它只会：

1. 选 `ERogueEnemyType`
2. 用 `RogueGameModeRules` 构造 `FRogueEnemyProfile`
3. 交给 `ARogueEnemy::InitializeEnemy()`

所以只要前几层补齐，出生链通常就能通。

---

### 6.2 什么时候需要改 `ARogueEnemy`

只有这两类情况才建议动：

#### A. 需要新的移动模型

例如：

- 瞬移
- 分段突进
- 保持扇形角度压迫
- 召唤型站桩敌人

做法：

1. 在 `ERogueEnemyMovementModel` 里加类型
2. 在 `GetMovementDirection()` 里补分支
3. 在 `RogueEnemyArchetypes` 里提供参数

#### B. 需要新的射击模式

例如：

- 五连射
- 螺旋弹幕
- 分裂弹
- 充能炮

做法：

1. 在 `ERogueEnemyShotPattern` 里加类型
2. 在 `HandleRangedAttack()` / `FireRangedShot()` 里补实现
3. 在原型里补所需参数

---

### 6.3 不推荐的做法

#### 不推荐一上来就新建一个完整敌人子类

原因：

- 会重复已有的血量、受伤、接触伤害、池化、注册表逻辑
- 会增加对象池和追踪器复杂度
- 会让敌人扩展方式变得不统一

只有当某个敌人和现有敌人类的生命周期完全不同，才考虑单开类。

---

## 7. 商店、掉落、经济怎么扩

当前商店相关责任如下：

- `FRogueShopSystem`：库存、价格、刷新、购买
- `ARogueShopTerminal`：场景中的交互终端
- `ARogueHUD`：商店界面绘制和点击
- `ARogueGameMode`：打开/关闭商店的流程调度
- `RogueGameModeRules` / `URogueGameBalanceAsset`：基础商店数值

### 7.1 当前价格规则

现在商店已经是两套价格曲线：

1. **卡牌价格按升级类型永久翻倍**
2. **手动刷新价格在当前补货周期内翻倍，下一个自动补货周期重置**

所以如果你后面要继续改商店，不要直接去 HUD 写死价格，优先改：

- `RogueShopSystem`
- `RogueGameModeRules`
- `URogueGameBalanceAsset`

---

## 8. 哪些地方最需要小心

### 8.1 `RogueCharacter` 仍然是扩展热点

虽然已经解耦过一轮，但它仍然掌握：

- 输入
- 移动
- 冲刺
- 玩家属性
- 多武器调度
- 地狱塔运行时状态

所以你加新武器时，最容易把这里重新变胖。

建议：

- 能抽到独立类的逻辑就抽
- 能进配置结构的参数就进配置
- 能进升级效果应用器的升级逻辑就不要写回角色本体

### 8.2 `HUD` 容易变成逻辑层

当前 HUD 负责：

- 画 HUD
- 画商店
- 画升级卡
- 处理点击命令

建议：

- HUD 只发“动作”
- 玩法判定尽量继续放在 `GameMode` / `ShopSystem`

### 8.3 高压短命对象一律优先考虑池化

当前已经池化的核心对象包括：

- 敌人
- 玩家子弹
- 敌人子弹
- 火箭
- 激光 Beam
- 命中特效
- 经验球
- 镰刀

如果你再加一个高频短命对象，不池化通常很快就会重新把中后期性能打回去。

---

## 9. 推荐的扩展流程

以后你接到一个新需求，推荐按这个顺序走：

1. 先判断它是“新内容”还是“新机制”
2. 能在 Data Asset 解决的先不改主流程代码
3. 先确定归属目录，不要边写边想放哪
4. 先补配置和枚举，再补运行逻辑
5. 高频对象优先做池化
6. 最后再补 HUD、商店、表现
7. 每做完一层就编译一次

---

## 10. 快速索引

### 添加武器最常改的文件

- `Source/ai/Public/Player/RogueWeaponConfig.h`
- `Source/ai/Public/Player/RoguePlayerBalanceAsset.h`
- `Source/ai/Public/Core/RogueTypes.h`
- `Source/ai/Private/Core/RogueUpgradeSystemRules.cpp`
- `Source/ai/Private/Core/RogueUpgradeEffectApplier.cpp`
- `Source/ai/Public/Player/RogueCharacter.h`
- `Source/ai/Private/Player/RogueCharacter.cpp`
- `Source/ai/Public/Combat/*.h`
- `Source/ai/Private/Combat/*.cpp`
- `Source/ai/Public/Core/RogueCombatPools.h`
- `Source/ai/Private/Core/RogueCombatPools.cpp`

### 添加技能卡最常改的文件

- `Source/ai/Public/Core/RogueTypes.h`
- `Source/ai/Private/Core/RogueUpgradeSystemRules.cpp`
- `Source/ai/Private/Core/RogueUpgradeEffectApplier.cpp`
- `Source/ai/Public/Core/RogueUpgradeDefinitionAsset.h`
- `Source/ai/Public/Core/RogueUpgradeRuleAsset.h`

### 添加敌人最常改的文件

- `Source/ai/Public/Core/RogueTypes.h`
- `Source/ai/Public/Enemies/RogueEnemyArchetypes.h`
- `Source/ai/Private/Enemies/RogueEnemyArchetypes.cpp`
- `Source/ai/Public/Enemies/RogueEnemyVisualResources.h`
- `Source/ai/Private/Enemies/RogueEnemyVisualResources.cpp`
- `Source/ai/Public/Core/RogueGameBalanceAsset.h`
- `Source/ai/Private/Core/RogueGameModeRules.cpp`
- `Source/ai/Public/Enemies/RogueEnemy.h`
- `Source/ai/Private/Enemies/RogueEnemy.cpp`

---

## 11. 最后的建议

后续扩展时，尽量保持这三条：

1. **内容数据进资产**
2. **运行机制进代码**
3. **新功能优先挂在已有模块边界上，而不是重新把核心类变成大杂烩**

如果你遵守这三条，这个项目后面继续加武器、敌人、商店机制、掉落机制，维护成本都会比较可控。
