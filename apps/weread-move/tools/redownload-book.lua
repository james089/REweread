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
local ShelfCache = require("shelf_cache")
local BookStatusStore = require("book_status_store")
local DownloadManager = require("download_manager")
local DownloadTitle = require("download_title")
local BookInfoService = require("book_info_service")

local function usage()
    io.stderr:write("usage: redownload-book.lua <bookId> [title]\n")
    os.exit(2)
end

local book_id = arg and arg[1]
if not book_id or book_id == "" then
    usage()
end

local function sanitize_error(err)
    return tostring(err or "unknown error")
        :gsub("[\r\n]+", " ")
        :gsub("%s+", " ")
        :gsub("^%s+", "")
        :gsub("%s+$", "")
end

-- This is the completion contract for the Qt caller.  Progress lines are
-- intentionally advisory; only this final line states which file was created
-- by *this* helper invocation.
local function emit_result(state, path)
    print("result state=" .. tostring(state or "error") .. " path=" .. tostring(path or ""))
end

local ok, err = pcall(function()

local function snapshot_status(status)
    local out = {}
    for key, value in pairs(status or {}) do
        out[key] = value
    end
    return out
end

local function find_book(book_id)
    local shelf = ShelfCache:new():load_shelf()
    for _, book in ipairs((shelf and shelf.books) or {}) do
        local id = tostring(book.bookId or book.book_id or "")
        if id == tostring(book_id) then
            return book
        end
    end
    return nil
end

local status_store = BookStatusStore:new()
local status = status_store:get(book_id)
local original_status = snapshot_status(status)
local book = find_book(book_id) or {}
book.bookId = book_id
book.book_id = book_id
local config = ConfigBridge:new()
local resolved_title, info = DownloadTitle.resolve(book_id, {arg[2] or "", book.title or "", status.title or ""}, function()
    print("state=metadata")
    config:reload()
    return BookInfoService:new(config):load(book_id)
end)
book.title = resolved_title
if info then
    info.infoUpdatedAt = info.updatedAt
    status_store:update(book_id, info)
end
status_store:update(book_id, {title = resolved_title})
print("title=" .. resolved_title)
book.author = book.author or status.author or ""
book.cover = book.cover or status.cover

local manager = DownloadManager:new{
    config = config,
    status_store = status_store,
}
local force = os.getenv("RM_WEREAD_FORCE") == "1"
local stop_after = tonumber(os.getenv("RM_WEREAD_STOP_AFTER") or "")
local opening_progress = os.getenv("RM_WEREAD_OPENING_PROGRESS") or "auto"
local job = manager:start_full_download(book, {
    force = force,
    opening = stop_after ~= nil and opening_progress ~= "0",
})
print("bookId=" .. tostring(book_id))
print("force=" .. tostring(force))
if stop_after then
    print("stop_after=" .. tostring(stop_after))
    print("opening_progress=" .. tostring(opening_progress))
end
if job.state == "done" then
    print("state=done cached=true")
    print("done path=" .. tostring(job.path or ""))
    print("selected=" .. tostring(#(job.selected or {})))
    print("assets=already-packaged")
    print("failed=0")
    emit_result("full", job.path)
    os.exit(0)
end

local cache = job.settings:get("cache", {})
print("download_book_images=" .. tostring(cache.download_book_images))
print("chapters=" .. tostring(#(job.chapters or {})))

while job.state ~= "done" do
    local state = manager:step_full_download(job)
    print(
        "state=" .. tostring(state) ..
        " index=" .. tostring((job.index or 1) - 1) .. "/" .. tostring(#(job.chapters or {})) ..
        " selected=" .. tostring(#(job.selected or {})) ..
        " assets=" .. tostring(#(job.assets or {})) ..
        " failed=" .. tostring(#(job.failed or {}))
    )
    if stop_after and #job.selected >= stop_after then
        local opening_path = manager:finish_opening_download(job)
        print("stopped_after=" .. tostring(#job.selected))
        print("opening path=" .. tostring(opening_path or ""))
        emit_result("opening", opening_path)
        os.exit(0)
    end
end

print("done path=" .. tostring(job.path or ""))
print("selected=" .. tostring(#(job.selected or {})))
print("assets=" .. tostring(#(job.assets or {})))
print("failed=" .. tostring(#(job.failed or {})))
emit_result(#(job.failed or {}) > 0 and "partial" or "full", job.path)

end)

if not ok then
    print("state=error message=" .. sanitize_error(err))
    os.exit(1)
end
