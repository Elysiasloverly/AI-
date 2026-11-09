# 美术细节与等级 HUD（2026-11-06）

本轮沿用深色战斗 HUD、和纸卡面和简洁符阵。下面记录已保存的资产入口；地图和数值表逐文件校验与用户本轮保存结果一致。

## 左下角当前等级

`/Game/UMG/HUD/WBP_PlayerVitals` 新增 `LevelText`，位于状态面板顶部，显示“等级 N”。字体、颜色与位置仍可在 WBP 设计器调整；保留控件名才能绑定。面板顶部增加留白，原生命、护甲、经验及冲刺条仍在其下。

`URogueVitalsWidgetBase::UpdateVitals` 使用现有 `FRoguePlayerVitalsView.Level` 更新文字，不依赖短暂的升级提示或反馈动画。升级后常驻显示；重新开局时根据新视图重置。等级至少显示 1，无需新增 Tick、计时器或 RPC。

## 调整入口

| 内容 | 保存位置与行为 |
|---|---|
| 敌人受击 | `BP_EnemyBase` 的受击材质改为 `DetailPass/Materials/M_EnemyPearlFlash`，边缘偏冷白、内部降低覆盖，连续受击保留模型纹理；沿用原受击组件生命周期 |
| 敌人动作 | 敌人表现组件降低移动倾斜、转向侧倾、攻击回摆和受击偏摆幅度；不改变移动、攻击与碰撞判定 |
| 地面装饰 | `Completion/Materials/MI_GroundAccent_*` 日间透明度 0.20、夜间 0.24；地标装饰透明度降低，避免与攻击预警抢色 |
| Boss | `BakaJourney/Materials/MI_BossLacquerRim` 微调边缘强度和金色，保留当前模型和施法参数 |
| 暗色 HUD | `BattleHUD/Materials/MI_BattleHUDSoft*` 降低边缘羽化；沿用原深色配色 |
| 玩家脚下圆环 | `M_PlayerBattleCompass` 改为更细的珍珠白边，保留队友色、头顶符号和深度遮挡处理 |
| 落地蓝焰 | `IceMeteor/Materials/M_GroundBlueFlame` 柔化核心，各火舌使用世界位置错开摆动相位，仍由原场地 Actor 驱动 |
| 经验晶体与商店 | `DetailPass/Materials/MI_SpiritCrystalFine`、`MI_SpiritCrystalHaloFine`、`MI_ShopLanternAuraFine` 调整高光与光晕 |
| 敌人死亡 | `BP_EnemyDefeat` 缩减普通碎片数量、稍延长收尾；沿用原对象池，声音未修改 |
| 选卡与商店卡 | `WBP_UpgradeCard`、`WBP_ShopOfferCard` 优化描述颜色、换行和间距；选择提示与状态印章分开，按钮悬停提供完整说明 |

上表材质路径以 `/Game/GameAssets/ArtDirection/` 为根。卡片原事件图和交互规则保留。着色代码源见 `ArtSource/DetailPass-2026-11-06`；玩家圆环源同步保存在 `ArtSource/BakaBattlePresentation/PlayerFocus.hlsl`。

## 武器表分类名称

`DT_WeaponConfig` 行编辑器的旧分类由 C++ `Category` 元数据决定。本轮统一为冰符·寒弹、冰焰·陨星、银刃、冰符·霜玉、焰符·灼光、星符·流光；共用光束参数归入“光束共用”。内部行名、字段名、枚举和已保存数值保留。

## 验证

Development Editor 编译通过。4 项状态栏定向测试通过，包含真实 WBP 的等级绑定、升级后常驻显示、濒死、重新开局及两个独立实例互不影响。

5 项敌人受击定向测试通过，其中 4 项含临时测试世界没有刷怪区域的预期警告。检查覆盖基类材质继承、真实 GAS 伤害、覆盖材质恢复和回池清理，以及复制状态。

7 类界面完成 1440×810 渲染预览；神社白天、神社黑夜、冰湖白天完成实际战斗材质渲染，未出现材质编译错误。界面部分使用样例数值，战斗材质部分使用临时生成的真实敌人和陨星落地效果；没有重跑完整百波、全量测试、打包或新增 GPU 性能基准。
