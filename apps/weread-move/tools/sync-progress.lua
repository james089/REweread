io.stdout:setvbuf("line")
os.setlocale("C", "numeric")

local app_dir = os.getenv("RM_WEREAD_APP_DIR") or "/home/root/xovi/exthome/appload/weread-move"
local koreader_dir = os.getenv("KO_DIR") or "/home/root/xovi/exthome/appload/koreader"
package.path = app_dir .. "/?.lua;" .. app_dir .. "/lib/?.lua;" .. app_dir .. "/views/?.lua;" .. app_dir .. "/tools/?.lua;" .. package.path
package.path =
    app_dir .. "/../../third_party/weread.koplugin/?.lua;" ..
    app_dir .. "/../../third_party/weread.koplugin/lib/?.lua;" ..
    "/home/root/xovi/exthome/appload/koreader/plugins/weread.koplugin/?.lua;" ..
    "/home/root/xovi/exthome/appload/koreader/plugins/weread.koplugin/lib/?.lua;" ..
    package.path

local KoreaderPaths = require("koreader_paths")
KoreaderPaths.append(koreader_dir)

local ConfigBridge = require("config_bridge")
local Client = require("lib.client")
local Content = require("lib.content")
local WeRead = require("lib.weread")
local ReadSyncProtocol = require("read_sync_protocol")

local function usage()
    io.stderr:write("usage: sync-progress.lua <bookId> <progressPercent> [summary] [elapsedSeconds] [chapterUid chapterIdx chapterOffset]\n")
    os.exit(2)
end

local book_id = arg and arg[1]
local progress = tonumber(arg and arg[2])
local summary = ReadSyncProtocol.utf8_prefix_bytes((arg and arg[3]) or "", 20)
local elapsed_seconds = ReadSyncProtocol.elapsed_seconds(arg and arg[4])
local chapter_uid = tostring((arg and arg[5]) or ""):match("^%s*(.-)%s*$")
local chapter_idx = tonumber(arg and arg[6])
local chapter_offset = tonumber(arg and arg[7])

if not book_id or book_id == "" or not progress then
    usage()
end

local has_verified_position = chapter_uid ~= "" and chapter_uid ~= "0"
    and chapter_idx ~= nil and chapter_idx >= 0
    and chapter_offset ~= nil and chapter_offset >= 0

progress = math.max(0, math.min(100, progress))

local function fail(message)
    -- Do not return raw HTTP or plugin errors: they may include user content
    -- or protocol details. The C++ caller only needs a deterministic failure.
    print("state=error message=read-sync-unacknowledged")
    os.exit(1)
end

local function missing_login_message()
    return "微信读书登录 Cookie 未配置。请在微信读书 App 的账号页使用扫码登录，或点续期 Cookie 后再重试。"
end

local ok, err = pcall(function()
    local config = ConfigBridge:new()
    if not config:is_cookie_configured() then
        error(missing_login_message())
    end

    local client = Client:new(config)
    pcall(function()
        client:renew_cookie()
    end)

    local reader_url = WeRead.reader_url(book_id)
    local html = client:get_text(reader_url, { referer = reader_url })
    local state = Content.extract_reader_state(html)
    local payload = WeRead.make_read_payload({
        book_id = book_id,
        -- Compatibility calls can only report a percentage. The older Web
        -- payload format still requires numeric chapter fields, so zero is
        -- sent solely for that legacy path and must never be advertised as a
        -- precise position. Exact callers supply all three verified fields.
        chapter_uid = has_verified_position and chapter_uid or 0,
        chapter_idx = has_verified_position and math.floor(chapter_idx) or 0,
        chapter_offset = has_verified_position and math.floor(chapter_offset) or 0,
        progress = progress,
        summary = summary,
        psvts = state.psvts,
        pclts = state.pclts,
        token = state.token,
        elapsed_seconds = elapsed_seconds,
    })

    local response = client:report_read(payload, reader_url)
    if not ReadSyncProtocol.response_succeeded(response) then
        error("read response was not explicitly acknowledged")
    end
    print("state=done bookId=" .. tostring(book_id) .. " progress=" .. string.format("%.2f", progress)
        .. " positionMode=" .. (has_verified_position and "exact" or "percentageOnly"))
end)

if not ok then
    fail(err)
end
