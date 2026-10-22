# Native downloader device handoff

Last updated: 2026-10-22 (America/Chicago).

The native sidebar candidate is installed below AppLoad on the exact supported
3.28.0.172 firmware (internal build 20260827113527). Read the repository root
AGENTS.md and docs/agent-handoff.md before touching the device. Preserve all
existing reader, account, library and plugin cache data.

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
