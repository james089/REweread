-- Private subprocess protocol. Never forward upstream logs or credential fields.
io.stdout:setvbuf("line")
os.setlocale("C", "numeric")
local app = "/home/root/weread-downloader/helper"
local ko = "/home/root/xovi/exthome/appload/koreader"
local root = "/home/root/.local/share/weread-downloader"
package.path = "/home/root/weread-downloader/lua/?.lua;" .. app .. "/?.lua;" .. app .. "/lib/?.lua;" .. package.path
require("koreader_paths").append(ko)
local ffi = require("ffi")
ffi.cdef[[unsigned int umask(unsigned int mask);
int getppid(void);int prctl(int,unsigned long,unsigned long,unsigned long,unsigned long);]]
-- A stopped UI/worker must not leave a helper sending requests in the background.
local worker_parent=ffi.C.getppid()
assert(ffi.C.prctl(1,15,0,0,0)==0 and ffi.C.getppid()==worker_parent,"worker_parent_changed")
ffi.C.umask(63) -- 0077 before any file is created, including helper caches.
local Json = require("json_util")
local Config = require("config_bridge")
local Sha256 = require("sha256")
local output = print
local function emit(row) output(Json.encode(row)) end
local function quote(s) return "'" .. tostring(s):gsub("'", "'\\''") .. "'" end
local function ensure(path) assert(os.execute("mkdir -p " .. quote(path)) == 0) end
local function valid_id(id) return type(id) == "string" and #id > 0 and #id <= 128 and id:match("^[%w_-]+$") end
ensure(root)
assert(os.execute("chmod 700 " .. quote(root)) == 0)

-- Force BOTH paths: overriding only session_path would silently import the
-- reading client's existing plugin credentials from the default config path.
local authSettings
local Session = require("session_auth")
local constructor = Config.new
Config.new = function(self)
    -- A fresh QR login must not merge the old account's refresh cookies. Keep
    -- its on-disk session until the new login succeeds, but read neither it nor
    -- the legacy reader's session into the login client.
    local session_path = arg[1] == "login" and "/dev/null" or root .. "/session.json"
    local settings = constructor(self, {config_path = "/dev/null", session_path = session_path})
    Session.prepare(settings)
    if settings.reload then
        local reload = settings.reload
        settings.reload = function(current) reload(current); Session.prepare(current); return current end
    end
    settings.flush = function(current)
        local values = current.values
        local path = root .. "/session.json"
        local temporary = path .. ".pending"
        local file = assert(io.open(temporary, "wb"))
        local ok = file:write(Json.encode({
            cookies = values.cookies or {}, wr_ticket = values.wr_ticket or "",
            wr_wrpa = values.wr_wrpa or "", logged_out = values.logged_out == true,
            account_name = values.account_name or "", auth_renewed_at = values.auth_renewed_at or 0,
            updated_at = os.time(),
        }))
        assert(file:close() and ok)
        assert(os.rename(temporary, path))
    end
    authSettings = settings
    return settings
end

-- The upstream QR helper uses print for its private protocol. Retain only the
-- QR URL needed by the UI and coarse success/error state, never uid/cookies.
print = function(text)
    local ok, row = pcall(Json.decode, tostring(text or ""))
    if not ok or type(row) ~= "table" then return end
    if row.state == "qr" and type(row.confirm_url) == "string"
        and row.confirm_url:match("^https://weread%.qq%.com/web/confirm%?uid=[%w_-]+$") then
        emit({state = "qr", url = row.confirm_url})
    elseif row.state == "done" then
        emit({state = "authenticated", accountName = authSettings and Session.name(authSettings) or ""})
    elseif row.state == "error" then
        emit({state = "error", code = "login_failed"})
    end
end

local function context()
    local settings = Config:new()
    assert(settings:is_cookie_configured(), "authentication required")
    Session.renew(settings, false)
    local cookies = settings:get("cookies", {})
    assert(type(cookies.wr_vid) == "string" and cookies.wr_vid ~= "")
    local scope = Sha256.hex("weread-downloader-account-v1\31" .. cookies.wr_vid)
    local account_root = root .. "/accounts/" .. scope
    ensure(account_root)
    -- The upstream source-index store otherwise falls back to a path relative
    -- to the helper's working directory, which differs between download and
    -- native exit-sync worker launches. Keep this DB inside our private,
    -- account-scoped plugin cache in both processes.
    settings.data_dir = account_root
    return settings, scope, account_root
