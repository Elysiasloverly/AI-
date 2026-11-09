# 主菜单

更新于 2026-11-04。主菜单是 `/Game/Map/MainMenu` 中的实时 3D 角色，加上可编辑的 `WBP_MainMenu`。目前使用浅色和纸、淡墨鸟居与樱枝、朱红主按钮和少量飘落花瓣。字体、动效与素材入口见 [字体与界面精修](UI-Typography-And-Menu.md)。

## 启动与操作

`GameDefaultMap` → `MainMenu` → `ARogueMainMenuGameMode` → `ARogueMainMenuHUD` → `WBP_MainMenu`。菜单没有战斗 Pawn；HUD 选择带 `MenuCamera` 标签的相机。

| 控件名 | 当前行为 |
|---|---|
| `SinglePlayerButton` | `ChooseInitialDay` 从白天池选择战斗图，再 `OpenLevel` |
| `MultiplayerButton` | 打开多人入口，可创建房间或用局域网搜房、IP 加入 |
| `SkillTreeButton` | 打开永久升级树，读取自己的本地进度 |
| `TutorialButton` | 打开六章可滚动教程 |
| `SettingsButton` | 打开设置；音效和背景音乐分别调节 |
| `QuitButton` | 退出游戏 |

单人和房主开局均使用当前昼夜地图池；详见 [地图集合](Map-Collection.md) 和 [联机大厅](Multiplayer-Lobby.md)。保留以上名称与 WBP 父类，按钮样式、尺寸和层级可以在设计器里修改。不要另外重复绑定同一点击事件。

## 编辑入口

| 内容 | 路径与位置 |
|---|---|
| 菜单整体 | `/Game/UMG/Menus/MainMenu/WBP_MainMenu` |
| 菜单布局 | `MenuPanel` → `MenuContent`；主入口竖排，教程与设置并排 |
| 原有 Logo | `Image_GameLogo` → `/Game/GameAssets/UI/Branding/T_BakaLogo` |
| 主菜单 3D 背景 | `/Game/Map/MainMenu` 的 `MenuWall` |
| 和纸山景材质 | `/Game/GameAssets/ArtDirection/MenuStory/Materials/M_MenuStoryBackdrop` |
| 背景纹理 | `/Game/GameAssets/ArtDirection/MenuStory/Textures/T_MenuStory` |
| 少量动态花瓣 | WBP 的 `MenuAmbientPetals`；材质 `M_MenuPetals` |
| 右上章节题签 | `MenuChapterBadge`、`MenuSeasonTitle`、`MenuSeasonCaption` |
| 教程 | `/Game/UMG/Menus/MainMenu/WBP_Tutorial`；章节按钮与 `ChapterSwitcher` |
| 设置 | `/Game/UMG/Menus/Common/WBP_SettingsMenu` |

主菜单保留原有角色、扫帚、相机位置和模型比例。背景采用屏幕坐标取景，避免山景被相机裁成局部。菜单灯光与 `MenuPaperExposure` 的局部曝光处理只影响主菜单关卡，具体设置见字体与界面精修说明。

## 源码分工

- `RogueMainMenuWidgetBase`：按既有名字绑定六个操作，转交 HUD。
- `RogueHUD.MainMenu.cpp`：页面开关、输入模式和单人开始。
- `RogueHUDWidgetFactory.h`：创建菜单，附加通用轻动效扩展。
- `RogueMenuMotionExtension`：入场、悬停与按压，只改渲染变换；不改布局槽、业务事件和音效。
- `RogueUIPolishBuilder`：编辑器一次性整理字体和菜单层级，结果保存为普通 WBP；运行时不会重建界面。

`Tools/create_main_menu.py`、`Tools/create_menu_background.py` 是早期搭建脚本，会重建控件或场景。手工改样式后请勿直接重跑，以免恢复旧版。当前布局以保存的 WBP 和 MainMenu 关卡为准。

## 配置与打包

`Config/DefaultEngine.ini` 的 `GameDefaultMap=/Game/Map/MainMenu`。主菜单、教程、设置的类路径在 `DefaultGame.ini` 的 `[/Script/ai.RogueHUD]`；`SinglePlayerMapPath` 是初始白天选图的回退路径，当前为 `/Game/Map/Day/Battle_Forest_Day`。

字体已作为项目资产内嵌，无需目标电脑安装系统字体。新增开放字体的许可文件随游戏一同打包。字体和图片原件、来源、生成记录保存在 `ArtSource/Typography` 与 `ArtSource/MenuStory`。
