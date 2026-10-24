# 开始菜单（主菜单）

白天与夜晚战斗地图现位于 `/Game/Map/Battle_Day` 和 `/Game/Map/Battle_Night`，默认入口沿用夜景。两张地图场景与玩法配置相同，白天副本仅调整环境灯光。详见 [昼夜地图](Battle-Day-Night.md)。

游戏启动后先进入独立的主菜单关卡，再从这里开始单人游戏。菜单 UI 是 UMG，背景是关卡里的 3D 实时画面（相机对着一面空白墙）；C++ 只负责按钮接线、相机切换与界面开关，外观全部在 WBP 里改。

## 启动流程

```text
GameDefaultMap = /Game/Map/MainMenu
  → ARogueMainMenuGameMode（无 Pawn）
      → ARogueMainMenuHUD（ARogueHUD 的主菜单模式）
          → WBP_MainMenu（常驻）
          → WBP_Tutorial（默认收起）
          → WBP_SettingsMenu（默认收起，复用游戏内设置面板）
  MenuCamera（Tags: MenuCamera）→ SetViewTarget，3D 墙作为 UMG 背景
  “单人游戏” → OpenLevel(/Game/Map/Battle_Night)
```

- 关卡：`/Game/Map/MainMenu`（磁盘 `Content/Map/MainMenu.umap`），World Settings 的 **GameMode Override** 是 `ARogueMainMenuGameMode`。
- `Config/DefaultEngine.ini` 里 `GameDefaultMap` 指向主菜单；`EditorStartupMap` 仍是游戏主地图，所以在编辑器点 Play 会直接进游戏，要调菜单请手动打开 `MainMenu` 关卡。
- 主菜单关卡不需要 `PlayerStart`，也没有 Pawn；`ARogueMainMenuGameMode` 的 `DefaultPawnClass` 为 `None`。

## 资产与代码

| 内容 | 路径 |
|---|---|
| 主菜单关卡 | `/Game/Map/MainMenu` |
| 主菜单 WBP | `/Game/UMG/Menus/MainMenu/WBP_MainMenu`（父类 `URogueMainMenuWidgetBase`） |
| 教程 WBP | `/Game/UMG/Menus/MainMenu/WBP_Tutorial`（父类 `URogueTutorialWidgetBase`） |
| 设置面板 | `/Game/UMG/Menus/Common/WBP_SettingsMenu`（复用游戏内设置） |
| GameMode | `Source/ai/Public/Framework/RogueMainMenuGameMode.h` |
| HUD | `Source/ai/Public/UI/RogueMainMenuHUD.h`（继承 `ARogueHUD`） |
| 菜单 Widget 基类 | `Source/ai/Public/UI/Widgets/RogueMainMenuWidgetBase.h` |
| 教程 Widget 基类 | `Source/ai/Public/UI/Widgets/RogueTutorialWidgetBase.h` |
| 生成脚本 | `Tools/create_main_menu.py`（重建地图与两份 WBP 的控件树） |
| 背景搭建脚本 | `Tools/create_menu_background.py`（墙 / 灯光 / 相机 / 场景捕获） |
| 背景渲染目标 | `/Game/UMG/Menus/MainMenu/RT_MenuBackground` |


## 3D 实时背景

主菜单关卡里放了一面空白墙和一组相机（World Outliner 的 MainMenuBackground 文件夹下）；`WBP_MainMenu` 的 `Background` 是透明的（HitTestInvisible），
所以 UMG 后面直接显示关卡相机画面。墙换成动画材质即可作为实时背景。

| Actor | 作用 |
|---|---|
| `MenuWall` | 空白墙（Cube + `BasicShapeMaterial`），换成自己的动画材质 |
| `MenuCamera` | 玩家视角相机，Tags 带 `MenuCamera`；HUD 会 `SetViewTarget` 到它 |
| `MenuCamera_Close` | 第二个机位，默认不启用（可做特写/切换） |
| `MenuBackgroundCapture` | `SceneCapture2D` 对着墙，每帧输出到 RT |
| `MenuKeyLight` / `MenuFillLight` | 两盏 **Movable** 灯，保证墙在未烘焙时也可见 |

- 渲染目标：`/Game/UMG/Menus/MainMenu/RT_MenuBackground`（1280×720）。想把它挂到 WBP 的 `Image` 上做后期/叠加也可以。
- 换墙：把 `MenuWall` 的材质换成动画材质；建议用 Unlit / Emissive，避免依赖关卡灯光。
- 换机位：改 `MenuCamera` 的位置/旋转，保留 `MenuCamera` 标签；HUD 找不到带标签的相机时不会强行切相机。
- 改完场景后运行 `Tools/create_menu_background.py` 会**删除并重建**这些 Actor，已有自定义材质/关键帧请先备份，或直接手动改关卡。

