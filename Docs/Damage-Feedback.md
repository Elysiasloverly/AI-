# 受伤反馈：屏幕泛红与镜头震动

本地玩家掉血时的两层表现：**屏幕四周泛红**（UMG，外观归 WBP）与**镜头震动**（相机震动类）。

## 分工

| 层 | 谁做 | 做什么 |
|---|---|---|
| 检测 | `ARogueHUD` | 每帧比较本地角色血量，发现下降就算强度 |
| 泛红 | `URogueDamageFlashWidgetBase` | 只驱动 `FlashRoot` 的透明度；颜色、边缘、材质、动画在 WBP |
| 震动 | `URogueDamageCameraShake` | 参数化的镜头振荡；可按伤害强度缩放 |

C++ 不引用任何 Slate 组件、不创建材质、不写死资产路径，和项目里 HUD/WBP 的既有约定一致。

## 触发链路

```text
服务端结算伤害 → Health 属性复制到客户端
  → ARogueHUD::DrawHUD / Tick
      → RefreshMenuWidgets()
          → UpdateLocalDamageFeedback(本地角色)
          血量下降 > 0.5 → TriggerDamageFeedback(伤害 / 最大生命)
              ├→ URogueDamageFlashWidgetBase::PlayDamageFlash(强度)
              └→ PlayerCameraManager->StartCameraShake(震动类, 缩放)
```

**为什么是轮询**：玩家 ASC 挂在 `PlayerState` 上，`Health` 是普通 `Replicated`，没有 RepNotify，
客户端收不到 GAS 的属性变化委托（`RogueCharacter.GAS.cpp:99-101` 写明；`RogueAttributeSet.cpp` 只说明 HUD 轮询、未用 RepNotify）。
HUD 本来每帧就在读这些数值推状态条，直接复用同一次读取比较血量，不额外增加遍历。

已处理的边界：

- **换 Pawn / 重生 / 重连**：基线跟着角色走，换角色只重建基线，不触发反馈；
- **治疗与上限提升**：只在血量**下降**时触发；`HandleMaxHealthChanged` 补的增量是上升方向，不会误报；
- **连续受击**：泛红取峰值（小伤害不会打断大伤害的残留），震动用 `CameraShakeMinInterval`（默认 0.08s）压成一次；
- **伤害占比**：强度是"本次伤害 / 最大生命"，不是固定值，所以重击和擦伤观感不同。

## 泛红 WBP

资产：`/Game/UMG/HUD/WBP_DamageFlash`（父类 `URogueDamageFlashWidgetBase`），
路径写在 `Config/DefaultGame.ini` 的 `DamageFlashWidgetClassPath`。

占位控件树（`Tools/create_damage_flash_widget.py` 生成）：

```text
RootCanvas
└── FlashRoot              CanvasPanel，全屏 —— C++ 只改这个的 RenderOpacity
    ├── CoreTint           全屏 Border，很淡的中心底色
    ├── TopEdge            上边缘
    ├── BottomEdge         下边缘
    ├── LeftEdge           左边缘
    └── RightEdge          右边缘
```

`FlashRoot` 是唯一的命名绑定（`BindWidgetOptional`）。**改名就失效**，
失效时 C++ 会打一条 Warning 说明原因，不会静默什么都不发生。

**初始必须是透明的**。`FlashRoot` 的 `RenderOpacity` 默认是 1，而这个控件一创建就
`AddToViewport` 并常驻可见（`HitTestInvisible`）。所以 C++ 在 `SetOwningRogueHUD` 里写 0、在
`NativeConstruct` 里写一次当前强度—— 少了这一步，
泛红会从进游戏起一直挂在屏幕上，直到真正挨过一次伤害、淡出结束才恢复。

自己用 `bBlueprintHandlesOpacity` 接管透明度时同样要遵守这条：在 WBP 里把
`FlashRoot` 的默认 `RenderOpacity` 设成 0，并且初始动画状态也停在 0。

### 优化泛红效果时的三条路

1. **换外观**：直接改四条 `Border` 的颜色/厚度，或换成 `Image` + 径向渐变材质。
   只要外层的容器还叫 `FlashRoot`，C++ 的淡出逻辑不用动。
