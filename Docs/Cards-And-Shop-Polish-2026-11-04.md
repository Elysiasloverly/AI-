# 选卡与商店界面精修

更新：2026-11-04。战斗HUD仍保持暗色，此次只精修选卡与商店。

| 编辑位置 | 本轮处理 |
|---|---|
| `/Game/UMG/Menus/Upgrade/WBP_UpgradeCard` | 图标76、标题22半粗体、正文18、朱红选择提示、内边距与分隔线 |
| `/Game/UMG/Menus/Shop/WBP_ShopOfferCard` | 同一图标与标题层次，正文和价格分区，已购/金币不足卡面 |
| `/Game/UMG/Menus/Upgrade/WBP_UpgradeSelection` | 保留三卡居中，衬线标题与更清晰的提示 |
| `/Game/UMG/Menus/Shop/WBP_Shop` | 保留三列两行，钱包半粗体，朱红刷新按钮和浅纸退出按钮 |

卡片仍为350宽；升级卡高320，商品卡高270。标题、正文、价格、图标和提示均在WBP设计器调整，原有控件名和事件图保留。`ArtIcon`仍由图标主题按升级类型赋材质；图标尺寸在WBP调整。

卡片悬停与按压沿用已有 `RogueCardMotionWidgetBase`，没有额外增加一层缩放动画。颜色反馈调整 Button Style 的 Normal、Hovered、Pressed、Disabled；`Border_Card`保留少量透明度，让底层按钮状态透出。注意正文、标题、价格都要避开各自的分隔线和内边距。

1440×810和1280×720的实际WBP样例预览均检查通过。没有修改购买、选卡、价格或战斗数值。完整交付记录与截图在工作区 outputs/Cards-And-DayNight-2026-11-04。
