# Native downloader device handoff

2026-10-29 安装20261029233645成功：1秒补传间隔、取消60笔/90秒批次限制、全量目标进度和无进展超时已部署。host/sidebar-ready及.install-ok匹配、完整install.sha256通过，无回滚，汉化QMD加载；boot ID保持不变，系统active/NRestarts=0，仅软重启。另在容器用合成POST实际执行生产nanosleep，61秒两笔之间真实等待至少1秒通过；未执行额外真实账号补传测试、未重置旧确认账本。

2026-10-29 1秒节奏修复：按用户明确要求修改并安装（加载确认中）。补传每笔成功后等待真实1秒再构造下一笔，不改ct，不逐笔GET；去掉60笔/90秒批次上限，目标为本次全部待传，归零结束。helper时长相关操作改按无进展超时、原生退出watchdog收到时长进度续期，避免旧绝对时限截断长补传；仍保留网络失败/未知回执保护及关闭取消。同步位置若有时长，由第一笔携带目标位置，取消先发rt=0造成同秒冲突的多余请求。Lua合成验证7265秒122笔/121次1秒等待、全量进度与防重；SDK、Qt6完整UI、Linux全量、QMD回放与shell检查通过。旧账本未重置，不自动重发已确认但未入账的历史差额。

2026-10-29 用户要求先暂停修改、测1秒间隔：同一最近补传书，一次GET/enter，3笔各60秒，第二/第三笔前各等待1秒，ct各不同。云端立即及约19/41/72秒回查均增加180秒；恰好3笔时长POST，不重试、不改待传/确认账本，无应用代码/安装变化。小批量测试表明1秒等待也可行，尚未对一小时规模验证。用户原先要求修改补传节奏，随后“等下”先要求此测试；节奏修复仍未实施。

2026-10-29 用户授权缓存进度/间隔2秒对照：同一最近补传书，只GET一次位置/会话，一次enter-read，3笔各60秒，第二/第三笔前各等待2秒，3笔ct均不同。成功回执3笔，云端立即及约21/43/74秒回查均增加180秒，章节坐标未变；恰好3笔时长POST，无重试、不改本地待传/确认账本。私有意图仅设备/tmp。缓存坐标+2秒节奏在此小样本验证有效，可据此修改补传节奏；尚未修改或安装节奏修复，不能据一次3笔保证一小时全量。

2026-10-29 用户明确授权两组各3×60秒真实对照：同一最近补传书，每组各一次enter-read，A只读一次上下文后连续3笔，B每笔GET进度再POST；总共恰好6笔时长POST，不重试、不改本地待传/确认账本，私有意图记录仅在设备/tmp。A三笔成功回执但两笔ct同秒（3笔共2个不同ct），云端差额立即/约14/35/67秒均120秒；B三笔ct各不同，差额立即/约16/37/69秒均180秒。云端章节坐标未变，测试完成，无额外写入。结果强烈支持同秒ct碰撞/去重，而非必须逐笔GET；尚未测试“保留缓存坐标、等待跨秒再发”，不能宣称其已验证。后续可实现按真实时钟错开ct（不伪造时间戳），保留每笔<=60秒及防重复账本，再经用户授权小批次验证。

2026-10-29 补传实际入账不足只读诊断：用户报告提交一小时多但云端约16分钟。设备账本确认按业务回执扣减，不能证明同额入账。只读GET核对最近补传会话的单书累计相对入场基线仅增加少量秒数，远低于回执累计；未执行额外POST、未改确认账本。上游make_read_payload的ct为os.time整秒，ts/rn另有随机值；离线构造60个payload只得到1种ct，证明突发请求可共用整秒字段，但不能据此证明服务端按ct去重，或保证间隔1秒即可修复。旧逐笔GET循环与新高速POST循环的节奏不同，快速补传可靠性未验证，不能因60秒单次成功推断60笔全入账。下一步需获用户明确测试范围后作小批次/节奏对照，以云端差额而非succ计效果，不重置旧回执或自动重传差额。

2026-10-29 安装20261029231009成功：待传时长显示、归零/不明回执禁用补传、逐笔成功进度条已部署。用户在本次软重启后解锁，host/sidebar-ready及.install-ok匹配、完整清单通过，无回滚；汉化QMD加载，boot ID不变，Xochitl active/NRestarts=0。未代用户执行真实补传测试。

2026-10-29 待传显示/补传进度验证通过，但设备更新20261029230112未完成：完整文件清单校验通过，软重启后180秒内未收到host/sidebar-ready（等待解锁），保护计时器已回退到安装20261029225457；当前.install-ok为旧版，Xochitl active，boot ID不变。此次新增待传/禁用/进度条尚未在设备生效，等待用户解锁后重新安装。SDK、Lua合成、Qt6完整UI和Linux全量回归通过；未代用户发真实时长。

2026-10-29 补传统计与进度（源码验证中）：逐书恢复“全部待传”=本机总秒数减成功回执账本累计，日期筛选不缩小补传范围。待传归零禁用补传按钮，C++入口同样拦截；不明回执显示待核对并禁用。补传每笔成功落盘后发送安全的进度事件，宿主透传，显示本轮已提交/目标及百分比和进度条；失败/不明回执不推进。60笔上限以本轮最多3600秒计算，剩余保留。新增待传150→25→0/零值拦截/不明回执阻止/按钮恢复及60/125=48%合成UI验证，Lua验证初值和逐笔成功进度。

2026-10-29 安装20261029225457：包含60秒实时上报、连续分笔补传、提交回执累计、原生手动拉取确认弹窗。install.sh保护事务成功，.install-ok匹配，完整install.sha256及QMD副本校验通过，无回滚；本次启动日志host-ready/sidebar-ready及rmtool-native-chinese.qmd加载确认。boot ID安装前后相同，Xochitl active/NRestarts=0，只有系统界面软重启。未操作真实书籍触发测试补传/覆盖，设备触控和真实云端入账仍待用户使用验证。

2026-10-29 恢复分笔补传（源码，尚未安装）：统计页逐书“补传时长”恢复；退出/手动上传进度也排空未确认时长。每笔最多60秒，连续发送、不等待60秒；每轮最多60笔/约90秒，剩余保留。独立补传只初始化一次云端位置/会话，全部分笔复用该云端坐标，不读取PDF或章节，不逐笔GET、不逐笔验证；协议仍携带固定云端坐标，不能描述成无位置字段。实时周期与补传共用uploaded-reading-time-v1账本，每笔POST前落盘意图，成功逐笔确认，明确拒绝释放，未知回执保留并阻止后续自动重传。包含跨日积压但只能计入服务端提交日，不能回填原日期。合成一小时=60笔/一次GET测试通过，云端位置保持，失败/未知/重复/尾段/分批续传覆盖；未发送真实账号写请求、未连接设备。

2026-10-29 原生手动拉取进度与提交累计（源码，尚未安装）：进度面板改为“本次已成功提交”时长，只累计当前阅读会话 rt=60 成功回执，不代表服务端已入账，失败不累计、重新打开重置。原生菜单仅微信读书导入书显示“拉取微信读书进度”；先获取当前云端坐标、章节序号/总数及摘要 snapshot，圆角全屏遮罩弹窗确认后才执行既有精确定位和原生翻页。取消不改本地进度；确认使用同一份快照，不重读云端位置，账号/书籍/UUID/本地文件校验，5分钟过期且一次性使用。覆盖后清空周期上报旧坐标上下文。SDK构建、Qt6离屏按钮触摸及累计/取消/确认合成测试、Lua快照防误用测试、全量Linux回归及固件联合QMD回放通过；未连接设备、未发送真实云端测试写请求。

2026-10-28 阅读会话状态面板：按用户确认，状态集中显示在原生页码滑条/目录/概览面板底部两行，只对当前导入书显示；入场已上传/尝试次数/等待Wi-Fi/暂停/终止及本次本机阅读计时。计时为本机数据，不代表云端已入账；保留原生工具栏避让，增加104底部空间，正文无常驻文字。官方SDK QML语法与联合补丁回放通过；设备Qt6合成回归、Linux全量、SDK构建及shell检查通过；安装20261029030245确认host/sidebar-ready和.install-ok，全清单通过、无回滚，boot ID不变，Xochitl active/NRestarts=0，本次仅软重启。未操作真实书籍验证视觉效果。
2026-10-28 Wi-Fi 入场门控：宿主绑定固件既有 LinkProvider.isOnline（原生 Navigator 的 wifiOnline 同源），初值离线；调度和执行两处均检查联网状态，断网停重试定时器，连接恢复由状态信号重新调度，无定时联网探测、不启用/扫描 Wi-Fi。已发出的请求仍按原超时收尾。保持休眠/锁定/切书限制。新增合成断网不启动、失败后断网不重试、恢复但休眠不请求和唤醒恢复验证。
2026-10-28 入场失败恢复：新增阅读入场状态机，失败后5/15/30秒、之后60秒退避重试，仅在同一本已导入书前台有效阅读时运行；退出/切书取消，休眠/锁定/后台/空闲暂停，忙碌时稍后再试，25秒超时可重试。登录/账号/来源/私有缓存错误终止，后台不弹扫码。记录固定安全错误码与尝试次数，不打印凭据/内容。Lua入场回执成功后先持久化ack及原计时基线，后续云端基线回读失败只重试回读，不重复enter-read、不重置startTotal/enteredAt。新会话仍不补入场前的旧时长，不改变退出时长POST的防重规则。新增设备合成测试覆盖首次失败后成功、成功停止重试、休眠暂停与唤醒恢复、关闭取消，以及回执后回读失败可续且不重复入场。设备Lua/Qt6 UI、Linux全量和SDK通过；安装20261029024939文件清单、.install-ok及host/sidebar-ready已确认。此次systemctl restart xochitl任务被取消，随后boot ID改变，实际发生整机重启；不能记为仅软重启。系统现active/NRestarts=0。本轮未向真实书发送测试入场/时长请求。


