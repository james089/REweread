-- No network, device paths, credentials or user data are accessed by this test.
local repo = assert(arg[1])
package.path = repo .. "/apps/weread-move/lib/?.lua;" .. package.path
-- Host Lua lacks KOReader's JSON extension; use a fixture encoder while keeping
-- the source-owned real decoder, including the protocol filtering path.
local function encode(value)
    if type(value) == "string" then return '"' .. value:gsub('[%z\1-\31\\"]', function(c)
        return string.format("\\u%04x", c:byte())
    end) .. '"' end
    if type(value) == "boolean" or type(value) == "number" then return tostring(value) end
    assert(type(value) == "table")
    local rows = {}
    if #value > 0 then
        for _, item in ipairs(value) do rows[#rows + 1] = encode(item) end
        return "[" .. table.concat(rows, ",") .. "]"
    end
    for key, item in pairs(value) do rows[#rows + 1] = encode(key) .. ":" .. encode(item) end
    return "{" .. table.concat(rows, ",") .. "}"
end
local fallback = require("json_util")
package.loaded["json_util"] = nil
package.preload["json"] = function() return {encode = encode, decode = fallback.decode} end
local Json = require("json_util")
local Sha = require("sha256")
local source = repo .. "/WereadDownloaderPlugin/lua/adapter.lua"
local function check(value) assert(value, "private adapter behavior failed at line " .. debug.getinfo(2, "l").currentline) end
local private = "/home/root/.local/share/weread-downloader"
local function run(operation, partial, age, responseCode, brokenParallel, renewal)
    local lines, writes, paths = {}, {}, {}
    local shelfRequests = 0
    local mask, renamed, isolated = false, false, false
    local scope = Sha.hex("weread-downloader-account-v1\31synthetic")
    local account = private .. "/accounts/" .. scope
    local Config = {}
    function Config:new(opts)
        local expected = operation == "login" and "/dev/null" or private .. "/session.json"
        check(opts.config_path == "/dev/null" and opts.session_path == expected)
        isolated = true
        local config = {values = {cookies = {wr_vid = "synthetic"}}, data_dir = ""}
        function config:set(key, value) self.values[key] = value end
        function config:get(key, default) return self.values[key] or default end
        function config:is_cookie_configured() return true end
        return config
    end
    local cache = {}
    function cache:new(root)
        check(root == account)
        return {save_shelf = function(_, books) check(#books == 1) end,
            load_shelf = function() return {savedAt = 1000 - (age or 0), books = {{bookId = "synthetic-1", title = "synthetic"}}} end}
    end
    local statuses = {}
    function statuses:new(opts)
        check(opts.root == account)
        return {get = function() return {downloadState = partial and "partial" or "full",
            failedChapterCount = partial and 1 or 0, fullFile = account .. "/books/synthetic-1/full.epub", imageAssets = true} end}
    end
    local manager = {}
    local starts, serialSteps = 0, 0
    local mapSaved = 0
    local content = {fetch_chapter_xhtml = function() return "<p>synthetic chapter</p>" end}
    function manager:new(opts)
      check(opts.cache_dir == account .. "/books")
      check(opts.config.data_dir == account)
        return {start_full_download = function()
            starts = starts + 1
            local before = mapSaved
            content.fetch_chapter_xhtml({}, {}, {bookId = "synthetic-1"}, {chapterUid = "chapter-1"})
            check(mapSaved == before + 1)
            return {state = brokenParallel and starts == 2 and "running" or "done"}
        end, step_full_download = function(_, job)
            serialSteps = serialSteps + 1; job.state = "done"; job.selected = {1}; job.chapters = {1}
        end}
    end
    local modules = {
        koreader_paths = {append = function(path) check(path:match("/appload/koreader$")) end},
        ffi = {cdef = function() end, C = {getppid = function() return 123 end,
            prctl = function() return 0 end, umask = function(value) mask = value == 63 end}},
        json_util = Json, config_bridge = Config, sha256 = Sha, shelf_cache = cache,
        reading_progress = {new = function(settings)
            check(settings.data_dir == account)
            return {cache = {books = {}}, close = function() end}
        end},
        session_auth = {prepare = function() end, renew = function(_, force)
            if force and renewal == "success" then responseCode = 200; return true, false end
            return false, renewal ~= "offline"
        end, name = function() return "synthetic account" end},
        parallel_download = {run = function() if brokenParallel then error("synthetic scheduler failure") end; return false end},
        download_io = dofile(repo .. "/WereadDownloaderPlugin/lua/download_io.lua"),
        shelf_service = {new = function(_, settings)
            check(settings:is_cookie_configured())
            check(settings.data_dir == account)
            return {client = {request = function(_, request)
                    shelfRequests = shelfRequests + 1
                    check(request.method == "GET" and request.url == "https://weread.qq.com/web/shelf/sync?userVid=synthetic&synckey=0&lectureSynckey=0")
                    if responseCode and responseCode < 0 then return encode({errCode = responseCode}), 200 end
                    return encode({books = {{bookId = "synthetic-1", title = "synthetic"}, {bookId = "../escape"}}}), responseCode or 200
                end, json_decode = function(_, text) return Json.decode(text) end},
                load_shelf = function(self) return self.client:gateway("/shelf/sync").books end}
        end},
        cover_cache = {new = function() return {ensure_cover = function(book) book.localCover = account .. "/covers/synthetic-1.jpg" end} end},
        book_status_store = statuses, download_manager = manager,
        ["lib.content"] = content,
        progress_source_store = {save = function(path, id, uid, html)
            check(path == account and id == "synthetic-1" and uid == "chapter-1" and html == "<p>synthetic chapter</p>")
            mapSaved = mapSaved + 1; return true
        end},
    }
    local environment = setmetatable({arg = {operation, "synthetic-1"}, package = {path = ""}}, {__index = _G})
    local internal_error
    environment.pcall = function(fn, ...)
        local success, result = pcall(fn, ...)
        if not success then internal_error = result end
        return success, result
    end
    environment.require = function(name) check(modules[name] ~= nil); return modules[name] end
    environment.print = function(text) lines[#lines + 1] = Json.decode(text) end
    environment.io = {stdout = {setvbuf = function() end}, open = function(path, mode)
        check(mask and path == private .. "/session.json.pending" and mode == "wb")
        paths[#paths + 1] = path
        return {write = function(_, bytes) writes[#writes + 1] = bytes; return true end,
            close = function() return true end}
    end}
    environment.os = {
        setlocale = function() end, time = function() return 1000 end,
        execute = function(command)
            check(command:find(private, 1, true))
            check(command:match("^mkdir %-p ") or command:match("^chmod 700 "))
            return 0
        end,
        rename = function(from, to)
            check(from == private .. "/session.json.pending" and to == private .. "/session.json")
            renamed = true; return true
        end,
        exit = function() error("synthetic exit", 0) end,
    }
    environment.dofile = function(path)
        check(path == "/home/root/weread-downloader/helper/tools/login-qr.lua")
        local settings = Config:new()
        settings:flush()
        environment.print(Json.encode({state = "qr", uid = "example", confirm_url = "https://weread.qq.com/web/confirm?uid=example"}))
        environment.print(Json.encode({state = "done", cookies = {ignored = "synthetic"}, accessToken = "example"}))
    end
    local ok = pcall(assert(loadfile(source, "t", environment)))
    check(mask and isolated)
    for _, row in ipairs(lines) do check(row.uid == nil and row.cookies == nil and row.accessToken == nil) end
    if operation == "login" then assert(ok, tostring(internal_error)); check(renamed and #writes == 1 and lines[1].state == "qr" and lines[2].state == "authenticated") end
    if operation == "status" then check(lines[1].accountScope == scope) end
    if operation == "logout" then
        check(lines[1].state == "logged_out" and #writes == 1)
        local saved = Json.decode(writes[1]); check(saved.logged_out and next(saved.cookies) == nil and saved.account_name == "")
    end
    if partial then check(not ok and lines[#lines].state == "error" and lines[#lines].code == "operation_failed")
    else assert(ok, tostring(internal_error)) end
    if operation == "cached-shelf" then check(shelfRequests == 0 and lines[1].cached) end
    if operation == "shelf" then check(shelfRequests == (renewal == "success" and 2 or 1)) end
    if (responseCode == 401 or responseCode == -2012) and renewal ~= "offline" then check(#writes == 1 and Json.decode(writes[1]).logged_out == true) end
    if renewal == "offline" then check(#writes == 0 and lines[#lines].state == "error") end
    if responseCode == 500 then check(#writes == 0) end
    if brokenParallel then check(starts == 2 and serialSteps == 1 and lines[#lines].state == "downloaded") end
    return lines
end
run("login")
run("status")
run("logout")
check(run("cached-shelf")[1].stale == false)
check(run("cached-shelf", false, 301)[1].stale == true)
check(run("shelf")[1].books[1].bookId == "synthetic-1")
check(run("shelf", false, 0, 401)[1].state == "login_required")
check(run("shelf", false, 0, -2012)[1].state == "login_required")
check(run("shelf", true, 0, 500)[1].state == "error")
check(run("download")[1].state == "downloaded")
run("download", true)
run("download", false, nil, nil, true)
check(run("shelf", false, 0, 401, false, "success")[1].state == "shelf")
run("shelf", true, 0, 401, false, "offline")
print("independent credentials, private caches, filtered QR protocol and partial-download rejection ok")
