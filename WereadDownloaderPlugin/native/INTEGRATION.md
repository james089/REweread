# 原生入口适配记录

以下保留研究初期的接口审查记录。插件现已完成目标固件的资源回放、安装与加载检查；当前实现和验收范围见 [设备交接记录](../DEVICE_HANDOFF.md)。

用户最终选择：系统主界面左侧栏，AppLoad 正下方增加“微信读书下载”，直接进入独立登录/书库页面；不进入设置页。

## 已核实的上游接口

- AppLoad commit `df974a6d641f37492df04013c53df2e5d6403488` 的 `xovi/template/appload.qmd` 在系统侧栏插入按钮，QML id 和 objectName 都是 `openAppLoad`。其余固件标识经过 hash，不能直接从数字猜测资源路径和控件名称。
- 同版本 `src/Launcher.h` 提供 `AppLoadLauncher.launchApplication(QString id, QVariantList args, QVariantMap env, bool window = false)`。可评估通过左栏按钮启动插件的 AppLoad 前端；这满足入口位于 AppLoad 下方，而不要求用户先打开 AppLoad。
- rmtool commit `69a47791355942a6008af4c9829e102a7b5ed230` 的 `native-import/README.md` 和 `qr-upload/native/ImportControl.cpp` 确认原生导入使用本机 HTTP `/upload`、multipart 字段 `file`，由系统生成 UUID/元数据与 EPUB 转换。
- rmtool 的原生导入控制通过运行中的 `Settings.webInterfaceEnabled` 临时启用服务，并恢复原设置。它不是可随意写文件替代的设置。设备本机地址准备、并发互斥、超时和崩溃恢复也必须一起处理。

## 必须先验证

1. 当前设备固件/内部 build/Xochitl 指纹、AppLoad 和 hashtab 精确对应。
2. 在 AppLoad 已应用之后插入新按钮，并回放检查位置和其他扩展共存。未定位到 `openAppLoad` 时构建应失败，不能悄悄插在别处。
3. 验证前端启动/关闭与原生页面、书库、系统休眠的生命周期；下载和扫描不能在系统 UI 主线程运行。
4. 从系统保留的 EPUB 读取来源 ID，得到对应 UUID；确认原生根目录语义。
5. 本地原生导入服务能够在无电脑的设备上开启和恢复；未验证不能声称已支持脱线导入。

USB 只读连接本次超时，未获得当前设备的版本或指纹。没有为了适配重启、升级 AppLoad、修改系统文件或读取用户书库。

## 许可证边界

这里只引用接口与适配事实，未复制上游实现、资源或固件。AppLoad/rmtool 为 GPL-3.0 项目；如后续复用其实现代码，应在实际复用范围内明确许可证，不能直接贴入 MIT 文件后声称自有代码。

来源：[AppLoad QMD](https://github.com/asivery/rm-appload/blob/df974a6d641f37492df04013c53df2e5d6403488/xovi/template/appload.qmd)、[Launcher](https://github.com/asivery/rm-appload/blob/df974a6d641f37492df04013c53df2e5d6403488/src/Launcher.h)、[原生导入说明](https://github.com/pretenderlu/rmtool/blob/69a47791355942a6008af4c9829e102a7b5ed230/native-import/README.md)。