2026-10-28 时长差额只读核对：本机按书按日记录显著大于成功回执累计；失败/无入场历史仍未提交，回执成功也不能视为完整入账。用户指出插件云端日数值，实时API与缓存一致，但发现cloud_reading_stats.normalize把云端每日桶起点用所选本机TZ转日期：本次桶均为UTC16:00（北京时间次日00:00），在Chicago会被标为前一日。上方云端为账号总日量，下方为单书本机日量，来源和日边界均不同。此为待修显示口径问题：不能声称云端桶可按任意用户时区重新划分，也不能以整桶平移恢复当地日明细。本轮仅诊断，没有改数据/补传/部署。


2026-10-28 零新增时长误报修复：最新短会话在入场网络请求完成前已退出，入场保存的startTotal等于退出总数；有效会话eligible=0却被uploadCombined统一标为skipped，导致worker position-unchanged返回timeUploaded=false并显示错误。新增liveSession区分：匹配未消费会话零增量正常完成(noNewTime=true/skipped=false)，不消费会话、不伪造时长；缺失会话/未确认回执仍保留原失败路径。设备纯合成零增量/后续正常增量/拒绝/不明回执与防重测试通过，SDK/Linux全量/shell语法通过。仅热更新reading_time_sync.lua并更新全清单校验，系统active/NRestarts=0，无重启，不补传旧时长。


2026-10-28 云端位置冲突保留：用户授权在冲突时保留云端位置。精确定位后若云端签名与基线不同，不再抛cloud_progress_changed；以刚读取的云端章节/偏移/百分比/摘要构造report_read，只附带当前已确认入场会话的有效时长，上传后回读验证目标坐标。不补历史、不绕过账号/原文/本地变更校验，不新增enter-read。结果新增cloudPositionPreserved标识，后台及前台展示“云端位置已保留”；后台提示保留到书架刷新后。合成测试验证20秒单次上报、云端坐标/摘要/百分比不变、无额外入场与回执计数推进；SDK/Linux全量、设备Lua、设备Qt6合成UI通过。安装20261029015328确认host/sidebar-ready、完整清单无回滚；boot ID不变、系统active/NRestarts=0。临时合成测试目录已清理。该报告仍是按回执提交，不等同于服务端最终统计入账。


2026-10-28 本机逐书日记录：用户明确同意下方使用本机记录并标注来源。readingStats改读当前账号local-reading-time-v1 books.days[所选日期]；全部日期读seconds，仅保留已导入且有正时长条目并按时长降序。上方云端总量/日期/周图保持。标题标注“仅本机记录，不含其他设备”，每条区分“本机当天/本机累计”，空列表明确本机无记录。合成UI增加同书不同日期125/20、空日不显示、全部150与云端字段240/42分离验证。SDK/Linux全量、设备Qt6合成UI通过；安装20261028220306确认host/sidebar-ready、完整清单无回滚。boot ID不变、系统active/NRestarts=0，临时UI目录已清理。


2026-10-28 统计日期与逐书列表口径修复：日期选择此前只切换总时长，下方列表始终显示累计。现仅“全部日期”绑定累计逐书列表；选日时清空该列表并明确说明当前云端接口未提供当天逐书明细，不把未知当成0或累计当日值。不新增本地推算或伪造云端记录。SDK/Linux全量、shell语法、固件回放、设备Qt6合成UI通过；安装20261028215538确认host/sidebar-ready、完整清单无回滚，boot ID保持不变、系统active/NRestarts=0。临时UI测试目录已清理。


2026-10-28 原生退出去掉重复enter-read：按用户要求，带WEREAD_READING_SESSION的自动退出sync只提交进度/有效会话时长，不再调用markRecent发送enter-read；打开书籍仍由beginReading发送一次，手动上传与导入最近阅读更新保持。设备纯合成Lua回归确认自动退出没有额外enter，手动上传仍发送；SDK、Linux全量、shell语法通过。仅热更新reading_progress.lua并更新安装清单，全清单校验通过、Xochitl active/NRestarts=0，无系统重启。另只读诊断确认一次实际退出位置同步成功但时长跳过，持久化入场会话停在前一次；旧版不保留entry失败原因，不能断言是网络还是回执失败。未补传历史时长，入场失败的诊断/恢复尚未改动。


2026-10-26 统计页工具栏：书架刷新/导入/退出多选/取消整行仅在书架tab显示，统计页保留独立刷新统计入口。仅一处可见性条件变更。SDK、Linux全量、shell语法、精确固件回放检查通过；安装20261027040205已确认，后续只读全清单校验通过。


2026-10-26 统计配置回填与鉴权提示：用户要求重新打开时显示已保存配置，新增仅当前已登录/已打开/匹配账号的编辑读取方法，默认隐藏、显示按钮可核对；关闭仍清理临时输入。只读诊断确认当前统计请求HTTP401鉴权失败，不是本地未保存；不输出密钥。Lua只将401/403映射为固定statistics_auth_failed，UI明确提示更换/核对密钥，其余异常保持粗粒度，保留旧缓存。新增回填/默认隐藏与401分类合成验证。SDK/Linux全量、设备Lua和Qt6合成UI通过。安装20261027035636确认host/sidebar-ready、完整清单无回滚；软重启boot ID不变。


2026-10-26 统计 API 输入可见性：输入框旁新增88高“显示/隐藏”按钮，切换保留输入文本并恢复输入焦点；默认隐藏，每次关闭清空文本并重置隐藏。不读取或展示已保存密钥。新增合成字符串显示切换/文本不变/关闭清空回归。SDK、Linux全量、设备Qt6合成UI通过。安装20261027035059确认host/sidebar-ready、完整清单无回滚；软重启boot ID不变、系统active/NRestarts=0。


2026-10-26 系统键盘遮挡最终修复：前两次将原生键盘当成独立窗口的解释不正确。固件 VirtualKeyboardWindow.qml 实为同场景 BaseView，z=10000；Qt Quick Popup 的 Overlay 被提升到场景上层，仍挡键盘。统计配置改为插件内普通 Item/Rectangle，完全移除 Popup/Overlay；宿主z改9999，低于原生键盘10000。保留页面内暗遮罩、上下40内边距与等宽88高按钮。新增同场景z10000模拟键盘接收真实QTest鼠标点击回归。SDK/Linux全量、设备Qt6完整UI及模拟键盘点击通过。安装20261027034812确认host/sidebar-ready、完整清单通过无回滚；boot ID不变、系统active/NRestarts=0。真实键盘触控仍需用户验收。


2026-10-26 统计配置输入二次修复：上一版 modal Dialog 会阻止独立系统键盘窗口的触控；改为 modeless Dialog + 本窗口遮罩触控拦截，不拦截系统键盘。撤掉固定440高度与128页脚，标题/说明/输入/按钮统一内容布局，按钮保持88高度且底部40留白、等宽；位置距顶部32。新增非模态、等宽/高度、按钮不溢出内容区、底部留白断言。官方SDK、Linux全量、设备Qt6合成UI通过。安装20261027034437确认host/sidebar-ready、完整清单通过无回滚；仅软重启，boot ID未变、Xochitl active/NRestarts=0。真实系统键盘触控需用户验收。


2026-10-26 统计 API 输入弹窗关闭修复：系统输入法窗口可能使应用暂时 Inactive；侧栏宿主改为仅在 Hidden/Suspended 时关闭页面，阅读计时的激活状态保护不变。统计配置弹窗使用 NoAutoClose，只有取消/保存才关闭，关闭时释放焦点并隐藏键盘。新增弹窗打开/聚焦保持显示回归；官方 SDK、Linux 全量、设备 Qt6 离屏 UI 通过（临时测试未携带图标资源，有预期图标路径警告）。安装 20261027034035 已确认 host/sidebar-ready、安装清单通过且无回滚；boot ID 不变，Xochitl active/NRestarts=0。真实系统输入法切换仍需用户触控验收。

