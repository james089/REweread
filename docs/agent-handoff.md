# REweread Agent 交接手册

更新时间：2026-10-13

2026-10-26 原生 cPages 分页兼容：只读诊断确认受影响导入书的正文与下载原文映射均可唯一定位，实际同步停在 `progress_layout_unsupported`，原因为该书 `.content` 使用 formatVersion=2/cPages，而非旧式 pages/redirectionPageMap/originalPageCount。新增 `WereadDownloaderPlugin/lua/native_page_layout.lua`，为上传、下载共同验证旧结构与 cPages 的同序一一对应映射，校验 original.value、pageCount、唯一页ID、有序idx.value、redir.value 和删除标志；不放宽正文/来源映射/并发变更校验，不改原生元数据或批注。C++ 本地百分比读取 cPages.pages，UI 为不支持的结构提供专用提示。纯 Lua 布局边界、设备合成精确上传/恢复及元数据不变、主机 Qt Core、设备 Qt6 合成 UI、Linux 全量、官方 SDK、全部活动补丁回放均通过。主机 Qt5 UI 构建因既有 QTimeZone::fromSecondsAheadOfUtc API 不兼容未通过，本轮未改无关时区代码，使用设备 Qt6 UI 验证。受影响真实书的新版只读预检已通过本地精确定位，主动阻断网络请求并禁止缓存保存，未执行真实云端写入。安装 `20261026173840`，host/sidebar-ready、汉化、worker/Lua 哈希一致已确认，无回退标志。注意软重启请求又遭取消并触发整机重启，内核 boot ID 改变；之后系统版本未变、Xochitl active/NRestarts=0，不能宣称本次仅软重启。原生菜单 QMD 保持不变。临时合成测试目录已清理；用户可直接重试，无需删除旧书或重下缓存。

2026-10-26 目录返回来源修正与双入口（覆盖下条图标方案）：用户恢复“目录 / 页面概览”两个独立入口，均在完整滑条上方，224×88 白底无边框圆角按钮，黑色系统 icon+文字，按钮间24、到滑条28、面板顶部留白32。`menu.qmd` 对 Pages 增加当次 overview 打开/TOC 激活的来源状态：阅读增强既有直接入口在同一同步调用中 openOverview+激活 TOC，此时 Back 关闭 overview 回正文；从已打开概览进入 TOC，Back 只关闭 TOC 回概览。概览关闭与换文档清理状态，章节点击保留原生处理。修改在独立 QMD 中，不修改阅读增强文件。新增 `toc_navigation_test.cpp`，在带真实 QQuickWindow 的 offscreen 合成页面中运行实际 QMD handlers，覆盖两个目的地、反复进出、文档替换，设备 Qt6/software 测试通过，无真实书籍操作。与全部现有补丁回放、Qt6 SDK QML 语法、官方 SDK 构建和 Linux 全量验证通过。设备 `2026102605` 专属 shortcuts-ready、contrast defaults ready、下载插件 host/sidebar-ready 已确认，Xochitl active/NRestarts=0；本次仅软重启，内核 boot ID 未变。既有插件 QMD 哈希未变，回退保护解除，测试/上传目录已清理。真实菜单点击和视觉效果由用户验收；本轮改动未提交。

2026-10-26 跳页入口最终样式（覆盖下条双按钮布局）：用户取消新增“页面概览”，只保留滑条左侧的目录图标。`menu.qmd` 现使用透明 ArkControls.IconButton、内置 `qrc:/ark/icons/list` 黑色 48px 图标、88×88 点击区，无文字/可见背景/边框；滑条行高120使按钮上下各留16，按钮与滑轨间24。无目录时隐藏并归还滑轨宽度；保留原生页面概览的其他入口。用户中途提出的圆角背景已被后续“透明背景、黑色系统图标”要求取代。联合补丁回放、Qt QML 语法、Linux 全量验证与 SDK 构建通过。设备 `2026102604` 的专属快捷入口 ready、contrast defaults ready、下载插件 host/sidebar ready 已确认；系统 active/NRestarts=0，软重启成功且内核 boot ID 未变。既有阅读增强/下载插件 QMD 哈希未变，回退计时器已解除，上传目录已清理。实际视觉和触控由用户验收。

2026-10-26 跳页快捷入口：`NativeReaderDefaults/menu.qmd` 为底部 DocumentPageSlider 增加“目录 / 页面概览”快捷行，两按钮均 192×88，位于滑条上方，保留原滑条宽度；拖动出现页面预览时隐藏，避免遮挡页码/章节标签。目录复用阅读增强已有 `rmtoolOpenTableOfContents()`，仅其 availability 与方法都有效时显示；概览复用原生 `openOverview()`。点击先隐藏 quick-browse 预览和滑条，不触发 accepted/openPage。与设备活动补丁联合资源回放、Qt QML 语法、官方 SDK 构建和 Linux 全量验证通过。菜单升级/回退脚本现支持保留已有菜单，合成测试覆盖首次安装回退及升级恢复旧菜单。用户授权安装 `2026102603` 后，设备确认 shortcuts ready、下载插件 host-ready，Xochitl active/NRestarts=0，无快捷入口 QML 错误，汉化及既有插件保持；本次软重启成功，内核 boot ID 未变。安装确认已保存，回退计时器已解除，上传目录已清理。未做真实触控或视觉验收。

2026-10-26 原生阅读菜单：`NativeReaderDefaults/menu.qmd` 启用 SettingsMenu 既有 adjustmentToolLoader 的顶层加载路径，隐藏外层 AdjustmentToolsMenu、阅读器 SearchButton 与 ShareMenu；保留 FilterMenu 和 TextSettingsButton 原生组件及其事件、可见性规则。PDF 的独立缩放/裁剪菜单保留并改名“页面缩放”。菜单升级脚本只新增独立 QMD 并扩展现有固件保护，不改阅读增强/微信插件 QMD。与设备全部活动 QMD 的本地联合回放、SDK 构建及 Linux 全量验证通过；macOS 全量验证两次停在已有 XOVI guard SIGTERM 120ms 时序断言，未修改或跳过该测试，Linux 全套通过。安装 `2026102602` 已确认 menu ready、contrast defaults ready、下载插件 host-ready、汉化模块加载，NRestarts=0、无菜单 QML 启动错误，既有插件 QMD 哈希未变。没有真实触控/视觉验收。注意本次仅请求 `systemctl restart xochitl`，但旧界面退出阶段发生 SEGV，系统故障保护继而触发整机重启；实际 kernel boot ID 改变，不得宣称本次仅软重启。新启动后界面稳定并确认加载成功；启动后菜单安装确认已写入，临时上传目录已清理。

2026-10-26 原生阅读器对比度默认值：新增独立 `NativeReaderDefaults/` XOVI 模块与 DocumentView QMD；打开 PDF/EPUB 时仅对 `.content` 中缺失 `viewBackgroundFilter` 且实时设置为空的文档，通过原生 `DocumentController.setViewBackgroundFilter(id, "off")` 初始化。显式空字符串为 Adaptive，必须保留，不能用 `value || "off"` 全局替换；显式 off/fullpage、笔记本、解析失败均不处理。原生读取缺省值来自 `QJsonValue::toString()`，并非全局 conf；`[Dialogs] BackgroundFilter` 只是引导完成标志。精确固件/hashtab 检查、系统更新禁用脚本、首次安装回退保护已实现。官方 SDK 构建、设备合成元数据及实际 QMD handler、动态模块/QML singleton 测试、与设备现有补丁共同回放、仓库全量静态验证均通过。设备安装标记 `2026102601`，软重启后 native defaults QMD ready、汉化及下载插件均加载，NRestarts=0，内核 boot ID 不变；既有阅读增强和下载插件 QMD 哈希未变。未操作真实书籍做设置或视觉验收。补丁在文档下次打开时生效，不批量改写已有元数据；设备临时测试文件已清理。

2026-10-23 最新工作树变更：退出同步把进度和待上传时长合并到 `report_read` 请求，最近阅读请求保留，写入后只做一次云端回读校验；时长回执账本和歧义请求防重保留。原“删除缓存”按钮仅对系统已移除书籍显示，额外“重新下载缓存”动作不提供。构建 `20261023060847` 已安装，host/sidebar readiness、Xochitl active、无回滚标记，远端 worker 哈希与本地一致。真实云端同步未作为测试执行。细节见 `WereadDownloaderPlugin/DEVICE_HANDOFF.md` 最新条目。

随后下载并发上限由 2 调至 3；该源码改动尚无设备实测。新书导入也复用下载开始时读取的云端位置快照和已校验的本地目录/XHTML，恢复时避免重复联网取目录与章节正文，保留 PDF 页面匹配及写入回读校验。合成覆盖已添加；设备安装与真书计时尚未完成。

这份文档面向第一次接手仓库的新 AI Agent。先读 `AGENTS.md`，再从头读完本文，然后才运行命令或接触设备。本文只记录可公开的项目状态，不包含设备地址、账号、书名、Cookie、API Key、二维码 UID、用户缓存或原始日志。

### 2026-10-22 退出同步附近章节优先

退出后上传精确进度改为先根据原生 PDF 页比例和缓存云端百分比筛选附近章节，只校验候选 XHTML 并做正文精确匹配；唯一命中仍需与新取回的微信读书章节正文和坐标一致性复核。无命中或多重命中才回退有界全书搜索。若书籍目录缺少章节字数，百分比估算改用清单文件大小，不为估算重新读取整本章节正文。新增退出日志只记录匿名阶段耗时与 nearby/full 模式。用户实际退出一书时记录总计约16.6秒：附近章节匹配约0.52秒，目录网络约1.50秒，远端章节正文约3.34秒，其余时间主要是云端读取、写入和回执。

随后进一步减少请求：退出同步通过已校验的显示目录缓存复用目录；进度写入成功并回读确认后，最近阅读标记复用同一份已验证云端状态，省去标记前重复读取，但仍在标记后回读验证位置、百分比与阅读时长。安装器将 Xochitl 重启移出临时安装事务，避免重启取消事务并触发回滚。新版安装标记 `20261023044641`，宿主与侧栏已就绪，回滚保护已解除。SDK 构建、Lua/shell 语法、固件接口回放和仓库安全门禁通过；本次优化尚无新的真实阅读会话耗时，不宣称具体提速。实现细节见插件 `DEVICE_HANDOFF.md`。

2026-10-23 本地索引问题经匿名只读诊断定位为源索引缺失，而非模块加载失败：上游注释索引库在 `settings.data_dir` 未设置时使用当前工作目录，不同 worker 的启动目录不一致。插件现在将 `data_dir` 固定到私有账号缓存目录，下载与退出同步共用同一路径；旧书首次网络回退后会写入该私有索引库，后续退出可本地命中。代码也移除了对章节内 emoji 的过度拒绝，仍校验缓存正文与源索引文本完全一致。设备 LuaJIT 合成同步回归、官方 SDK 构建、安全门禁通过。安装标记 `20261023051236` 已验证 host/sidebar 就绪、Xochitl active、回滚计时器 inactive。旧书第一次退出可能仍网络获取并写入源索引；再测下一次退出才能验证缓存命中和提速。

### 2026-10-22 原生下载插件进度同步与章节显示

