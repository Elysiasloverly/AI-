# 属性系统迁移记录：从双数据源到单一数据源

> ## ⚠️ 本文是**历史记录**，描述的是一个**已被删除**的自研属性系统
>
> 文中提到的 `FRoguePlayerAttributeSet`、`FRoguePlayerAttributeSystem`、`FRogueUpgradeEffectApplier`、
> `SyncAttributesFromSystem`、`BaseGroup` / `UpgradeGroup` 与三个计算器（Additive/Multiplier/Clamp）
> —— **当前代码里全部不存在**。请勿据此阅读源码，否则会找不到任何对应实现。
>
> **当前架构**：属性系统已整体迁移到 UE 的 **GameplayAbilitySystem（GAS）**，
> 唯一数值真相源是 `URogueAttributeSet`。请先阅读：
>
> | 想知道 | 看这里 |
> |---|---|
> | 现在是什么结构 | [Project-Structure.md](./Project-Structure.md) |
> | 怎么迁到 GAS 的、有什么验证证据 | [GAS-Main-Architecture-Plan.md](./GAS-Main-Architecture-Plan.md) |
> | 接下来打算做什么 | [Optimal-Portfolio-Plan.md](./Optimal-Portfolio-Plan.md) |
>
> **本文保留的价值**：它记录了"为什么曾经自研、自研方案暴露了哪些问题"——
> 这恰好解释了后来为什么选择 GAS。
>
> ⚠️ **第 6 节「遗留问题」针对的是那套自研系统**：`reinterpret_cast` 类型双关、
> Multiplier/Clamp 计算器未使用等问题**随系统一起消失了**，
> 不要试图在现网代码里找它们（现在的属性钳制走 `PreAttributeChange`，见 `RogueAttributeSet.cpp`）。

---

## 1. 背景：自研属性系统是什么

项目实现了一套**模板化属性系统**，用于管理玩家基础属性与升级加成的结算：

```
FRoguePlayerAttributeSet          —— 属性集合（MaxHealth / MoveSpeed / MaxArmor / DashCooldown ...）
FRoguePlayerAttributeSystem       —— 属性系统（持有 BaseGroup + UpgradeGroup + 3 个计算器）
  ├─ BaseGroup                    —— 基础属性修饰器组（来自 BalanceAsset）
  ├─ UpgradeGroup                 —— 升级属性修饰器组（来自升级选择）
  └─ Calculators                  —— 加算 / 乘算 / 钳制 三个计算器，按阶段依次结算
```

核心机制：每个属性 = 一组 `Modifier`（修饰器），按 `Operation`（Additive/Multiplier/Clamp）分阶段计算，`CalculateAttribute` 把铺平后的修饰器排序、按「属性 + 阶段」分组交给对应计算器。

**设计目标**：基础属性与升级加成**分离结算**，未来可扩展乘算/钳制类升级（如"伤害 ×1.5"）。

---

## 2. 迁移前的问题：双数据源

迁移前，升级逻辑存在「两套数据源」：

```cpp
// 旧代码：FRogueUpgradeEffectApplier::ApplyToCharacter
case ERogueUpgradeType::MaxHealth:
    Character.MaxHealth += Upgrade.Magnitude;   // ① 直接改旧字段（实际生效）
    if (AttrSys) AttrSys->GetUpgradeGroup()->Add(...);  // ② 顺手喂属性系统（但没被消费）
    break;
```

问题：
- 升级既直接改旧字段，又往属性系统加 Modifier，但**没有 Sync**——属性系统的计算结果从未被真正使用；
- 属性系统沦为「装饰品」，只承担了基础属性的计算，升级路径没有走它；
- 存在两套数据源，靠手动 `SyncAttributesFromSystem` 维持一致，脆弱且冗余。

---

## 3. 迁移内容

### 3.1 `FRogueUpgradeEffectApplier::ApplyToCharacter`（重写）

升级不再直接改旧字段，改为「**只写 UpgradeGroup → 末尾统一 Sync → 做联动**」：

- **8 个数值属性**（MaxHealth / MoveSpeed / MaxArmor / DashCooldown / PickupRadius / Recovery / Armor / ExperienceGain）→ 走属性系统单一数据源；
- **武器类升级**（AttackPower / AttackSpeed / AttackRange / ProjectileSpeed / 各 Count）→ 保持原样，走 `ApplySharedWeapon*Bonus` / `DispatchWeaponUpgrade`；
- **联动**移到 Sync 之后：升级 MaxHealth/ArmorCapacity 时等量回复生命/护甲，DashCooldown 收缩剩余冷却。

### 3.2 `SyncAttributesFromSystem`（改一行）

给冲刺冷却加下限钳制，防止属性系统累加 `-Magnitude` 把冷却减成负数：

```cpp
DashCooldownDuration = FMath::Max(Attr->DashCooldownDuration, DashMinCooldown);
```

---

## 4. 迁移后的数据流

```
升级选择 → ARogueCharacter::ApplyUpgrade（当时是 FRogueUpgradeEffectApplier::ApplyToCharacter）
   → 数值属性：UpgradeGroup->Add(Magnitude, Additive, &字段)
   → 武器属性：分发到武器
   → 统一 SyncAttributesFromSystem()
       → GetFinalAttributes() → CalculateAttribute()（脏标记触发重算）
           → 清零 → 加算 Base + Upgrade → 得到最终值
       → 写回运行时字段
   → 联动：生命/护甲等量回复、冲刺冷却收缩
```

**核心收益**：属性系统成为**唯一权威数据源**，旧字段只是缓存。未来加乘算/钳制升级，只需在 `UpgradeGroup` 里加对应 `Operation` 的 Modifier，无需改结算逻辑。

---

## 5. 测试结论

- 编译通过，编辑器正常启动；
- PIE 运行无崩溃、无 Assertion、无运行时错误；
- 升级「最大生命 / 移动速度 / 冲刺冷却 / 护甲上限」等数值表现均正常。

---

## 6. 遗留问题与说明

| 事项 | 状态 | 说明 |
|---|---|---|
| `GetModifiers()` 的 `reinterpret_cast` | 保留未改 | `TSharedPtr<Derived>` 强转 `TSharedPtr<Base>`，单继承下可正常工作，属 UB 边缘。权衡后暂不动，避免引入风险 |
| `@todo 数据切片` 注释 | 确认为误标 | 实际是上述 reinterpret_cast 类型双关，非真正切片 |
| Multiplier / Clamp 计算器 | 已就位未使用 | 当前升级全部为 Additive，乘算/钳制计算器为未来扩展预留 |
