## 2026-10-29 Authorized cached-position / one-second spacing test

User asked to pause the proposed pacing change and test one-second spacing.
Same recent backfill book, one initial reader GET/entry, exactly three rt=60
reports with one-second waits before reports two/three. All ct values differed.
Receipts succeeded; cloud delta was +180 immediately and at approximately
19/41/72 seconds. No retries, per-report GETs, local ledger edits or app changes.
This is a successful three-report sample, not a one-hour scale validation.

## 2026-10-29 Authorized cached-position / two-second spacing test

User authorized exactly three rt=60 reports reusing the initial cloud reader
context, waiting two seconds before reports two and three. Same recent backfill
book; one initial GET and one entry report. All three ct values differed and
all three receipts succeeded. Saved coordinates remained unchanged. Cloud
readingTime increased by 180 seconds immediately and remained +180 at about
21/43/74 seconds from test start. No per-report GETs, retries, or local
acknowledgement/backlog ledger edits occurred; private intents remain outside
the repository on-device. This validates cached context with two-second spacing
for this three-report sample, not arbitrary bulk duration or daily attribution.
No application pacing code or installed build was changed by this test.

## 2026-10-29 Authorized three-report pacing comparison

User explicitly authorized two groups of exactly three rt=60 submissions on the
same most-recent backfill book. Each group sent one entry report. Group A fetched
reader context once then sent three reports without GETs between them. Group B
fetched reader context before each report. Exactly six duration POSTs were sent,
with no retries and no local acknowledgement/backlog ledger changes. Private
write-intent records remain on-device outside the repository. Saved chapter
coordinates remained unchanged.

| Group | Duration receipts | Distinct integer ct values | Observed cloud increase |
| --- | --- | --- | --- |
| A cached context, consecutive POSTs | 3 accepted | 2 (two reports shared one second) | 120 seconds |
| B GET before each POST | 3 accepted | 3 | 180 seconds |

A retained +120 at immediate and approximately 14/35/67-second reads; B retained
+180 at immediate and approximately 16/37/69-second reads. This strongly supports
same-second report deduplication as an explanation. It does not identify the
server's exact key or prove that GET itself is required. Cached-context reports
paced into distinct real clock seconds remain an untested candidate fix; never
fabricate future timestamps or replay historical acknowledged differences.

## 2026-10-29 Rapid backfill credit shortfall

User reports more than an hour acknowledged but roughly 16 minutes displayed
in cloud statistics. Read-only inspection confirms the acknowledgement ledger
is advanced per positive receipt, without proving corresponding cloud credit.
A fresh saved-record GET for the latest backfill context shows only a small
increase relative to its captured entry baseline, far below submitted duration.
No additional POST, account change, or acknowledgement reset was performed.

The installed upstream constructor sets `ct` to integer `os.time()`, while
`ts` and `rn` vary separately. A synthetic batch of 60 payloads produced one
unique `ct`. This establishes a same-second field collision, not the server's
deduplication key or a proven fix. The previous per-chunk reader GET loop also
introduced network pacing that the new fast POST-only loop lacks. Neither the
single rt=60 experiment nor accepted batch receipts establish full batch credit.
A controlled small-batch pacing comparison remains necessary; do not blindly
replay acknowledged records or claim a one-second sleep is sufficient.

## 2026-10-29 Restore chunked backlog uploads

User requested restoring the downloader's prior sequential <=60-second backlog
reporter, reusing the initial cloud position. This is distinct from KOReader's
periodic reporter and the independent app's one-shot duration submission.
The downloader now initializes one cloud reader context for foreground backfill,
then signs each chunk independently without per-chunk GET/verification or sleeps.
A batch stops after 60 chunks or approximately 90 seconds of work. Exit/progress
sync also drains recorded unacknowledged time using its existing captured target;
foreground time-only backfill preserves captured cloud coordinates throughout.
The API still requires coordinate fields; it is not a position-free endpoint.