修正版插件安装标记 20261023041430 已确认；当前书库来源图标加载已确认。真实正文阅读退出后约2秒启动后台上传、约19秒后成功，期间没有打开插件，未走未变位置快捷路径；仅检查匿名事件码与耗时。安装唤醒锁和合成设备测试文件已清理。

注意原生书库当前使用 syncserviceexplorer/ContentGridItem.qml 与 ContentListItem.qml 的 entityId，旧 navigator/ContentEntriesGridItem.qml 仅用于旧入口/最近菜单，不足以验证真实书库图标。退出上传应抢占整架 reading-progress；进插件本身只读取显示，不批量上传。已增加本次打开/关闭位置+排版签名，未改变时跳过精确位置上传，只处理当日待传时长。

用户已重新要求启用退出自动上传，覆盖下文较早的“取消”记录：原生阅读器关闭插件来源书籍后默认排队上传精确进度，并带当日待传时长；账号/scope/UUID/EPUB 来源及云端冲突校验保留。锁屏、休眠、应用非活动时延后，重新打开会取消该书排队任务；后台失败不唤出二维码，不自动重试含歧义的时长。原生书库封面右下角显示来源图标，同步时切静态同步图标，失败带叹号；详细实现和验证见插件交接。

新增按日明细：使用设置页选择的 IANA 时区分日（默认 UTC），跨日按该时区拆分；统计页可筛选全部/日期/无日期。上传仅处理所选时区的当日新增，历史或无日期部分暂存，云端历史归属尚未验证，不宣称已支持云端每日补传。累计回执与按日回执共同保留，跨所选时区日界线停止，避免历史被算到当天。更改时区只影响之后的记录；旧版已保存的逐日合计没有区间时间戳，无法精确迁移日期，保留原日期。设备已确认原生阅读事件产生非零本地记录；未上传真实时长。

新增原生阅读时长统计：插件底部“书架 / 阅读时间统计”tab，按书显示本地累计、云端累计及待上传时长；周视图显示所选时区内的七日柱状图，可翻周并点选日期，日视图可前后翻日。只有带有效导入来源与账号 scope 的原生书籍计时；使用打开/关闭/翻页、显示休眠、窗口可见性、子菜单、密码锁及应用活动状态暂停/恢复，静置五分钟停表，新增时长不回填历史。UI 独占 `local-reading-time-v1.json`，worker 独占 `uploaded-reading-time-v1.json`；上传前持久化 pending，业务确认后才推进确认计数，网络歧义不盲重发。统计页独立上传和原有“上传进度”顺带上传两个入口均已实现；没有恢复此前取消的“退出阅读自动上传”。云端累计沿用进度读取中的 readingTime，到账汇总可能延迟；未伪造真实账号时长做测试。详细边界和验证见插件 `DEVICE_HANDOFF.md`。

后续新增：导入确认成功、手动上传进度成功后，原生书库通过 `markOpened()` 更新最近阅读，云端发送保留原位置且无阅读时长字段的进入阅读请求。设备验证云端书架最近阅读时间更新，章节/偏移、百分比与累计时长不变；标记失败只作次要提示，不影响主操作成功。用户已取消“退出阅读自动上传”，该功能未安装、相关改动已撤销。详见插件交接记录。

随后优化章节识别：持久化经过账号/manifest 校验的目录和本地章节显示结果，以页码、排版、manifest 与原生 PDF 的 SHA-256 判定失效；缓存命中的本地章节先于网络请求推送，精确上传/恢复不使用这些显示缓存。设备只读测量中，热缓存无 PDFium 加载/目录请求，本地章节几乎立即显示，全架云端校验仍取决于网络。导入批次完成或上传进度完成后统一刷新一次插件书架，让云端最近阅读排序显示出来。

`WereadDownloaderPlugin/` 已支持已导入书籍的独立“上传进度”（本地→云端）和“下载进度”（云端→本地）；新导入只自动拉取一次云端位置，旧书进入/刷新不得自动覆盖本地位置。手动失败的待重试状态与新导入自动意图区分保存。云端百分比按章节字数与正文位置估算，本地仍为原生页数比例，两者分母不同；不得声称云端原始 progress 字段是最远进度。

右侧显示两行本地/云端百分比及章节名称，下方并排 144×56 上传/下载按钮；导入和删除缓存保持 144×88，列表行高不变。云端缺失章节 ID 或偏移不兼容时，仅显示估算允许使用经校验完整缓存中的唯一摘要匹配；上传/下载的精确位置校验不放宽。本地章节由当前原生 PDF 正文匹配校验过的下载缓存，无正文页面明确显示“当前页无正文”。

最新安装标记 `20261022230840` 已确认 host/sidebar 加载。完整 Linux 回归、官方 SDK 构建、设备合成 Lua 行为测试以及主机 Qt5/设备 Qt6 界面测试通过，设备合成截图已检查布局；真实书只读核对完成，不应把它描述为本次真实上传/下载测试。详细实现、异常与设备验证记录见 `WereadDownloaderPlugin/DEVICE_HANDOFF.md`。临时测试文件和安装唤醒锁已清理。

### 2026-10-21 页脚章节/页码与独立跳转预览

页脚左侧 readerFooterChapter 显示当前章节名并打开目录；右侧 readerFooterPages 通过 readerGlobalPageLabel 显示全书页码并打开 showReaderJump 独立层。阅读设置中的旧跳转滑块/百分比/章节行已移出，后续 readerSettingsY>=744 减88，笔反向映射补回88；其他四列/全宽/底部安全区域保留。手指页脚及笔 handleStylusTap 有入口，预览层遮挡并禁止底层翻页。

全书页码不能再用局部分页窗口数量。readerFullIndex 使用当前书+排版签名，空闲每80ms计算一页，触摸后退让750ms；完成后停表，休眠暂停，不改可见窗口。首次未完成显示“页码计算中”，失败显式显示失败，不猜总页数。ReaderStore.loadPageIndex/savePageIndex 共用经过指纹/签名/checksum/边界验证的布局缓存方法，独立 reader-page-index-v1.json，不能覆盖 reader-window-v1.json。正文/排版变化失效，完成结果下次打开可恢复；目前未持久化未完成草稿。

拖动预览120ms节流，不改变阅读位置；预览卡按当前排版缩小正文及图片组，上方章节名，下方页码。索引未完成可按文本位置预览/跳转；完成则按真实页序选页，完成时自动刷新已打开预览。松手刷新最新目标后50ms提交同一start，已索引目标直接切两页缓存（含纯图尾页）。关闭取消。closeReaderJump 仅真正打开时设置600ms防穿透，不能无条件设置：首个候选设备验证曾因此导致刚进入阅读的滑动失败，已修复并增加行为断言。测量/预览图片标题识别显式传目标图片，避免依赖当前正在读的另一页。

新增 tests/validate-reader-page-preview.mjs 与缓存Qt行为脚本；扩展 document cache unit 覆盖独立文件、不覆盖窗口、字体/源变化及损坏失效。exit-ui 增加真实左右页脚点击、完整索引、预览保持原位置、准确跳转及截图；saveSelfTestScreen 可选固定reader-preview标签，仅隔离测试且环境显式允许时生成合成正文截图，不能传任意路径。初个候选已证实151页预览/跳转成功；最终构建再次核对合成正文预览截图。最终源码完整Linux回归、shell语法、宿主Qt文档/页码缓存与进度队列行为、官方SDK构建已通过。最终二进制 SHA-256 5f270f4223c93b8d708dc6613551f186cac6b6d6614daddd9913f2469936218c。连接曾在上传前USB SSH超时，用户唤醒后已原子安装最终版，exit-ui干净退出0；完整文件门禁、真实删除状态刷新、书架翻页、正文滑动、两侧页脚点击、全书索引/预览/跳转、边距选中、设置标签/宽度/安全边距和详情按钮均ok。合成151页索引同步验收耗时12193ms；正常模式逐页80ms计时切片且交互退让，首次索引可能较慢，不是后台线程，勿承诺即刻显示总页数。正常应用已恢复，服务active、进程存在、无QML启动失败，设备哈希与最终构建一致；恢复前系统active/NRestarts=0。原始设备日志及截图不加入Git。旧稳定回退备份 before-page-preview 是68256阅读优先版；任何 before-page-preview-final 是候选，不能混为稳定备份。本任务未开启单色快刷，未改系统汉化/XOVI或helper、未写云进度。

### 2026-10-21 阅读优先：停用笔迹、进度合并与翻页退让

用户明确优先阅读体验、先取消笔迹。正常阅读 readerInkEnabled=false，隐藏笔迹层、侧栏和入口手势，禁用笔迹起/续/落笔路径，不读取每页 strokes、不清直接画布；原笔迹/段落笔记文件完全保留。底层仅 reader-ink 隔离回归及显式 reader-turn-perf 对照可启用；无正常 UI 开关。脚注仍保留行框定位，普通无脚注页不生成只供笔迹/脚注使用的行框。

翻页改 ReaderStore.queueProgress：只更新待保存行，800ms 无新翻页后统一 QSaveFile 原子落盘；savedTextOffset 等读取会合并待保存数据，快速恢复不会回到旧磁盘位置。切书、离开阅读的 saveProgress、休眠、Window关闭、QCoreApplication.aboutToQuit 和析构都强制刷新，失败保留待保存行，已有其他书记录不覆盖。正常退出/休眠保障即时保存，异常断电可能丢未落盘尾部，不能承诺每次翻页已写盘。

翻页按下/实际翻页先停止补排，至少安静750ms后才补下一页，后续单页400ms节奏和前方5页目标不变；不是后台线程，也不是整本预排。正文改先统一更新当前页字段，再一次提交 readerRenderedPageHtml，避免字段变动重复生成/重排 HTML；不是整页图像预渲染。仍保留必要缓存未命中时的当页分页。

新增 reader_progress_queue_smoke.cpp 和 run-reader-progress-queue-smoke.sh，覆盖翻页不写盘、内存恢复、停顿自动落盘、强制保存合并多书、析构刷新、写失败保留/恢复重试。新增 validate-reader-performance.mjs 验证关闭笔迹后无画布/笔迹读取、缓存页不分页、格式化一次且脚注定位保留；分页测试改可控时钟、检查交互暂停和非同步保存。完整 Linux（含新增用例）、宿主 QtCore/Gui进度和旧笔迹数据行为测试、shell语法、官方SDK已通过。

性能验证：reader-turn-perf 始终 main.cpp QTemporaryDir 和独立退出会话，默认跳过账号初始化，无云进度/时长写入。16次缓存翻页分别记录 setReaderPage JS耗时和 Qt frameSwapped 首帧；不包含物理墨水刷新完成，也不是官方客户端对照。初始旧诊断构建采用重复文本，最后退出阶段139（样本采集完）；不使用其夸大的重复文本速度作主结论。最终同构建改用每段独立编号正文，对照只允许在该隔离模式用 RM_WEREAD_PERF_BASELINE=1 开笔迹/同步保存，正常模式即使env=1也不能启用。退出改计时器离开frameSwapped回调，最终对照已正常退出。同一最终构建、独立编号文本各16次：对照 JS均值29.25ms/Qt首帧101.75ms，优化12.12ms/79.25ms；首帧中位数100/77.5ms，范围82–140/62–102ms。对照开启笔迹+同步QSaveFile保存，非旧发布版精确复刻。两组最终均干净退出；优化 reader-no-ink-selftest=ok。设备 exit-ui 全链路通过，设备Qt6独立进度合并/故障重试测试通过；保留笔迹诊断 reader-ink 七阶段全部通过，正常应用重新启动成功、服务 active、无 QML 启动失败；系统 active/NRestarts=0。实际仅更新应用二进制，当前hash见本段；已保存的用户书籍/笔迹及系统汉化/XOVI配置不动。正常回退可用 rm_weread_qt.before-reading-priority-baseline（上一稳定图标版本），其余 before-reading-priority-* 为诊断/中间候选，不应误认全部为稳定版本。详见 docs/reader-performance-2026-10-21.md。构建 SHA-256：68256ddbb1d09c8aee8951159d1b089f59f1ab946b4f46d5d526d9ef7d90bb0c。

