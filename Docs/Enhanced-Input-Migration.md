# 当前 Enhanced Input 配置

更新于 2026-11-09；已读取项目实际输入资产与绑定代码。

## 资产与按键

映射上下文为 `/Game/Input/IMC_Default`，输入动作统一位于 `/Game/Input/IA/`。当前上下文共 13 条映射，运行时绑定 10 个动作。

| 动作 | 值类型 | 当前映射 | C++ 触发事件 |
|---|---|---|---|
| IA_Move | Axis2D | W / S / A / D | Triggered |
| IA_Look | Axis2D | Mouse2D | Triggered |
| IA_Zoom | Axis1D | MouseWheelAxis | Triggered |
| IA_Dash | Boolean | LeftShift | Started |
| IA_Interact | Boolean | E | Started |
| IA_TogglePauseMenu | Boolean | Escape | Started |
| IA_Choice | Boolean | Q | Started |
| IA_ChooseUpgrade1 | Boolean | One | Started |
| IA_ChooseUpgrade2 | Boolean | Two | Started |
| IA_ChooseUpgrade3 | Boolean | Three | Started |

跳跃已移除，当前上下文不映射空格。方向键也不在当前映射中；需要替代控制方式时，可在 IMC_Default 增加映射。

## 代码如何取得资产

Character::SetupEnhancedInput 优先读取 BP_RogueCharacter 中配置的软引用；未配置才按上面的默认目录加载。它为本机 LocalPlayer 添加映射上下文，服务端不会代替远端客户端设置本地输入。

必需动作缺失会记录缺失清单并终止本次角色动作绑定，没有旧版 BindAction / BindAxis 回退。日志里的动作总数只能确认加载数量，具体按键与修饰器仍须检查资产和实际操作。

## 主动选卡

IA_Choice 复用现有的 Q 映射，绑定 Started，每次按下请求一次开关。升级只累计次数；Q 打开，Q 或 Esc 收起，1 / 2 / 3 确认选择。完整状态流见 [Manual-Upgrade-Selection.md](Manual-Upgrade-Selection.md)。

Esc 在 UMG 按钮获得焦点时也需要有效，所以 HUD 的输入预处理器仅处理本机战斗视口内的 Esc；其他角色动作继续走 Enhanced Input。

## 移动轴与菜单

数字键默认输出 X 轴，因此 W/S 需要通过 Swizzle 转成 Y；S/A 需要 Negate。相关方向与组合顺序应在 IA_Move 的映射修饰器中确认，不应在 C++ 再翻转一次。

世界不因菜单暂停。正在显示商店或选卡时，HUD 使用 GameAndUI 并显示光标；仅有未消费次数不会切换输入模式或夺走移动、视角输入。购物与选卡均不免伤。

修改 bTriggerWhenPaused 只影响真正暂停世界时的动作触发，不等同于暂停/设置菜单的服务端伤害保护。