Periodic reports and backfill share the durable acknowledgement ledger to avoid
counting the same local seconds twice. Unknown outcomes remain reserved and block
replay; explicit rejection releases the reservation. Older-day durations are
eligible, but requests cannot backdate service-side daily attribution. Synthetic
one-hour backfill verifies exactly 60 reports and one cloud GET, unchanged saved
coordinates, and no PDF/chapter work. No new real-account writes were performed.
Positive receipts remain evidence of submission, not equal verified cloud credit.

# WeRead Web reading-time reporting audit

## 2026-10-29 Authorized 20-second / 65-second follow-up

On the same requested book and independent desktop session, the user authorized
one further entry, 20 seconds of actual waiting, and exactly one rt=65 report.
The same Lua constructors and project acknowledgement guard were used. Both
requests returned succ=1. Saved readingTime showed zero increase at approximately
22, 39 and 71 seconds after entry acknowledgement; saved position was unchanged.
No write was retried and no tablet connection or historical ledger change occurred.

Compared with the preceding 20-second/rt=60 success, this is consistent with a
per-report duration boundary near 60 seconds. It does not prove an exact cap:
other server-side state and reporting cadence may differ, and the readback window
is finite. Do not call the positive receipt verified credit or infer a universal
limit from these two samples. Private intent and results remain outside Git.

## 2026-10-29 Independent desktop 20-second / 60-second comparison

The user explicitly authorized one entry, 20 seconds of actual waiting, and one
rt=60 report on the requested book, using an independently scanned desktop
session without contacting the tablet. Request construction used the plugin's
upstream Lua make_enter_read_payload / make_read_payload, with the local
read_sync_protocol.lua acknowledgement guard. Desktop Node provided HTTPS,
monotonic waiting, private storage and hash primitives. The external Lua source
was pinned to cc100e0de301ef973d3195afeb47c04931b0b261 and kept ignored; the
tablet's dependency hash was not read, so binary/runtime identity is not claimed.

Entry and duration both returned succ=1. The controlled wait was 20 seconds;
the first readback at approximately 22 seconds after entry acknowledgement
showed readingTime increased by 60 seconds. Further read-only checks near
39 and 71 seconds retained the same 60-second increase. Saved chapter/offset,
progress and summary stayed unchanged. Exactly one duration write was made;
no historical ledger was replayed or altered, and no tablet data was touched.

This observation disproves a blanket claim that credited duration must always
be no greater than actual time since entry. It does not show arbitrary one-hour
backlog acceptance, establish server thresholds, prove daily attribution or
explain every prior failure. Earlier 10-second/3600-second non-credit results
remain distinct observations. No additional exploratory write was authorized.

The first desktop implementation incorrectly forwarded SSR pclts=0 instead of
performing the Web reader's client-timestamp initialization. It also discarded
outer business flags when unwrapping response.data and lacked safe receipt
diagnostics, so its earlier entry failure cannot be assigned a precise server
code. The tool now uses the Lua constructor and retains only numeric/boolean
receipt fields, distinguishing explicit rejection from an unconfirmed response.

Reviewed 2026-10-26. This is protocol research, not an official API contract.
No authenticated writes were made during this audit. Public frontend assets
were retrieved without account cookies; downloaded assets remain ignored.

## Primary evidence

