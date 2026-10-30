# 游戏 Logo

Logo 已接入 `/Game/UMG/Menus/MainMenu/WBP_MainMenu`，替换原来的文字标题显示。

| 内容 | 位置 |
|---|---|
| UI 纹理 | `/Game/GameAssets/UI/Branding/T_BakaLogo` |
| 原始透明 PNG | `SourceArt/UI/Branding/Baka-Logo-v1.png` |
| 图片控件 | `WBP_MainMenu → MenuPanel → MenuBox → GameLogoSize → GameLogoHeader → GameLogoScale → Image_GameLogo` |

图片使用 UI 压缩、无 Mipmap、sRGB、保留 Alpha，不参与鼠标命中。`GameLogoScale` 选择 ScaleToFit，保持原图约 2:1 的比例。

在设计器修改 `GameLogoSize` 的宽高即可调整显示区域；当前为372×186。图片的 Brush 指向 `T_BakaLogo`，可以在此更换。原 `TitleText` 以 Collapsed 保留在标题容器中，避免旧蓝图或动画引用丢失。Logo 不在运行时通过 C++ 强行赋值。

安装时只替换标题区域，并按新增标题高度扩展 `MenuPanel`；六个按钮、副标题、提示文字、按钮样式和已有图表均保留。主菜单地图无需重新保存。

原图由内置 image_gen 生成，设计采用冰蓝字标、朱红符札感叹号、冰晶和弹幕弧线。
