# 界面样式、Boss 血条与平滑状态条

更新于 2026-10-24。当前采用较克制的东方符卡风格：菜单为靛蓝月夜鸟居，卡牌为浅色和纸、细朱红边饰和淡阴阳符印；战斗状态条采用深色底，保证场景中的可读性。所有界面均保存为实际 WBP，可在设计器里继续修改。

## 常用编辑位置

| 界面 | 内容浏览器路径 | 调整入口 |
|---|---|---|
| 主菜单 | `/Game/UMG/Menus/MainMenu/WBP_MainMenu` | 背景、左侧菜单面板、按钮样式 |
| 升级选卡页 | `/Game/UMG/Menus/Upgrade/WBP_UpgradeSelection` | 标题、提示、三张卡的容器与间距 |
| 升级卡 | `/Game/UMG/Menus/Upgrade/WBP_UpgradeCard` | 标题、说明和“点击选择”的布局 |
| 商店 | `/Game/UMG/Menus/Shop/WBP_Shop` | 标题与钱包、三列两行商品区、底部刷新和退出 |
| 商品卡 | `/Game/UMG/Menus/Shop/WBP_ShopOfferCard` | 卡牌外观、说明、独立价格区 |
| 升级树 | `/Game/UMG/Menus/Progression/WBP_UpgradeTree` | 整页、分页与连线颜色 |
| 升级树节点 | `/Game/UMG/Menus/Progression/WBP_UpgradeTreeNode` | 纵向流式排列的标题、说明、状态和购买行 |
| 玩家状态条 | `/Game/UMG/HUD/WBP_PlayerVitals` | 血条、护甲条、经验条、波数与金币 |
| Boss 血条 | `/Game/UMG/HUD/WBP_BossHealthBar` | 屏幕上方中央的大血条、标题和血量文字 |

商店的标题与钱包分居左右，说明与补货时间各占一行；商品名称、说明和价格各占独立区域，底部操作留在面板内。选卡页的三张卡居中等宽排列。商品卡原有蓝图里的两处蓝色按钮底色赋值已移除，避免每次刷新内容时覆盖设计器的新样式。

保留原有绑定名称后，C++ 会继续填入文本和数值、处理点击。例如商品卡的 `Button_Offer`、`Border_Card`、`Text_Title`、`Text_Desc`、`Text_Cost`，以及选卡的 `HorizontalBox_Cards`。可以改尺寸、字体、颜色和容器，但改控件名或父类时需要同步原有绑定。商品购买、刷新和选卡仍通过 HUD 原有请求链执行。

`RogueUIThemeBuilder` 是编辑器内一次性生成与应用样式的工具，运行游戏不会再次应用主题或覆盖设计器修改。不要在有手工修改后盲目重新执行批量生成脚本；脚本会重建部分控件树。

## 生成素材

图片由内置 `image_gen` 生成功能生成，原始图片已复制入项目：

- `Content/GameAssets/UI/Touhou/Source/touhou_shrine_night_calm_v2.png`：当前菜单背景，月夜鸟居与稀疏弹幕弧线。
- `Content/GameAssets/UI/Touhou/Source/touhou_spell_seal_v2.png`：透明符印，只作小尺寸、低透明度的角落水印。
- `Content/GameAssets/UI/Touhou/Source/touhou_background_minimal.png`：保留的上一版浅色淡墨背景。
- `Content/GameAssets/UI/Touhou/Source/touhou_panel_paper.png`：浅色面板纸纹；UMG 只采样中央区域，不显示繁复边缘。
- `Content/GameAssets/UI/Touhou/Source/touhou_panel_indigo.png`：战斗 HUD 深色面板纸纹，同样只采样中央。

导入纹理位于 `/Game/GameAssets/UI/Touhou/Textures`。背景最大尺寸为 2048，符印为 512，和纸面板为 256，均使用 UI 纹理设置与无 Mip。当前按钮使用 WBP 中的圆角细描边样式，普通、悬停、按下与禁用状态可以在按钮的 Style 中分别编辑。最初的 `touhou_menu_background` 和装饰按钮仍保留为素材草稿。

