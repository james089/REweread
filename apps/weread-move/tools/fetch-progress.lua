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
local Json = require("json_util")
local WeRead = require("lib.weread")
local ProgressPosition = require("progress_position")

local book_id = arg and arg[1]
if not book_id or book_id == "" then
    io.stderr:write("usage: fetch-progress.lua <bookId>\n")
    os.exit(2)
end

local function fail(message)
    print(Json.encode({ state = "error", message = tostring(message or "unknown") }))
    os.exit(1)
end

local function emit(row)
    print(Json.encode(row))
end

-- Extract exactly the JSON object assigned to __INITIAL_STATE__. A simple
-- `%b{}` match is not sufficient because JSON strings can themselves contain
-- braces. This parser does not inspect or log credentials in that state.
local function initial_state_from_html(html)
    local marker_start, marker_end = html:find("window.__INITIAL_STATE__", 1, true)
    if not marker_start then
        return nil
    end
    local equals_at = html:find("=", marker_end + 1, true)
    if not equals_at then
        return nil
    end
    local object_at = html:find("{", equals_at + 1, true)
    if not object_at then
        return nil
    end

    local depth = 0
    local in_string = false
    local escaped = false
    for index = object_at, #html do
        local character = html:sub(index, index)
        if in_string then
            if escaped then
                escaped = false
            elseif character == "\\" then
                escaped = true
            elseif character == '"' then
                in_string = false
            end
        elseif character == '"' then
            in_string = true
        elseif character == "{" then
            depth = depth + 1
        elseif character == "}" then
            depth = depth - 1
            if depth == 0 then
                local ok, decoded = pcall(Json.decode, html:sub(object_at, index))
                return ok and type(decoded) == "table" and decoded or nil
            end
        end
    end
    return nil
end

local function reader_position(client, book_id)
    local reader_url = WeRead.reader_url(book_id)
    local ok, html = pcall(function()
        return client:get_text(reader_url, { referer = reader_url })
    end)
    if not ok or type(html) ~= "string" then
        return {}
    end
    local state = initial_state_from_html(html)
    if not state then
        return {}
    end
    local reader = type(state.reader) == "table" and state.reader or {}
    local progress = type(reader.progress) == "table" and reader.progress or {}
    local progress_book = type(progress.book) == "table" and progress.book or {}
    local position = ProgressPosition.from_progress_book(progress_book)
    -- The percent is useful even when the browser did not expose a complete
    -- saved-position tuple. Detailed adoption is intentionally omitted then.
    position.progress = tonumber(progress_book.progress)
    return position
end

local ok, err = pcall(function()
    local config = ConfigBridge:new()
    if not config:is_api_configured() then
        error("api key missing")
    end

    local client = Client:new(config)
    local result = client:gateway("/book/getprogress", { bookId = tostring(book_id) })
    local book = type(result) == "table" and result.book or {}
    local position = reader_position(client, book_id)
    local progress = tonumber(position.progress or book.progress or result.progress or 0) or 0
    progress = math.max(0, math.min(100, progress))
    emit({
        state = "done",
        bookId = tostring(book_id),
        progress = tonumber(string.format("%.2f", progress)),
        chapterUid = position.chapterUid ~= "" and position.chapterUid or nil,
        chapterIdx = position.chapterIdx,
        chapterOffset = position.chapterOffset,
        summary = position.summary ~= "" and position.summary or nil,
        -- These fields come from the documented gateway response and are useful
        -- for conflict UI, but never used to replace a local position by itself.
        updateTime = tonumber(book.updateTime),
        recordReadingTime = tonumber(book.recordReadingTime),
    })
end)

if not ok then
    fail(err)
end