2026-10-26 时长功能收敛与独立云端统计：按用户要求停用手动上传时长（UI/C++/worker/Lua 入口均拒绝），手动上传进度不带历史时长。原生阅读开始新增异步 enter-read，会话随机标识传给退出同步；成功入场后更新云端冲突基线并记录当前本地计数，退出仅一次提交此会话新增有效秒数，且不超过实际经过时间；旧积攒时长、未入场/断网进入、已消费会话和不明回执均不重放。未翻页退出也走独立会话关闭路径。旧本地计数/回执保留。统计页只显示账号云端总量、每日/周图、导入书籍云端累计，不显示本地/待传/历史或时长上传按钮。用户明确选择“完全独立，另加统计API配置”，新增密码框配置入口与账号绑定0600文件，不读取独立 App 配置，显式退出清除统计凭据；只读 overall/monthly 官方统计，未知数据不填本地。日期桶按所选时区显示，缓存防跨时区混用。设备纯合成 Lua 会话单次增量/无旧账重放/拒绝/未知回执防重、云端统计独立配置/账号绑定、进度上传兼容测试通过；宿主Core、SDK和Linux全量通过。设备 Qt6 离屏合成 UI 全套通过，包含独立统计配置权限/账号绑定、云端统计、手动时长停用与既有交互回归。安装 20261027033218 已确认 fresh host/sidebar-ready、安装清单及本地/设备 worker/.so 哈希一致，无回滚；软重启后 boot ID 不变，Xochitl active/NRestarts=0，中文版保持。临时合成测试目录已清理。本轮未额外提交真实时长；新增统计 API 仍需用户在插件内配置，真实书籍入场/退出整合与配置键盘触控未作用户验收。

2026-10-26 阅读时长时区修复：Qt 固定时区 ID（如 UTC-05:00）直接传给 libc TZ 会反向解释为 UTC+5，导致 UI 与 Lua 对今天的日期不一致、零提交却返回成功。新增 src/reading_time_zone.h，在 worker 启动 Lua 时转换 ISO→POSIX 符号，WEREAD_TIME_ZONE 保留 Qt ID、IANA TZ 保持不变。Core 行为测试覆盖 -12 至 +14 在 libc 中的真实日期/小时及半小时偏移；主机 Core、官方 SDK、设备 Qt6 合成 UI、Linux 全量通过。上传完成显示本次已提交/当天剩余/往日未传，不再被自动书架刷新覆盖。历史记录只保留不伪装成今天。真实诊断上传使用正确时区已产生成功回执和账本推进，但云端累计增量明显少于提交量，重复回读仍如此：不能将成功回执称为全部入账，也不能据差额盲目重传。远端计时/历史补传能力仍待研究，不批量修改既有回执账本。安装 20261027023316 已完成，host/sidebar-ready、汉化激活、安装清单和本地/设备 worker/.so 哈希一致；软重启成功、boot ID 不变、Xochitl active/NRestarts=0，原生菜单清单保持。临时合成 UI 测试目录已清理。


2026-10-26 cPages 历史记录与非同序映射兼容：新增只读 resolve/originalPage/nativePage，过滤 deleted CRDT 记录、按 idx 排列有效页，再经 redir 转换系统页码与原始 PDF 页码；支持重复/缺失原始页和空白页。相同 idx 只有指向同一 PDF 时允许，冲突映射仍拒绝。上传和本地章节读取使用映射后的 PDF 页；下载进度转换回系统页，重复页优先距当前页最近的有效位置。正文/来源坐标、账号/缓存哈希和并发变化校验保持，章节定位缓存 key 升为 v2；本地百分比排除删除记录。设备合成测试覆盖历史删除、乱序存储、重复页、反向定位、已移除目标拒绝和元数据不变；主机 Core、Linux 全量及官方 SDK 通过。受影响真书只读预检完成精确章节/原文定位，主动禁止网络与保存。辅助程序热更新 2026102701 已安装，worker 与两份 Lua 哈希一致、清单校验通过；不改常驻 .so/QMD、不重启 Xochitl，boot ID 保持不变、系统 active。真实云端写入未作为测试执行，临时合成测试目录已清理。


Last updated: 2026-10-22 (America/Chicago).

### 2026-10-23 Reuse download-time cloud bookmark and verified chapter cache

The download client exposes only its already-cached reader-state HTML. The
adapter extracts the saved progress record into the account-private progress
cache and stores the catalog only when it matches the completed download
manifest. The record remains a download-start snapshot for the immediate import,
as requested; it is consumed once, and a later retry fetches current cloud state.

After native import, restore validates the account-scoped manifest, catalog
fingerprint, chapter fingerprint, byte length and checksum, then uses the local
XHTML to locate the anchor before scanning the generated PDF. Missing or invalid
local cache falls back to the existing network catalog/chapter fetch. The native
PDF page write and metadata readback checks are unchanged. Synthetic regression
coverage verifies no cloud, catalog or chapter request is repeated on the cached
path; device install and real-book timing have not yet been performed.

### 2026-10-23 Combine exit position/time report; limit cache deletion to removed books

The existing “删除缓存” row action is retained; the additional “重新下载缓存”
action is removed, and the delete action is visible only after the native copy
is removed. Clear that book's plugin cache and use ordinary import to rebuild
legacy chapter maps.

Exit synchronization now attaches eligible, journaled reading seconds to the
same `report_read` write as the exact chapter/offset/progress. Multiple 60-second
time chunks each carry the target position and require their own positive receipt;
the existing no-blind-retry journal is preserved. The separate enter-read request
still updates “recently read”. The earlier immediate position readback and the
time uploader's extra final fetch are replaced by one final cloud fetch after
the enter-read request, which verifies the target chapter and offset. Cloud
reading-time aggregates can lag, so the duration ledger still advances only on
the write receipt. Standalone time uploads are unchanged.

A native-library deletion does not remove the plugin's account-scoped EPUB
cache. To rebuild legacy chapter maps, remove the native copy, use “删除缓存” on
the removed-book row, then ordinary import. Do not delete the account directory:
it also holds reading-time/progress records. Plugin book caches live under
`/home/root/.local/share/weread-downloader/accounts/<scope>/books`.

This supersedes earlier redownload-button instructions below. Host Qt UI test,
full repository suite, C++ core tests, LuaJIT synthetic progress and reading-time
tests, shell/Lua syntax, repository safety check and official SDK build passed.
Build `20261023060847` is installed; host/sidebar readiness passed, Xochitl is
active, the rollback marker is absent, and the installed worker SHA-256 matches
the local SDK binary. No user book/cache was removed. The combined duration path
was synthetic-only; no real cloud progress or reading time was submitted.

The native sidebar candidate is installed below AppLoad on the exact supported
3.28.0.172 firmware (internal build 20260827113527). Read the repository root
AGENTS.md and docs/agent-handoff.md before touching the device. Preserve all
existing reader, account, library and plugin cache data.

### 2026-10-22 Exit-sync nearby chapter matching

Exit progress synchronization now estimates likely chapters from the local
native PDF page fraction and the cached cloud percentage. It reads and validates
only those nearby chapter files first, then performs the exact 64-character
body-text match. A unique nearby hit is still checked against freshly fetched
WeRead chapter XHTML and coordinate agreement before upload. No hit or multiple
hits trigger the prior bounded full-book search, preserving the conservative
fallback for uncertain matches. An unchanged page/PDF cache hit can directly
seed the previous exact chapter as another candidate. Missing catalog word
counts use manifest byte sizes for estimates so percentage calculation does
not reopen every chapter body.

The exit worker reports aggregate phase timings only (page extraction, cache
key, catalog, local chapter search, source fetch, cloud read/write/verify, recent
mark, and reading-time upload); it emits no title, content, account or book ID.
Official SDK build, Lua syntax, shell syntax, exact-firmware peer replay and
repository safety checks passed. Build 20261023043806 installed; fresh host and
sidebar readiness markers confirmed, rollback guard disarmed. Real reading
sync timing has not yet been sampled on this build, so the expected speedup is
not claimed as measured.

### 2026-10-22 Follow-up network reductions

Exit sync now calls the account- and manifest-validated display catalog cache;
it fetches the catalog only when that cache is absent or invalid. After the
progress POST has been read back and verified, that same verified position and
reader HTML seed the recent-reading POST, eliminating its redundant pre-POST
reader GET. The recent-reading POST still gets a fresh readback and verifies
chapter, offset, progress and reading-time invariants. Build 20261023044641 is
installed; fresh host/sidebar markers passed and rollback timer is inactive.
In the preceding real run, catalog retrieval took about 1.5s and the redundant
recent pre-read was part of a 2.5s phase; these are the targeted costs, not a
measured post-change result. A new real reading session is needed to measure the
actual gain. Installer restart now occurs after the guarded file transaction
has exited, avoiding Xochitl restart canceling that transaction.

### Historical: 2026-10-23 annotation-store source index attempt

This intermediate fix pinned the upstream annotation database to the plugin
account cache and temporarily added a one-time network fallback. Both have been
superseded: source maps are now captured directly during download, and exit sync
never fetches chapter XHTML.

### 2026-10-23 Capture exact source maps during download (supersedes fallback)

The previous note's one-time chapter-network fallback is removed. The downloader
now wraps the original chapter fetch and stores only compact visible-text byte
runs plus source codepoint offsets under the account-scoped book cache. It does
not copy chapter text into the map; the existing verified XHTML cache remains
the source of text. Exit sync locates the matching chapter locally, reads its
map, checks the map hash against that cached XHTML, and never calls
`fetch_chapter_xhtml`. A legacy book without a map returns
`progress_source_map_missing` and asks for its plugin cache to be cleared and
the book re-downloaded. The clear-cache action now also accepts imported books;
it removes only that book's plugin download cache and leaves the native library
copy and registry alone. Parallel workers inherit `data_dir` as well.

