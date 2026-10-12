# XOVI / AppLoad 系统更新兼容性与恢复

核查日期：2026-10-12。本文先记录本地审查，再记录用户后续授权的升级部署；用户阅读数据和 API 凭据不在变更范围。

## 后续授权部署结果

用户明确授权后，通过设备官方更新服务完成 3.28.0.172 安装。先关闭旧直接注入，确认原厂运行，再由独立、无重启策略、带超时及原厂恢复动作的 systemd 临时服务，仅加载 qt-resource-rebuilder 重建映射。新映射报告 20231 条记录。隔离试运行 AppLoad v0.6.0 成功，日志确认 hook 加载，用户确认入口可见；之后才生成本机精确验收记录、启用 guarded ExecStart。

目标机 `NotifyAccess=all`、`WatchdogSec=60`，允许监督器子进程发送心跳。完成重启验收：实际版本仍是 3.28.0.172，AppLoad 自动加载，Xochitl 无崩溃重启，根分区只读。没有手工切换启动槽。

另外修复新系统的私有刷新 ABI：旧 QRect overload 含 EPContentType，新 overload 不含；两种完整符号分别解析、使用独立函数指针。真机 direct ink ready 为 960x1696 stride=3840；小区域绘制/刷新/内存恢复探针通过，不打开数据 stores。字体面板、图文正文、阅读统计自检通过，字体及图文截图已检查。新系统初始启动比原有短时测试窗口慢，短窗口无自检结果不能直接定性死锁。长时间手写体感、休眠耗电和完整排版组合仍不属于本轮通过项。

最终应用 SHA-256：`62aeaaea2e765a5823cc8cf7756ab97b62fe01644bb685ef62ee4c0b2fb3f80d`。升级前已有用户数据文件在设备本地逐文件校验全部一致；校验清单、原始日志和截图不入仓库。应用、旧 AppLoad 和旧映射均保留设备端备份。

## 已确认的原因

3.28.0.172 在未注入时可以启动。恢复旧的直接 `LD_PRELOAD` 后，日志先报告 hashtab 仅适用于 3.27.3.0，再报告 AppLoad QML hook 标识无法解析，随后 Xochitl ABRT。多轮失败后设备由新系统分区切回旧分区。120 秒 transient 回退定时器在几秒后的系统重启中消失，不能作为启动保护。

## 上游核查

| 组件 | 上游最新发布 | 结论 |
| --- | --- | --- |
| XOVI core | v0.3.3，2026-03-29 | 不能把 extensions bundle 的 v19 当 core 版本 |
| rm-xovi-extensions | v19-23052026，2026-05-23 | 系统更新后仍须重新生成版本相关 hashtab |
| AppLoad | v0.6.0，2026-09-19 | 包含 3.28 hook 修复；不是本设备验收结论 |

