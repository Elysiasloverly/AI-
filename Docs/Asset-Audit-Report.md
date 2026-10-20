# 资源审计报告（新旧冲突 / 无用老资源）

- 审计时间：2026-10-21 00:50
- 审计方式：AssetRegistry 全量扫描（硬引用 + 软引用），只读，未删除任何资产
- 复现命令：
  ```bash
  UnrealEditor-Cmd.exe ai.uproject -run=pythonscript -script="Tools/audit_assets.py" -unattended -nopause -nullrhi -nosplash -abslog="Saved/Logs/audit.log"
  ```
- 原始数据：`Saved/DataDump/asset_audit.json`（1000 条资产明细，含每条引用关系）

## 结论摘要

| 项目 | 数量 | 体积 |
|---|---|---|
| 资产总数（注册表） | 1000 | 1.55 GB |
| 重定向器残留 | **0** | — |
| 完全无引用资产 | 405 | **697.9 MB** |
| 其中旧版 StarterContent | 260 | **611.4 MB** |
| 断链依赖（指向已不存在的旧路径） | 4 条 / 3 张地图 | — |
| 项目自有目录中的无用资产 | **7 个** | 约 0.3 MB |
| 重名资产组 | 354 组（两边都在用 15 组，单边在用 288 组） | — |

**核心结论**：项目自有内容（BP / Data / UMG / Input / Map）很干净——没有重定向器、没有重名冲突、数据资产引用全部有效；
真正的新旧冲突集中在**导入的素材包**：`GameAssets/StarterContent` 与 `GameAssets/Library/StarterContent` 是同一套 StarterContent 的两份拷贝，
旧的一份只剩 3 个资产被项目真正使用，其余 260 个（611 MB）已无人引用。

---

## 一、新旧资源冲突

### 1. 重复地图（旧副本）

| 地图 | 状态 | 证据 |
|---|---|---|
| `/Game/GameAssets/StarterContent/Maps/Minimal_Default` | **现役** | `DefaultEngine.ini` 的 GameDefaultMap 指向它 |
| `/Game/Map/Minimal_Default` | **旧副本** | 引用数 0；加载后与现役地图**完全一致**（各 20 个 actor，含 1 个 PlayerStart、1 个 RogueEnemySpawnRegion、1 个 BP_RogueShopTerminal）；文件时间早 18 小时 |

### 2. 地图内残留旧目录路径（断链来源）

`Content/StarterContent` 已在资源重组时移到 `Content/GameAssets/`，但三张地图里还各留了 1–2 处旧路径字符串：

| 地图 | 断链引用 |
|---|---|
| `Minimal_Default`（现役） | `/Game/StarterContent/Maps/Minimal_Default` |
| `StarterMap`（样板图） | `/Game/StarterContent/Maps/StarterMap`、`.../StarterMap_BuiltData` |
| `Advanced_Lighting`（样板图） | `/Game/StarterContent/Maps/Advanced_Lighting_BuiltData` |

影响：当前加载**没有报错**（已实测两张地图都能正常打开、actor 完整），属于脏引用而非功能故障。
处置：打开地图 → 重新保存一次（编辑器会自动剔除失效引用），或对三个包做 Fix Up Redirectors。

### 3. 双份素材包（最大的冲突源）

| 目录 | 资产数 | 有引用 |
|---|---|---|
| `/Game/GameAssets/StarterContent` | 267 | **7** |
| `/Game/GameAssets/Library/StarterContent` | 350 | 262 |

同名重复共 354 组。其中 **15 组两边都在被使用**（同一张贴图/材质存在两份、且都有引用方），
例如 `Shape_Cube`、`Shape_Sphere`、`M_Metal_Copper`、`T_Fire`、`T_White`、`T_Noise_04`、`VectorField_A`。
这类不是"坏了"，但会让体积翻倍、并让后续替换美术时容易改错一份。

`Realistic_Starter_VFX_Pack_Vol2`、`Vefects`、`HologramShader` 也各有部分资产在老位置/`Library` 之间交叉引用。

### 4. 数据层与代码层：未发现冲突

- `DT_EnemyArchetypes` 13 行全部指向新命名的弹体 BP（`_Marksman` / `_Cannoneer` / `_Repeater` / `_Ringcaster` 等），无旧名引用；
- `DA_GameBalance`、`DA_UpgradeDefinitions`、`DA_UpgradeRules` 均被 `BP_RogueGameMode` 正常引用；
- `DT_WeaponConfig` 被 `BP_RogueCharacter` 引用；
- 全项目 **ObjectRedirector 数量为 0**，说明改名时没有留下重定向器；
- `DefaultEngine.ini` 里 3 条历史 `StructRedirects`（`AttributesModifier` 等）属于旧类型兼容，保留无害。