end

local function emitShelf(accepted, scope, cached, stale)
        local progress = require("reading_progress").new(authSettings, scope, root .. "/accounts/" .. scope)
        local savedProgress = progress.cache.books
        progress:close()
        local rows = {}
        for _, book in ipairs(accepted) do
            local url = tostring(book.cover or "")
            local host = url:match("^https://([^/:?#]+)")
            local allowed = host and (host == "cdn.weread.qq.com" or host == "res.weread.qq.com"
                or host:match("%.qpic%.cn$") or host:match("%.qq%.com$")
                or host:match("%.file%.myqcloud%.com$") or host:match("%.image%.myqcloud%.com$"))
            rows[#rows + 1] = {
                cloudProgress = savedProgress[book.bookId] and savedProgress[book.bookId].displayMethod == 1 and tonumber(savedProgress[book.bookId].displayProgress) or -1,
                cloudChapter = savedProgress[book.bookId] and savedProgress[book.bookId].cloudChapter or "",
                restoreAutomatic = savedProgress[book.bookId] and savedProgress[book.bookId].restoreAutomatic or false,
                cloudProgressStale = true, restorePending = savedProgress[book.bookId] and savedProgress[book.bookId].restorePending or false,
                bookId = book.bookId, title = tostring(book.title or ""):sub(1, 1536),
                author = tostring(book.author or ""):sub(1, 768), localCover = book.localCover or "",
                cover = allowed and #url <= 2048 and url or "",
            }
        end
        emit({state = "shelf", accountScope = scope, books = rows, cached = cached, stale = stale})
end