### 2026-10-21 书架图标操作与设置文字统一

移除书架顶部“继续阅读”入口；同步和退出改 64x64 无视觉文字的按钮，Canvas 分别画双箭头和标准电源符号，不使用 Accessible 附加对象：设备精简 Qt 报 Non-existent attached object（SDK 构建不拦截），首次隔离自检发现后已移除该属性，后续不得重新引入。同步时黑底白图标、禁止重复点击，无旋转动画；退出仍只是 quitToSystem，不关机/重启设备。阅读设置九项标题统一 readerSettingsLabelFontSize=20（原字重18已统一），底部进度同步/拉取结果 Text.AlignRight、右边距24。

完整 Linux、shell 语法和官方 SDK 构建通过。exit-ui 新增实际九标题字体/状态对齐属性断言，并分别保存临时合成书架与设置截图；首次隔离自检因上述属性加载失败，系统自动恢复 active/NRestarts=0，修正后 reader-settings-label-status-selftest=ok，既有完整文件/删除/书架滑动/正文/进度/宽度/详情/退出回归全部通过。合成书架截图核对：仅同步双箭头与电源图标，无继续阅读；合成设置截图核对九标题字号一致、底部状态靠右且无裁切。有效回退备份 rm_weread_qt.before-header-icons；修正前候选另存 before-header-icons-final（不要将它当可用回退）。正常应用已重新启动，系统 active/NRestarts=0。没有调用真实同步/退出图标做网络或系统动作测试，沿用既有回调；原删除/宽度/详情按钮/正文与退出回归继续运行。构建 SHA-256：3c7f7439d340db7b63de637c855185d077237093046cfbff9d9a74eaadf29e5e。

### 2026-10-21 已下载书直接阅读、设置全宽与详情入口

书架封面 TapHandler 改走 openShelfBook：共享 hasCompleteDownload 成功则直接 openOrDownloadBook，不进详情/不取元数据；未完成仍进详情。发现/笔记书籍入口同样对已完成书直接阅读。openDiscoverBookDetail 增加 forceDetails，新的 openReaderBookDetails 先保存当前阅读窗口/待写笔迹，按 currentBookId 找书，明确 forceDetails=true 进入详情，避免被直接阅读策略循环带回。

阅读设置保持原主体结构，只调整几何：控件列从 x122 到右侧24，四列/两列/灯光/字号按可用宽度分配；移除字号两侧装饰 A，三个字号单元横向铺开。字体/字重拆为独立整行，额外64基准像素；readerSettingsY 对>=620增加64，面板1280上限/1000基高，保留先前章节预览、网络和缩进移除映射。手写笔命中同步所有横向计算及字重新增行反向映射。底部四操作等分可用宽度，状态单独置于其下，保留安全边距；“重绘本页”改“书籍详情”，手指和笔同一动作。

JS 行为覆盖完整书跳过详情/网络元数据、未完成仍进详情、强制查看详情，720/954/1200宽下选项/底部对齐，以及新字重行笔命中。完整 Linux、shell 和官方 SDK 通过。设备 exit-ui 增加实际设置控件右边界对齐、真正点击详情按钮并验证当前书ID；合成 ink-probe 增加本地元数据保证详情测试不联网。应用已备份 rm_weread_qt.before-reader-fullwidth 并原子更新；短暂 USB 超时后恢复连接。设备 reader-settings-width-selftest=ok、reader-details-button-selftest=ok；完整文件直接放行、删除后状态、书名/底部标签、书架/正文滑动、进度跳转、边距选中、底部安全区与退出均通过。合成设置截图实际核对：字号/四列/字体/字重/灯光均铺满可用宽度，独立字重行、底部四按钮完整且状态独立显示。系统 active/NRestarts=0，正常应用已重新打开。真实完成书跳过详情由 JS 行为和构建验证，本次设备测试没有为点击用户书籍采集画面；详情按钮则有真实 Qt 点击验收。构建 SHA-256：d44e065b1a4b29c5b24300edbefc50a8b45eaeb144fa50708024b335a1b98c96。

### 2026-10-21 已完成文件与下载任务状态分离

用户报告已下载书进入下载管线并失败。只读设备初始检查：9 本有 full EPUB，8 行状态 full、1 行 downloading；出错行失败计数 0、fullFile 有效，ZIP CRC 通过，106 章对应 107 个 spine 项，任务进度停在 11/106，Qt 记录退出码 15，不能据此声称网络失败或文件损坏。排查期间设备状态自行变为全部 9 行 full、错误记录消失；未为诊断写元数据/重下书籍。只读新规则审计识别 9 本完整文件。

根因是书架过去只看文件名，而阅读入口只接受 downloadState=full/failed=0；任务进度、取目录或修复会覆盖这些任务字段。已有 fullFile 在自有 helper 中只在整本成功打包时设置，后续 partial 保留旧 fullFile，因此它本身记录上次成功的完整产物，不应被新任务状态撤销。

新增共享 local_download_state.h/completedLocalEpub，DownloadStore 和 ShelfStore 使用同一规则：优先有效 fullFile（非空普通文件、full.epub 后缀、canonical 路径在本书目录内），不让 downloading/partial/failedChapterCount 的后续任务字段撤销该旧产物；旧 full 记录无路径时仅允许 full 且失败数 0 的目录扫描回退。无完成记录的任意 full 文件名、partial 文件、空文件、缺失或越界路径仍拒绝。ShelfStore 两种元数据来源均传入 fullFile/failedChapterCount，封面和阅读门禁一致。没有改 Lua 的旧原型读取/缓存快捷路径，也没有自动修复历史元数据；正常 Qt 快捷路径会在启动 Lua 前直接使用已有完整文件。

宿主 Qt Core 行为覆盖 downloading+旧 full 直接 done 且进程不启动、partial/失败修复不撤销旧 full、无完成记录不得仅凭文件名放行、partial/空文件/越界拒绝。完整 Linux、shell 语法、官方 SDK 通过。设备 exit-ui fixture 设为 downloading+失败数 1+旧 fullFile，completed-file-gate-selftest 检查下载 API 直接 done/无进程/书架 full，再跑真实删除和阅读 UI 回归。设备 completed-file-gate-selftest=ok，书名/标签、真实删除按钮、书架/正文翻页、进度跳转、设置和退出回归全部通过；备份 rm_weread_qt.before-completed-file 并原子更新，系统 active/NRestarts=0，正常应用已重新打开。未联网下载用户书籍做测试，也未写云端进度。更新脚本停止应用前再次检查在途 helper，有下载则退出等待，避免编译期间新发起任务被中断。构建 SHA-256：ec511c27c22e25145c81330bd06f19775df5bbe9764563bdf2ef17a1b70f03cb。

### 2026-10-21 书名透明度与底部标签样式

按用户要求，未完整下载书籍的 shelfBookTitle.opacity 与封面一样为 0.5，完整书为 1；下载完成/删除后随同一书籍状态刷新。底部删除魔法笔记本入口，保留书架/发现/我的三个标签，180x58、间距 32、圆角 8，选中纯黑底白字，未选中沿用浅底黑字。仅移除导航入口，未删除笔记本数据或功能代码。旧测试对必须出现魔法笔记本入口的契约按新需求更新。

新增 exit-ui 的 shelf-title-tabs-selftest，检查完整/未完整书名实际 opacity 和三标签黑底白字；原删除按钮回归同时确认删除后书名变为 0.5。完整 Linux、shell 语法、官方 SDK 构建通过。设备 shelf-title-tabs-selftest=ok，实际删除按钮后封面及书名透明度回归、书架/正文滑动和设置/退出回归均通过。备份 rm_weread_qt.before-shelf-tabs 后原子升级；系统 active/NRestarts=0，正常应用已重启。测试仅使用隔离合成书，不访问用户封面截图。构建 SHA-256：6b72e03bebe0559bd464e9d5e03c0bbeacf6982f52002014b36822bd6a9af776。

### 2026-10-21 删除下载回调中断：真实按钮复现与修复

用户反馈上一轮仍不能自动恢复。上一轮只验证了 normalizeBook 对已删除文件的重算，未验证 QML 实际按钮链路，不能把它表述为完整 UI 修复。新增 exit-ui 临时书库 fixture：195 本中的 shelf-probe-0 放合成 full EPUB/陈旧 full 元数据/下载记录；打开真实发现页、滚动到真实“删除下载”按钮，注入 Qt 点击，再检查 downloads 的 deleted、书架 remote、tile.fullyDownloaded=false 与实际 cover.opacity=0.5，随后继续原滑动/正文/设置/退出回归。

修复前设备明确复现 download-delete-ui-selftest=fail deleted=true refreshed=false，证明文件已删而刷新未完成；真实用户数据只做计数，两个 deleted 记录均无残留 EPUB，不输出 ID/书名。原按钮调用 deleteDownload 会 emit changed 重建 Repeater 并销毁当前回调所属 delegate，后续 shelfStore.reload 不可靠。改为 DownloadStore 成功删除后发 downloadRemoved(bookId)，main.cpp 在对象层连接 ShelfStore.reload，移除按钮内后续 reload；失败不发成功信号。之前的文件存在优先修复仍保留。

Linux 全回归、shell 语法、宿主四组 Qt Core 行为、官方 SDK 构建通过；新增成功信号实际在目录消失后触发且不误删目录外文件的合成断言。修复后同一设备/同一真实按钮点击路径通过 download-delete-ui-selftest=ok deleted=true refreshed=true，滑动/22 页书架/正文翻页/进度跳转/边距选中/底部安全区/退出全部通过。备份 rm_weread_qt.before-delete-ui-fixed 并原子升级，系统 active/NRestarts=0，正常应用已重新打开。全部删除测试仅操作临时合成书，不删除用户书籍。构建 SHA-256：24d70cc1f9a8cf193b7020ed4c6c74e72f4b710c56af27eb6b28e8f056c239a1。

### 2026-10-21 删除下载后书架状态纠正

发现页删除动作原本已调用 shelfStore.reload；实际问题是 ShelfStore.normalizeBook 在找不到 EPUB 时仍保留 book-status 的 full/partial。现在本地 full/partial 文件存在则对应完整/部分，否则统一 remote，使文件删除后下载状态、动作文字、完成角标和封面透明度一致。保留元数据和阅读位置，不清除账户或缓存总目录。