Source-map and adapter Lua tests, the synthetic LuaJIT sync test, repository
checks and the official SDK build pass. Build `20261023053958` is installed;
fresh host/sidebar readiness markers passed, Xochitl is active, and its rollback
timer is inactive. The deployed `progress_source_store.lua` SHA-256 matches the
host build. Real exit timing and a legacy-book cache rebuild still need a device
reading session; no user book was downloaded or cache cleared during install.

The initial `redownload-cache` implementation incorrectly fell through into
the import cover-validation path, which could fetch/stamp a cover while the UI
remained at “validating”. It now returns `cache_ready` immediately after the
download helper's full-cache result and canonical-path checks, before EPUB
cover scanning or native submission. Build `20261023053958` is installed and
its worker hash matches; a real cache rebuild still needs a device run.

Important corrections already made:

- Independent login reuses the project's login-qr.lua, with isolated session paths.
- Shelf retrieval uses GET /web/shelf/sync, not POST. A failed shelf request must
  not be described as failed QR authentication.
- Stock multipart upload rejects Qt's automatically quoted boundary with
  `No file sent`. native_http.h sets an explicit unquoted boundary. The exact
  request is covered by a mock HTTP server on host and device.
- An existing user-requested cached EPUB was successfully submitted to the stock
  importer, accepted with HTTP 201, discovered in the library root and converted
  by the system to PDF. The registry subsequently resolved it to imported.
  No second network download or direct native metadata writes were used.
- Explicit known pre-import rejection becomes failed/retryable. Ambiguous network
  outcomes remain pending to prevent accidental duplicate imports.
- Per-book import controls and selection targets are enlarged. Imported status
  shows a check in the main reader's #17885b green; remote text opacity is 0.5.
- Plugin-private download_io.lua avoids repeated mkdir subprocesses and coalesces
  intermediate progress writes per second. Final completion writes and chapter
  resume manifests remain immediate. Network throughput gains are not measured.

Validation: full Linux repository validations, host core/UI/transaction tests,
Lua adapter and download I/O tests, official Qt SDK build, and the device upload
framing test passed. Device Qt loading and service readiness are checked by the
installer. No claim of human visual QA or stock body-page reading acceptance.

The installer stages only project-owned code under /home/root, preserves private
account/cache data, backs up its previous package, and uses a rollback timer for
UI restart. The readiness check uses the recorded restart timestamp to allow for
slow resource rebuilding without accepting a stale readiness message.

Deployment status of the final enlarged controls/green check candidate: the guarded
install at UTC 2026-10-22 02:25:12 timed out waiting for host/sidebar readiness
while the device reported sleep activity. Its rollback timer restored the previous
package (module SHA-256 14262a4cbe4e7ed4fa4a967ab39e979f3389117c8ceb15beaf86de78b48c2892).
The desired final source-built module is SHA-256
1b15e7cb43db82d225e6f27d279acd198970d25a4415d6a4446eda55d5fd1c87.
The system is active with NRestarts=0. Wait for the user to wake and unlock the
device, then rerun scripts/install.sh and verify readiness before reporting the
final UI deployed. The successfully imported document and its registry mapping
remain intact because all account/registry/cache data is outside the package.

Final deployment completed after the user woke/unlocked the device. Guarded
install 20261022023020 succeeded with fresh host/sidebar readiness. The installed
module matches the final source-built SHA-256 above and the entire package
manifest verifies. System service active, NRestarts=0. Temporary installation
wake lock released. UI changes and multipart/progress fixes are now deployed.

Badge correction: imported status now copies the main app's coverDownloadBadge
implementation: 44 px circle, #239B56 background, white 4 px Canvas check with
matching normalized vertices and rounded caps/joins. The previous plain green
check was not an accurate match. Host UI geometry/color assertions, complete
Linux validations and official SDK build passed for this correction.

Further upload diagnosis: the native Qt 6 QHttpPart implementation lowercases MIME
part headers, which the stock importer fails to recognize. An unquoted boundary
alone did NOT fix the real Qt upload. native_upload.h now streams explicit
title-case Content-Disposition/Content-Type with a token boundary and exact
Content-Length; the upload fixture asserts this and fails against the previous
Qt 6 implementation. The corrected actual worker successfully re-imported the
next user-selected failed book from its complete cache, with states validating,
importing, imported and no chapter download.

Shelf behavior: enter -> status -> cached-shelf. Cached shelf loading performs
no remote request and reconciles known native UUID metadata for deletion. Pending
or conflicting entries still require a full source scan. After 300 seconds,
refresh once in the background; manual Refresh always fetches the full shelf.
Serial cover prefetch is removed; only current-page image delegates request
covers. Full source-ID discovery/recovery remains on remote/manual refresh.

The list is paged with an adaptive number of complete 176-pixel rows, 4-pixel
spacing, a right-aligned page indicator alongside the status, and horizontal
DragHandler swipes (80-pixel threshold). Selection persists across page changes.
Host tests send an actual mouse drag, exercise page boundaries, selection
preservation and cached reopening; Lua tests prove fresh/stale cache behavior
and no shelf HTTP request for cached-shelf.

Latest combined candidate module SHA-256:
fe2735c0293781490e32111777e80492654f7cba8d10b10cddff16d0ba3659f5
Worker SHA-256:
cd45c55a9e8095ff9a52acbf1040c21de75c34dca5d382d61ce2f5741051f9d6
On-device cached-shelf took 0.135 s and full shelf refresh took 6.416 s in one
run (worker/data timing, not end-to-end display latency). Both reconcile the two
plugin-imported documents as imported. Final guarded deployment is awaiting a
fresh host/sidebar readiness signal after unlocking, not yet declared complete.

Final combined deployment 20261022024536 was confirmed after the user unlocked
the tablet: fresh host/sidebar readiness, complete package manifest and expected
module hash verified. .install-ok written and owned rollback timer stopped before
it fired. System active with NRestarts=0; temporary wake lock released. The final
combined candidate is now deployed, superseding the pending status above.

Cover cache and touch/auth follow-up (source candidate pending deployment):
- CoverStore downloads public CDN images in two private Lua/libcurl subprocesses,
  using the same verified TLS transport as downloads. Qt Network's TLS handshake
  failed against these CDN URLs on this device; no certificate bypass was added.
  No account/session/config is loaded or sent by fetch-cover.lua. Redirect targets
  remain HTTPS and on the allowlist, response bytes are bounded to 4 MiB, decoded
  dimensions are bounded and images are saved as scaled private PNGs atomically.
- Account scope, book ID and URL hash define durable covers-v2 paths; cached
  images survive reopening. Current-page requests jump ahead in the queue, the
  remaining shelf is prefetched while open. Closing cancels children; importing
  pauses cover traffic so it does not compete with book downloading.
- The real device test cached five real covers and reopened them with zero
  requests. No covers, URLs or shelf contents were added to repository artifacts.
- Old DragHandler code reads a reset translation on release: it fails the new
  touch test on device Qt 6. The fix stores per-gesture position displacement
  before release. Device touch-event regression covers repeated left/right,
  boundaries, short and vertical gestures; human physical-finger acceptance is
  still distinct from synthetic touch-event injection. No mouse test is used as
  evidence for the touch fix.
- Removed the logged-in relogin button. An expired session (401/-2012/-2013)
  becomes locally unusable without deleting cached books, then automatically
  returns to QR login. Generic network failures retain the session and shelf.
  A retry-QR control exists only on the login page after a stopped/failed login.
- Host, Lua, complete Linux root validations, official SDK build and the device
  Qt 6 touch/auth/cache fixture passed. Device tests use private temporary fake
  accounts; no real logout or account switching was performed.

Final cover/touch/auth deployment confirmed after unlock: install stamp
20261022031127. Native module SHA-256
e975524fe091f68d27758369ce9ca6020f76089fc63958e741a66620ffa1bab6,
worker SHA-256
559ed7ba6cb3fa88393cbb49be3ab25f37c5ffe4103f80f50cb2698d666b74f5.
Complete installed manifest, fresh host/sidebar readiness and on-device native
QML component loading all passed. System active, NRestarts=0. Temporary wake
lock released. Synthetic QA files only are removed from /tmp; user cache and
account files remain intact. Cover caching continues while the plugin is open.

Download optimization and single/multi-select deployment (2026-10-22):
- The toolbar defaults to single-select mode. Its adjacent toggle switches to
  multi-select, where book checkboxes appear and the toolbar shows “导入所选（N）”.
  Per-book import buttons remain available in single-select mode. Returning to
  single-select clears the selection. The native Qt UI test exercises both
  modes, button visibility, selection clearing, and touch paging.
- Full-book downloads use at most two concurrent chapter requests through
  libcurl multi, with independent request state per lane and ordered EPUB
  assembly, existing retries, and chapter checkpoints. If multi transport is
  unavailable, the existing serial flow remains available before chapter work.
- On-device same-book, same-eight-chapter sample: serial 25.467 s, parallel
  19.643 s (about 23% faster), zero failed chapters in both runs, and identical
  output size (428,757 bytes). This is one limited sample, not a general speed
  guarantee. The delayed-server simulation measured 1.650 s versus 0.833 s.
- Full Linux repository validation, plugin Lua/core/UI tests, source safety and
  whitespace checks, and the official Qt 6.10.3 SDK build passed. Device install
  20261022131311 completed after unlock: fresh host/sidebar readiness, complete
  SHA-256 manifest, expected module hash, and active Xochitl verified. Temporary
  installation wake lock released. No user book was imported during benchmarking.