来源：[XOVI](https://github.com/asivery/xovi/releases/tag/v0.3.3)、[extensions](https://github.com/asivery/rm-xovi-extensions/releases/tag/v19-23052026)、[AppLoad v0.6.0](https://github.com/asivery/rm-appload/releases/tag/v0.6.0)。

[PR #59](https://github.com/asivery/rm-appload/pull/59) 于 2026-09-07 合并，提交 `3b42440e369a82535fb93df4a9155de9199cf487`；GitHub compare 确认 v0.6.0 在该提交之后 17 个提交，无落后提交。PR 报告在 Move 3.28.0.157 测试，并明确不兼容 <=3.27。不要把新 AppLoad 直接装进当前 3.27 系统。

[Issue #65](https://github.com/asivery/rm-appload/issues/65) 记录重建 hashtab 后旧 AppLoad 仍然崩溃；[Issue #75](https://github.com/asivery/rm-appload/issues/75) 讨论 3.28.0.172 同类问题。必须同时匹配 hook 和映射，不能只重建映射。

v0.6.0 官方 aarch64 archive 发布摘要为 `2e7912d15df910631dab847ea5d344081ad75c02a46aaac78bdf731accc53a1f`。第三方下载或任意 master 构建不替代发布资产校验。extensions master 的 `6c709dd8416ddf89034831073d76fb4ee3372905` 更新 qmldiff，但没有对应新 bundle release；本轮未擅自混用。

本轮在仓库外下载上述官方 archive，实际 SHA-256 与发布摘要一致；没有上传设备或提交依赖。v0.6.0 还包含 [PR #77](https://github.com/asivery/rm-appload/pull/77) 的 3.29 修改，上游明确注明不会破坏 3.28 兼容性；这仍不是本机验收凭据。

## 本地保护设计

`scripts/xovi-guarded-start.sh` 是条件启动监督器：仅当人工验收凭据与当前 OS、Xochitl、XOVI、AppLoad、资源重建组件及映射完全一致时允许注入。未知、缺失、文件变化或遗留故障标记均降级到原厂 Xochitl。凭据不是 API Key，只包含版本和校验值；不得自动把当前文件自签为“兼容”。

注入前持久写入 in-flight 标记。异常退出回到原厂进程；未正常收尾的标记在重启后继续阻止注入。正常退出和有序停止通过监督器处理。保护不依赖 Wi-Fi、SSH 或临时定时器。

安装器先调用 `--check`，再允许任何上传、应用替换或根分区修改。永久 drop-in 清空继承的 LD_PRELOAD/XOVI_ROOT，以 shell 监督器替代直接注入。保留系统自带 override。没有经过验收的凭据时，安装器会拒绝继续；已有安装的纯应用升级仍走 `upgrade-weread-qt-files.sh`。

本地模拟测试不证明 Qt UI、休眠、看门狗或内核层兼容性。持久标记可阻止下次启动再次注入，但不能保证内核挂死或硬件掉电时本次启动不中断。原厂 Xochitl 自身损坏也不在保护范围。

验收记录文件为 `/home/root/xovi/compatibility-receipt.env`，以严格的 `key=value` 文本读取，不作为 shell 执行。字段：`format=1`、`os_identity`（`/etc/version` 原文去换行）、`os_identity_sha256`、`xochitl_sha256`、`xovi_sha256`、`appload_sha256`、`qrr_sha256`、`resource_inventory_sha256`。资源摘要按脚本中的有序文件列表算法计算，覆盖 hashtab 与同目录资源；仅允许两个已审查的活动扩展。此文件必须在目标组合真实验收之后人工制作，不能为了让安装继续而先生成。当前仓库没有任何预先批准的 3.28 记录。

故障标记为 `/home/root/xovi/.xovi-injection-in-flight`，不在用户阅读数据目录中。不要自动删除它重试；先诊断异常、确认注入进程已停止及原厂系统健康，经过重新验收后才能清理。监督器运行期间也存在该标记，所以安装前应处于原厂模式，不允许正在注入时覆盖依赖。`XOVI_GUARD_TEST_MODE` 只用于本地假进程测试，不应写入生产 systemd 环境。

## 映射重建边界及后续验收

hashtab 来自目标系统实际 Xochitl QML 资源，不能从旧版文件改版本号，也不能在 Mac 上凭空生成。上游 [资源重建说明](https://github.com/asivery/rm-xovi-extensions/tree/master/qt-resource-rebuilder) 要求系统/组件更新后重建。重建阶段必须隔离扩展，只允许 qt-resource-rebuilder，不能先加载旧 AppLoad。

初次本地审查时未生成 3.28 验收凭据、未运行设备重建、未切换启动分区；后续授权部署结果见上文。以后也只有得到用户单独授权，准备好原厂恢复路径、版本匹配资产及不依赖临时定时器的保护后，才能进行受控重建和单次加载验证。必须确认 hashtab 版本正确、AppLoad hook 加载、UI 显示、应用启动退出、原厂恢复，再制作精确校验凭据。即使上游发布支持相同主版本，也不得跳过真机验收。