---

## 二、无用老资源清单（项目自有，建议处理）

| 资产 | 大小 | 判定依据 | 建议 |
|---|---|---|---|
| `/Game/BP/TestPickup` | 25.6 KB | 引用数 0，测试用拾取物 | 归档/删除 |
| `/Game/BP/NewBlueprint` | 58.1 KB | 引用数 0，4 月创建的空白蓝图 | 删除 |
| `/Game/BPEnum/EN_OrbiterMoveEnum` | 2.7 KB | 引用数 0；移动模型已是 C++ 枚举 `ERogueEnemyMovementModel` | 删除 |
| `/Game/BP/BP_RogueArena` | 56.7 KB | 引用数 0；`Docs/Map-Authoring.md` 明确"GameMode 不再要求 BP_RogueArena" | 删除（地图未放置） |
| `/Game/BP/Enemy/Projectiles/BP_EnemyProjectile` | 38.8 KB | 引用数 0；变体已改为独立 BP（`make_projectile_variants_independent.py`），文档定位为"占位模板" | 保留或删（模板价值自行判断） |
| `/Game/BP/Enemy/Projectiles/BP_EnemyProjectile_Boss` | 41.0 KB | 引用数 0；`BossProjectileClass` 无任何 BP/数据表指向它 | 删除，或接到某个敌人 BP 的 Boss 弹体槽 |
| `/Game/Map/Minimal_Default` | 189 KB | 引用数 0，与现役地图内容一致的旧副本 | 归档 |

> 删除前注意：这些资产目前**没有任何引用者**，但删除属于不可逆操作，建议在编辑器内删除（会进回收站）而不是直接删磁盘文件。

### 连带释放

上表中 4 个资产是 `StarterContent` 里 3 个素材的**唯一引用者**：
`M_Metal_Copper`（只被 `BP_EnemyProjectile_Boss` 用）、`Shape_Cylinder`（只被 `BP_RogueArena` + `BP_EnemyProjectile` 用）、
`Shape_Sphere`（只被两个弹体 BP 用）。删掉这些 BP 后，它们也会变成无引用。
`StarterContent/Shapes/Shape_Cube`、`Shape_Torus`、`Materials/M_Tech_Hex_Tile_Pulse` 仍被现役资产引用，**不能删**。

---

## 三、假阳性说明（看起来无引用，实际在用）

| 资产 | 为什么不是孤儿 |
|---|---|
| `/Game/Data/Player/DA_PlayerBalance` | 由 `RogueCharacter.GAS.cpp` 的默认路径 `/Game/Data/Player/DA_PlayerBalance` 加载，不走资产引用 |
| `/Game/BP/BP_RogueCharacter` | `DefaultGame.ini` 的 `PlayerPawnClass` 配置引用 |
| `WBP_PlayerVitals` / `WBP_EnemyHealthBar` / `WBP_DamageFlash` / `BP_ExperiencePickup` / `IMC_Default` | `DefaultGame.ini` / `DefaultInput.ini` 配置引用 |
| `Minimal_Default`（现役） | `DefaultEngine.ini` 的 GameDefaultMap |

审计脚本已把 ini 里出现的 `/Game/` 路径标记为"配置在用"；C++ 硬编码路径需要人工核对（上表已核）。

---

## 四、建议的清理顺序

1. **关闭编辑器**（当前有 DebugGame 实例在跑），先备份 `Content/` 与 `Config/`。
2. 删项目自有 7 个无用资产（编辑器内删除，走回收站）。
3. 打开三张地图各保存一次，消掉 4 条旧路径断链；确认 `Minimal_Default` 的灯光构建数据仍正常。
4. 归档 `/Game/Map/Minimal_Default`（确认现役地图无误后）。
5. 评估 `GameAssets/StarterContent` 剩余 260 个未引用资产（611 MB）：
   建议先把 `Content/GameAssets/StarterContent` 整体移出工程（或打成 zip 备份后删除），
   跑一遍启动 + 一波战斗验证没有紫黑材质/丢失模型，再决定是否永久删除。
6. 15 组"两边都在用"的重名资产属于长期治理项，建议后续统一到 `Library/` 一份、把另一份的引用重定向后删除。

> 本次审计未修改任何资产。需要我按上面的顺序执行清理（每步先备份、可回滚）时再说一声。
