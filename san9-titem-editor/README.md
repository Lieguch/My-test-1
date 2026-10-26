# San9TItemEditor

三国志9 PK 宝物档 `D_TItem.s9` 的离线编辑器（Win32 / C / 无第三方依赖）。

## 构建

推送到 `main` 或在 Actions 页面点 **Run workflow**，产物在 Artifacts 里下载 `San9TItemEditor`。

本地构建（需 MinGW-w64 i686）：双击 `build_local.bat`。

## 安全设计

- 保存前自动备份为 `D_TItem.s9.bak.YYYYMMDD_HHMMSS`
- 写 `.tmp` → 回读逐字节 `memcmp` 校验 → 一致才覆盖 → 删 `.tmp`
- 任何一步失败，原文件不动
- 校验：能力值 ≤ 15、状态 ∈ {1,2}、种类 ≤ 32、图像 ≤ 113、文件长度必须 4240

## 文件格式

16 字节头 + 176 条 × 24 字节（每条 12 个 little-endian u16）

| # | 含义 |
|---|---|
| f0 | 序号（只读） |
| f1 | 名称ID |
| f2 | 保留 |
| f3 | 图像代号 |
| f4 | 宝物种类 |
| f5 | 忠诚度 |
| f6 | 能力值（≤15） |
| f7 | 兵法代号（65535=无） |
| f8 | 持有武将（65535=无） |
| f9 | 状态（1未出现 / 2已出现） |
| f10 | 地点代号 |
| f11 | 保留 |

## 源码编码说明

- `titem_editor.c`：UTF-8，编译时用 `-fexec-charset=GBK` 让中文字面量以 GBK 落入 exe（ANSI API 正确显示）
- `titem_editor_gbk.c`：GBK 字节版，作为 fallback 直接编译