local authenticationExpired = false
local function execute()
    local operation = arg[1]
    if operation == "login" then
        -- C++ launcher clears every login-debug/logging environment variable.
        dofile(app .. "/tools/login-qr.lua")
        return
    end
    if operation == "logout" then
        local settings = Config:new()
        local vid=settings:get("cookies",{}).wr_vid
        if type(vid)=="string" and vid~="" then
            local oldScope=Sha256.hex("weread-downloader-account-v1\31"..vid)
            os.remove(root.."/accounts/"..oldScope.."/statistics-api-v1.json")
        end
        settings:set("cookies", {}); settings:set("wr_ticket", ""); settings:set("wr_wrpa", "")
        settings:set("account_name", ""); settings:set("auth_renewed_at", 0); settings:set("logged_out", true); settings:flush()
        emit({state = "logged_out"}); return
    end
    if operation == "status" then
        local settings = Config:new()
        if settings:is_cookie_configured() then
            local _, scope = context()
            emit({state = "authenticated", accountScope = scope, accountName = Session.name(authSettings or settings)})
        else
            emit({state = "login_required"})
        end
        return
    end
    local settings, scope, account_root = context()
    if arg[3] then assert((operation == "sync-progress" or operation == "close-reading" or operation == "report-reading" or operation == "preview-native-restore" or operation == "apply-native-restore") and scope == arg[3], "account_changed") end
    if operation=="cloud-time" then
        local ok,result=pcall(function()return require("cloud_reading_stats").fetch(settings,scope,account_root,tonumber(os.getenv("WEREAD_STATISTICS_BASE_TIME")) or 0)end)
        emit(ok and result or {state="statistics_error",code=result=="statistics_auth_failed" and "statistics_auth_failed" or "statistics_unavailable",accountScope=scope});return
    end
    local ShelfCache = require("shelf_cache")
    local shelf = ShelfCache:new(account_root)
    if operation == "upload-time" or operation == "reading-progress" or operation == "sync-progress" or operation == "restore-progress" or operation == "restore-complete" or operation == "prepare-restore" or operation == "mark-recent" or operation == "begin-reading" or operation == "close-reading" or operation == "report-reading" or operation == "preview-native-restore" or operation == "apply-native-restore" then
        local progress = require("reading_progress").new(settings, scope, account_root)
        progress.onTimeChunk=function(done,total,remaining)
            emit({state="time_upload_progress",accountScope=scope,bookId=arg[2],uploadedSeconds=done,totalSeconds=total,remainingSeconds=remaining})
        end
        local ok, result = pcall(function()
            if operation == "reading-progress" then
                progress:refresh((shelf:load_shelf() or {}).books or {}, emit)
                return {state="reading_progress_done", accountScope=scope}
            end
            if operation == "upload-time" then return progress:uploadTime(arg[2]) end
            if operation == "prepare-restore" then return progress:prepareRestore(arg[2]) end
            if operation == "mark-recent" then return progress:markRecent(arg[2]) end
            if operation == "preview-native-restore" then return progress:previewNativeRestore(arg[2],os.getenv("WEREAD_NATIVE_PULL_TOKEN"),os.getenv("WEREAD_NATIVE_PULL_UUID")) end
            if operation == "apply-native-restore" then return progress:applyNativeRestore(arg[2],os.getenv("WEREAD_NATIVE_PULL_TOKEN"),os.getenv("WEREAD_NATIVE_PULL_UUID")) end
            if operation == "report-reading" then return progress:reportReading(arg[2],os.getenv("WEREAD_READING_SESSION"),os.getenv("WEREAD_READING_REPORT_ID")) end
            if operation == "begin-reading" then return progress:beginReading(arg[2],os.getenv("WEREAD_READING_SESSION")) end
            if operation == "close-reading" then return progress:closeReading(arg[2],os.getenv("WEREAD_READING_SESSION")) end
            if operation == "restore-progress" then return progress:restore(arg[2]) end
            if operation == "restore-complete" then return progress:restored(arg[2]) end
            return progress:sync(arg[2])
        end)
        progress:close()
        if not ok then
            local codes={progress_not_located=true,progress_layout_unsupported=true,progress_network_failed=true,progress_source_map_missing=true,
                progress_cache_changed=true,progress_offset_unsupported=true,cloud_progress_unavailable=true,
                cloud_progress_changed=true,progress_local_changed=true,progress_weight_unavailable=true,progress_sync_unconfirmed=true,progress_restore_pending=true,progress_restore_expired=true,
                time_cache_unavailable=true,time_cloud_unavailable=true,time_upload_rejected=true,time_upload_uncertain=true,time_report_duplicate=true,time_report_not_due=true}
            emit({state="error", code=codes[result] and result or "progress_failed"})
            return
        end
        emit(result)
    elseif operation == "cached-shelf" then
        local stored = shelf:load_shelf()
        local saved = stored and tonumber(stored.savedAt) or 0
        emitShelf(stored and stored.books or {}, scope, true, not stored or os.time() - saved >= 300 or saved > os.time())
    elseif operation == "shelf" then
        local service = require("shelf_service"):new(settings)
        -- The installed upstream defaults to an Agent Gateway requiring a
        -- separate API key. This plugin deliberately uses its independent web
        -- login session, so route ONLY shelf sync through the cookie-authenticated API.
        service.client.gateway = function(client, api)
            assert(api == "/shelf/sync")
            local cookies = settings:get("cookies", {})
            local text, code = client:request({
                url = "https://weread.qq.com/web/shelf/sync?userVid=" .. cookies.wr_vid .. "&synckey=0&lectureSynckey=0",
                method = "GET", timeout = 30,
                headers = {["Accept"] = "application/json", ["Referer"] = "https://weread.qq.com/"},
            })
            if code == 401 then authenticationExpired = true; error("authentication_required") end
            assert(code and code >= 200 and code < 300, "shelf_http_failed")
            local result = client:json_decode(text or "")
            local errorCode = type(result) == "table" and tonumber(result.errCode or result.errcode)
            if errorCode == -2012 or errorCode == -2013 then authenticationExpired = true; error("authentication_required") end
            assert(type(result) == "table" and (result.errcode == nil or tonumber(result.errcode) == 0))
            if type(result.books) ~= "table" and type(result.data) == "table" then result = result.data end
            assert(type(result.books) == "table")
            return result
        end
        local books = service:load_shelf()
        local accepted = {}
        for _, book in ipairs(books or {}) do
            if valid_id(book.bookId) and #accepted < 2000 then accepted[#accepted + 1] = book end
        end
        shelf:save_shelf(accepted)
        emitShelf(accepted, scope, false, false)
    elseif operation == "download" then
        local id = arg[2]
        assert(valid_id(id))
        local book
        for _, candidate in ipairs((shelf:load_shelf() or {}).books or {}) do
            if candidate.bookId == id then book = candidate; break end
        end
        assert(book and book.title and book.title ~= "")
        -- Capture the exact original chapter source before the upstream helper
        -- rewrites it for EPUB packaging. Only compact offsets are persisted;
        -- chapter text remains in the verified download cache.
        local Content = require("lib.content")
        local fetchChapter = Content.fetch_chapter_xhtml
        assert(type(fetchChapter) == "function")
        Content.fetch_chapter_xhtml = function(client, chapterSettings, chapterBook, chapter)
            local html, extra = fetchChapter(client, chapterSettings, chapterBook, chapter)
            local sourceId = chapterBook and (chapterBook.bookId or chapterBook.book_id) or id
            local uid = chapter and (chapter.chapterUid or chapter.chapter_uid or chapter.uid)
            if sourceId == id and uid and type(html) == "string" then
                pcall(require("progress_source_store").save, account_root, id, tostring(uid), html)
            end
            return html, extra
        end
        local statuses = require("book_status_store"):new({root = account_root})
        local manager = require("download_manager"):new({
            config = settings, status_store = statuses, cache_dir = account_root .. "/books",
        })
        require("download_io").attach(statuses, manager)
        local job = manager:start_full_download(book)
        local downloadStartReaderRecord
        if job.client and type(job.client.cachedReaderState) == "function" then
            local cachedOk, cachedValue = pcall(job.client.cachedReaderState, job.client)
            if cachedOk and type(cachedValue) == "string" then
                local recordOk, record = pcall(require("saved_progress").extract, cachedValue, Json.decode)
                if recordOk and type(record) == "table" then downloadStartReaderRecord = record end
            end
        end
        local parallelOk, parallelFailure = pcall(function() require("parallel_download").run(manager, job, function(done, total)
            emit({state = "progress", done = done, total = total})
        end) end)
        if not parallelOk then
            if authenticationExpired or tostring(parallelFailure):find("微信读书登录已失效", 1, true) then error(parallelFailure, 0) end
            -- Resume only verified chapter checkpoints with a fresh serial
            -- client. Never submit a partial EPUB after a parallel failure.
            job = manager:start_full_download(book)
        end
        while job.state ~= "done" do
            manager:step_full_download(job)
            emit({state = "progress", done = #(job.selected or {}), total = #(job.chapters or {})})
        end
        local status = statuses:get(id)
        assert(status.downloadState == "full" and (tonumber(status.failedChapterCount) or 0) == 0)
        assert(type(status.fullFile) == "string" and status.fullFile ~= "" and status.imageAssets == true)
        -- Keep the reader-state bookmark already fetched at download start.
        -- The user's requested snapshot semantics allow it to stay fixed while
        -- chapters download; import-time restore can use local catalog/XHTML.
        local progressOk, progress = pcall(function()
            return require("reading_progress").new(settings, scope, account_root)
        end)
        if progressOk then
            pcall(function() progress:prepareDownloadedRestore(id, job.chapters, downloadStartReaderRecord) end)
            pcall(function() progress:close() end)
        end
        emit({state = "downloaded", path = status.fullFile, accountScope = scope, coverUrl = book.cover or ""})
    else
        error("unsupported operation")
    end
end
local ok, failure = pcall(execute)
if not ok and (authenticationExpired or tostring(failure):find("微信读书登录已失效", 1, true)) then
    local settings = authSettings or Config:new()
    local renewed, rejected = Session.renew(settings, true)
    if renewed then
        authenticationExpired = false
        ok, failure = pcall(execute)
    elseif not rejected then
        -- An offline renewal is not evidence that the account was logged out.
        authenticationExpired = false; failure = "session_renewal_unavailable"
    end
end
if not ok then
    if authenticationExpired or tostring(failure):find("微信读书登录已失效", 1, true) then
        if authSettings then authSettings:set("logged_out", true); authSettings:flush() end
        emit({state = "login_required"}); return
    end
    emit({state = "error", code = "operation_failed"})
    os.exit(1)
end