新增 shelf_download_state_smoke.cpp：临时合成 full 文件先确认 full，删除文件但保留 raw full 元数据，确认 remote/空路径/全部下载。宿主 Qt5 无法编译 ShelfStore 原有 QProcess::finished 重载，未为测试改动产品信号连接；改用官方 SDK Qt6 编译测试并在设备运行。完整 Linux 回归、shell 语法和 SDK 构建通过。设备合成文件删除行为测试和 reader-ink 七阶段通过；备份 rm_weread_qt.before-deleted-shelf 后原子更新最终应用，系统 active/NRestarts=0，正常应用重新启动且服务 active。未删除任何用户书籍做测试，真实发现页触摸/详情视觉仍待用户验收。二进制 SHA-256：f688b8d7d3fc2fc021cbbb76a886f8f5ed993c08c3bedcd5705fb2c52014e080。

### 2026-10-21 详情下载进度、移除本页下载与书名门禁

详情可见新布局在阅读进度下增加下载状态文字和进度条（y712/752），后续操作区下移 64px；按当前 bookId 查下载记录，百分比按章节估算、打包前最高 99%，full 状态显示 100%。暂停/失败保留当前记录章节数，不继承别的书。当前书的取消按钮不会取消别的书；移除书架“下载本页”和对应 UI 调用，继续阅读按钮收拢，底层批量队列兼容 API 和回归所需 currentShelfPageBooks 保留。

下载 helper 在 start_full_download 之前通过自有 download_title.lua 选择有效书名：调用参数、书架、本地状态依次检查；空白、Untitled（大小写不敏感）、unknown、未命名/未命名书籍和书籍 ID 均不接受。缺失时调用已有 BookInfoService 获取元数据，失败或仍无有效名称则停止，不下载正文/生成占位名文件。有效名称写回状态，title= 协议刷新 Qt 下载记录；元数据获取期显示“正在获取书籍名称”。不批量重命名或重打包此前已经下载的 Untitled 文件。

完整 Linux、shell 语法、宿主 Qt Core 四组、官方 SDK 构建通过；Lua 合成测试覆盖有效缓存不联网、占位名获取成功、无效响应和离线失败。未联网下载用户书籍，未声称实测元数据网络端到端。应用及 redownload-book.lua/download_title.lua 已部署，旧文件备份 .before-detail-title；设备 LuaJIT 书名行为用例和 reader-ink 七阶段通过。随后书架状态修复的最终二进制见下段，不把此中间构建当作最新版。

### 2026-10-21 完成角标改为绿色圆底白勾

按用户要求，coverDownloadBadge 去掉“已下载”文字，改为 44px 绿色（#239B56）圆底、Canvas 白色勾；右下 8px 间距及 full/非空路径条件不变，无新增输入处理。完整 Linux 回归、shell 语法与官方 SDK 构建通过。已备份旧程序 rm_weread_qt.before-green-check 并原子更新；设备 reader-ink 七阶段通过，系统 active/NRestarts=0，正常应用已重启且服务 active。未导出真实用户封面，实际彩色角标视觉待用户验收。二进制 SHA-256：263ddf1416283ff11b89ae732b985fdeab80c1e864aad84e720370844d373267。

### 2026-10-21 书架封面透明度和下载进度圈

未完整下载封面 opacity=0.5，完整下载保持 1；标题不变淡。中央白底圆形进度圈独立于封面透明度，只有当前进程 running、bookId 匹配且记录 starting/running 时显示，排队或历史中断记录不冒充当前任务。无输入处理器，保持原封面点击和书架滑动路由。Canvas 按百分比变化重画，不加连续旋转动画。

DownloadStore 将 helper index 的已处理/总章节数保存到当前下载记录，启动新任务清零、已处理数限制在总数内；排队记录不继承其他书的数值。百分比按章节数估算，非字节/剩余时间；完成打包前上限 99%，成功后圈消失并沿用已下载角标。失败/取消停止显示圈，已有缓存保留。

完整 Linux、shell 语法、宿主四组 Qt Core 行为检查、官方 SDK 构建通过；新增真实解析进度、上界和跨书排队隔离测试。已备份并原子更新单个应用，备份 rm_weread_qt.before-cover-progress；设备 reader-ink 七阶段 passed=true，系统 active/NRestarts=0，正常应用已重启且服务 active。合成空书架仍有既存 undefined 颜色/标题占位警告，不将其当作实际下载圈视觉验收。未为本次测试联网下载用户书籍，实际下载期间视觉效果尚待用户验收。构建 SHA-256：24b9a9d71946d30844833aa4b86ff9fa753561158295e8fe6d6d15a393d6b650。

### 2026-10-21 阅读前必须完整下载（已部署）

用户明确改政策：每次进入未完整下载书籍，有网络即下载整本，未完成不允许阅读；删除详情“删除下载”。当前正常入口不再调用 downloadOpeningChapter。DownloadStore.hasCompleteDownload 要求 book-status.json 对应 real bookId 的 downloadState=full、failedChapterCount=0，且当前书目录内存在非空完整文件（canonical 路径防止越界）；不能仅凭本地任意 EPUB、书架旧状态或 full 文件名放行。Lua 满足同样 full/无失败状态后才可短路缓存，继续保留已验证章节续传。

详情首次显示/切换书籍会自动排整本下载，网络从断到连时可触发；同一本的元数据刷新不重复排队。阅读请求设置明确 pending intent，显示整本下载等待/进度；最近阅读、下载记录、冷开/内存恢复和目录章跳转均门禁。离线完整书可读，不完整书提示联网。partial/error/cancelled 保留缓存但不打开，onEpubReady 必须本次 state=done 且元数据/文件完整才兑现阅读意图；旧 opening-only 完成信号只触发整本下载，不打开。返回书架可撤销阅读意图并保留后台下载；其他书完成仍不抢占界面。旧 opening API/队列兼容层仍存在，但正常 UI 不再请求开头章节。

详情可见 Row 和旧隐藏布局的删除入口均移除，目录/修复插图两列排列。下载管理区原有删除能力不属于本次移除范围。

完整 Linux 检查、shell 语法、官方 SDK 构建通过。宿主 Qt5 Core 四组行为测试通过，下载文件测试覆盖 full 状态、部分/失败状态、文件为空、元数据缺失、越界链接；JS 覆盖离线、部分缓存、仅本次 full 通知放行、opening/partial 通知拦截及后台不抢占。设备 LuaJIT 合成续传通过，加入旧 fullFile 不能绕过 partial/失败状态用例。设备 reader-ink 七阶段通过；这些不是用户真实书籍的整本联网下载端到端验收，未为测试额外下载用户书籍。

仅备份并原子更新应用和 helper/lib/download_manager.lua，无在途下载任务，用户数据/系统配置保留；正常应用已重新启动。备份 rm_weread_qt.before-fullread-gate、download_manager.lua.before-fullread-gate。二进制 SHA-256：e464e8095429fc36fdd2c45f519f5096a4338602ac12cd397d02c41775198a94；下载器 SHA-256：d57d299443cfe5425dfa0ad00b658f83cee6de7163060b30ba55e1ffbab700bc。后续不要恢复“partial 本地路径即可读”或“opening 完成即进入正文”的旧源码断言。

### 2026-10-21 边距胶囊统一、固定两字缩进与底部安全区（已部署）

用户要求边距与行距/段距完全相同的四列胶囊，删除首行设置并固定两字，修复底部按钮裁切。margin 控件现宽 132、基准高 56、间距 12、radius=height/2，边距行上下留白也匹配另两行。删除首行控件、笔命中和 Settings 别名，readerFirstLineIndentChars 改为 readonly 2；移除自检/恢复的赋值，旧偏好值不能覆盖两字，旧磁盘键不必删除。

底部原 y+height 等于面板高度，圆角/边框被 clip 边界截断。重分配去掉的首行空间和边距行留白：基高 906，原坐标 >=548 减 26，仍保留进度/网络附加间隔；底部增加 24 基准像素，实际至少 20 的安全留白。笔反向映射同步更新。不要把先前 908 基高或 indent 行的坐标重新加回来。

完整 Linux 检查、shell 语法、官方 SDK 构建通过；逻辑测试覆盖胶囊规格、固定缩进不被覆盖、扩大控件笔命中和底部边界。设备 reader-ink 七阶段、滑动、进度跳转、宽边距单选黑底与 footer 安全边界均通过；实际合成画面已核对三行四列胶囊一致、无首行行、底部四个按钮完整且留白。账号/用户书籍/偏好其他项不变，旧边距值保留。未做真实手写笔物理坐标验收。

已备份并原子更新单个应用可执行文件，备份 rm_weread_qt.before-margin-pills；正常应用重新打开。二进制 SHA-256：50f0701f5ab0950dde88dacdc19ecac8710a747530682cbc5bddf35853c49326。同步协议没有新增变化，手机进度问题仍未归因。

### 2026-10-21 目录去掉整本下载、同步归因澄清（已部署）

用户质疑“零章节字段导致手机进度不更新”的确定性。当前确认的是 Qt 未传真实章节位置、helper 的兼容路径置零，以及一次真实零时长调用返回 succ=1。尚无受控的上传前后和手机回读实验，不能宣称这是已证实的唯一根因，更不能将请求成功当作手机位置采用成功。本轮只读检查最近本地记录和 Web/gateway 进度快照：云端两者百分比一致，但与最近本地不同；缺少同期快照，不能据此判断字段语义、同步顺序或手机冲突。没有再次写云进度或上报时长，没有改同步协议。

按用户明确要求删除 readerCatalogPanel 的“下载整本/下载中”按钮及点击入口，回到书架按钮扩至该行宽度。书籍详情等其他下载入口保留，目录列表与跳转不改。原有 scaffold 契约调整为目录不得出现整本下载操作。完整 Linux 检查、shell 语法和官方 SDK 构建通过。仅备份并更新应用程序，备份 rm_weread_qt.before-catalog-download-removal，未修改 helper/系统或用户数据。二进制 SHA-256：b4c28cc152f48e91c84328a701091952d6a367e14271a1382939e3617bd6081f。

设备 reader-ink 七阶段通过，退出恢复系统 active、NRestarts=0，正常应用已重新打开。目录按钮删除由源码契约与构建验证，本次未导出正常用户目录画面。

### 2026-10-21 设置面板同排加高与章节预览（已部署）

用户仍报告边距选中不明显，后续要求整个面板加高、行距/段距标题与选项同排，以及拖动进度时显示目标章节。确认实际运行文件哈希与上一版一致，正常保存边距为 64。找到另一差异：边距和首行的手指 onClicked 直接同步 rebuildReaderPagination，笔和其他选项则延迟局部重排。现手指统一 applyReaderSettingChange(true)，重排定时器保留原 textOffset、仅预排目标两页，先让选中状态显示。

面板上限 1160（原 840），readerSettingsY/H 按高度伸展控件和留白，不拉伸文字；标签和选项同排居中。基础坐标 744 后新增 44 的章节预览空间，788 后累计新增 68（包含网络行额外留白），总基高 908。笔点击反向映射同一坐标，字体选择面板仍保持自身坐标。右下关闭手势收窄为 24px 空白边缘，避开控件。后续改布局不能只改视觉坐标而遗漏笔命中。

进度拖动只改百分比预览，readerProgressPreviewTitle 按缓存正文长度和已加载章节 textStart 二分查找标题，不联网、不分页、不改变当前位置；松手沿用实际跳转。目录为空显示暂无章节信息，不猜测章节。

