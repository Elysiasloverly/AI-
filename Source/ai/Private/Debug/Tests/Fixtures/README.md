# 旧版平衡资产测试样本

`DA_GameBalance.uasset` 是分组布局迁移前的二进制样本，仅供 `RogueGameBalanceTests` 加载到临时包并验证旧字段迁移；游戏运行使用 `Content/Data/Core/DA_GameBalance.uasset`。

保留该样本的文件名和旧格式，不要用新版编辑器重新保存。测试默认从本目录读取，也可通过 `-RogueBalanceLegacyPackage=绝对路径` 指定独立旧包。测试不会保存样本。