- Human visual acceptance of the toggle and physical touch remains for the user;
  host UI and native QML-load checks do not replace that acceptance.

### 2026-10-23 Raise bounded full-download concurrency to three

- Full-book downloads now run up to three chapter lanes through the same
  libcurl multi event loop. Each lane retains an independent settings/client
  context; retries, verified checkpoints, image handling, and ordered EPUB
  assembly are unchanged. The scheduler rejects more than three lanes.
- The existing two-lane device sample showed about 23% improvement over serial.
  Three-lane speed and service throttling have not yet been measured on-device;
  host parallel HTTP coverage now exercises three lanes, including inline-image
  requests that cannot yield from their callback.

2026-10-22 inline-image coroutine failure correction:
- The initial parallel version fails when upstream fetches an inline image from
  inside a C string.gsub callback. coroutine.yield is prohibited there. The easy
  handle had already been registered with curl_multi, so the swallowed image
  error left a pending handle associated with a completed coroutine; later
  completion produced “cannot resume” or could terminate the helper.
- parallel_http.await now checks coroutine.isyieldable before registering a
  multi handle. Non-yieldable requests use curl_easy_perform synchronously on
  the same verified, connection-caching transport. Normal chapter requests still
  overlap. Missing isyieldable support conservatively uses the synchronous path.
- Adapter scheduler errors now restart through verified checkpoints and continue
  serially; known authentication expiry still returns to login. Partial books
  remain blocked from native submission.
- New device inline-image fixture fails on the previously installed scheduler
  and passes with the fix. Adapter test exercises fallback to serial. Full Linux
  validations and official SDK build pass. The affected real book resumed from
  31/82 and completed 82/82 with zero failed chapters. Seven cached chapters with
  unresolved remote image sources from earlier failed runs were fetched again
  and the complete EPUB regenerated. No native import was initiated for QA.
- Only lua/parallel_http.lua and lua/adapter.lua were atomically updated while
  the worker was idle; no UI restart. Previous scripts/manifest are backed up in
  /home/root/weread-downloader.lua-recovery-20261022. Complete installed manifest
  verified and Xochitl active. Native binaries remain from install 20261022131311.

2026-10-22 book progress, native trash action, and stable badges:
- Download/start/validation/import status includes the current book title using
  the in-memory shelf; titles are not added to diagnostic logs.
- books now has a dedicated booksChanged notifier for shelf/login changes.
  Per-book state/progress is delivered through bookUpdated to existing delegates,
  avoiding ListView model replacement and Canvas badge reconstruction per tick.
- Imported rows offer “移出书库”. Worker reconciles the source ID to one live
  native UUID and holds its registry lock through the operation. Only that UUID
  goes to the host. The host calls the firmware's EntitySelection.add and
  selectionMoveToTrash APIs, restoring earlier unrelated selection afterwards.
  No native metadata is edited directly. Worker confirms the native trash state
  before emitting removed. EPUB/chapter/cover caches are retained for reimport.
- Host and device Qt6 tests cover title-bearing progress, no shelf reset or
  delegate replacement across progress, removal handshake/status, and rejecting
  removal for a non-imported row. Native removal uses a synthetic host in those
  tests; no real user document was trashed for QA. Full Linux tests, shell/source
  checks, official SDK build, and exact-firmware QMD replay passed.
- Guarded install 20261022165901 completed after unlock. Fresh host-ready,
  sidebar-ready and native-removal-ready markers verify the real firmware exposes
  the required selection API. Complete package manifest verified, Xochitl active,
  temporary wake lock released. A real-book removal action remains user acceptance.

2026-10-22 session maintenance, account menu and initial cached covers:
- session_auth.lua renews the isolated session when used after 30 minutes and
  retries an auth-rejected shelf/download once after renewal. A network-failed
  renewal preserves login; confirmed expiry still returns to QR. The private
  session persists auth_renewed_at and cached account_name. Profile retrieval
  uses /api/userInfo; no profile/token data is added to diagnostics.
- Header account name opens a cancel/confirm logout dialog. Confirm clears this
  plugin's credentials and returns to QR, preserving books/covers. No real user
  logout was performed for QA; synthetic lifecycle/UI tests cover confirmation.
- CoverStore.begin now precedes booksChanged. Previously initial delegates read
  an unset account scope; already-cached images never emitted a later ready
  callback, so only page recreation displayed them. Dedicated initial-shelf and
  Image.Ready checks pass without paging, with a private synthetic cover root.
- Lua session/adapter tests, host/device Qt UI, full Linux validations and SDK
  build passed. Real device renewal and profile retrieval succeeded (only boolean
  diagnostics exposed). Two-hour continuous usage has not been observed.
- Install 20261022170728 confirmed after unlock and wake lock released. User
  prefers to perform visual acceptance themselves; minimize follow-up testing
  and token-heavy narration. Keep necessary build/install safety checks.

2026-10-22 rounded controls / removal feature withdrawn:
- All page/action/dialog buttons share RoundedButton.qml (12px corners, 32px
  horizontal padding). Header account/return buttons are both 72px high.
- Logout dialog uses the full window Overlay, a 50% black modal dimmer, 40px
  padding, 18px corners and equal-width 88px cancel/confirm buttons.
- Multi-select hides per-book import controls. Single-select shows disabled
  “已导入” for imported books. Native trash/removal UI, controller methods,
  worker command and host QMD bridge are removed at the user's request. Source
  reconciliation of books removed externally remains for cached reimport.
- Full Linux validation, UI loading/behavior, official SDK build, shell/source
  checks and exact-firmware replay passed. User performs visual acceptance.
- Guarded install 20261022171753 confirmed after unlock; installation marker
  verified and temporary wake lock released.

2026-10-22 long-press selection and live import queue:
- Removed the single/multi toggle. A row TapHandler long press enters multi-select
  and selects the pressed book when importable. Return exits multi-select before
  closing the page. Batch action reads “导入（N）” and returns to single mode.
- Disabled cancel and per-book controls collapse out of layouts. Queued books
  show “排队中” as state/action; clicking that action removes only the waiting
  item and restores its prior remote/failed/removed state. Active work uses the
  existing global cancel action. Imported books cannot be queued again.
- canQueue permits additions during an import, while a single worker still
  serializes books/native submissions. Queue entries are deduplicated. A map of
  pre-queue states restores waiting entries on cancellation/failure/close; queue
  state is in-memory only. Queue stop-on-failure behavior is preserved.
- Host/device Qt tests cover actual touch long press, paging, live additions,
  queued-item cancellation/requeue, and execution order 1→3→2 without duplicates.
  Full Linux validations, shell/source checks and official SDK build passed.
  User performs visual acceptance; no actual user books were submitted for QA.
- Guarded install 20261022173043 completed with fresh host/sidebar readiness;
  installation marker verified, temporary wake lock released.

2026-10-22 EPUB covers and removed-book cache cleanup:
- An affected cached EPUB lacked both cover metadata and an image despite a
  valid shelf thumbnail cache. Original DownloadManager ignores a failed cover
  request. Worker now checks the packaged EPUB, falls back to its cached PNG or
  the bounded public-only cover fetcher, and embeds cover metadata, image and
  first spine page before submission. Missing cover failures are retryable and
  keep chapter data. Existing covers are retained; repeated repair is idempotent.
- Epub.stamp accepts an optional cover, preserves existing packed chapter/assets,
  guards image/expanded sizes, and validates the finished archive. repair-cover
  repairs only the cached EPUB; it does not modify or duplicate a native book.
  Existing native copies must be removed through the system and reimported to
  receive the corrected cover.
- Multi-select has an explicit “退出多选” toolbar action.
- Removed books with cached downloads show a “删除缓存” action with the same
  fixed 164×104px size as import, placed side by side. Minimum/preferred/maximum
  layout dimensions are locked for import, queued and cache actions. Worker rechecks native removed
  state before deleting only that account/book download directory and resetting
  its status. Credentials, shelf/cover thumbnails and other books are retained;
  symlink/path escapes are rejected. Success hides the cache action, persists
  no complete-file shortcut, and permits a fresh chapter download on import.
- Core tests cover cover insertion/idempotence and targeted cache removal,
  sibling preservation and symlink rejection. Host/device Qt UI tests cover
  explicit selection exit and equal-width cache action. Full Linux validations,
  source/shell checks and official SDK build passed. No real cache was deleted
  or native library book replaced for QA.

- Install 20261022180347 confirmed fresh host/sidebar readiness. Import/queued/cache
  actions now use fixed 164×104px dimensions side by side; focused host UI size
  assertions and SDK build passed. Temporary install wake lock released.

### Reading progress and smaller fixed actions (2026-10-22)

- Per-book import, queued, cache removal and progress sync actions use fixed
  144×88px geometry, 20px horizontal padding (four-character labels fit).
- Only imported books show local/cloud percentages. Local is current native
  page-position estimate (first page 0%, last page 100%); unknown remains “—”.
  Cached cloud values are labelled pending update. Refresh returns the shelf
  first, then a cancellable independent worker reads only imported books.
  Per-book notifications preserve delegates/green badges; downloads/closing/
  account transitions stop the progress reader.