The public reader loads
[9.f252b488.js](https://cdn.weread.qq.com/web/wrwebnjlogic/js/9.f252b488.js).
In that asset, the FETCH_READER_BOOK_READ action computes `rt` from the elapsed
wall-clock milliseconds since `startReadingTime`, converts to seconds, and
includes it in the signed `/web/book/read` request. On success the frontend
resets that start time. The reading view starts a 30-second report timer;
activity/visibility handlers exclude paused intervals. Repeated identical
progress can suppress further reports. The entry report is separate from
active duration reporting.

The same frontend displays aggregate book reading time from
`state.reader.progress.book.readingTime`, matching this plugin's saved-record
parser. The observed discrepancy is not explained by the plugin selecting a
different frontend aggregate field.

The installed upstream make_read_payload constructs `rt` from elapsed_seconds,
current ct/ts, a random rn, reader-token sg, session ps/pc, and the final signature.
This establishes request construction; it does not establish server-side
crediting rules or support for historical imports.

## Errors in our implementation assumptions

- The 60-second chunk size, up-to-60-iteration loop and 90-second work budget
  are our own policies, not limits demonstrated in the official frontend.
- The upload loop treats an accumulated local backlog as many fresh reports
  in a short interval. This is different from the frontend's active-session
  reporting model; successful receipts do not prove equivalent time credit.
- The ledger's confirmedSeconds records acknowledged submission. It must not
  be described as verified cloud credited seconds. Existing receipt records
  must not be reset or retransmitted based solely on aggregate differences.
- Re-fetching reader HTML before every chunk is our implementation choice,
  not the frontend's normal timer behavior.

## Still unknown

The public client alone does not establish server caps, deduplication windows,
credit for offline intervals, backdated daily attribution, aggregation latency,
or the exact cause of the measured submission/aggregate discrepancy. Do not
claim a server cap or historical-upload prohibition without further evidence.
No supported bulk offline-time-import endpoint has been established here.

## Required before calling duration upload reliable

Separate active-session reporting from offline backlogs. Preserve actual
session timestamps/dates, durable positive receipts and uncertain outcomes.
Validate credited time separately from request acknowledgement, using a
bounded, explicitly authorized real-account test and appropriate readback.
Use synthetic fixtures for request construction, pause/resume, date boundaries
and duplicate protection. Do not run further write experiments automatically.
Until these points are resolved, progress sync and local time statistics may
work, but historical duration synchronization remains unverified.

## Existing REweread application comparison

The independent application's Main.qml syncCurrentReaderProgress(true) reads all
pending whole seconds from ReadingTimeStore and reserves them as one durable
submission. ProgressSyncStore passes that entire elapsedSeconds value to
helper/tools/sync-progress.lua. The Lua helper renews the session, fetches reader
state once, builds one make_read_payload with the entire elapsed_seconds, and
calls report_read once. There is no 60-second splitting or duration-upload loop.
Automatic exit/sleep calls includeElapsed=false; only the deliberate manual sync
includes accumulated duration. The C++ helper timeout is 25 seconds.

Both this path and the downloader plugin use the same upstream make_read_payload
and /web/book/read endpoint. The independent app also accepts a positive business
receipt without verifying an equal increase in aggregate readingTime; its
implementation demonstrates the intended one-request strategy, not independently
validated server credit for arbitrary duration. Existing handoff explicitly says
its earlier regression did not test real-account writes. No live account writes
or device changes were made during this source comparison.

## Readback-source cross-check

A read-only authenticated comparison used the frontend's explicit
`GET /web/book/getProgress?bookId=...` endpoint, rather than SSR HTML alone.
Its `book.readingTime` agreed with the HTML's saved `reader.progress.book` value.
The endpoint also returned recordReadingTime separately; do not substitute that
field without establishing its meaning. This rules out an observed HTML/API
disagreement in that comparison, not all possible accounting delay or scope
issues. `/web/book/readingStat` returned reader counts/status, not the current
account's personal accumulated duration; `/web/book/readInfo` primarily supplied
book information and finished-reading markers. Neither should replace the
personal progress duration just because its endpoint name includes statistics.
No write requests or local ledger updates were made for this comparison.

## Additional read-only checks and remaining causal gap

The account's existing statistics API configuration was checked internally
without exposing credentials; the configured cookie identity matched the
independent downloader session before the official read-only gateway was used.
A monthly `/readdata/detail` target-book readLongest.readTime agreed with the
Web getProgress book.readingTime and SSR result. The overall ranking omitted
the target (rankings are limited), so absence must not be interpreted as zero.
The weekly statistic is a separate period, not a replacement book-total field.

The live GET /web/getConfig signing token matched the token extracted from
reader HTML. Token values were neither printed nor persisted in research files.
This excludes an observed stale-token mismatch in that check; it does not
validate all server-side session requirements.

The official reader separately calls FETCH_READER_ENTER_BOOK_READ at entry,
sets a local start time, and later submits elapsed active seconds. Its enter
payload omits rt/ts/rn/sg. Our duration path fetches reader HTML and directly
calls the duration report; it does not reproduce that entry/elapsed lifecycle.
The single-3600 test also asserted one hour without observing a one-hour Web
session. These are confirmed flow differences, not proof of a particular
server rejection rule. The server's silent-credit decision is still unknown.
Further causal tests must control entry state, actual elapsed duration, and
position changes separately; a success receipt alone is not sufficient. No
additional report_read or enter-read writes were made during these checks.

## Explicitly authorized entry/wait/report validation

The user subsequently authorized exactly one enter-read request, a 20-second
wait, and one duration report with rt=20 on the selected book. Both requests
returned positive business receipts. A monotonic clock measured 20.000 seconds
between entry acknowledgement and the duration submission. The immediate saved
book readingTime readback increased by exactly 20 seconds. The chapter/offset
and the local uploaded-duration ledger remained unchanged. No repeat report was
sent. No account identifiers, book identifiers or personal records are stored
in this document.

This validates that particular entry/elapsed/report sequence and its observed
credit. It does not isolate entry initialization from realistic elapsed timing,
as both differ from the previous direct-3600 test. It does not prove arbitrary
one-hour offline backlog acceptance, nor justify replaying prior acknowledged
submissions. A changed implementation must retain this distinction.

## Explicitly authorized 10-second wait / 3600-second report comparison

The user next authorized one enter-read request, 10 seconds of actual waiting,
and one rt=3600 report. Entry and duration both returned positive receipts.
The monotonic wait measured 10.000 seconds. Immediate readback and another
read-only check after 20 seconds both showed zero increase in saved book
readingTime. Position and the local duration acknowledgement ledger were
unchanged. No second duration report was sent.

Together with the preceding 20-second/rt=20 success, this disproves the claim
that entry alone makes arbitrary durations effective in these tested cases.
It does not establish the server's exact allowed duration, skew tolerance,
maximum per request, or eventual credit beyond the observed readback window.

## 2026-10-28 Main-app comparison and daily per-book availability

Source inspection: `apps/weread-qt/ReadingStatsPage.qml` presents local today,
month, total and seven-day totals. `ReadingTimeStore::recordObservedInterval`
keeps daily milliseconds and a set of hashed book keys; it does not keep each
book's daily milliseconds or query cloud daily book records. Thus its similar
statistics screen is not evidence of an existing cloud daily-per-book path.

The downloader's independent `native/reading_time_store.h` already keeps
`books[id].days[date]` seconds for native reading, and preserves those records
separately from upload receipts. It can support an explicitly labelled local
per-book daily list without changing upload behavior. It cannot substitute for
account-wide cloud history, and historical timezone changes are not reconstructible
from date buckets alone. User previously requested cloud-only statistics, so this
research does not silently reintroduce local statistics into the UI.

Live read-only gateway discovery with the plugin's own configured key succeeded.
`/_list` listed `/readdata/detail` with mode/baseTime, the four documented period
modes, and no distinct daily book-history endpoint in its available list. A monthly
response exposed account daily `readTimes` and period `readLongest` rows with
book/readTime/tags; preferBooks did not supply a daily per-book decomposition.
An exploratory daily mode returned HTTP499, not daily records; this response
alone does not establish why the call failed or prove no private endpoint exists.
No account identifiers, book data, key values or response bodies are retained.
No UI changes or device installation were performed for this research.


## Latest exit conflict diagnosis

Read-only preflight reproduced `cloud_progress_changed` before any upload. The
latest entry session existed, was unfinished and had no ambiguous duration POST.
The baseline saved position matched the exact local source-map position. Current
cloud position had the same chapter but different chapterOffset and summary,
with a later updateTime; it did not match the local position. This is an actual
position disagreement, not a timestamp-only comparison false positive. Logs do
not identify which client or action changed the remote record. Since sync checks
conflict before its combined progress/time POST, duration was also withheld.
No cloud write, receipt rewrite, conflict bypass or historical replay was done.

## Installed KOReader WeRead plugin behavior (source inspection)

Inspected the device's installed `weread.koplugin` version 1.6.0, specifically
`weread/lib/read_report.lua` and UI lifecycle wiring. Its scheduled reporting
has a 30-second default interval (10-second minimum), not exit-only submission.
Each tick with no explicit elapsed override builds a report whose elapsed time
is the configured interval. `_send` first sends enter-read if its in-memory
session has not entered, then sends the timed report. Sleep suspends the task;
normal document close stops it. `_precheck` skips offline ticks and schedules
another attempt; there is no historical offline-duration accumulation/replay
in this reporter. Pending progress-position upload in `progress_sync.lua` is
separate and must not be mistaken for stored duration backlog.

Accepted responses use success/synckey checks and increment the report count;
there is no before/after cloud readingTime credit check in that success path.
Therefore its periodic design is a useful comparison for live reporting, not
proof of reliable historical duration replay or of full credit per request.
Only source was read; the KOReader UI/runtime was not launched or reinstalled,
and no real read report was sent. No upstream code was copied into project code.


## 2026-10-28 Explicitly authorized one-hour backlog retry

User explicitly requested one more attempt to submit one hour from the same
book's actual unsubmitted local reading records. Verified sufficient unsubmitted
local-day time and no pending ambiguous receipt. Sent exactly one rt=3600 report
using freshly read cloud coordinates, summary and signing context; no new
enter-read or artificial reading session was sent. A private account-scoped
probe journal was written before the POST to prevent accidental repetition.
The response was positively acknowledged. Immediate readback and another
readback roughly 42 seconds later both showed zero increase in book readingTime.
The saved cloud position fingerprint and ordinary submission ledger were
unchanged. The probe did not mark the backlog as uploaded or replay the POST.
This observation establishes no credited increase within that observation
window, not a universal proof that every offline reporting method is impossible.
No account/book identifiers, credentials or actual book-total values are recorded.

## Explicitly authorized two-minute retry

Using the same preserved-cloud-position path as the preceding one-hour retry,
sent exactly one rt=120 report from actual unsubmitted local time, without
enter-read or repeated POST. Positive acknowledgement; immediate readback and
another approximately 60 seconds later both showed zero increase relative to
this attempt's freshly read baseline. Position fingerprint and ordinary receipt
ledger unchanged. The baseline differed from the prior experiment, so do not
attribute an intervening change to either probe without independent evidence.
A separate private probe journal preserves the attempt; backlog was not marked
credited. No program deployment or account/config changes were made.

## Explicitly authorized enter-read then one-hour retry

At the user's explicit instruction, sent one enter-read using the current cloud
position. After a positive acknowledgement, fetched current context again,
verified the position fingerprint was unchanged, then sent exactly one rt=3600
report. Both business acknowledgements were positive. Immediate readback and
another roughly 32 seconds later showed zero additional book readingTime.
Cloud location and ordinary receipt ledger remained unchanged. This attempt's
baseline matched the preceding two-minute attempt's baseline; no further
intervening credit was observed. Private probe journal persisted before each
write. No loop, artificial hour of waiting, or backlog-success marking occurred.

## Explicitly authorized enter-read, real wait, then 30 seconds

At the user's explicit instruction, sent one enter-read and checked its positive
business acknowledgement. Waited using a real 30-second sleep; process/tool and
network overhead made the measured acknowledgement-to-duration request interval
approximately 55 seconds. Then sent exactly one rt=30 report at the unchanged
current cloud position. The freshly fetched readingTime immediately before this
report matched the entry baseline. Immediate readback after the report increased
by exactly 30 seconds. Position fingerprint and the ordinary submission ledger
were unchanged. This reproduces real observed time credit for the enter/wait/
report sequence; it does not prove historical offline backlog credit or a
specific undocumented server acceptance threshold. The private probe journal
stores the observed credit separately; no general upload feature was changed.