当前素材提示词见 [UI-Spellcard-Art-Prompts.json](UI-Spellcard-Art-Prompts.json)。上一版提示词见 [UI-Art-Prompts.json](UI-Art-Prompts.json)，其中的 `selected_background` 记录了当时的“浅色和纸底＋淡墨鸟居轮廓”。

## 玩家生命与护甲的变化

真实数值和伤害仍由 GAS 结算。文字实时显示真实生命与护甲；只有进度条比例进行平滑插值。受击后主条追随真实值，残影短暂停留后消退，连续受击重新计时；治疗正常追随目标。首次显示、濒死、复活或容量变化时重新对齐，避免沿用上一状态的残影。

在 `WBP_PlayerVitals` 的 Class Defaults → `Rogue|血条动画` 可调：

| 参数 | 默认值 | 用途 |
|---|---:|---|
| 启用平滑血条 | 开 | 关闭后即时显示真实比例 |
| 主血条变化速度 | 14 | 越大追随越快 |
| 受击残影停留时间 | 0.2 秒 | 受击后残影等待多久开始回落 |
| 受击残影消退速度 | 4 | 越大残影消退越快 |

`HealthDamageBar` 和 `ArmorDamageBar` 是可选残影条，位于主条下面。删掉它们不影响真实属性与主条结算。

### 状态变化反馈与紧凑排列（2026-10-24）

`WBP_PlayerVitals` 的左下区域改为自动高度的纵向排列。未解锁护甲时，整行收拢；已解锁且护甲耗尽时，仍保留空条及数值。左上角波数和金币保持原位置。

受伤、治疗及经验增长会显示短暂的变化数值，同行有轻微放大和圆角边光。护甲降到 0 提示“护甲破裂”；低生命仅有很轻的呼吸边光。升级时经验条先精确到满，短暂停留后归零追随新等级的真实比例；一次连升多级只提示最终等级。生命、护甲和经验文字始终立即显示真实值。

Class Defaults → `Rogue|状态反馈` 可关闭反馈、变化数值或低生命提示，也可调整强调时间、上浮距离和缩放幅度。默认强调 0.28 秒、数字停留 0.7 秒、上浮 10、缩放幅度 0.012。`Rogue|血条动画` 增加经验增长速度 10、升级满条过渡 0.24 秒。首次显示、隐藏后重现、换 Pawn、濒死及复活会重建基准；生命或护甲容量改变时也重新对齐对应条，不把新容量误判成受击或治疗。

设计器可改 `HealthRow` / `ArmorRow` / `ExperienceRow` 的排版，`HealthFeedback` / `ArmorFeedback` / `ExperienceFeedback` 的边光外观，以及对应 `*ChangeText` 的字体颜色。基类保留这些控件的原始变换与样式，在动画结束后恢复。绑定均为可选；请保留命名以使用默认反馈。

### 敌人头顶血条

`/Game/UMG/HUD/WBP_EnemyHealthBar` 使用深色细边框、红色主条与暖色伤害残影，外框及填充均为随高度变化的胶囊圆角。填充采用 Scale，下降的端头也保持圆润。普通条默认 96×10，Boss 头顶条宽度为 1.25 倍，屏幕上方的大 Boss 血条仍由独立 WBP 显示。

Class Defaults → `Rogue|EnemyBar|变化效果` 可调主条速度（18）、残影停留（0.14 秒）、残影速度（7）、受击提亮时长（0.14 秒）、强度（0.48）和颜色。正常填充颜色读取设计器的 `FillBar`；`DamageBar` 可单独改颜色。血条不抖动、不改变实际生命。

HUD 的控件池可能把同一个血条交给另一只敌人，敌人 Actor 也会重复出池。因此显示基类同时比较本地 Actor、生成周期及已复制的表现代次，换对象时直接对齐并清除旧残影。隐藏及销毁也清理动画；无需新增网络复制数据。