- Manual “同步进度” uploads native → WeRead using verified source book identity,
  current account, cache checksums/catalog fingerprint, unique PDF text anchor,
  original HTML coordinates and a fresh cloud-record conflict check. Unicode
  and UTF-16 coordinates must agree at the target; edited native page maps are
  rejected. Reading time is zero, business acknowledgement and position readback
  are required. Whole-book percent uses catalog word weights, or cached text
  byte weights when word counts are unavailable, so it is an estimate; the
  chapter/offset is mapped separately and never inferred from native percent.
- New imports instead restore cloud → native. Persist restore intent before
  submission, find the cloud anchor uniquely in the generated native PDF, then
  request the host QML to call LibraryController.setLastOpenedPage. No direct
  writes to native metadata. Worker confirms the saved page before clearing
  intent. Late-confirmed imports retry on refresh. Failed restores remain
  pending; their sync button retries cloud restoration instead of pushing 0%.
- Native API comes from exact-firmware DocumentView use; current QMD host hook
  includes a guarded Library.entryForId lookup and an acknowledgement. Exact
  firmware/peer replay validates the hook; real-user restoration/upload is not
  performed as QA. No fake positions or reading time are sent to real accounts.
- Lua behavior tests in an isolated device temp directory cover imported-only
  reads, checksum/identity mapping, zero-time exact upload, cloud conflict,
  business rejection, restore candidate and edited-layout refusal. Qt tests
  cover background updates/foreign scope refusal, sync dimensions/dispatch and
  automatic-restore acknowledgement. Root Linux validations, local core/UI,
  device Qt6 UI tests, shell syntax and official SDK build passed.
- A read-only integration fetch using independent plugin credentials retrieved
  cloud percentages for all 4 currently imported records, with 0 unknown/error
  rows. Private progress cache stays in the account directory, mode 0600.
- Install 20261022204613 confirmed fresh host/sidebar readiness and guarded timer
  cancellation. Installed worker then returned local and cloud percentages for
  all 4 imported records (0 errors). Temporary test fixtures/diagnostics removed
  and install wake lock released. Synthetic Qt6 progress-row screenshot checked:
  both percentages and the full 144×88 sync label fit without clipping.

### Sync action visibility follow-up

- Action RowLayout visibility must derive from book state, never child.visible:
  effective child visibility inherits the parent, creating a dependency cycle
  when shelf delegates are created while the worker is busy. Imported rows now
  keep their sync action visible in single-select mode (disabled during a
  foreground operation). Cached removal visibility also uses explicit state.
- Focused Qt UI regression loads a fresh shelf while the worker remains alive,
  checks visible/disabled sync, then visible/enabled sync after completion.
  Host UI regression, full Linux validation and official SDK build passed.
- Install 20261022205116 confirmed fresh host/sidebar readiness; temporary wake
  lock released. User does their own visual acceptance.

### Explicit upload/download actions and progress line (2026-10-22)

- Replaced the ambiguous sync action with separate “上传进度” and “下载进度”
  buttons, each fixed 144×88px. Upload always invokes native→cloud even if a
  prior restore is pending; successful upload clears that pending intent.
  Download always invokes cloud→native. Automatic new-import restoration also
  invokes the explicit download path. Neither manual action silently reverses.
- Both percentages now occupy a dedicated line BELOW imported status/green
  badge, rather than sharing the author line. Delegate height is 208px and page
  capacity uses 212px stride so long titles plus the extra line fit.
- Qt UI tests assert both action dimensions, explicit download handshake,
  refresh-time visibility and progress-line placement below status. Lua isolated
  test covers explicit upload while restore intent exists. Host UI, device Lua,
  root Linux validation and official SDK build passed; no real progress changed
  by tests.
- Device Qt6 UI also passed the explicit actions and below-status placement.
  Install 20261022205641 confirmed fresh host/sidebar readiness. Temporary
  synthetic fixtures removed and install wake lock released.

### Compact inline reading progress (2026-10-22)

- User requested percentages on the SAME row as imported status/green badge.
  Progress now follows the badge, with safe eliding on constrained widths.
  Restored 176px delegate height and 180px pagination stride. Explicit upload
  and download actions remain 144×88px. Host Qt behavior test checks horizontal
  placement and equal vertical center; full Linux validations and SDK passed.

### Cloud restoration corrections and automatic-intent separation

- A real saved cloud offset can refer to invisible HTML markup/whitespace just
  before the summary. Source.savedPosition resolves both raw Unicode/UTF-16
  interpretations to the next visible character and validates the exact summary
  there. It does not relax matching with a guessed offset tolerance. The
  originally failing pending book now yields an in-bounds native page in a
  read-only diagnostic (no metadata changes or cache save during diagnostics).
- Snapshot SHA-256 now uses the tablet's existing libcrypto.so.3 through FFI in
  the isolated helper. The shared pure-Lua account-ID hash is unsuitable for
  large .content files because its byte-varargs expansion can exceed LuaJIT's
  stack. Namespace hashing remains unchanged. Standard abc/100000-byte vectors
  and native fixture restore passed; no crypto library is redistributed.
- Worker preserves sanitized restore helper errors instead of converting every
  failure to progress_not_located. Concurrent native metadata/layout changes
  continue to refuse restoration; a retry with a stable snapshot passed.
- restorePending means a manual retry is available, NOT permission to restore
  on entry/refresh. Only prepareRestore before a new import sets the separate
  restoreAutomatic flag. Starting ANY restore consumes that automatic intent,
  including failures. Legacy pending records with no automatic flag never
  auto-run. Success/upload clears both intents. New imports retain their one
  initial cloud→local restore; existing books require the download button.
- Device synthetic Lua tests cover bookmark-in-markup resolution, wrong summary,
  Unicode-unit ambiguity and long snapshot hashing. Qt regression covers a
  manual pending restore staying idle across refresh, plus automatic new-import
  intent dispatch. Full Linux regression and official SDK build passed.
- Compact layout install 20261022205911 was confirmed before this diagnostic.
  Initial correction install 20261022210743 encountered canceled system restart
  and restored the previous build. Retry 20261022210831 confirmed fresh
  host/sidebar readiness. Temporary diagnostic fixtures removed and install wake
  lock released. User can retry download; no actual existing-book page was
  changed during the diagnostic itself.

### Percent semantics follow-up (read-only investigation)

- User reported that the displayed cloud percentage disagrees strongly with
  the current page/total in the phone reader, despite correct position restore.
  Read-only checks found reader-state and web/book/getProgress agree with each
  other on raw progress and chapter/offset, while catalog/current-position
  estimates differ materially from that raw percentage. This does not establish
  whether raw progress means historical extent, a client-supplied value or
  another provider convention. Do not claim that semantic without verification.
- Current UI displays raw record.progress as cloud percent and native page
  fraction as local percent. Neither establishes a common content-progress
  denominator. The earlier blanket explanation blaming pagination/local alone
  was premature. No account progress or native position changed in this check.

### Chapter-position percentage implementation

- Cloud display now uses chapter word-count weights and the saved chapter UID
  plus intra-chapter original-HTML offset. The offset is mapped to normalized
  visible text before calculating the chapter fraction; raw HTML length is not
  treated as word count. Raw record.progress remains private sync/conflict data
  and is never used as a display fallback. Local percent still uses native pages.
- Derived values have a separate method marker, position key and five-minute
  calculation cache. Legacy raw-only cache entries display unknown until a
  successful calculation. Missing/ambiguous chapter weights or bookmark mapping
  display unknown rather than a guessed percentage. Position sync is unchanged.
- Synthetic device tests passed weighted chapters, raw-percent independence and
  invalid catalogs, alongside existing upload/restore safeguards. Full Linux
  regression and official SDK build passed. A read-only real-account probe
  calculated two imported books and left two unknown; the reason for those
  unknown values still needs investigation. No real reading position was changed.
- Follow-up USB connection timed out before further diagnosis/deployment. The
  installed release remains 20261022210831 and still displays raw cloud percent.
- Connection subsequently recovered. Read-only diagnosis confirmed one unknown
  record has a chapter UID absent from its catalog; another has an offset that
  does not match its saved summary in the original chapter. Both remain unknown
  rather than guessing. Synthetic device tests passed again. Install
  20261022225227 confirmed fresh host/sidebar readiness and the exact success
  marker. The installed release now uses the chapter-position percentage.
  Temporary synthetic fixtures removed; install wake lock released. No actual
  cloud progress or native reading position was changed by verification.

### Display bookmark compatibility correction

- Strict original-offset validation rejected usable saved summaries from other
  source serializations. Percentage display now accepts an exact unique summary
  of at least six normalized characters in the confirmed chapter. This fallback
  is display-only; upload/restore still use their existing strict mappings.
- Missing chapter UID may be resolved from a complete verified download only:
  account and catalog identity, every chapter checksum/fingerprint, bounded
  total bytes/time, and a unique summary match across the entire book are
  required. The selected chapter's original source is fetched for the final
  weighted percentage. No raw server percent fallback or guessed chapter index.
- Synthetic device tests cover wrong offset with unique summary, short/repeated
  summary rejection, missing chapter ID resolution, and modified-cache refusal.
  Full Linux regression and official SDK build passed. Read-only verification
  calculated all four imported books without changing reading positions.