完整 Linux 检查、shell 语法、官方 SDK 构建和宿主行为测试通过。设备 reader-ink 七阶段通过；实际 Qt 点击“宽”后数值 104、单项 selected、黑色背景及其他档未选中均通过（reader-margin-selection-selftest=ok），进度触区跳转和书架/正文滑动通过。合成设置画面已核对：行距/段距同排、宽边距黑底白字、章节名 Ink probe、加高后所有功能完整显示。笔命中扩大后控件的逻辑行为测试通过，非真实笔坐标验收。用户实际书籍体验仍待确认。

已备份并原子更新应用，无在途下载任务，helper 和系统配置未改；正常应用已重新打开。二进制 SHA-256：605b639e85c4f601c910ff0c816c0aa9a897a882ccc004f20b84805a1c192140。旧程序备份 rm_weread_qt.before-settings-layout，账号、书籍和原偏好保留；截图仅为隔离合成书，不入库。

### 2026-10-21 进度跳转与统一选中样式（已部署）

进度条原手指触区仅 12px，拖动每个位置事件都调用同步排版且跳转后设置面板仍遮挡正文。现在触区 48px，拖动只预览百分比，松手一次提交；笔触路径共用 requestReaderProgressJump，先关闭面板、抑制 600ms 翻页穿透，再用 50ms 单次定时器检查当前书籍/界面后跳转。重复请求覆盖目标。goToReaderTextOffset 的目标窗口从 12 页改为 2 页，继续用原有空闲逐页预排；不是预排整本或从书头扫描到目标。

页边距、首行、字重与字体选择卡片改成和行距/段距一致的黑底白字。页边距以窄/标准/宽/特宽文字显示，未改变设置值或字体加载逻辑。旧默认边距 64 不等于菜单标准 72，保持实际值不变，按最近档显示标准选中。灯光和顶栏快捷灯光也按最近档单选，避免阈值范围相交造成双选；关灯时使用 0。新增 validate-reader-progress-jump.mjs 逻辑行为测试；exit-ui 合成正文增加真实 Qt 指针在可视条上方 10px 处点击，检查面板关闭和目标偏移，日志 reader-progress-jump-selftest=ok。

完整 Linux 检查、shell 语法与官方 SDK 构建通过；设备合成翻页/笔迹七阶段和滑动回归通过，修正自检坐标映射后实际跳转回归通过。QQuickWindow 不能直接当作 mapToItem 的 Item 参数，使用 root.contentItem；注入 C++ int 坐标时先 Math.round，并按实际命中坐标计算预期偏移。不要把测试入口的早期类型异常写成产品点击成功。当前二进制 SHA-256：081406c9ed65f75d77744878fcf9583594cb28e79244dbebb04cd156444781be。用户实际手指手感及真实大书耗时仍待验收，不能承诺与原生固定同速。

最终设置面板合成截图已视觉核对：标准边距、两字首行、加黑均为黑底白字，灯光仅关档高亮；截图未入库。最终扩展触区实际 Qt 点击、目标偏移、滑动和退出提示通过，正常应用已重新打开。字体卡片样式已实现和构建检查，但本次截图是主设置面板，不冒充卡片截图验收。

仅更新应用可执行文件，旧程序保留 rm_weread_qt.before-progress-ui，后续自检候选保留额外备份；用户数据和系统配置不变。自检图片只能使用隔离合成书画面，不获取正常用户阅读屏幕。

### 2026-10-21 历史时长提醒不再拼接到同步结果（已部署）

用户要求去掉反复显示的历史“部分阅读时长提交结果未确认”。根因是 Main.qml 阅读设置区只要 hasUncertainSubmission 为真，就在每次 ProgressSyncStore.statusText 后追加整个账户的账本提醒。现在该处仅显示本次进度同步/拉取状态；新失败和超时仍显示，历史 uncertain 账本完全保留，不标为成功、不清除、不重报，阅读统计不变。本次是显示调整，不是账本记录迁移。

完整 Linux 检查、shell 语法及官方 chiappa SDK 构建通过。备份并原子升级单个应用可执行文件，短暂退出应用后 reader-ink 七阶段隔离回归通过，系统恢复 active、NRestarts=0，随后正常应用重新启动且服务 active。无在途下载任务；helper 及系统配置不改。新二进制 SHA-256：8b8ec2b670f8dcddb7e1b8c99ca0a3b0b96c09035abbde113db6d4ce55e537ea。用户实际打开设置面板的提示验收待确认；未导出用户画面或正文。

### 2026-10-21 进度同步成功误判（已部署）

用户报告每次手动同步均显示失败。只读诊断确认网页获取、状态解析、载荷构造正常。用户明确授权一次真实进度写入后，用最近保存的实际百分比、零阅读时长执行测试，服务端返回数值 succ=1、不含 errCode；旧 ReadSyncProtocol 只承认 errCode=0，误判为未确认。不能将这一成功响应解释为手机端精确章节位置已验收；百分比兼容路径仍发送零章节字段，此次未改位置协议或读取用户正文。

read_sync_protocol.lua 现在认可 succ=1/true 与旧 errCode=0；非零 errCode、明确拒绝、未知 succ 和未知响应仍不确认。时长账本、超时处理、失败不重放规则不变。新增真实 helper 配假客户端的响应到退出状态集成测试，覆盖成功/拒绝/未知响应与零时长、中文摘要边界。宿主 Lua、设备 LuaJIT、完整 Linux 检查、shell 语法及官方 SDK 增量构建通过。

仅原子升级 helper/lib/read_sync_protocol.lua，备份 read_sync_protocol.lua.before-web-ack-fix；安装后在设备重跑协议和 helper 假网络响应测试通过，无需重启。没有再次写真实进度，也没有重发历史待确认时长；下一次 helper 生效。真实用户点击后的界面提示和手机端进度仍待验收。诊断不输出凭据、书名、正文或原始响应。

### 2026-10-21 EPUB 兼容性修复（已部署）

用户提供的本地 EPUB 严格检查发现 XHTML 未声明的 `&nbsp;`，以及内容相同的重复 ZIP 图片成员和 OPF href；用户确认手动规范实体并去重后的副本可在原系统打开。ZIP CRC 正常不等于 EPUB/XML 合法，不要再以 unzip -t 代替章节 XML 验证。

Qt 当前 DownloadManager 的 finish_full_download（完整/部分）和 finish_opening_download 在调用外部 Content.save_book_epub 前，经过自有 epub_compatibility.lua：复制正文输入，将 &nbsp; 转成等价 &#160;；按 href 对资源去重，仅允许媒体类型和实际字节一致的重复。同名不同内容报错，避免静默丢图；缓存正文和校验清单不变，续传读回的旧正文也在打包前规范。当前未支持 workspace_text_dir，明确拒绝而非跳过。旧 download_full 同步原型路径未改，不要宣称所有上游插件打包入口都已修复。

完整 Linux 测试、17 个 shell 语法检查、官方 chiappa SDK 增量构建通过。宿主 Lua 及设备 LuaJIT 合成行为测试通过；设备实际 Content/ZIP 写入集成覆盖 full、partial、opening，逐成员 libxml2 严格解析、OPF 单一图片 href、ZIP 唯一路径、CRC 与原始图片字节通过，合成续传回归通过。未将用户正文或书籍文件入库。用户确认的是此前手动修复副本；新自动生成文件尚待实际原系统导入验收，未改变 UI，无新增正文页面渲染验收。

仅原子升级 helper/lib/epub_compatibility.lua 和 download_manager.lua；后者备份为 download_manager.lua.before-epub-compatibility。无需重启，后续 helper 进程生效；现有已完成 EPUB 不自动迁移，命中 fullFile 缓存会继续返回旧文件，不能让用户误以为再次普通点击下载必然重打包。设备 Xochitl active、NRestarts=0；账号、已有下载、进度和中文系统保留。

后续另一份本地样本发现空 media-type，且同一 href 的重复清单项分别为空与 image/png；该文件仅在仓库外手动修复。当前自动兼容层要求重复资源 media_type 相同，尚未处理这种情况；后续应增加独立合成用例再实现类型识别，不能宣称已全部覆盖。字体讨论没有新增代码或系统修改。

### 2026-10-21 下载 HTTPS 连接复用（已部署）

仅 DownloadManager 的 Lua 客户端新增 persistent_http.lua，使用设备现有 libcurl，每个任务复用连接/DNS/TLS，会话选项逐请求重置；旧客户端仍处理认证、业务错误、跨来源重定向与有界重试。保留图片、章节缓存和原有 30 秒阅读状态缓存，没有增加并发。TLS 证书与主机名验证开启，设备需指定标准 ECDHE/AES-GCM 套件；不要通过关闭验证来解决握手兼容性。缺少库或 RM_WEREAD_PERSISTENT_HTTP=0 时在发请求前沿用旧传输，已发送请求失败不做隐式传输切换重试。

同组 8 章原方式 66.966 / 64.139 秒，新方式 22.070 / 22.195 秒；新方式正文、CSS、图片均与基准一致。最后一轮原方式请求失败，未计入性能样本。临时目录 8 章实际流程验证中断后复用已完成 2 章且无需 HTTP，最终失败 0、EPUB 完整性通过；临时内容已删除。现有完整检查、shell 语法、官方 SDK 构建、设备虚拟 HTTP 行为与合成续传/状态缓存检查通过。不得宣称所有整本书固定快三倍。

仅原子升级 helper/lib 两个 Lua 文件，无需重启 App，新任务生效；旧 download_manager.lua.before-persistent-http 可回退。安装时用户 App 已退出，系统界面 active、NRestarts=0，未强行重新打开；账号和用户缓存保留。详见 docs/download-connection-reuse-2026-10-21.md。

### 2026-10-21 完整下载封面角标

用户要求在书架已完整下载的封面右下角显示完成标记。Main.qml 的 coverDownloadBadge 为白底黑字“✓ 已下载”，图片之上 z=1、右下间距 8；只有 downloadState=full 且 localEpubPath 非空时显示，partial/remote 不显示。角标无输入处理器，沿用整本完成时 shelfStore.reload 的自动刷新。

现有完整 Linux 检查、shell 语法和官方 SDK 构建通过，应用文件已升级，设备 reader-ink 隔离回归成功。正常应用已重新打开；角标在用户实际封面上的视觉验收待用户确认，未读取或导出用户封面截图。账号、下载内容和阅读记录保留。

### 2026-10-21 用户验收手势，移除书架按钮

用户明确确认手势“很好”，要求移除书架上一页/下一页。已删除可见按钮，保留页码和手势，封面网格恢复 y=142 / height=root.height-232。exit-ui 自检改为用真实 Qt 注入滑动逐页遍历 195 本虚拟书的 22 页；shelf-swipe、shelf-pagination、reader-swipe 均 ok，exit-ui shown，系统恢复 active、NRestarts=0。完整 Linux 检查、shell 语法、官方 SDK 构建、设备 reader-ink 隔离回归通过；已更新并重新打开正常应用，账号和下载内容保留。

用户同时询问下载提速。此次只读检查本项目下载器与设备实际插件程序源码：仍逐章串行，EPUB 正文一般依次请求 e_0/e_1/e_3，CSS/资源另取，已有 30 秒阅读状态缓存。未改下载协议、并发、图片或缓存；没有新的实际提速测量。连接复用及少量并发是待验证方向，不能把删除按钮或历史样本 42% 改进作为本轮新的提速承诺。

### 2026-10-21 Move 分支：书架与正文滑动修复

