# 可重复的性能测试

这套测试用于比较同一台电脑上代码修改前后的开销。测试入口仅在开发构建启用，正常游戏不会运行，也不会改地图、数据表和正式冰砖存档。

## 测试条件

- UE 5.5，BakaEditor Win64 Development，独立 `-game` 进程。
- 1920×1080、DX12、真实离屏渲染，保留当前项目画质；关闭垂直同步与帧率上限。
- 白天空场、白天 200 敌人＋4 张归心符、黑夜 400 敌人＋20 张归心符，共三档。
- 敌人为突击、步枪和重装三类正式蓝图，固定黄金角分布，AI 和攻击照常运行。为了使前后人数、位置保持可比，存活敌人的移动组件在本测试里禁用，敌人高血量、玩家无碰撞，不会通过击杀和走位减轻负载。
- 战斗档另有其余六种武器各 3 件，基于正式武器表的内存副本，定向开火冷却设为 0.65 秒。固定的是压力场景，不是某一波的最终平衡配置。
- 原有对象池仍按项目配置预热；空场因此可以发现池中对象的后台开销。
- 每档 3 轮，每轮预热 15 秒后采集 20 秒。预热中截图，截图不进入采样窗口。
- 音频关闭。因此本测试不测混音成本，也不覆盖联机复制、实际移动寻路、完整通关及打包 Shipping 性能。

## 运行

先保存并关闭 UE 编辑器，编译一次 Development Editor，再分别使用不同标签采集：

```powershell
& 'C:\Users\Elysia\AppData\Local\Programs\Python\Python310\python.exe' 'F:\AI_WorkSpeace\baka\Tools\capture_performance.py' --label after
```

默认输出目录：`Saved/Profiling/RoguePerformance/after`。同名 CSV 已存在时工具会停止，避免覆盖旧证据。`--repeat`、`--warmup`、`--seconds` 可调整；做前后对比时必须保持一致。

也可单独执行开发控制台命令：

```text
Rogue.Perf.Capture label enemyCount talismanCount warmupSeconds captureSeconds absoluteOutputFolder
```

完成 CSV 写入后，此命令会退出当前测试进程。不要在需要保留的游戏会话里执行。

## 指标口径

`FrameTime` 是实际逐帧时间，FPS 使用 `1000 / 平均帧时间` 换算；P95/P99 越低，较慢帧越稳定。每档报告取三轮均值/分位数的中位数，并保留每轮数据，避免把最快一次当结果。

CSV 同时提供 `GameThreadTime`、`RenderThreadTime`、`GPUTime`，以及 `Exclusive/GameThread/ProjectileMovement`、`CharacterMovement` 等引擎分类。线程并行并可能含等待，不能将这些列相加。`RoguePerf` 自定义区域用于观察索敌、池取用、归心符扫掠/表现和测试驱动本身；其中也有包含关系。

必须检查 `AliveEnemies`、`Talismans`、`ActiveProjectiles`、Draw Calls 和截图，防止把少画敌人、弹幕被提前消耗或关闭渲染产生的数字当成优化。不同机器、地图视角、开发/打包配置之间的 FPS 不能直接比较。

## 2026-11-01 修改

1. 敌人回池时单独暂停 CharacterMovement Tick，激活后恢复之前的开关并强制更新地面；重复回池不会丢失恢复状态，客户端活动位同步也应用相同规则。
2. 增加 `RogueEnemyProjectile` 对象通道（GameTraceChannel1）。敌人胶囊直接忽略该通道，敌方弹体继续对玩家 Pawn 重叠、对场景 WorldStatic 阻挡。正式子蓝图的旧 ObjectType 在运行时纠正，模型、材质、尺寸保持蓝图配置。
3. 归心符扫掠预筛选不再先按中点排序，保留从出发点进行的最终命中顺序排序。

没有缩短 30 秒弹体寿命，没有削减怪物/武器数量，也没有修改地图灯光或降低画质。池容量和战斗数值保持原配置。

## 直线敌方弹幕的新旧路径对照

增加了 `--paired --scenes day200 night400`，在同一个构建里交替使用组件模拟与集中更新，每档默认三对。也可通过 `--batch-mode component` 或 `--batch-mode batch` 单独采集。原理、兼容范围和测试入口见 [Enemy-Bullet-Batching.md](Enemy-Bullet-Batching.md)。

## 真实战斗补测

正常移动、受伤、击杀、拾取、选卡和双进程联机的 18 份补测见 [Real-Battle-Benchmark.md](Real-Battle-Benchmark.md)。实战负载和存活时长会变化，应结合实际敌人、弹幕数量和采样长度解读，不能与固定压力场景直接计算优化比例。

后续 P0 的卡顿归因、资源就绪检查、双端准备同步与九份 60 秒长样本见 [Performance-P0.md](Performance-P0.md)。新入口会等待实际队列稳定，采样期间不再截图；仍保留后续新编译与慢帧，不将其删除。