- Install 20261022225800 confirmed fresh host/sidebar readiness and exact success
  marker. Temporary synthetic fixtures removed; install wake lock released.

### Chapter names and compact progress controls

- Imported rows show local and cloud percentages plus chapter names above the
  side-by-side upload/download buttons. These two buttons are now 144×56;
  import/cache-delete remain 144×88. Delegate height/paging density unchanged.
- Cloud name uses the resolved real catalog chapter, including verified summary
  recovery for missing IDs. Local name matches current native PDF page text
  against the complete checksum/catalog-verified download. Native metadata and
  content snapshots must remain stable; no percentage-based chapter inference.
  A loaded page with no text reports 当前页无正文. Ambiguous matches remain unknown.
- Local and cloud title fields are forwarded through controller row updates;
  cloud title is kept in private progress cache. Names render as plain text with
  independent single-line elision. No reading position changes during refresh.
- Device synthetic tests cover both names and empty pages; host Qt interaction
  test checks title propagation, 144×56 buttons and information above controls.
  Read-only real-book checks found cloud names for all four imported books,
  two local chapter matches and two native pages with no text.
- Full Linux regression and official SDK build passed. Host Qt5 and device Qt6
  offscreen interaction tests passed; inspected synthetic device screenshot for
  chapter/percentage placement above the shorter controls without row growth.
- Install 20261022230840 confirmed fresh host/sidebar readiness and the exact
  success marker. Synthetic device fixtures removed; install wake lock released.

### Recent-reading markers after import/upload

- Confirmed native import and confirmed progress upload request native
  Library.entryForId(uuid).markOpened() through the existing MainView host.
  This updates native recent-reading metadata through the stock API without
  changing lastOpenedPage. Pending/conflicted/failed imports do not request it;
  ordinary refresh and explicit cloud-position download do not request it.
- After import (including its initial position-restore attempt), and after a
  successful explicit upload, a cloud enter-reading request preserves the fresh
  saved chapter/offset/percentage/summary and omits reading-time fields. It uses
  the existing provider payload builder and business-ack check; readback verifies
  position, raw percentage and accumulated reading time did not change. Updated
  raw record is saved as the new conflict baseline. Existing restore flags and
  derived display cache are preserved.
- A user-authorized device check verified an advanced shelf readUpdateTime,
  unchanged chapter/offset, percentage and reading time. No raw account/book
  values were recorded. Synthetic Lua tests cover enter payload without timing,
  preserved position and recent-marker failure after successful primary upload.
  Controller tests check native signal dispatch for import and upload success.
- Recent-marker errors remain secondary warnings and do not fail a successful
  import/upload or erase a prior restore warning. Canceled automatic-exit-upload
  feature remains absent; no native reader lifecycle hook/settings toggle added.
- Full Linux regression, host Qt interaction, device synthetic Lua tests,
  official SDK build and exact-firmware peer replay passed. Install
  20261022233012 confirmed fresh host/sidebar readiness and exact success marker.
  Temporary fixtures removed; install wake lock released. Native recent signal
  dispatch is covered synthetically; real import/upload end-to-end acceptance
  of the new native marker remains user-facing verification.

### Chapter display cache and action-triggered shelf refresh

- Display-only catalog cache is account/manifest-fingerprint validated and
  persisted with private progress data. Unchanged local chapter results use a
  key containing native UUID/page, layout content hash, download manifest hash
  and streamed SHA-256 of the native PDF. Stable metadata/content snapshots are
  still checked. Changed native page/layout/PDF/manifest invalidates the result.
  No cached chapter result is used by precise upload/restore position mapping.
- Local cache hits for all imported books emit before any cloud request. Warm
  refresh skips PDFium and chapter scans. Missing-cloud-ID display matching
  normalizes cached XHTML directly instead of building per-character HTML
  source maps for every chapter. Original-source mapping remains for offsets.
- Read-only aggregate device timing: cold cache setup about 21 seconds for four
  books; warm complete cloud validation about 6 seconds, first local cache result
  below 0.01 seconds, zero PDF document loads and zero catalog HTTP requests.
  These are one-session measurements, not network latency guarantees.
- Successful import/upload marks the shelf dirty; after the import queue drains,
  exactly one shelf request updates plugin ordering from cloud readUpdateTime.
  Explicit shelf loads/closing the plugin clear the dirty flag. Tests model
  persisted import results and wait for the follow-up shelf rather than a
  transient idle gap. Secondary recent-marker errors still preserve success.
- Full Linux regression, official SDK build, host Qt5 and device Qt6 interaction
  tests passed. Synthetic device Lua tests cover multi-chunk file hashing,
  persistent local-title cache reuse without PDFium, PDF-change invalidation,
  existing sync/restore guards and display behavior. UI regression checks one
  follow-up shelf per import batch/upload rather than per imported book.
- Install 20261022234052 confirmed fresh host/sidebar readiness and exact
  success marker. Temporary synthetic fixtures removed; install wake lock
  released. First actual refresh establishes persistent display caches; network
  validation remains asynchronous on later cached entries.

### Cloud-position restore with incompatible source offset

- A newly imported book at its cover failed cloud-position restore because the
  saved cloud chapter offset did not agree with that chapter's current HTML
  serialization. Current local page text is not an input to restore; only upload
  needs the current native page to contain identifiable text.
- Restore now permits an exact unique saved summary in the confirmed cloud
  chapter when strict raw-offset mapping fails. It still requires at least 16
  following source characters and a unique 32-character (or unique 16-character
  fallback) match across the complete native PDF, plus stable native snapshots.
  Upload offset construction and cloud conflict safeguards remain unchanged.
- Read-only comparison on the latest imported book reproduced the installed
  progress_offset_unsupported failure and found a valid target with the fix.
  No native page was changed by the comparison. Synthetic tests cover the
  incompatible offset fallback, unrelated summary refusal and no native writes.
- Download errors no longer tell the user to first read a local body-text page;
  they explain that the cloud position could not uniquely match local text.
- Full Linux regression, official SDK build, host UI tests and device synthetic
  restore tests passed (including current native page at a textless cover).
  Install 20261022234854 confirmed fresh host/sidebar readiness and exact success
  marker. Temporary synthetic fixtures removed; install wake lock released.
  Actual native position remains unchanged pending the user's explicit retry.

### Reading-time statistics tab and native tracking

- Bottom navigation now mirrors the main app's 88px footer with 书架 and
  阅读时间统计. Statistics are per-book, sorted by latest local/cloud activity,
  showing local measured cumulative seconds, last-fetched cloud cumulative
  readingTime, pending local seconds, and an independent 上传时长 action.
  Existing 上传进度 also attempts pending time after successful position sync.
  Zero/absent local time never creates a synthetic duration or extra time POST.
- Only UUIDs registered as imported with a valid account scope/book ID start
  the native clock. Tracking does not require opening/logging into the plugin
  UI, works offline, and stays isolated by the original account scope. No old
  native duration is inferred from timestamps or imported from the standalone app.
- Native DocumentView open/close/page/suspend/visibility/subdialog hooks plus
  MainView passcode and application-state gates pause the clock for hidden,
  locked, suspended or non-reading views and while the plugin is open. A steady
  clock checkpoints every five seconds while active, rejects missed intervals
  over 15 seconds, and stops after five minutes without a page change. Page
  activity or wake/unlock resumes it. This measures active-view time, not gaze.
- The UI exclusively owns account/local-reading-time-v1.json (version 1, 0600,
  atomic saves). Worker exclusively owns uploaded-reading-time-v1.json with
  confirmed local-through counters and a durable pending journal BEFORE POST.
  Separate writers prevent display/reading checkpoints from losing receipts.
  Business rejection releases the pending reservation; transport ambiguity or
  crash leaves it uncertain and blocks blind retransmission. There is no
  automatic discard/clear of uncertain records. Data remains for later audit.
- Upload uses existing signed read payloads, preserves a fresh cloud position,
  and sends only recorded, unconfirmed deltas in chunks <=60 seconds, bounded
  to 60 chunks/roughly 90 seconds of work per operation. Positive business
  receipts confirm delivery like the independent app; cloud aggregate counters
  may lag and are refreshed without resending accepted chunks. Long backlogs
  can remain pending for another explicit upload. Local counters never reset.
- Existing download/restore/progress information reads already fetch the cloud
  readingTime field; statistics reuse that private record rather than adding a
  separate per-book statistics network request. Removed books' local history
  survives cache deletion; time-only upload accepts their retained registry
  identity, while position uploads retain native-source verification.
- Synthetic tests cover fractional seconds, pause, missed sleep, idle cutoff,
  restart/corrupt-data handling, imported-only offline controller events, stats
  tab switching, time upload dispatch, chunked deltas, repeat-no-op, explicit
  rejection and ambiguous-response refusal. Device Lua tests cover combined
  progress+time upload. No real account time was fabricated for a device test.
- Full Linux regression, official SDK build, host Qt5 core/UI tests, device Qt6
  timer/UI tests and exact-firmware peer replay passed. Inspected synthetic
  device statistics screenshot for footer, durations and the upload control.
  Real nonzero cloud-time receipt validation awaits the user's first measured
  reading session and explicit upload; no synthetic duration was sent remotely.

