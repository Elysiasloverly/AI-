# 局域网房间与准备大厅

主菜单点击“多人游戏”，选择“创建房间”或“加入房间”。默认房间上限 4 人；创建者作为监听服务器，同时也是一名玩家。

## 使用流程

1. 创建者填写昵称、可选房间名，再点击创建房间。
2. 其他玩家在加入页面刷新局域网房间列表，选择房间加入；也可以输入房主 IPv4 地址，可带端口，例如 `192.168.1.10:7777`。
3. 大厅显示房主地址、玩家名单及各自准备状态。房主也需要点击准备。
4. 所有玩家准备完毕，房主的“开始游戏”按钮才可用。服务端再次校验房主身份和全员准备状态，再统一切换到战斗地图。
5. 玩家可以取消准备或离开房间。房主退出后，客户端断开并返回主菜单。

搜房使用 `OnlineSubsystemNull` 的局域网会话；需要处于允许互通的局域网。广播被网络隔离或网卡路由影响时，可以尝试 IP 加入。直接 IP 连接支持 IPv4 和 `localhost`，允许端口范围 1–65535。连接超时会显示原因并返回主菜单。

## 地图与个人存档

大厅复用 `/Game/Map/MainMenu`，通过 URL 的 `game` 选项临时使用 `RogueLobbyGameMode`，不新增地图。开始后服务端带所有连接者进入 `/Game/Map/Battle_Night`，清除大厅的 GameMode 覆盖，使用战斗地图自己的规则。

大厅 PlayerState 只保存昵称、房主标记与准备状态。战斗开始后仍由原有成长握手提交每位玩家自己的本地升级树快照，不使用房主进度替换客户端进度。

## 修改 WBP 样式

全部位于 `/Game/UMG/Menus/Multiplayer`：

| 资产 | 用途 |
| --- | --- |
| `WBP_MultiplayerMenu` | 创建 / 加入选择、昵称与房间名称 |
| `WBP_RoomBrowser` | 局域网列表、刷新、IP 输入与加入 |
| `WBP_Lobby` | 玩家列表、准备、房主开始、离开 |
| `WBP_RoomEntry` | 一条可加入的房间 |
| `WBP_LobbyPlayerEntry` | 一位玩家的昵称和准备状态 |

三张页面父类是 `RogueMultiplayerWidgetBase`，类默认值 `Page` 分别为 0、1、2。列表行类由页面的 `EntryWidgetClass` 指定。布局、字体、图片和按钮样式均在 WBP 中修改；生成脚本只创建缺失资产，保留已有样式。

保留自动绑定名称：`CreateRoomButton`、`JoinRoomButton`、`RefreshButton`、`JoinIPButton`、`ReadyButton`、`StartButton`、`BackButton`、`PlayerNameInput`、`RoomNameInput`、`IPInput`、`StatusText`、`RoomTitle`、`AddressText`、`Entries`。每张页面只需要自己的控件，不必创建其他页面的控件。列表行保留 `TitleText`、`DetailText`、`ActionButton`。

`ToggleReady` 和 `StartGame` 可以由自定义 WBP 调用；房主及全员准备检查始终由服务端执行，隐藏或禁用按钮只用于界面反馈。

## 实现位置

- `Networking/RogueRoomSubsystem`：创建、搜索、加入、连接超时、离开与异步回调清理。
- `Networking/RogueLobby`：大厅 GameMode、GameState、PlayerController、PlayerState，处理准备状态复制及房主开局权限。
- `UI/Widgets/RogueMultiplayerWidgetBase`：WBP 按钮绑定及列表数据更新。
- `DefaultEngine.ini` 的 `OnlineSubsystem`：默认使用 `NULL` 服务。
- `DefaultGame.ini` 的 `RogueRoomSubsystem`：地图及三张页面的类路径。

调试启动使用 `UnrealEditor-Win64-DebugGame.exe`；对应命令行工具是 `UnrealEditor-Win64-DebugGame-Cmd.exe`。普通 `UnrealEditor.exe` 使用 Development 项目模块，添加 `-debug` 不能替代此处的 DebugGame 程序选择。

## 本次验证

自动测试检查地址格式及全员准备门槛，并回归独立成长快照、局内属性及 Boss 奖励。实际双进程检查局域网发现、加入大厅、准备同步、未准备和非房主开局阻拦、同步进入战斗、各自本地成长进度、房主断开后的返回与重新建房。单独验证 IP 加入及对已开始房间的拒绝反馈。

上述双进程测试在同一台电脑上使用独立游戏进程与独立 QA 存档完成，不等于跨两台物理电脑的网络环境测试。

## 编辑器多人试玩切图

`DefaultEngine.ini` 的 `[ConsoleVariables]` 设置 `net.AllowPIESeamlessTravel=1`。UE 5.5 默认会在 PIE 中禁用无缝切图；只设置 GameMode 的 `bUseSeamlessTravel` 不够。大厅开始游戏使用绝对地址并显式添加 `listen`，清除临时大厅 GameMode 覆盖，同时保留房主的监听身份。

不要把此开关改回 0：普通断线重连式切图不适用于目前“开局后禁止新玩家加入”的大厅流程。大厅会在切图时保留已连接的玩家，再由战斗地图创建战斗 Controller、PlayerState 和角色。

2026-10-22 切图修复回归：3 项联网自动检查通过；独立游戏双进程通过局域网搜索、准备、全员开局及战斗中 12 秒持续检查；真实 PIE 双窗口通过创建、IP 加入、准备、全员开局及战斗中 10 秒持续检查。两种方式均确认双方各有 2 名玩家、2 个角色且大厅 UI 已关闭。PIE 退出未再触发会话引用唯一性检查。

## 战斗重开与初始化失败恢复

战斗 GameMode 在 InitGame 中执行统一的无缝旅行约定，防止旧蓝图中保存的 false 默认值让重开退回断线登录。所有联机环境（含 PIE）都用带 listen 的绝对 ServerTravel URL。开局后的新访客仍被拒绝；现有连接在无缝旅行中保留。

本项目继承 AGameModeBase：其 HandleSeamlessTravelPlayer 替换 Controller 和 PlayerState，PlayerState 仅复制引擎身份字段。新建 ASC、属性、武器、商店、选卡和复活状态，不复制上一局的运行态。新局再次向所属客户端读取本地永久进度。若将来改为 AGameMode 或增加自定义 CopyProperties，必须重新检查这一约定。

成长握手默认期限 30 秒，可在 PlayerState 类默认值调整至 5–120 秒。期限内重试；超时标记失败并退出参与者、返回菜单说明原因，不使用空进度冒充成功。失败者不再阻挡团灭，也不再被 Pawn 归属兜底重新接管。
