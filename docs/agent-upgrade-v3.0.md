# Agent 升级到 REweread 3.0.1

先读 `AGENTS.md` 和 `docs/agent-handoff.md`。本次是 source-only 源码里程碑，不附二进制或第三方依赖，不代表全部功能已完成实机验收。

## 3.0.1 升级重点

使用匹配系统 3.28.0.172 的 chiappa SDK 5.8.203 / Qt 6.10.3，避免旧 SDK
导致每次启动重新编译 QML。构建脚本会拒绝不匹配的 Qt，不删除旧构建产物。
3.0.1 增加同书短时阅读状态复用、失败后刷新和明确的下载后打开意图。
更新后验证首次启动、正文图文页、手写翻页/擦除，以及后台下载完成不会切书。
详见 [3.0.1 发布说明](releases/v3.0.1.md) 和
[性能验证记录](performance-investigation-2026-10-13.md)。

## 已安装设备

1. 确认工作区没有需要保留的未提交修改，然后 `git pull --ff-only`，核对 `VERSION` 为 `3.0.1`。
2. 运行 `node scripts/check-repository.mjs`、`node tests/run-all.mjs`，对 shell 脚本执行 `bash -n`。
3. 需要字体时运行 `scripts/download-reader-fonts.sh`；运行 `scripts/build-weread-qt.sh` 使用官方 chiappa SDK 构建。字体不进 Git。
4. 获得设备操作授权后，确认当前连接、系统版本、运行中的应用和备份。严禁删除 `/home/root/.local/share/rm-weread/`，不读取或输出凭据。
5. **已有可用 AppLoad 时**：`MOVE_HOST=root@DEVICE bash scripts/upgrade-weread-qt-files.sh`。该脚本保留已有字体、数据及带时间戳的旧应用备份，不重新安装 XOVI、不改根分区。
6. 验证应用版本、正文图文页、翻页、自由笔迹清空/橡皮、回到原页的笔迹、阅读统计和退出回到系统。自检只证明指定场景，不能替代真实书写验收。

USB 地址通常为 `10.11.99.1`。如设备已有另一个地址的可信主机记录，只有核实主机公钥一致后才传 `MOVE_HOST_KEY_ALIAS`；不要关闭 SSH 主机校验。

## 首次安装或系统更新后 AppLoad 消失

不要直接重跑旧恢复命令。先读 [兼容矩阵](compatibility.md) 和 [XOVI/AppLoad 恢复说明](xovi-appload-recovery.md)。3.28.0.172 需要兼容的 AppLoad v0.6.0 及按该系统重新生成的映射，不能沿用 3.27 的映射。

`install-weread-qt-appload.sh` 会检查 guarded startup 凭据；不能伪造凭据跳过验证。首次安装涉及根分区重新挂载为可写、系统服务修改或重启时，必须提前说明影响并取得同意。不要为了升级应用再次启用 Developer Mode；首次启用会恢复出厂。

## API Key 配置与保留

原有百度 OCR、DeepSeek 配置无需重填。只更新用户提交的服务，另一服务及未修改的凭据保持不动。不要将密钥放进源码、终端参数、Git 或日志。

用户在 **我的 → 百度 OCR → 开启浏览器配置** 手动开启临时 HTTPS 页面，输入设备显示的配对码，在服务选择器中选择百度 OCR 或 DeepSeek。百度填写同一 OCR 应用的 API Key 和 Secret Key（AppID 不填）；DeepSeek 填其 API Key。详细申请、权限与绑定步骤见 [云服务配置指南](baidu-ocr-configuration-flow.md)。

## 不要误报的能力

阅读统计从本机更新后的阅读开始，不等于微信读书云端历史。只有明确成功回执才算同步成功；不明结果不得无条件重试。云笔记目前是拉取缓存，本机手写不自动发布。整本离线下载和精确远端章节偏移仍需独立验收。
