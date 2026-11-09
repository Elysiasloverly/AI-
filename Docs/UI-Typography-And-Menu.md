# 字体与界面精修

更新于 2026-11-04。本轮围绕清楚、舒展和柔和反馈，保留东方符卡的朱红、和纸与淡墨元素。

## 怎样修改

| 需要修改 | 入口 |
|---|---|
| 中文正文、数字、按钮文字 | `/Game/UMG/Fonts/F_BakaUI`；正文 `Regular`，按钮和小标题 `Semibold` |
| 大标题 | `/Game/UMG/Fonts/F_BakaDisplay`；书卷感衬线字 |
| 替换底层字体文件 | `/Game/UMG/Fonts/Faces` 下三份 FontFace |
| 主菜单按钮、Logo、位置 | `WBP_MainMenu` 的 `MenuPanel/MenuContent` |
| 背景绘画、动态花瓣 | `/Game/GameAssets/ArtDirection/MenuStory` |
| 升级卡、商品卡 | `WBP_UpgradeCard`、`WBP_ShopOfferCard` |
| 升级树节点与翻页 | `WBP_UpgradeTreeNode`、`WBP_UpgradeTree` |

字体来自开放的 Noto Sans SC 和 Noto Serif SC；项目采用固定字重，并完整保留中文字符集。原件、许可与来源记录位于 `ArtSource/Typography`。旧 `F_DroidMono_CJK` 留作兼容，当前 WBP 和新生成控件优先使用 `F_BakaUI`。

## 本次呈现规则

- 主菜单：主操作用朱红，联机与成长入口次一级，教程和设置并排，退出保持轻量。保留现有 3D 角色与 Logo。
- 文字：大标题使用衬线，正文使用清晰的中文无衬线。长说明使用常规字重，避免整张卡全部加粗；说明行距约 1.22。
- 卡牌与商品：保留已有专属图标、圆角、购买状态和卡牌反馈，统一字体与标题层级。
- 升级树：保留每页最多四列、前置连线和动态尺寸；加深背后遮罩，避免角色和背景干扰页眉文字。
- 设置、教程、联机、结算和战斗 HUD：一起更新保存的文字字体；教程正文行距约 1.2。伤害数字、数量角标等紧凑文本保留原有字号。

## 通用轻动效

`RogueMenuMotionExtension` 由 HUD 的菜单工厂附加。主菜单约 0.6 秒入场，按钮稍有错峰；悬停或键盘聚焦放大至 1.018、抬起 2 像素；按压缩至 0.985。隐藏、关闭或禁用动效时恢复设计器中的原始变换、轴心和透明度。

扩展只处理渲染变换，不改 Canvas 排版、点击事件或音效。原有音效系统继续负责按钮声音。需要在某个 WBP 中自行制作动画时，可在 Construct 取得 `RogueMenuMotionExtension`，设置 `bEnabled=false`，再播放自己的动画，避免两个系统同时修改渲染变换。

花瓣是主菜单里的非交互 UI 材质：七片低透明度花瓣缓慢下落，只覆盖右侧，不产生 Actor，不参与碰撞，也不会挡按钮。

## 主菜单灯光与纸面背景

`/Game/Map/MainMenu` 的 `MenuPaperExposure` 是只服务主菜单的后处理体积，优先级 200。它将 Local Exposure 的 Highlight Contrast Scale 和 Shadow Contrast Scale 均设为 1，关闭局部曝光压缩，避免明亮角色周围的纸面形成一圈暗晕。修改时只覆盖这两项，不改战斗地图或全局曝光配置。

相应地，主菜单四盏灯已重新匹配：`MenuKeyLight`（`DirectionalLight_1`）强度为 0.03；三盏 `MenuFillLight`（`PointLight_0/1/2`）强度各为 40。它们不投射阴影。数值只适用于当前菜单材质、曝光和相机，不应用于战斗场景。需要改亮度时先调主菜单灯光，避免重新启用局部曝光来掩盖过曝。

原有角色、扫帚、相机的位置与比例保持原值。背景墙采用 Unlit 材质和屏幕坐标取景，角色使用场景灯光，花瓣在 WBP 中单独呈现。

## 维护边界

`RogueUIPolishBuilder` 是编辑器工具。批量整理只在执行工具时发生，正常运行不会强制恢复字体或布局。手工改 WBP 后不要反复执行批量生成脚本。字体、背景和动效参数可独立替换，无需修改战斗数据或本地升级进度。