本轮 Development Editor 编译通过，两个 WBP 编译保存成功。10 项相关测试均成功，覆盖实际绑定、样式归位、受击/回血/破甲、经验跨级与多级提示、濒死复活、HUD 生命周期和敌人对象池重置。另用实际 WBP 录制 39 帧连续画面检查表现，预览数据是本地样例视图，不会改 GAS 或真实存档；本轮未重复执行全量玩法或双进程联机测试。

动画预览与验证记录位于工作区 `outputs/Vitals-Feedback-2026-10-24`，原件及改动冲突检查记录在 `work/vitals-feedback-2026-10-24`。本轮动态预览工具只属于编辑器模块，正常游戏不会创建预览界面。

## Boss 血条与联机

服务器从已登记且仍存活的 Boss 中选择显示对象，将生命、最大生命和生成周期 ID 写入 GameState 的 `BossHealthView` 并复制。HUD 读取这份视图，因此 Boss 在摄像机外、Actor 超出普通相关距离时，客户端仍能获得血量视图。

显示对象死亡、回收到池或游戏结束时重新评估；同时有多个 Boss 时保持当前对象，当前对象消失后再切换。生成周期 ID 区分对象池复用，换 Boss 时进度条直接对齐。HUD 销毁时移除血条，不依赖玩家当前是否存在 Pawn。

配置入口为 `DefaultGame.ini` 的 `BossHealthBarWidgetClassPath`。Boss WBP 需保留 `FillBar`，可选 `DamageBar`、`HealthText`。Class Defaults 中可调插值速度 14、残影延迟 0.25 秒和消退速度 4，也可以通过 `GetBossView` 与 `OnBossHealthUpdated` 添加自己的外观。

## 验证与备份

本次使用 Development Editor 配置；未重复编译 DebugGame。针对性验证覆盖血条受击、治疗、帧率差异，实际 WBP 绑定与濒死复活重置，Boss 死亡、对象池复用、团灭隐藏及 HUD 生命周期。

双独立进程测试中，房主和加入者均观察到屏幕外 Boss 的 `500/500 → 250/500 → 隐藏`。界面预览使用实际 WBP 和演示视图数据，不执行商品购买；完整游戏中还验证了受伤状态条和真实升级选卡显示。

工作记录与原始备份在 `C:/Users/Elysia/Documents/Codex/2026-10-13/f-ai-workspeace-ai/work/touhou-ui-2026-10-23`。写入前检查原文件与已安装版本的哈希，发现其他修改会停止覆盖。测试使用 `QA_Touhou*` 独立存档槽。


## 2026-10-24：其余界面的排版统一

本轮重排 11 个 WBP，沿用浅色和纸、深色正文和简洁按钮：

| 界面 | WBP 名称 | 调整 |
|---|---|---|
| 设置 | `WBP_SettingsMenu` | 五行设置的标签、当前值、左右调整按钮对齐，状态与应用操作独立 |
| 暂停菜单 | `WBP_PauseMenu` | 紧凑居中面板，统一按钮尺寸，注明战斗仍会继续 |
| 游戏结束 | `WBP_DeathScreen` | 标题、战绩摘要与重开/主菜单/退出三项操作分区 |
| 等待复活 | `WBP_ResurrectionScreen` | 移除固定分辨率的全屏模糊，突出倒计时并保留战场可见 |
| 多人选择 | `WBP_MultiplayerMenu` | 昵称、房间名、创建/加入操作分组；输入框采用浅色底 |
| 局域网搜房 | `WBP_RoomBrowser` | 房间列表与 IP 直连分区，刷新与加入按钮位置固定 |
| 房间大厅 | `WBP_Lobby` | 名单、准备状态、准备/房主开始/离开操作分区 |
| 房间/玩家行 | `WBP_RoomEntry`、`WBP_LobbyPlayerEntry` | 房间名或玩家名、人数/状态与操作列对齐 |
| 教程 | `WBP_Tutorial` | 正文使用独立滚动区域，返回按钮固定，统一行距 |
| 昼夜切换 | `WBP_NightTransition` | 屏幕上方居中提示，标题、倒计时与说明分区，继续显示战场 |

此次复用并移动原有命名控件，按钮事件图和原有回调保持一致。生成后仍可直接在 WBP 设计器里修改尺寸与样式；输入框的字体、底色和边框在各自 Style 中调整。

