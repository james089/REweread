# Changelog

All notable changes to REweread are documented here.

## [Unreleased]

## [3.0.0] - 2026-10-12

Source-only milestone; no public binary distribution. End-to-end cloud writes,
whole-book offline acceptance and the full typography matrix remain pending.

### Added

- My → Reading statistics: local daily/monthly reading time, book totals and
  recent activity, with foreground-only accounting and account isolation.
- Persistent reading-time upload ledger, uncertain-result protection, cloud
  personal-note cache and conservative remote progress matching.
- Checksum-verified chapter reuse, bounded retries, resumable downloads and
  explicit partial EPUB status.
- Reader font preview grid and ordered image/caption pagination.
- Guarded XOVI startup with exact firmware/resource validation and stock-system
  fallback after injection failure; documented AppLoad v0.6.0 recovery on 3.28.

### Fixed

- Support the firmware 3.28 framebuffer refresh ABI while preserving the older ABI.
- Read legacy session encoding in memory without rewriting credentials.
- Track and restore temporary direct-ink pixels on canvas clearing, page
  transitions and handoff to Qt; preserve the background rather than painting
  white over body text. Bounded tracking falls back safely to Qt.
- Freehand eraser now deletes intersected strokes on the current book/page,
  including newly queued ink and writing in the page margins.

### Validation boundary

- OS 3.28.0.172: AppLoad entry confirmed, reboot persistence and stock-system
  return checked; font, image, statistics and direct-ink probe self-tests passed
  before this release preparation. These do not prove continuous handwriting.
- Freehand page-clear/eraser regression reported during release preparation;
  see release notes for the fix and its actual validation status.
- Local handwriting upload, precise native chapter offsets, clean-device
  install/uninstall and long-duration battery acceptance remain incomplete.

### Licensing

- Relicensed all project-owned code under the OSI-approved MIT License with
  the consent of all current contributors.
- Preserved separate third-party notices and the existing service, content,
  trademark, dependency and source-only release boundaries.

## [2.0.0] - 2026-07-14

### Added

- Explicit AI handwriting-reply pen mode: after a deliberate pause, a single
  free-ink block is OCRed by Baidu and its text is sent to the user's DeepSeek
  account; reply sentences reveal progressively beside the source ink in
  LXGW WenKai.
- The temporary browser pairing page can now configure either Baidu OCR or
  DeepSeek. Each service has a separate owner-only credential file, and saving
  one never clears the other.
- New shelf-level Magic Notebook: a blank full-screen writing page sends only
  the paused handwriting batch through Baidu OCR and DeepSeek. The writer's
  ink first fades from the paper; the reply then draws as native skeletonized
  pen paths with tiny framebuffer updates and fades away in local stages.
- Added build-time, checksum-pinned OFL handwriting fonts from Google Fonts:
  Ma Shan Zheng, Liu Jian Mao Cao, Zhi Mang Xing and Long Cang.
- Named persona cards now define a character's name, era, background and
  speaking style so identity questions have consistent answers.

### Fixed

- Magic Notebook never starts an answer until the question ink layer has
  cleared and the e-paper compositor has settled.
- Reply animation no longer relies on QML Text refresh batching; the first
  glyphs no longer appear as word-sized chunks or trigger a whole-page flash.

## [1.5.0] - 2026-07-14

Source-only milestone focused on Chinese input, handwriting annotations and
cloud OCR on reMarkable Paper Pro Move.

### Added

- Built-in Chinese keyboard with pinyin candidates, explicit candidate paging,
  handwriting input and an English mode for password fields.
- Reader pen capsule with independent color and tool selection for text-snapped
  highlighting, free handwriting, erasing, block OCR, handwritten-note display
  and confirmed page clearing.
- Fast native pen ink with direct framebuffer updates, palm rejection and one
  settled refresh after writing instead of a scene redraw for every point.
- Spatially and temporally grouped handwriting blocks with inline Baidu OCR;
  recognition is optional and never replaces the original strokes.
- User-started HTTPS setup from the My page for binding Baidu OCR credentials
  with a short-lived pairing code and owner-only device storage.
- Documentation for obtaining Baidu OCR credentials, upgrading through an
  Agent, configuring cloud recognition and using all 1.5 handwriting features.

### Fixed

- Handwriting ink coordinates now map through `Window.contentItem`, so the
  keyboard displays the first stroke in the correct place.
- Pinyin and handwriting candidates can be paged instead of becoming stuck on
  the first five results.
- Reader handwriting persistence is batched after idle, never runs while a
  stroke is active and no longer rebuilds the canvas during the save handoff.
- Reader OCR operates on the selected nearby-stroke block and returns inline,
  without opening a full-page recognition window.
- Body text is clipped and paginated above the reserved footer area.

### Release boundary

- This remains a source-only, noncommercial, unofficial milestone.
- No application binary, SDK, font, KOReader, XOVI, AppLoad,
  `weread.koplugin`, credentials, user data or book content is distributed.
- No `v1.5.0` tag or public GitHub Release is created by this source update.
- Public binary distribution remains blocked by service/rights review,
  unresolved upstream licensing and clean-device lifecycle validation.

## [1.0.0-rc.1] - 2026-07-13

First source-only release candidate of the Qt application for reMarkable Paper Pro Move.

### Added

- Native-feeling Chinese shelf, book detail, reader, table of contents and settings flows.
- Device QR login, account renewal, progress sync, chapter-first and whole-book downloads.
- Chinese typography, images and captions, notes, cached social comments and stylus highlights.
- Wi-Fi, frontlight, battery, power-button and magnetic-folio integration.
- Agent-focused setup, safety, troubleshooting, legal and user documentation.

### Fixed

- Stable `textOffset` restoration across repagination and reading-setting changes.
- Page fill, image flow, social underline mapping and touch/pen event routing.
- Deep suspend through the official Move systemd hooks, including Wi-Fi teardown, VPDD wait and bounded retry.
- Wi-Fi reconnection after resume and stock-system restoration after application exit.

### Release boundary

- This is a source-only, noncommercial, unofficial release candidate.
- No application binary, SDK, font, KOReader, XOVI, AppLoad, `weread.koplugin`, user data or book content is distributed.
- Public binary distribution remains blocked by WeRead service/rights review and unresolved upstream licensing.

[1.0.0-rc.1]: https://github.com/nasonliu/REweread/releases/tag/v1.0.0-rc.1
[1.5.0]: https://github.com/nasonliu/REweread/tree/main
[2.0.0]: https://github.com/nasonliu/REweread/tree/main