用户报告书架手势完全无效、正文手势不灵敏。书架原代码读取 Qt 6.10 没有的 translation 属性；独立设备探针还确认 activeTranslation 在释放的 onActiveChanged 回调里已归零（不要根据较新文档假定仍保留）。最终在活动期间缓存最后位移，释放时判定，并将封面 TapHandler 改为 DragThreshold，拖动取消开书。

PageGesture.js 统一方向判定：水平至少 36 逻辑像素且水平位移至少为垂直的 1.15 倍；正文滑动在左右半屏均有效，不再按起手半屏限制方向。轻触抖动 20 像素以内仍按左右半屏翻页；保留模态面板、目录、评论和笔触抑制守卫。

完整 Linux 测试、shell 语法、官方 SDK 构建通过，已部署。设备 reader-ink 七阶段通过；exit-ui 临时生成 195 本虚拟书和合成正文，注入真实 Qt 鼠标 press/move/release 事件：封面区域左右滑动、22 页按钮分页、正文左半屏左滑及右半屏右滑均输出 ok；退出提示 shown，系统恢复 active 且 NRestarts=0。早期未缓存位移的候选版确实在该输入测试失败，已替换。注入入口仅允许 exit-ui，不能在正常阅读中调用。自动输入不是用户手指触摸验收，实际手感仍待用户确认。账号和下载内容保留。

### 2026-10-21 Move 分支：退出提示与书架分页（已设备更新）

源码新增退出遮罩，等待 frameSwapped 后再退出，2500 ms 兜底退出；自检模式默认直接退出，仅 exit-ui 测试真实提示。书架每页仍为 9 本，新增上一页/下一页可见按钮，首尾禁用，保留左右滑动和边缘点击。网格上边由 142 移至 180，为分页按钮留空间，底边不变。

exit-ui 自检在临时数据目录生成 195 本虚拟书，跳过账号辅助进程，调用真实分页按钮处理器，检查 22 页与末页 6 本，然后检查退出首帧。完整 Linux 源码检查和官方 SDK 编译通过。USB 恢复后已升级应用文件；设备 reader-ink 隔离回归通过，exit-ui 输出 shelf-pagination-selftest=ok 和 exit-ui-selftest=shown。静态退出提示截图中文字与布局已核查；退出后 Xochitl active、NRestarts=0。正常应用已重新启动。上述虚拟书架按钮处理器测试不能代替用户真实触摸验收。账号、下载内容和阅读记录保留。

### 2026-10-21 Move 分支：启动反馈

正常 AppLoad 会话在停止系统界面后先运行独立 `--startup-splash`，显示“正在启动 REweread… / 请稍候”，首帧后退出，随后完整应用接管。该分支绕过账号存储和 Main.qml；其他隔离自检不增加此画面。启动反馈不等于完整启动提速。

官方 SDK 构建、完整 Linux 测试通过；设备静态画面自检通过，Qt 应用构造完成后首帧约 3246 ms（不含 AppLoad/系统切换），自身静态预览中文字正确。reader-ink 七阶段隔离回归通过，退出恢复系统且 NRestarts=0，正常应用再次启动成功。未删除账号、下载内容或阅读记录。尚需用户实际从 AppLoad 点击确认体验。

## 1. 一句话现状

REweread 当前源码里程碑为 `3.0.1`：在 2.0 魔法笔记本、百度 OCR / DeepSeek 分离配置基础上，加入本机阅读统计、同步账本、个人云笔记缓存、离线章节校验续传、字体预览、图文分页及新系统启动保护。验证范围与未完成项见 `docs/releases/v3.0.1.md`，升级必读 `docs/agent-upgrade-v3.0.md`。源码更新不代表全部功能通过验收，也不附公开二进制。

它仍不是可面向普通用户广泛分发的正式产品。当前存在服务授权、上游许可证、干净设备安装/卸载和长期电池测试等阻塞项。

3.0.1 归档启动与下载性能修复、业务登录错误提示和后台下载导航修复。性能验收二进制的版本字段更新前为 3.0.0；本次同步的是 3.0.1 源码，不重新部署设备。

### 2026-10-13 启动与下载性能后续修复

构建默认已改为 chiappa SDK 5.8.203 / Qt 6.10.3，与设备一致。旧 Qt 6.8 预编译单元失效及现场编译的问题已通过匹配 SDK 消除，会话脚本也显式设置可写 QML 缓存路径。空缓存首次与重复启动的 Qt 首帧分别为 4957 / 4903 ms（从 main 开始，系统切换另计），均无 QML 缓存不匹配错误；不要再将初步缓存修复的 18.6 秒作为当前结果。

下载客户端短时复用同一本书的阅读状态，30 秒过期，章节失败立即失效并在既有有界重试中刷新。8 个真实章节耗时 22.0 → 12.8 秒，正文/CSS/图片逐字节一致；临时目录整本下载 106/106、失败 0、184 秒。后台下载完成不再强制打开书，只响应当前明确打开意图及当前书的续传。真实 Main/ReaderStore reader-ink 隔离回归通过，退出恢复系统且 NRestarts=0。

最终部署二进制 SHA-256：`0ef9170e0fac804edbf4d28a849376da831c5dc35fe79859bead0abc92a9000e`。系统、XOVI、AppLoad、字体、账号及用户内容保留；旧应用有时间戳备份。详见 [性能排查与最终修复](performance-investigation-2026-10-13.md)。本轮没有切换官方原生下载协议，没有公开发布。

### 2026-10-12 同步与离线下载开发增量

基于 main `0271ab2` 补充了阅读时长账号账本、个人云笔记离线缓存、手动云进度正文锚点定位、章节校验续传和 partial EPUB 状态。详细已完成项与协议限制见 [官方客户端接口与同步能力补齐](official-client-sync-gap.md)。本轮增量归入 3.0.0 源码里程碑。

后续增量包含“我的 → 阅读统计”（独立每日统计、北京时间、账号代次防串号）和阅读字体预览网格。统计仅从更新后的本机实际阅读开始，不代表云端历史总时长。设备自检不得计时/上传/保存阅读位置；标注自检已禁用对真实笔记的破坏性写入。现有安装的应用文件升级可用 `scripts/upgrade-weread-qt-files.sh`，无需重写 XOVI 系统配置。

本轮通过官方 chiappa SDK 构建、全套源码验证、本机 Qt 账本行为测试，以及设备 LuaJIT 的隔离下载续传测试。隔离测试不访问真实账号/书籍；它不能替代完整 UI、线上同步或安装升级验收。不要把这些结果写成已部署或已验证真实账号上传。

真机结果：统计页和字体面板已显示并检查截图，字体笔点击通过；云端进度、笔记列表及单本笔记只读调用成功，未测试真实账号写入。已修复旧 session 非 UTF-8 字节导致的 Qt 账号范围识别失败，且不写回 session。完整 52 组合排版自检尚未完成。后续系统升级和 USB 连接已恢复，详情如下；不要再依据历史 WLAN 超时判断当前部署失败。

关键约束：仅手动“同步进度”提交持久时长；结果不明的请求保留 uncertainty，不能无条件重放。云笔记当前只读取和缓存，本机手写不会自动发布。精确上传所需远端 chapterOffset 编码尚未验证，四参上传仍是百分比兼容模式。

### OTA / AppLoad 恢复事故与本地保护

**后续授权执行已完成**：官方更新服务升级至 3.28.0.172，隔离重建映射（20231 条），AppLoad v0.6.0 独立试运行并由用户确认入口后启用 guarded ExecStart；重启后仍在 3.28.0.172，AppLoad 自动加载，根分区只读，无 Xochitl 崩溃重启。应用适配了移除 EPContentType 参数的新刷新 ABI，direct ink 小区域刷新恢复测试、字体面板、图文正文和统计页自检通过；已有用户数据文件校验全部一致。最终应用 SHA-256 为 `62aeaaea2e765a5823cc8cf7756ab97b62fe01644bb685ef62ee4c0b2fb3f80d`。下面的“仅本地/未部署”描述保留为早期审查背景，不代表当前部署状态。详情见恢复文档。

后续只读日志确认：在 3.28.0.172 补回直接 XOVI 注入后，旧 3.27.3.0 hashtab 被拒绝，AppLoad hook 解析 panic 导致 Xochitl 连续 ABRT、数轮启动失败，最终回到 3.27.3.0。120 秒临时回退定时器随重启消失，不能提供启动保护。当前旧系统可以加载 AppLoad；不能据此认定新系统兼容。

当前 AppLoad v0.6.0 和目标系统 hashtab 已部署，并在用户确认隔离试运行后启用启动保护；失败时回到 stock。上游版本不能跨系统盲目套用，见 [XOVI / AppLoad 兼容性与恢复](xovi-appload-recovery.md)。应用文件升级与系统恢复分开，不能直接调用旧恢复脚本试错。

## 2. 接手后的第一组动作

3.0 发布准备期间修复了自由笔迹翻页残留和橡皮只擦高亮的问题。`InkCanvasItem` 为直写层保存有界原始像素并在清除/离页/Qt 接管前恢复；`ReaderStore::erasePageStrokesAtPath` 删除当前书当前页命中的整笔。设备 `ink-lifecycle` 独立空白页自检已通过，但用户真实页连续书写复测仍待确认。具体构建校验和见 `docs/releases/v3.0.0.md`。

**2026-10-13 用户复测未通过**：真实阅读页仍有残留或擦除问题，不能继续宣称 3.0 已解决。运行中二进制已核对为本轮最终构建，并非启动了旧程序。后续发现 `strokeBounds` 用零尺寸矩形求并集导致脏区域退化，以及部分分页路径未同步笔迹可见页；正在补真实 Main 阅读器隔离场景，之前空白画布自检不足以证明正文页正常。

**后续修复与设备回归**：独立 QML 行为测试确认，context property 并不是 Window 属性，旧 `root.selfTestMode` 实际为 `undefined`，导致正常阅读时翻页载入笔迹被守卫跳过。已改为 `runtimeSelfTestMode` context 加显式根属性，并统一各分页路径的笔迹激活；旧页延迟保存不再切换可见笔迹模型。另修复完整笔画脏区域计算，保留有界直写背景直到明确清除或换页，而非仅凭延时认定 Qt 已接管。新增 `reader-ink` 真 Main/ReaderStore 回归：临时合成中文正文、三笔保存、翻页返回、整笔橡皮、清页像素还原、分页窗口重建，连续两轮在设备上全部通过，退出后 Xochitl 正常运行且 NRestarts=0。测试隔离数据、会话、设置及 OCR/AI 配置，跳过账户 helper；只记录合成测试结果，不记录用户内容。部署构建 SHA-256：`ef4df20c2e616a7126fa13c7e7f7c7481c1f570e178c0d35edc6706672f18b78`。自动回归不等于真实笔交互验收；用户复测仍待确认。现有自由笔迹的 pageIndex 存储也不等于跨字号重排的正文锚点方案，不要宣称已解决后者。

**2026-10-13 最终用户验收**：在上述修复版部署及两轮回归之后，用户明确回复“都解决了”，确认翻页残留、橡皮和清空问题均已解决。此前“仍待确认/未通过”描述为修复过程的历史记录，以本条为当前验收状态；该确认不扩展到跨字号重排锚定或长期稳定性。此次 GitHub 同步只提交源码、测试与文档，不包含设备数据或二进制。

