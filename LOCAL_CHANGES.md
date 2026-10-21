# Paper Pro Move 本地定制版

保存日期：2026-10-20。上游：https://github.com/nasonliu/REweread
上游基线：fae2b8f261fc6f3405875230e12259c0055e4859（3.0.1）。
此目录为独立项目，不再依赖原 Codex 工作目录。保留上游许可证和历史；没有发布到远程。

## 当前修改

- 兼容现有 weread.koplugin 1.6 的模块路径、会话配置及无界面网络辅助层。
- 关闭他人划线、想法、评论的后台获取及正文标记/弹窗；保留个人笔记、离线下载和进度同步。
- 默认字号 42，上限 64；已保存的个人字号优先。
- 正文解析结果保存在设备对应书籍目录的 reader-document-v1.json，权限仅限文件所有者。
- 校验源章节内容及资源元数据；缓存损坏或来源变化时重建。个人批注、阅读位置独立读取。
- 继续阅读首次分页从 12 页减到 2 页；增加只记录耗时的诊断信息。

## 已保存的可执行文件

build/local-artifacts/rm_weread_qt 是设备已安装的 ARM64 程序。
SHA-256：a7b1c2d74d8be476d27f72d4621a7d9b191625a9a5d5ca9942f18dd6c6789a71
同目录 SHA256SUMS 可验证文件；helper/ 保存匹配的 Lua 辅助代码。
build/ 按上游规则被 Git 忽略，二进制仍保存在本地项目文件夹内。备份整个项目时应包含它。

没有复制设备账号、Cookie、API Key、SSH 密码、书籍、笔记、字体、SDK 或设备日志。

## 验证范围

- 全部既有源码检查通过。
- 个人笔记与公共社交请求关闭的隔离行为测试，在本地和设备均通过。
- 新增 tests/reader_document_cache_smoke.cpp：临时合成书验证冷热打开内容一致、插图信息一致、损坏缓存回退及源章节变更失效。
- Move 合成书正文解析：首次约 1076 ms，缓存命中约 64 ms；不是用户真实书籍端到端打开测速。
- 设备 reader-ink 隔离回归（翻页、返回、笔迹、橡皮、清页）通过，启动无 QML/模块错误。
- 实际阅读速度及长时间使用仍需用户体验确认。

## 在当前 Mac 上构建

此前创建的 Colima 配置 reweread-build 和 Docker SDK 卷 rm_chiappa_sdk 保留在 Mac 上，未复制到本项目。
匹配设备的 SDK 为 chiappa 5.8.203 / Qt 6.10.3。

```bash
cd ~/Dev/REweread-Move
colima start --profile reweread-build --cpu 4 --memory 6 --disk 24 --vm-type vz --mount-type virtiofs
export DOCKER_HOST="unix://$HOME/.colima/reweread-build/docker.sock"
bash scripts/build-weread-qt.sh
```

首次在新目录构建会按上游脚本重新获取源码依赖；SDK 卷可复用。
检查需 Linux 行为时，可使用已有 Node 24 容器：

```bash
docker run --rm --platform linux/arm64 -v "$PWD:/work" -w /work node:24-bookworm node tests/run-all.mjs
```

缓存行为测试的本次编译配方另存 downloads/local-tests/build-document-test.sh（被 Git 忽略）。
该脚本在装有 SDK 的 Linux 环境内执行，不能直接作为 Mac 原生命令运行。

## 设备更新边界

当前设备已独立配置 AppLoad、系统汉化和拼音输入法。不要为了更新此程序再次运行会改写 Xovi/系统启动配置的首次安装脚本。
正常更新仅备份和替换 /home/root/weread-qt/rm_weread_qt，必要时同步 helper 文件；用户数据在 /home/root/.local/share/rm-weread/，应保留。

## 后续翻页预排优化

- 原先翻页时同步补算 8 页，改为显示页面后由 400 ms 单次计时器逐次预排 1 页，最多保留当前页及后续约 5 页。
- 快速翻页耗尽缓存时仅同步补算所需的下一页；书末不重复调度。
- 切书、改字号、离开阅读页时取消旧任务；打开设置/目录或批注时暂停预排。
- 这仍在 Qt 界面线程分片执行，不是独立后台线程；复杂单页和向前翻出当前窗口时仍可能等待排版。
- 新增 tests/validate-reader-idle-pagination.mjs 行为验证，并纳入完整检查。实际翻页速度仍需设备体验确认。