2. **换成动画**：在 WBP 里做一个 `Anim_DamageFlash`（改材质参数或控件透明度），
   在 `OnDamageFlash` 事件里播放。这时把 **`bBlueprintHandlesOpacity` 勾上**，
   C++ 就不再写 `FlashRoot` 的透明度，避免两套逻辑互相覆盖。
3. **加内容**：`OnDamageFlash(Intensity)` 里还能叠受击音效、暗角噪点、低血量常驻警示
   （用 `GetFlashStrength()` 读当前剩余强度）。

### 可调参数（WBP 细节面板）

| 参数 | 默认 | 说明 |
|---|---|---|
| `bBlueprintHandlesOpacity` | false | 勾上后 C++ 不再驱动透明度，交给 WBP 动画 |
| `HoldSeconds` | 0.06 | 保持峰值不透明度的时长 |
| `FadeOutSeconds` | 0.55 | 从峰值淡到 0 的时长 |
| `MaxOpacity` | 0.9 | 满强度时的最大不透明度（颜色越深应越小） |
| `MinIntensity` | 0.2 | 极小伤害的下限强度，保证"掉了血有反应" |

## 镜头震动

`URogueDamageCameraShake` 继承 `ULegacyCameraShake`，默认值按俯视角相机调过：
Roll 振幅 1.6°（最容易读出"挨了一下"）、Pitch 0.9°、Yaw 0.5°、位移 Z 2.0cm，时长 0.22s。

- HUD 用 `StartCameraShake(类, 缩放)` 触发，缩放 = `Lerp(0.55, 1.0, 强度)`；
- `ARogueHUD::DamageCameraShakeClass` 默认指向原生类；想换手感就继承它做个 BP 覆盖参数，
  再把 BP 赋给 HUD 的 `DamageCameraShakeClass`（蓝图里改，不需要改 ini）；
- 置空则只做泛红。

调参入口是 `URogueDamageCameraShake` 构造函数里的三段：
`OscillationDuration / BlendIn/Out`、`RotOscillation`、`LocOscillation`。

## 生成与配置

```bat
"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "F:\AI_WorkSpeace\baka\Baka.uproject" ^
  -run=pythonscript -script="F:\AI_WorkSpeace\baka\Tools\create_damage_flash_widget.py" ^
  -unattended -nopause -nullrhi -nosplash
```

脚本用 `URogueWidgetBuilder::BuildDamageFlash` 建控件树（Python 读不到 protected 的 `WidgetTree`），
**重复执行会重建控件树**：已经手改过外观后不要再跑，否则配色会被覆盖。

`Config/DefaultGame.ini`：

```ini
[/Script/ai.RogueHUD]
DamageFlashWidgetClassPath=/Game/UMG/HUD/WBP_DamageFlash.WBP_DamageFlash_C
```

WBP 未配置时只会有震动，并在日志里打 Warning；两者都不配则受伤没有任何屏幕反馈。

## 验证

强度曲线抽成了 `FRogueDamageFlashState`（峰值 / 保持 / 淡出 / 下限），不依赖 Slate，
可以直接跑自动化用例：

```bat
UnrealEditor-Cmd.exe "F:\AI_WorkSpeace\baka\Baka.uproject" ^
  -ExecCmds="Automation RunTests Rogue.UI.DamageFlash;Quit" ^
  -unattended -nopause -nullrhi -nosplash
```

- `Rogue.UI.DamageFlash.StrengthCurve`：峰值、Hold、淡出归零、连续受击取峰值、结束后失活；
- `Rogue.UI.DamageFlash.MinimumIntensity`：极小伤害抬到下限、超过 1 的输入按 1 处理。

外观与手感（颜色深浅、震动幅度）不属于自动化范围，需要进游戏实际挨打确认。

## 已知边界

- 泛红与震动的触发点是**本地 HUD**，因此只在有本地玩家的机器上发生，服务器与观战者不会看到；
- 伤害在服务端判定，客户端血量是复制结果：网络抖动导致一次复制合并多段伤害时，
  客户端只会看到一次性较强反馈（强度按合并后的差值算）；
- 护甲吸收后的血量下降才触发。纯粹掉护甲而血量不变时不会泛红 —— 如果希望护甲破裂也有反馈，
  应在 `ARogueHUD` 里额外比较护甲，或在 GAS 侧加 GameplayCue，而不是在这里硬改阈值。
