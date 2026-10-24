# 打包说明

白天与夜晚战斗地图现位于 `/Game/Map/Battle_Day` 和 `/Game/Map/Battle_Night`，默认入口沿用夜景。两张地图场景与玩法配置相同，白天副本仅调整环境灯光。详见 [昼夜地图](Battle-Day-Night.md)。

## 常用命令（Windows / Shipping / 精简包）

```bat
"C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="F:\AI_WorkSpeace\baka\Baka.uproject" ^
  -noP4 -platform=Win64 -clientconfig=Shipping ^
  -build -cook -stage -pak -iostore -compressed ^
  -nodebuginfo -noprereqs ^
  -map="/Game/Map/MainMenu+/Game/Map/Battle_Night+/Game/Map/Battle_Day" ^
  -archive -archivedirectory="F:\AI_WorkSpeace\baka\Packaged\Baka_Demo_Win64" ^
  -unattended -utf8output

```

> 注意：`-map=` 参数请在 **cmd / .bat** 中执行。Git Bash(MSYS) 会把 `/Game/...` 改写成 `F:/.../PortableGit/Game/...`，日志中出现 `Unable to find package for path`，此时只靠 `GameDefaultMap` 兜底把菜单地图烤进包，游戏主地图仍可能缺失，务必确认 `-map` 生效。

- `-nodebuginfo`：去掉 `.pdb`（本项目上一次打包仅此一项就差 204 MB）；
- `-noprereqs`：不带安装前置组件；
- `-map=`：烤主菜单 + 游戏主地图（`+` 分隔），避免样板图进包；
- `-iostore -compressed`：IoStore + Oodle 压缩。

## 必须强制 Cook 的目录（易踩）

有些资产**只由 C++ 按约定路径 `LoadObject` 加载**，不在任何蓝图的引用图里，Cooker 扫描不到，  
打包后会静默缺失。当前清单已写在 `Config/DefaultGame.ini` 的  
`[/Script/UnrealEd.ProjectPackagingSettings]` 里：

| 目录                  | 缺失后果                                          |
| ------------------- | --------------------------------------------- |
| `/Game/Input`       | `IMC_Default` + 10 个 `IA_*` 丢失 → **角色完全没有输入** |
| `/Game/Data/Player` | `DA_PlayerBalance` 丢失 → 玩家数值退回 C++ 默认         |
| `/Game/UMG/Menus`   | 主菜单 / 教程 / 设置 WBP 丢失 → 启动后没有菜单                |
| `/Game/UMG/HUD`     | 玩家状态条 / 敌人血条 / 受伤泛红 WBP 丢失 → 战斗界面缺失        |

新增"按路径加载"的资产目录时，同步往 `DirectoriesToAlwaysCook` 里加一条。

地图不靠目录 Cook：主菜单是新的 `GameDefaultMap`，`DefaultGame.ini` 已用 `+MapsToCook` 列出
`/Game/Map/MainMenu` 与游戏主地图；命令行打包时 `-map=` 也要同时带上两张图（`+` 分隔）。

其它路径加载（已确认会随引用进包，无需额外配置）：  
`Data/Core`、`Data/Upgrades`、`Data/Enemies`、`Data/Weapons`、`BP/World/BP_ExperiencePickup`。  
WBP 不再依赖蓝图引用：菜单与 HUD 的 WBP 都按 ini 路径加载，已统一列进 `DirectoriesToAlwaysCook`。

## Shipping 日志

Shipping 包不写日志文件（`-log -abslog=` 也不产生文件），所以打包后的运行问题优先靠  
**资产是否进包** + 编辑器内复现来判断。需要看日志时用 Development 配置打包。

## 验证包 vs 正式发布包

新构建输出到 `Packaged/Baka_Demo_Win64`，是**精简验证包**（旧验证包仍叫 `Packaged/AI_Demo_Win64`），用于快速验证玩法与体积；  
对外发布还应补上这些：

