# 原生阅读位置只读原型

这些工具不进入插件安装包，不改变系统书库，不上报阅读进度或时长。只输出计数与校验结果，不输出书名、正文、文档/账号标识或登录凭据。

本地测试（在仓库根目录）：

```sh
lua WereadDownloaderPlugin/tests/progress_probe_test.lua
```

设备使用已安装的 KOReader LuaJIT 和系统 PDFium。将本目录的 `.lua` 放入设备私有临时目录，在 KOReader 目录运行：

```sh
./luajit /tmp/wrdp-progress-probe/probe-native-progress.lua /tmp/wrdp-progress-probe
```

默认不联网。增加最后一个参数 `cloud-read`，仅为第一本成功定位的书读取云目录、原始章节和云端保存的位置，验证当前账号、目录指纹、章节 ID、正文映射与已保存摘要的坐标。调用层只允许阅读页面 GET、章节正文读取 POST 和目录读取 POST，其他请求（包括 `/web/book/read`）直接拒绝。独立使用下载插件登录态，凭据更新仅在内存，不续期或落盘，也不借用独立 App 登录态。

```sh
./luajit /tmp/wrdp-progress-probe/probe-native-progress.lua /tmp/wrdp-progress-probe cloud-read
```

`previewMappingReady=true` 只证明本次样本可生成预览候选，不代表功能已经接入 UI 或完成上传验证。`remoteOffsetUnitResolved=false` 表示尚未区分原始 HTML 的 Unicode 字符数和 UTF-16 单位；候选仅在这两个坐标相同时通过。云端保存摘要也必须在同章原始正文中唯一匹配且偏移相符。

该原型只支持恒等页映射，不支持编辑过的页面布局、复杂 HTML/CSS/实体或 OCR。读期间系统位置或排版元数据发生变化时拒绝候选。默认扫描预算约 25 秒、32 MiB 章节内容；云读取为额外的有限请求，网络耗时不包含在本地扫描预算中。运行后移除本次上传的临时脚本目录，不删除任何书籍或缓存。

当前数据、测试覆盖和剩余限制见 `docs/native-library-progress-sync-research.md`。