## 按钮行为

| WBP 控件名 | 点击后 |
|---|---|
| `SinglePlayerButton` | `OpenLevel` 打开 `SinglePlayerMapPath`（默认 `/Game/Map/Battle_Night`） |
| `MultiplayerButton` | 在 `HintText` 显示“多人游戏暂未开放”（功能未实现） |
| `SkillTreeButton` | 在 `HintText` 显示“技能树暂未开放”（功能未实现） |
| `TutorialButton` | 显示 `WBP_Tutorial` |
| `SettingsButton` | 显示 `WBP_SettingsMenu`；面板里的“返回”回到主菜单 |
| `QuitButton` | `QuitGame` 退出桌面 |

按钮名是 C++ 的 `BindWidgetOptional` 绑定名；**改名会静默失绑**，改样式不受影响。

> 注意：Tools/create_main_menu.py 会清空并重建两份 WBP 的控件树；改过样式后不要再跑，除非想恢复占位版。

## 改样式

字体统一是 `/Game/UMG/Fonts/F_DroidMono_CJK`（DroidSansMono + 中文回退），新加的 TextBlock 也请选它。

- 打开 `WBP_MainMenu`：`Background` 现在是透明（HitTestInvisible），用来露出关卡相机画面；想改回纯色/图片就在 WBP 里改这个 Border。`MenuPanel` 是居中面板，`MenuBox` 里依次是标题、副标题、6 个按钮、`HintText`。
- 打开 `WBP_Tutorial`：`TutorialScroll` + `TutorialText` 是可滚动正文，`CloseButton` 是返回按钮。
- 想加点击动效：在 WBP 里给按钮加动画即可；不要在 WBP 里再绑一次 `OnClicked`（会和 C++ 的绑定同时触发）。需要换行为就改 C++ 或改控件名后在 WBP 里自己绑。
- 想换提示动效：实现 `OnHintChanged` 事件，`HintText` 的默认写法仍会更新文字。

## 配置

`Config/DefaultGame.ini` 的 `[/Script/ai.RogueHUD]`：

```ini
MainMenuWidgetClassPath=/Game/UMG/Menus/MainMenu/WBP_MainMenu.WBP_MainMenu_C
TutorialWidgetClassPath=/Game/UMG/Menus/MainMenu/WBP_Tutorial.WBP_Tutorial_C
SettingsMenuWidgetClassPath=/Game/UMG/Menus/Common/WBP_SettingsMenu.WBP_SettingsMenu_C
SinglePlayerMapPath=/Game/Map/Battle_Night
```

> 原生子类 `ARogueMainMenuHUD` 不会自动读父类 `[/Script/ai.RogueHUD]` 段，代码里对空值做了回退：仍以这段为准即可，不要另建 `[/Script/ai.RogueMainMenuHUD]` 重复配置。

`Config/DefaultEngine.ini`：`GameDefaultMap=/Game/Map/MainMenu`。

## 打包注意

主菜单是新的启动地图，**必须和游戏主地图一起 Cook**：

- `Config/DefaultGame.ini` 已加 `+MapsToCook=(FilePath="/Game/Map/MainMenu")` 和游戏主地图；
- 命令行打包时 `-map=` 用 `+` 连接两张图：

```bat
-map="/Game/Map/MainMenu+/Game/Map/Battle_Night+/Game/Map/Battle_Day"
```

只烤游戏主地图时，打包版启动会找不到 `GameDefaultMap`。

## 验证

重建/核对资产（只读检查，不会改玩法配置）：

```bat
UnrealEditor-Cmd.exe F:\AI_WorkSpeace\baka\Baka.uproject ^
  -run=pythonscript -script="F:\AI_WorkSpeace\baka\Tools\verify_main_menu.py" ^
  -unattended -nopause -nullrhi -nosplash
```

无头跑一次菜单关卡，日志里应出现：

```text
[Rogue] 主菜单按钮绑定：单人=1 多人=1 技能树=1 教程=1 设置=1 退出=1 提示=1
[Rogue] 主菜单模式 HUD 就绪：MainMenu=WBP_MainMenu_C Settings=WBP_SettingsMenu_C Tutorial=WBP_Tutorial_C
```

手动验收：打开 `MainMenu` 关卡 PIE，依次点六个按钮——单人进游戏、多人/技能树出提示、教程可滚动并能返回、设置能改音量/画质并返回、退出关闭游戏。