| 项     | 验证包（当前）               | 正式发布包                                                           |
| ----- | --------------------- | --------------------------------------------------------------- |
| 编译配置  | Shipping              | Shipping + `-distribution`（开启发行期检查、关闭开发期旁路）                     |
| 调试符号  | `-nodebuginfo`，无 pdb  | 游戏目录不含 pdb，但**单独归档 pdb** 用于崩溃定位                                 |
| 崩溃报告  | 无 `CrashReportClient` | 加 `-crashreporter`，便于收集崩溃转储                                     |
| 运行库前置 | `-noprereqs`          | 加 `-prereqs`（VC++/DX 安装器，干净机器可直接跑）                              |
| 版本信息  | 未配置，沿用引擎默认（`++UE5+Release-5.5-CL-40574608` / Epic Games, Inc. / UnrealGame） | `DefaultGame.ini` 配 ProjectVersion/CompanyName/Description，替换图标 |
| 签名    | 未签名                   | 代码签名（可选，减少 SmartScreen 拦截）                                      |
| 分发形式  | 绿色文件夹                 | 绿色 zip 或安装器（Inno Setup / NSIS）                                  |
| 烤图范围  | 主菜单 + 主地图          | 按需加前端/其他地图与本地化                                                  |

正式包命令示例（在验证包参数基础上替换）：

```bat
BuildCookRun ... -clientconfig=Shipping ^
  -distribution -prereqs -crashreporter ^
  -nodebuginfo ^
  -archive -archivedirectory="...\Packaged\Release"
```

生成后把 `Binaries/Win64/*.pdb` 单独拷到版本目录存档，游戏目录里不要带。

## 分发给别人：解压即玩

Shipping 包默认依赖系统安装的 VC++ 运行库，干净机器会报缺 DLL。做法是把运行库  
**应用本地化**（放在 exe 同目录，Windows 会优先加载，无需安装）：

```bat
:: 1) 复制 VC++ 运行库到 exe 同目录（路径按本机 VS 版本调整）
copy "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT\*.dll" ^
     "Packaged\Baka_Demo_Win64\Baka\Binaries\Win64\"

:: 2) 打包 zip（store 模式，pak 已压缩，再压没有收益且更慢）
tar --options "zip:compression=store" -a -c -f "Packaged\Baka_Demo_Windows.zip" ^
    -C "Packaged" "Baka_Demo_Win64"
```

旧产物：`Packaged/AI_Demo_Windows.zip`（542 MB）。按上面命令重新打包后会生成 `Packaged/Baka_Demo_Windows.zip`，解压后双击 `Baka.exe` 运行。

> 每次重新打包后需要重做第 1 步（复制运行库），再压缩。  
> 包里已自带 XAudio2、DirectML、Aftermath、EOS SDK 等引擎侧 DLL，无需额外安装。

## 编译报 C1056（无法更新 obj 时间戳）

本机安全软件会拦截 UBA 的文件元数据操作，表现为 Shipping 编译大量  
`fatal error C1056`。`-NoUBA` 在 UE 5.5 不生效，需在  
`%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` 里临时关闭：

```xml
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
  <BuildConfiguration>
    <bAllowUBAExecutor>false</bAllowUBAExecutor>
    <bAllowUBALocalExecutor>false</bAllowUBALocalExecutor>
    <MaxParallelActions>1</MaxParallelActions>
  </BuildConfiguration>
</Configuration>
```

构建完成后还原为空配置即可。

## 体积现状（2026-10-21）

Shipping 包 541.8 MiB：exe 136.7 MB + 本地 DLL 约 25.6 MB + 内容 336.0 MB（ucas 325.8 + pak 9.92 + utoc 0.28，含 global.ucas）+ Engine 43.0 MB + 根目录清单文件约 0.5 MB。  
内容大头：角色、Library 素材、地图与武器（分项缓存口径未统一，本轮未重测，暂不列具体值）。  
如需继续压缩，优先考虑合并 `Vefects` / `Library` 的双份拷贝、更换体积较大的森林背景网格。