只在一个干净 Git checkout 中工作。先执行：

```bash
git status --short
git branch --show-current
git log -5 --oneline --decorate
node scripts/check-repository.mjs
node tests/run-all.mjs
for file in scripts/*.sh apps/weread-move/*.sh; do bash -n "$file"; done
```

预期：

- 基线来自 `main`。
- `VERSION` 与 `release-manifest.json`、`CHANGELOG.md` 和当前 `docs/releases/v<version>.md` 一致，当前为 `3.0.1`。
- 安全检查和静态验证全部通过。
- 不存在 SDK、字体、构建目录、第三方 checkout 或用户数据的待提交文件。

如果 `git status` 报 `bad tree object`、`packfile ... far too short` 或长期卡住，不要继续 commit、gc、repack 或删除 `.git/objects`。在磁盘空间正常后重新 clone，再把确实需要保留的未提交源码逐文件比较迁移。不要用一个新的 `.git` 目录直接覆盖旧工作区，因为旧工作区可能有未提交内容和忽略目录。

## 3. 当前 Git 与发布状态

关键公开提交：

| 提交 | 含义 |
| --- | --- |
| `11322fa` | PR #1 合并到 `main` |
| `a515c45` | `1.0.0-rc.1` 源码发布准备 |
| `a27145a` | 官方 systemd 深度休眠、VPDD 等待和重试 |
| `d2b9278` | 休眠前断开 Wi-Fi、唤醒后恢复 |

发布状态：

- `3.0.1` 是当前 source-only 源码里程碑；提交源码不等于发布普通用户二进制。
- GitHub PR #1 已合并，保留 `1.0.0-rc.1` 的历史记录。
- `v1.0.0-rc.1` 是 Draft prerelease 名称，不是公开标签。
- 未创建公开 `v3.0.1` 标签或 Release；不要因为 `VERSION` 已更新就自动发布。Draft 可用于源码审查，不得误称正式发布。
- Draft 中只附源码归档和 `SHA256SUMS.txt`。
- 源码归档 SHA-256：`2eaf1c08db968606ac539edc2247e1c3aad267cdb3ca6ab9538b323fd1e571d8`。
- 不得擅自把 Draft 点成 Publish，也不得附加应用二进制或依赖包。

检查当前远端状态：

```bash
gh pr view 1 --repo nasonliu/REweread
gh release view v1.0.0-rc.1 --repo nasonliu/REweread
git ls-remote --tags origin refs/tags/v1.0.0-rc.1
```

最后一条目前应为空。若出现公开标签，先确认是谁发布以及是否完成了 release checklist。

## 4. 当前产品架构

不要把三代实现混在一起：

1. `apps/weread-qt/`：当前产品。Qt 6 / Qt Quick UI、阅读器和设备桥接。
2. `apps/weread-move/lib/` 与 `apps/weread-move/tools/`：当前 Qt 应用仍在调用的 Lua 辅助层。
3. `apps/weread-move/views/`、`native_app.lua`、`app.lua`：历史原型，不是当前 UI。

实际运行链：

```text
Main.qml
  -> C++ stores
  -> QProcess
  -> KOReader 提供的 LuaJIT
  -> apps/weread-move/tools/*.lua
  -> 用户自行安装的 weread.koplugin 模块
  -> WeRead Skill/Web 接口
  -> /home/root/.local/share/rm-weread/
```

因此“独立 APP”只表示 UI 和启动生命周期独立，不表示已经摆脱 KOReader LuaJIT 或 `weread.koplugin`。不要在文档或发布说明里写成完全独立运行时。

## 5. 源码导航

| 要改什么 | 先看哪里 |
| --- | --- |
| 页面/UI/手势/弹窗 | `apps/weread-qt/Main.qml` |
| 评论范围纯映射 | `apps/weread-qt/SocialAnchor.js` |
| 正文、分页、位置 | `reader_store.*` 和 `Main.qml` reader helpers |
| 下载和章节打开 | `download_store.*`、`apps/weread-move/lib/download_manager.lua` |
| 评论与缓存 | `notes_store.*`、`fetch-notes.lua` |
| 登录和退出 | `account_store.*`、`login-qr.lua`、`logout.lua` |
| 云端进度 | `progress_sync_store.*`、`fetch-progress.lua`、`sync-progress.lua` |
| 手写笔 | `stylus_store.*` 和 QML pen palette/hit layers |
| 快速墨迹 | `ink_canvas_item.*`、`direct_ink_framebuffer.*` |
| 拼音与手写输入 | `PinyinEngine.js`、`PinyinLexicon.js`、`Main.qml` keyboard helpers |
| 百度 OCR | `ocr_store.*`、`ocr_setup_server.*`、`docs/baidu-ocr-configuration-flow.md` |
| 自由笔迹锚点 | `FreeNoteAnchor.js`、`reader_store.*`、`Main.qml` |
| 电源/磁吸套 | `power_store.*` |
| Wi-Fi | `network_store.*` |
| 前灯 | `frontlight_store.*` |
| AppLoad 安装 | `scripts/install-weread-qt-appload.sh` |
| XOVI 持久化 | `scripts/install-xovi-autostart.sh` |
| 会话和系统恢复 | `scripts/weread-qt-session.sh` |
| 发布元数据 | `VERSION`、`CHANGELOG.md`、`release-manifest.json` |

`Main.qml` 超过八千行。可以规划组件化，但不要把一次行为修复和大规模拆文件绑在同一个提交里。静态测试大量依赖源码契约，重构前先找到对应 validator。

## 6. 设备端目录边界

```text
/home/root/weread-qt/                              当前应用
/home/root/weread-qt.previous/                     最近一次原子升级回退
/home/root/xovi/exthome/appload/weread-move/        AppLoad 入口
/home/root/xovi/exthome/appload/weread-move.previous/ AppLoad 回退
/home/root/.local/share/rm-weread/                  用户账号、书籍和缓存
/home/root/xovi/exthome/appload/koreader/           用户安装的运行时
/usr/lib/systemd/system/xochitl.service.d/99-xovi-appload.conf
```

前三类程序文件可以在明确任务中替换。用户数据目录不得删除、打包、打印或上传。卸载脚本默认保留数据；只有用户明确要求，并提供脚本要求的完整确认短语时才允许删除。

不要假定 Wi-Fi 地址固定。默认 USB SSH 是 `root@10.11.99.1`；WLAN SSH 必须由用户先明确开启，再通过 `MOVE_HOST` 传入当前地址。

## 7. 标准开发与部署

本地：

```bash
./scripts/bootstrap-remarkable-sdk.sh
./scripts/download-reader-fonts.sh
./scripts/build-weread-qt.sh
```

部署前再次运行安全检查和测试。正式安装/升级：

```bash
MOVE_HOST=root@DEVICE_ADDRESS ./scripts/install-weread-qt-appload.sh
```

安装器会：

1. 检查架构、XOVI、AppLoad、KOReader 和插件模块。
2. 上传到 `/home/root/weread-qt.installing`。
3. 停止旧应用。
4. 原子切换应用和 AppLoad 目录。
5. 保留 `.previous` 回退目录。
6. 后续失败时自动恢复旧目录和 Xochitl。
7. 重写持久化 XOVI drop-in；这一步会短暂把根分区重挂载为可写，必须提前告知用户。

卸载预演：

```bash
MOVE_HOST=root@DEVICE_ADDRESS DRY_RUN=1 ./scripts/uninstall-weread-qt-appload.sh
```

不要在用户的唯一设备上为了“测试卸载”执行真实卸载。当前候选版只完成了 dry-run，真实卸载应留给干净或备用设备验证。

## 8. 真机验证标准

本地测试只能证明源码契约。以下项目必须在真实 `954 x 1696` 设备上验证：

- 第一次点击 AppLoad 图标能否进入。
- 书架是否是九宫格彩色封面，左右滑动是否换页。
- 详情页是否按当前进度快速进入章节，而不是阻塞下载整本。
- 正文页而非版权页的中文缩进、分页、图片和图注。
- 修改字号/间距后是否保持 `textOffset`。
- 评论虚线是否与正文对齐，点击后是否弹窗且不翻页。
- 笔和手触是否正确分流，最终高亮是否吸附到文本。
- 退出是否恢复 Xochitl。
- 电源键和磁吸套是否真正进入深度休眠。

不要只看截图第一屏，不要用版权页判断分段，不要用静态 validator 宣称手写笔、休眠或评论交互已经在设备上通过。

## 9. 深度休眠：最容易踩坑的部分

### 正确路线

必须调用官方 `systemctl suspend`。不要直接写 `/sys/power/state`，因为那会绕过官方 sleep hooks：

- 配置电源键和 Folio wake source。
- 关闭并卸载 Wi-Fi/BT 驱动。
- 调整 regulator。
- 唤醒后恢复驱动和运行时 wake source。

应用自己的 `rm-weread-qt` wake lock 只负责阻止阅读期间 autosleep，不等于完整系统休眠流程。

### VPDD 保护窗口

彩色电子纸刷新后，G2194 的 VPDD 有最长约 30 秒保护窗口。设备公开 `vpdd_timeout_ms` 和 `vpdd_length`，但 regulator 编号可能变化，所以代码按 `name == VPDD` 动态查找，不能硬编码 `regulator.13`。

典型失败：

```text
Can't suspend, vpdd timer running
Resource temporarily unavailable
```

处理规则：

1. 先显示休眠封面。
2. 保持应用 wake lock。
3. 等实时 `vpdd_timeout_ms` 归零。
4. 再释放 wake lock 并调用 `systemctl suspend`。
5. `systemctl` 返回 0 只表示任务被 systemd 接受，不表示内核成功休眠。
6. 延迟检查 `systemd-suspend.service`；失败时有界重试。

即使 VPDD 显示为 0，第一次 suspend 仍可能因瞬时 busy 失败；当前代码允许最多三次尝试。

### 测试方法

- 断开充电线，避免 `udev.charger` 干扰判断。
- 触发休眠后至少等 45 到 60 秒再唤醒。
- 用 kernel journal 确认 `PM: suspend entry (deep)` 和最终 wake IRQ。
- 合套后很快重新打开，只会取消 pending sleep；这不算深度休眠测试。
- Folio 唤醒时可能同时出现 power-key IRQ，应用层原因文本偶尔先显示“电源键”，以内核 SPLD 最终 wake reason 为准。

已经验证过一次电源键深睡保持数分钟，最终只由 power button 唤醒。磁吸套的快速开合与 Folio wake routing 已验证；长期合套电池测试仍应补做。

## 10. AppLoad/XOVI 的坑

- AppLoad 文件存在不代表图标会显示。先确认 Xochitl 进程环境中真的有 `LD_PRELOAD=/home/root/xovi/xovi.so`。
- `/etc` 下临时创建的自启动 service 可能在重启或系统更新后消失。
- 当前持久化入口是 `/usr/lib/systemd/system/xochitl.service.d/99-xovi-appload.conf`。
- 系统更新可能替换 `/usr/lib`，因此更新后必须重新检查兼容性和 drop-in。
- 不要同时保留“微信读书”和“微信读书·系统”两个入口；安装器会清理旧入口。
- 图标要点两次通常是旧进程、旧 transient unit 或 launcher 提前返回，不要用让用户多点一次来掩盖。

## 11. 登录和账号的坑

当前 QR 流程：

