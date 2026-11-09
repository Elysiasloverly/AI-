# 当前 UI 维护入口

已保存的 WBP 是最终外观来源。日常在 UE Designer 编辑，运行时 C++ 只更新绑定数据。

验证全部界面（不保存 WBP）：

```powershell
& "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "F:/AI_WorkSpeace/baka/Baka.uproject" -run=pythonscript -script="F:/AI_WorkSpeace/baka/Tools/UI/manage_ui.py" -unattended -nullrhi -nosound
```

输出 `Saved/QA/UI/last-report.json`。它在内存中编译检查，不运行历史主题生成器，不保存地图。

需要重建一个基础控件树时，追加 `-RogueUIRebuild=/Game/UMG/.../WBP_Name`。
仅支持脚本 BUILDERS 列表中的资产；必须给出准确现存包路径。执行前自动备份并校验包，编译成功才保存。
这个操作恢复原生占位布局，会替换选中 WBP 的人工样式。备份在 `Saved/QA/UIBackups/`。
升级树使用 `Tools/create_upgrade_tree.py` 的显式重建参数；它默认保留已有控件。


## 当前玩家状态栏（2026-11-09）

- 资产：`/Game/UMG/HUD/WBP_PlayerVitals`，620×144 暗色底部状态栏。
- `LevelBadgeContent` 在等级框内共同居中标题和数字；等级下方的 Q/待选计数已经移除。
- `PendingUpgradeHintText` 在状态栏上方显示淡蓝色呼吸提示；`PlayerDashRing` 在角色旁显示现有冲刺冷却。
- 名称/数值采用独立绑定，ValueScale 控制长数字，变化数字有单独区域。

| 编辑器入口 | 应当何时使用 |
|---|---|
| `RogueUIThemeBuilder.refine_compact_vitals` | 明确重排当前状态栏时使用，会覆盖它管理的区域；需要先备份 |
| `use_player_side_dash_ring` | 从旧右侧冲刺分区迁移；不是当前界面的普通刷新操作 |
| `add_pending_upgrade_counter` | 早期等级下方计数的制作工具，当前方案已取消该行，不要在日常维护中调用 |
| `RogueWidgetBuilder.build_player_vitals` / manage_ui.py 的 rebuild 参数 | 重建基础占位树，会丢失当前精修结构与样式 |

当前制作实现还包括 `Source/aiEditor/Private/UI/RogueCompactVitalsPolish.cpp`、`RoguePlayerSideVitals.cpp`；运行时 View 与圆弧在 `Source/ai/Private/UI/Widgets/`。

详见 [状态栏维护](../../Docs/Centered-Vitals-Polish.md)、[主动选卡](../../Docs/Manual-Upgrade-Selection.md)。

编辑器源码分组：

| 目录 | 用途 |
| --- | --- |
| `Source/aiEditor/Private/UI/Authoring` | 当前绑定、字体、排版和基础控件树工具 |
| `Source/aiEditor/Private/UI/LegacyThemes` | 历史风格生成器；仅供手动重现/学习，不自动运行 |
| `Source/aiEditor/Private/UI/RogueWidgetAuthoring.h` | 共用控件复用、纹理刷子和编译收尾 |
| `Source/aiEditor/Private/Preview` | 临时预览 |
| `Source/aiEditor/Private/AssetSetup` | 资产创建/接线 |
| `Source/aiEditor/Private/Support` | 保存、编译和迁移检查辅助 |

旧的 `Tools/create_main_menu.py`、`create_hud_widgets.py` 等是初始化/历史制作脚本，可能清空布局；不作为日常验证入口。
