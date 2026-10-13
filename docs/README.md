# Documentation index

- `quick-start-user-guide.md`: 面向普通用户的中文快速应用与手势指南。
- `agent-handoff.md`: 新 Agent 的当前状态、架构、已知坑、验证基线和下一步优先级。
- `agent-upgrade-v3.0.md`: Agent 安全升级到 3.0.1；应用文件升级与系统恢复分开，保留字体、数据和 API Key。
- `agent-upgrade-v2.0.md`: 2.0 历史升级记录，不覆盖当前升级流程。
- `performance-investigation-2026-10-13.md`: 启动与下载瓶颈、REA 分析、代码修复及真机性能验收。
- `releases/v3.0.1.md`: 3.0.1 源码维护版本的修复内容与验证范围。
- `official-client-sync-gap.md`: 阅读时长、笔记、进度与离线下载的实现及验证边界。
- `xovi-appload-recovery.md`: 新系统映射、隔离试运行和启动崩溃保护。
- `baidu-ocr-configuration-flow.md`: Agent 与普通用户申请、绑定和排查百度 OCR 凭据的完整指南。
- `handwriting-annotation-plan.md`: 自由笔迹、正文锚点、块级 OCR 和重排显示设计。
- `handwriting-input-evaluation.md`: 云端手写识别方案的技术选型记录。
- `legal-and-commercial-use.md`: MIT 开源许可、微信读书条款和产品化权利边界说明。
- `xiaohongshu-community-install-post.md`: 可直接用于小红书的社区须知、Agent 安装提示词和使用说明。
- `dependencies.md`: external SDK, launcher, runtime, plugin and font sources.
- `troubleshooting.md`: symptom-first diagnostic guide for AI Agents.
- `release-checklist.md`: blockers before source or binary release.
- `compatibility.md`: tested device, OS, SDK and runtime matrix.
- `releases/`: milestone-specific release notes and known limitations.
- `weread-web-flow.md`: WeRead browser/API research notes; endpoints are unstable.
- `weread-native-font-rendering.md`: historical Lua renderer font notes.
- `weread-native-runtime-proof.md`: historical Lua framebuffer proof.

The root `README.md` and `AGENTS.md` are authoritative. Historical documents must not override the current Qt architecture.