- Initial real reading check produced no ledger. Native Document.id is a UUID
  value; the controller now parses both identifiers with QUuid instead of exact
  string equality (handles braces and case), while preserving the registry UUID
  in the ledger. The offline regression uses a braced upper-case identifier.
  Installed corrected build 20261023002831; host/sidebar readiness confirmed,
  install wake lock released. Real native event/clock validation remains pending;
  do not claim actual-device recording or real cloud-time upload is verified.

### Per-day reading statistics (2026-10-22)
- Account-local ledger keeps cumulative seconds plus `days` keyed by the device's
  ISO local date. Steady-clock elapsed intervals are split at local midnight;
  timezone/daylight transitions use Qt local calendar boundaries. Existing
  cumulative seconds without day buckets remain an explicitly undated balance.
  No date is inferred from lastRead. Date totals plus undated balance preserve
  the previous cumulative total. Cloud display is still explicitly cumulative.
- Statistics supports all dates, individual recorded dates (including today)
  and undated history. Uploadable-today and held historical balances are shown
  separately. Both time-only and combined progress uploads use today's bucket
  and per-day acknowledgement counters in the worker-owned journal. Existing
  aggregate receipts survive migration. Previous/undated deltas are retained,
  never silently charged to today's reading. Historical cloud attribution is
  unverified; this build does not backdate requests or claim cloud daily sync.
  Crossing midnight during request preparation stops further POSTs.
- Verified the preceding UUID-format fix with a real native reading event:
  matched/active state and a nonzero private counter, inspected only as aggregate
  counts (no titles, identifiers or account material). No real time uploaded.
- Date splitting, migration, filtering, today's uploads, historical deferral,
  ambiguous-reply refusal and midnight upload cutoff pass synthetic tests.
  Full Linux regression, host Qt5 core/UI, official SDK, device Qt6 core/UI,
  device Lua time and combined-progress tests pass. Inspected synthetic daily
  statistics screenshot. Build 20261023003914 installed; native host/sidebar readiness confirmed,
  rollback guard disarmed, install wake lock and synthetic fixtures cleaned.

### Exit upload and native library badges (2026-10-22)
- The user explicitly reinstated the previously canceled automatic exit upload.
  It is enabled by default for plugin-imported native books. DocumentClosed
  checkpoints local time and queues the provenance-bound UUID/book/account job.
  A monotonic 1.5-second minimum lets native position persistence settle. Jobs
  serialize with plugin work; locked, display-sleeping or inactive views defer
  new jobs. Reopening the same native UUID drops queued work or cancels its
  running worker. Ordinary sleep/menu/page events do not themselves upload.
- Independent background `exit-sync` reuses the existing exact position upload,
  recent-reading update and today's pending-time upload. Worker and Lua adapter
  both verify the original account scope; registry/native EPUB provenance and
  existing cloud-conflict/location guards remain mandatory. Auth/network errors
  stay passive and never launch login QR in the background. No blind retries;
  ambiguous time receipts retain the existing pending journal. No wake lock is
  held by reading or exit sync. Each job has a three-minute upper bound.
- Worker termination is graceful before forced kill, and the Lua adapter installs
  a Linux parent-death signal so stopping a worker cannot orphan an uploader.
- Native library cover bottom-right gets a passive 48px source badge for all
  known imported UUIDs, independent of whether the plugin page was opened.
  Queued/running upload shows a static sync icon; normal completion restores the
  source icon; failures add an exclamation indicator. Manual progress/time
  uploads also reflect this state. No touch handler or continuous animation.
  Badge artwork is project-drawn SVG, not downloaded vendor artwork. The native
  archive/excluded-sync warnings retain their space. Registry changes are
  watched atomically, with cached lookups rather than disk reads per tile.
- Focused regressions cover offline close queueing, duplicate close suppression,
  sleeping defer/resume, reopening cancellation, account-bound arguments,
  missing/removed provenance, silent auth failure and badge component states.
  Firmware peer replay verifies exact grid/reader/host insertion. Full Linux
  regression, host Qt5 UI and official SDK build pass; real-device worker rejects
  a synthetic wrong-account job with account_changed before any upload. No real
  reading position or duration was submitted in this guard test. Device badge
  rendering/load checks and installation are tracked in the following note.

### Correction: current explorer badges and exit priority
- Build 20261023005648 was loaded (device/source SHA comparison confirmed),
  and an actual background success was observed in sanitized runtime markers.
  However its icon hook targeted the legacy ContentEntriesGridItem, used by the
  Recent menu, not the current navigator. Static replay/component rendering did
  not prove the actual shelf hook. Current grid/list delegates are instead
  /qml/device/view/syncserviceexplorer/ContentGridItem.qml (entityId, _thumbnail)
  and ContentListItem.qml (entityId, thumbnailBackground). Both now receive
  passive source/sync/error badges and are included in replay validation.
- Exit jobs now preempt the whole-shelf reading-progress subprocess; starting
  whole-shelf reads defers to queued exit work. A focused five-second blocked
  shelf-read regression verifies the exit worker starts before that read ends.
  Foreground work still serializes normally, and exit batches refresh only once.
- Capture opening page/layout/native-PDF size+mtime signature (not a percentage)
  and compare after native close/save settling. If unchanged, skip the expensive
  exact position upload; only today's measured pending time is eligible for its
  own upload, retaining account/provenance/uncertain-receipt guards. A changed
  page/layout/PDF takes the existing exact synchronization path. Reopening
  timestamps are ignored. No broad scan/upload is triggered by opening the
  plugin. Entering the plugin still reads the cloud progress for display.
- Time upload retains a fresh cloud position before each acknowledged chunk,
  but refreshes the display aggregate once at batch end rather than after every
  chunk, reducing HTTP requests without weakening receipts or lost-reply guards.
- Added privacy-safe queue/start/page-open/unchanged markers for distinguishing
  actual exit work from foreground refresh without logging titles or IDs.
  Deployment and the current native-library/exit runtime check follow below.

- Installed corrected build 20261023041430. Native host/sidebar readiness and
  current-library-badge-ready markers confirmed without opening the plugin.
  A real imported body-text reading session then closed: queue -> start in
  roughly 2 seconds, followed by successful exact position/combined-time result
  roughly 19 seconds after start. No page-open marker occurred during this
  sequence, and no position-unchanged shortcut was taken. This is the first
  explicit exit-without-entering-plugin runtime verification for the corrected
  current-explorer hooks. Only event codes/timing were inspected, no titles,
  identifiers, tokens or page content. Real cloud synchronization was authorized
  by the user's reinstated default-exit feature and performed for their actual
  reading session, not fabricated test time.
- Full Linux regression, host Qt5 core/UI, official SDK, device Qt6 signature/UI
  and device Lua time regressions passed. Priority regression interrupts a slow
  whole-shelf read before it can delay an exit job. Install rollback timer is
  disarmed; install wake lock and temporary synthetic device fixtures cleaned.
# 2026-10-26 cPages identity-layout compatibility

- Actual read-only diagnosis reproduced `progress_layout_unsupported` before
  matching; the affected native document used formatVersion 2/cPages. Its cached
  XHTML and original source map both matched the current PDF page uniquely.
- `native_page_layout.identityCount()` now validates legacy identity maps and
  cPages original/page counts, unique IDs, strictly ordered CRDT indices,
  identity redirections and nondeleted pages. Upload and restore share it.
  Complex or malformed layouts remain rejected; no native records are rewritten.
- Native percentage reads cPages pages; unsupported layouts get a specific UI
  message rather than the generic text-location failure.
- Pure layout tests and device synthetic sync/restore tests passed, including
  no mutation of native metadata. Host Core, device Qt6 UI, official SDK,
  Linux validations and replay against current peer QMDs passed. The host Qt5
  UI build fails on the preexisting Qt6 timezone API and was not changed here.
- The real document passed new local preflight with HTTP explicitly blocked
  and cache saving disabled. No real cloud progress/time was submitted.
- Installed `20261026173840`: fresh host/sidebar markers, translated module,
  matching worker/Lua hashes and active Xochitl/NRestarts=0 confirmed. The
  requested UI restart was canceled and the system rebooted; the kernel boot
  ID changed. OS identity and independent reader menu remained unchanged.
  No rollback marker; temporary synthetic test files were removed.
# 2026-10-29 周期上报（覆盖旧退出一次性时长方案）

按用户要求以KOReader生命周期为参照，60秒精确定时，首tick才入场/上报；正常周期复用私有session.context，仅POST一次60秒，不执行本地PDF/正文匹配或云端位置GET。上下文900秒TTL、明确拒绝刷新、必要时续期600秒冷却，未知结果不重放，报告ID持久化去重；每次tick是新间隔，不补历史。退出/手动进度仅rt=0，保留退出精确位置和实际坐标冲突判断，不以时间/百分比/摘要归一化判位置冲突。空闲/后台/锁定/睡眠/断网/关书停止调度，恢复重等60秒；在途取消仍可能有服务端已接收的结果，不能保证撤回。

缓存坐标不能同时保证实时检测别的客户端进度变化，已向用户解释。外部KO代码仅研究，未复制无许可证实现。合成Lua验证跨worker无GET/no PDF/no正文、每tick60、无退出积压、拒绝/续期/不明回执/去重；SDK与容器Qt6 offscreen UI通过（不是设备实测）。电脑8787测试服务已关闭，保留私有登录/实验记录；尚未安装本轮修改到设备。