```text
GET /api/auth/getLoginUid
-> 展示 /web/confirm?uid=...
-> 轮询 /api/auth/getLoginInfo?uid=...&otp=
-> 合并 Set-Cookie 和 token
-> 0600 保存 session
```

- 不要恢复旧 `/web/login/getuid`、`getinfo` 或 `session/init` 路线。
- 手机显示登录成功但设备无反应时，先查 `Set-Cookie`；`wr_vid` 不一定在 JSON 中。
- 任何状态输出都不能包含 token、Cookie 或真实二维码 UID。
- 退出账号只删除活动凭据，缓存按文档保留。
- 当前缓存还没有完成严格的按账号隔离。切换账号时不要把旧账号私有书架、进度或评论当作新账号数据展示，这是 1.0 稳定版前的重要缺口。

## 12. 阅读、分页和进度的坑

- 缓存目录必须使用真实 WeRead `bookId`，否则会打开错书。
- 阅读位置必须持久化 `textOffset`。`pageIndex` 在改字号、字体、间距后不稳定。
- 正常正文页应尽量填满可用行，目标约 95%；章节末尾和图片边界可以留白。
- 中文段落分页使用“两边至少各留两行”的拆分规则，不要为了保持整段而制造大空白。
- 首行缩进只影响第一行，不能把每一行容量都按缩进扣减。
- 章节标题另起页、加大加黑；章节尾分隔、图片与图注都属于页面流。
- 图片不能固定在屏幕上只让文字翻页。
- 自检必须采样真实 `readerBodyText`，不要用封面、标题页或版权页。
- 自检不得覆盖用户真实设置或阅读进度。

## 13. 评论和注释的坑

社交评论范围是章节相对偏移，不是当前页像素坐标。正确链路：

```text
远端 plainStart/plainEnd
-> 与 pageStart/pageEnd 相交
-> markText/XHTML 文本锚定
-> 当前页本地文本位置
-> TextEdit.positionAt()/positionToRectangle()
-> 虚线几何和同一组 hit rect
```

- `count > 0, visible = 0`：数据已取回，页范围映射错了。
- `visible > 0` 但点不到：渲染层或事件层错了。
- 虚线和点击区域必须共享几何，不能一个用富文本下划线、另一个用估算坐标。
- `readerSocialTouchLayer` 必须位于翻页区域之上，并消费事件。
- 评论只在当前页停留约 3 秒后加载；翻走就取消或降级上一页请求。
- 优先读缓存，不要每次重复请求整本书评论。
- 等一个评论弹窗加载完成后再允许下一次请求，避免 QML model 重建和 helper 堆积导致假死。
- 注释链接和评论链接都不能同时触发翻页。

评论接口是未公开 Web surface，随时可能变化。遇到“手机有评论、设备没有”时，先确认数据获取、页范围、文本锚定、绘制、点击五层中的哪一层失败，不要一上来重写 API。

## 14. 手写笔和触摸的坑

至少分清三套坐标：原始笔设备坐标、窗口/QML 坐标、TextEdit 文档坐标。

- 文本吸附使用可见 `TextEdit.positionAt()` 和 `positionToRectangle()`。
- 不要用字体大小和行高粗略反推最终高亮位置。
- 笔专用标注控制拒绝手触；普通按钮按需求接受笔和手。
- 颜色胶囊、橡皮擦和评论/注释点击必须消费事件，不能顺带翻页。
- 社交评论虚线偏移和手写高亮偏移是两类问题，不能共用一个“坐标补偿值”。
- 跟手的临时黑色墨迹走 `direct_ink_framebuffer.*`：应用启动时在 QML 引擎之前捕获设备自带 `EPFramebuffer` 辅助缓冲，逐点只改帧缓冲并约每 8ms 合并一次单色局部刷新，不能同时调用逐点 `QQuickPaintedItem::update()`。
- 这条路径移植自 MIT 许可的 Quill 最小适配边界；仓库只保存源码适配和许可文本，不保存厂商 `libqsgepaper.so`。设备不支持时必须自动回退到 Qt `InkCanvas` 和 `EPScreenModeItem::Pen`。
- 当前 Move 真机启动状态应出现 `direct ink ready 960x1696 stride=3840`。QML 窗口与帧缓冲宽度可能不完全相等，绘制前必须按实际窗口尺寸缩放，不能硬编码 Paper Pro 的 `1620x2160`。
- 落笔期间优先保证黑色笔头连续跟手；停笔约 280ms 后再由持久化 Qt 图层一次性刷新为所选颜色。不要让彩色刷新排队阻塞实时单色更新。
- 页面笔迹比输入法多一条持久化链路。保存计时器必须在落笔期间停止，临时笔迹与已保存笔迹通过 `clientStrokeId` 去重；只改变时间、分组或 OCR 元数据时不能触发 `InkCanvas` 整层重建。
- 百度 OCR 只识别用户选择的相邻笔迹块，不识别整页。拼音、本地自由笔迹、荧光笔和符号笔记不依赖 OCR。
- Agent 指导 OCR 时先读 `docs/baidu-ocr-configuration-flow.md`；AppID 只用于核对应用，设备表单只填同一 OCR 应用的 API Key 与 Secret Key。真实凭据不得进入聊天、环境变量、命令或日志。

## 15. Docker、磁盘和构建的坑

- Apple Silicon 使用 `linux/arm64` Docker 容器和 `rm_chiappa_sdk` volume。
- `build-weread-qt.sh` 会删除旧 build 目录后全量构建；QML AOT 最后几步可能长时间停在 94%，要确认 Docker 容器是否仍在运行，不要立即启动第二个构建。
- Codex 工具可能先返回输出，但底层 Docker/SSH/SCP 进程仍在运行。检查进程或继续 poll，不能因为暂时没有新输出就当作完成。
- 磁盘写满曾导致 Docker ext4 只读、构建中断和 Git pack 损坏。优先删除生成的 `apps/weread-qt/build/`，完整退出并重开 Docker；未经用户同意不要 prune volumes、镜像或其他项目数据。
- SourceForge 字体下载可能很慢。不要绕过固定 SHA-256；可以复用本机已校验的忽略目录缓存，但不能提交字体。
- 设备 BusyBox 不接受某些 GNU 缩写参数，例如 `head -20`。使用 `head -n 20` 或 `sed -n '1,20p'`。
- 编译输出存在 Qt locale 警告时，先确认脚本已经导出 `LANG=C.UTF-8 LC_ALL=C.UTF-8`；不要把无关警告误判成链接失败。

## 16. 设备卡死时

不要连续点评论、翻页、注释和笔胶囊，这会让事件与 helper 请求继续排队。

先抓脱敏状态：

```bash
ssh "$MOVE_HOST" '
  pidof rm_weread_qt
  tail -80 /tmp/rm-weread-qt.err
  tail -80 /tmp/rm-weread-qt-session.log
  ps | grep -E "luajit|rm_weread_qt" | grep -v grep
'
```

设备是 BusyBox，命令选项要保守。原始 helper 输出可能包含书名、评论或账号信息，不能贴进公开 issue。

若应用进程已死而系统没有恢复：

```bash
ssh "$MOVE_HOST" '/home/root/xovi/start || systemctl start xochitl'
```

这是恢复手段，不是正常退出方案。正常路径必须由 `weread-qt-session.sh` 恢复 Xochitl。

## 17. 发布与法律边界

本项目自有代码采用 OSI 批准的 MIT License，可在保留许可证和版权声明的前提下使用、修改、分发及商业使用。该授权不覆盖微信读书服务、内容、账号数据、商标、第三方运行时或未明确许可的上游代码。

当前不能发布普通用户二进制，原因至少包括：

- 没有腾讯对微信读书接口、内容、评论和商标的书面许可。
- `weread.koplugin` 上游没有明确 LICENSE。
- 字体、QR 库、XOVI/AppLoad/KOReader 的再分发义务还要逐项审查。
- 干净设备安装/卸载和长期电池测试未完成。

已经删除了旧的 `package-weread-plugin.sh`，不要恢复，也不要把用户自行安装的插件打进发布包。

当前允许做的是：维护公开源码、生成 source-only 候选归档、保持 GitHub Draft。把 Draft 发布、创建公开标签或附加二进制前，必须重新逐项核对 `docs/release-checklist.md`。

## 18. 当前未完成事项

按优先级：

### P0：可靠性验收

- 在断开充电线的情况下做 24 到 48 小时深睡电量记录。
- 做一次磁吸套保持关闭超过 60 秒的受控深睡测试，确认只由 Folio 打开唤醒。
- 记录阅读中、pending VPDD、deep suspend 三种状态的电耗差异。
- 修正 Folio 与 power-key IRQ 同时到达时应用层“唤醒原因”偶尔误标的问题。

### P0：安装生命周期

- 在干净兼容设备上测试首次安装。
- 在备用或干净设备上真实执行默认保留数据的卸载，再验证 Xochitl 和 drop-in 恢复。
- 验证系统更新后恢复流程。

### P1：账号与内容层

- 完成缓存按账号隔离，避免切换账号时暴露旧账号私有数据。
- 降低对 KOReader LuaJIT 和无许可证 `weread.koplugin` 的依赖；替代实现必须先做权利链和接口风险审查。
- 继续确认阅读时长、评论、本地批注和云端进度哪些有稳定且允许使用的同步渠道。

### P1：阅读器回归

- 用多本含图片、图注、表格、注释和长正文的书做正文页矩阵测试，但测试数据不能进仓库。
- 重复评论打开/关闭压力测试。
- 检查每种受支持字体、字号、行距和段距组合下的 95% 页面填充。

### P2：代码结构

- 逐步把 `Main.qml` 拆成稳定组件，每次只迁移一个行为域并保留对应测试。
- 把更多静态源码契约升级为纯逻辑单元测试，减少只靠字符串断言的覆盖。
- 不要在行为修复中顺手重写整个 UI。

## 19. 不要重复走的弯路

- 不要把 Lua 原型当当前产品部署。
- 不要用 `pageIndex` 代替 `textOffset`。
- 不要直接套用远端评论 offset 画线。
- 不要把评论 hit layer 放在翻页层下面。
- 不要把图片固定在页面上。
- 不要在休眠时只释放 wake lock，而绕过官方 systemd hooks。
- 不要硬编码 regulator 编号。
- 不要因为 `systemctl suspend` 返回 0 就宣布休眠成功。
- 不要用版权页做中文段落验收。
- 不要让用户反复手测可以脚本化的点击压力问题。
- 不要删除用户数据来解决缓存或登录问题。
- 不要把 SDK、字体、插件、书籍或日志放进 GitHub Release。
- 不要把 source-only Draft 说成正式公开 1.0。

## 20. 新会话建议的开场回复

新 Agent 可以这样向用户确认：

> 我已经阅读 AGENTS.md、docs/agent-handoff.md、docs/agent-upgrade-v3.0.md 和当前版本发布说明。当前源码基线是 3.0.1 source-only 里程碑，没有公开二进制或公开 v3.0.1 标签。接下来先检查 Git、仓库安全和测试；已有可用 AppLoad 时只升级应用文件。不会读取或输出账号、书架、百度或 DeepSeek 凭据和原始日志；高影响系统操作前先说明并取得授权。

完成这些后，再根据用户的新目标进入实现，不要重新从旧 KOReader UI 或早期设计稿开始。
