# External dependencies

Dependencies are downloaded at build/install time or installed on the device. They are not committed to this repository.

## Build dependencies

| Dependency | Tested version | Source | Local location |
| --- | --- | --- | --- |
| Docker | 29.2.0 | https://www.docker.com/ | host installation |
| reMarkable chiappa SDK | 5.8.203, aarch64 host | https://developer.remarkable.com/links | Docker volume `rm_chiappa_sdk` |
| Qt | 6.10.3 from SDK/device | included by reMarkable SDK | SDK sysroot |
| QR Code Generator | v1.8.0 | https://github.com/nayuki/QR-Code-generator | ignored `downloads/sources/` |
| Node.js | 18+ | https://nodejs.org/ | host installation |

The official SDK URL used by `scripts/bootstrap-remarkable-sdk.sh` is:

```text
https://storage.googleapis.com/remarkable-codex-toolchain/3.28.0.172/remarkable-production-image-5.8.203-chiappa-public-aarch64-toolchain.sh
```

If reMarkable changes the link, use the official links page and update the URL and SHA-256 together. Never bypass a checksum mismatch.

The pinned SDK SHA-256 is `ae0dcec8ff161a90f72b5bbeb77f11c5a14c6c6045c1655584561935994c1832`.
An aarch64 Linux host with the official SDK installed at `/opt/codex/chiappa/5.8.203` can build directly without Docker. macOS hosts still need a Linux build environment. The scripts retain the Docker volume workflow.

## Device dependencies

| Dependency | Tested version | Source | Purpose |
| --- | --- | --- | --- |
| Developer Mode | device feature | https://developer.remarkable.com/documentation/developer-mode | SSH and custom software |
| XOVI | v19-23052026 | https://github.com/asivery/xovi | injects AppLoad into Xochitl |
| rm-xovi-extensions | v19-23052026 | https://github.com/asivery/rm-xovi-extensions | XOVI service layout |
| AppLoad | v0.5.3 | https://github.com/asivery/rm-appload/releases/tag/v0.5.3 | launcher entry |
| KOReader | v2025.10 tested | https://github.com/koreader/koreader | current LuaJIT/runtime provider |
| weread.koplugin | metadata v0.2.0 tested | https://github.com/QiuYukang/weread.koplugin | WeRead client/content/cookie modules |

AppLoad can also be installed through Vellum where compatible:

```text
https://remarkable.guide/guide/software/vellum/index.html
```

Do not blindly install the newest XOVI/AppLoad on an older device image. Their hooks track Xochitl internals and require a compatibility check after every reMarkable OS update.

2026-10-12 upstream audit and subsequent authorized device validation: AppLoad v0.6.0 with a rebuilt hashtab and guarded startup passed on OS 3.28.0.172, including reboot. The table above records the older installation. XOVI core's release is v0.3.3 (the v19 identifier is the extensions bundle). Do not install the 3.28 hook set on 3.27. See [guarded recovery and evidence](xovi-appload-recovery.md) for the exact validation scope and remaining long-term tests.

## Fonts

| Font | Source | License note |
| --- | --- | --- |
| LXGW WenKai | https://github.com/lxgw/LxgwWenKai | SIL Open Font License; include license in a release package |
| Source Han Sans SC（思源黑体） | https://github.com/adobe-fonts/source-han-sans | SIL Open Font License 1.1; pinned upstream source and checksum |
| Source Han Serif SC（思源宋体） | https://github.com/adobe-fonts/source-han-serif | SIL Open Font License 1.1; pinned upstream source and checksum |
| ChillKai（寒蝉正楷体） | https://github.com/Warren2060/Chillkai | SIL Open Font License 1.1; pinned release archive and checksum |
| ChillHuoSong（寒蝉活宋体） | https://github.com/Warren2060/ChillMovableType | SIL Open Font License 1.1; pinned release archive and checksum |
| WenQuanYi Micro Hei | https://sourceforge.net/projects/wqy/files/wqy-microhei/ | Apache-2.0 or GPL option; document the selected terms |
| WenQuanYi Zen Hei | https://sourceforge.net/projects/wqy/files/wqy-zenhei/ | GPL with font embedding exception; include notices |

Fonts are downloaded by `scripts/download-reader-fonts.sh` to `downloads/fonts/`, pinned by source/release URL and SHA-256. The reader exposes 微米黑、正黑、霞鹜文楷、思源黑体、思源宋体、寒蝉正楷和寒蝉活宋（plus system fallback). Public binary distribution must include the applicable font licenses even though font binaries are not stored in this source repository.

方正、仓耳、汉仪及来源无法由权利方确认的字体不随项目打包，除非未来提供兼容的再分发许可和权威来源。

## WeRead API dependency

The app currently uses two service surfaces:

- WeRead Skill/gateway API with a user-bound `wrk-...` API Key for shelf, search, metadata and progress.
- Cookie-authenticated Web API for QR login, chapter content, comments, Cookie renewal and related reader data.

The API Key is obtained by the user from the WeRead mobile app's WeRead Skill settings. It is user data and must never be committed.

The Web endpoints are not a stable public SDK. Agents must expect response fields, authentication and anti-abuse controls to change. Validate an endpoint with a redacted helper before changing production code.

## Licensing boundary

The repository's project-owned source code is open source under the OSI-approved MIT License, including commercial use when its notice is preserved. MIT does not grant rights to WeRead services, book content, third-party runtimes, fonts or trademarks. Do not publish a binary release until the following are resolved:

- `weread.koplugin` has no explicit upstream LICENSE; obtain permission or replace and review any derived code.
- QR Code Generator MIT notice in binary distributions.
- Font license notices.
- XOVI/AppLoad GPL obligations if any component is redistributed rather than installed separately.
- WeRead terms, account risk, content caching and trademark/branding review.

MIT is the complete license grant for project-owned code; separate service, dependency, content and trademark rights may still be required. See `docs/legal-and-commercial-use.md` before any public distribution or business discussion.