`RogueUIThemeAdditional.cpp` 只属于编辑器模块，保存后的 WBP 在游戏中独立加载。测试使用实际 WBP 配合演示视图数据验证排版，多人预览中的房间与玩家名单是示例，不会搜索网络、创建房间或发送准备请求。

Development 编译通过；11 个 WBP 编译与保存成功，核对事件图节点未变。5 项相关测试通过，包括死亡/复活 UI 的复制顺序与可见性漂移处理。本轮没有重跑全量玩法测试，也没有重新执行完整多人开局测试。

本轮原件备份与预览：`C:/Users/Elysia/Documents/Codex/2026-10-13/f-ai-workspeace-ai/work/ui-rest-2026-10-24`。


## 圆角与商店叠加

游戏内面板、卡牌、按钮、输入框和进度条已使用 Rounded Box 画刷：面板通常为 16，卡牌内框为 14，普通按钮与输入框为 10，状态条为 5 个 Slate 单位。完整屏幕背景与受伤遮罩保持原来的铺满方式。和纸纹理、颜色、文本、控件位置和事件图保持原样。

圆角存放在各 WBP 的 Brush / Style → Outline Settings → Corner Radii 中，可直接继续调整。相关圆角工具只在编辑器里执行，游戏运行时不会重写你的样式。

昼夜倒计时使用视口层级 60，商店使用 64；较高的商店位于上方。倒计时为 HitTestInvisible，不接收鼠标命中，因此不会挡住商品购买、刷新和退出操作。打开商店时，商店面板会盖住它后面的倒计时；关店后，如果阶段尚未结束，会继续显示服务器剩余时间。倒计时不会因逛商店而暂停。

叠加预览用实际 WBP 与正式层级验证：商店仍位于倒计时上方，标题正常排布，退出按钮保持启用，倒计时忽略鼠标命中。预览只使用演示商品数据，不执行购买。

## 克制的符卡风格（2026-10-24）

按最新风格要求，在明确的东方元素和界面留白之间取中间方案。主菜单采用较安静的月夜鸟居，背景只保留稀疏弹幕弧线；主要操作使用朱红色，其他操作为靛蓝底与细线。商店、选卡和升级树节点保留浅色纸面、大字和原来的布局，以细朱红边饰、标题竖线和淡符印提供辨识度。

装饰保存在 WBP，可以直接在设计器调整或删除：

| 控件 | 用途 | 默认强度 |
|---|---|---|
| `Spirit_CardTitleMark` | 卡牌标题旁的小朱红竖线 | 3×29 |
| `Spirit_CardSeal` | 卡牌角落符印 | 94×94，Render Opacity 0.065 |
| `Spirit_*_Seal` | 部分面板及升级树节点的符印 | 92×92，Render Opacity 0.045–0.05 |
| `Spirit_HeaderRule` | 弹窗内容上方的细朱红横线 | 高 2，Render Opacity 0.65 |
| `Spirit_Atmosphere` | 全页背景的低对比遮罩 | 主菜单 Alpha 0.14，其余全页 Alpha 0.48 |

所有装饰忽略鼠标命中；包裹原内容的 Overlay 使用 SelfHitTestInvisible，让原按钮继续接收点击。玩家状态反馈层、变化数值和敌人胶囊血条不重新生成，保留本轮已完成的动画。`RogueSpellcardThemeBuilder` 只在编辑器内一次性执行，正常运行不会覆盖手工设计。

Development Editor 编译通过。20 个 WBP 保存成功，另 1 个敌人血条保持原件；21 个界面的事件图与属性绑定契约一致。10 项相关 UI 测试通过，16 张实际 WBP 预览覆盖主菜单、商店、选卡、升级树、多人页面和战斗 HUD，并再次检查商店位于昼夜倒计时上方且倒计时不拦截点击。预览卡牌和部分状态使用样例数据；未重复执行全量玩法或双进程联机测试。

工作区预览与验证记录：`outputs/Spellcard-UI-2026-10-24`。原件备份与应用脚本：`work/spellcard-ui-2026-10-24`。
