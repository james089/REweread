-- Runs DownloadManager against the real chapter cache with a fake Content module.
-- Invoke locally with:
-- npx --yes --package=fengari-node-cli fengari tests/download-resume-smoke.lua

package.path = "apps/weread-move/lib/?.lua;" .. package.path

local registry = {}
local sequence = 0

local function copy(value)
    if type(value) ~= "table" then return value end
    local out = {}
    for key, item in pairs(value) do out[key] = copy(item) end
    return out
end

package.preload["json"] = function()
    return {
        encode = function(value)
            sequence = sequence + 1
            registry[sequence] = copy(value)
            return "test-manifest:" .. tostring(sequence)
        end,
        decode = function(value)
            local id = tonumber(tostring(value):match("(%d+)$"))
            assert(id and registry[id], "test JSON registry lost manifest")
            return copy(registry[id])
        end,
    }
end

local content = { catalog = {}, fetches = {}, failures = {}, saved = {} }

package.preload["lib.client"] = function()
    return { new = function(_, settings) return { settings = settings } end }
end

package.preload["lib.cookie"] = function()
    return { has_login_cookie = function() return true end }
end

package.preload["lib.weread"] = function()
    return { normalize_cover_url = function() return "" end }
end

package.preload["lib.content"] = function()
    return {
        ensure_reader_state = function(_, book) book.chapters = content.catalog end,
        fetch_catalog = function() return content.catalog end,
        fetch_single_chapter_content = function(_, _, _, chapter)
            local uid = tostring(chapter.chapterUid)
            content.fetches[uid] = (content.fetches[uid] or 0) + 1
            local remaining = content.failures[uid] or 0
            if remaining > 0 then
                content.failures[uid] = remaining - 1
                error("transient failure " .. uid)
            end
            return "<p>" .. uid .. "</p>", {
                { href = "images/" .. uid .. ".jpg", data = "asset-" .. uid },
            }
        end,
        save_book_epub = function(_, _, selected, _, suffix)
            table.insert(content.saved, { suffix = suffix, selected = #selected })
            return "/tmp/test-" .. suffix .. ".epub"
        end,
    }
end

local ChapterCache = require("chapter_cache")
local DownloadManager = require("download_manager")
local Sha256 = require("sha256")

assert(Sha256.hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
    "bundled SHA-256 must match the standard vector")

local function legacy_adler32(text)
    local a, b = 1, 0
    for index = 1, #text do
        a = (a + (text:byte(index) or 0)) % 65521
        b = (b + a) % 65521
    end
    return string.format("%04x%04x", b, a)
end

-- Both values are valid numeric account IDs.  Their digit deltas (+1,-1,-1,+1)
-- preserve Adler's byte sum and weighted byte sum, so the old scope collided.
assert(legacy_adler32("rm-weread-cache-v1\31" .. "1111")
    == legacy_adler32("rm-weread-cache-v1\31" .. "2002"),
    "regression fixture must collide under the retired Adler account scope")

local root = "/tmp/rm-weread-download-resume-smoke"
assert(os.execute("rm -rf " .. root))

local active_vid = "account-a"
local settings = {
    get = function(_, key)
        if key == "cookies" then return { wr_vid = active_vid } end
        return {}
    end,
    is_cookie_configured = function() return true end,
}
local config = { reload = function() end }
local status = {
    values = {},
    get = function(self, book_id) return self.values[book_id] or {} end,
    update = function(self, book_id, patch)
        local value = self.values[book_id] or {}
        for key, item in pairs(patch) do value[key] = item end
        self.values[book_id] = value
        return value
    end,
}

local function new_manager()
    local manager = DownloadManager:new{
        config = config,
        status_store = status,
        cache_dir = root,
        chapter_cache = ChapterCache:new{ root = root },
    }
    manager.settings = function() return settings end
    return manager
end

local original_vid = active_vid
active_vid = "1111"
local collision_scope_a = new_manager():account_cache_scope(settings)
active_vid = "2002"
local collision_scope_b = new_manager():account_cache_scope(settings)
assert(collision_scope_a ~= collision_scope_b,
    "SHA-256 account scopes must separate numeric IDs that collided under Adler")
active_vid = original_vid

local function run_to_end(manager, book, opts)
    local job = manager:start_full_download(book, opts or {})
    while job.state ~= "done" do
        manager:step_full_download(job)
    end
    return job
end

content.catalog = {
    { chapterUid = "a", title = "A", version = "1" },
    { chapterUid = "b", title = "B", version = "1" },
}
local book = { bookId = "book", title = "Book" }

local interrupted = new_manager():start_full_download(book)
new_manager():step_full_download(interrupted)
assert(content.fetches.a == 1, "first chapter must be fetched before interruption")

run_to_end(new_manager(), book)
assert(content.fetches.a == 1 and content.fetches.b == 1, "resume must reuse the verified completed chapter")
assert(content.saved[#content.saved].suffix == "full", "all verified chapters must produce a full EPUB")

local cache = ChapterCache:new{ root = root }
local damaged = assert(io.open(cache:chapter_path("book", "a"), "wb"))
damaged:write("corrupt")
damaged:close()
run_to_end(new_manager(), book, { force = true })
assert(content.fetches.a == 2 and content.fetches.b == 1, "corrupt chapter must be fetched again while intact cache is reused")

content.catalog = {
    { chapterUid = "a", title = "A", version = "2" },
    { chapterUid = "c", title = "C", version = "1" },
}
run_to_end(new_manager(), book, { force = true })
assert(content.fetches.a == 3 and content.fetches.c == 1, "catalog change must not reuse stale manifest chapters")

content.catalog = { { chapterUid = "scope", title = "Scope", version = "1" } }
run_to_end(new_manager(), book, { force = true })
active_vid = "account-b"
run_to_end(new_manager(), book, { force = true })
assert(content.fetches.scope == 2, "a different account scope must not reuse cached chapters")

content.catalog = {
    { chapterUid = "ok", title = "OK", version = "1" },
    { chapterUid = "retry", title = "Retry", version = "1" },
}
content.failures.retry = 2
local retried = run_to_end(new_manager(), book, { force = true })
assert(content.fetches.retry == 2 and #retried.failed == 1, "failed chapter must retry a bounded number of times")
assert(content.saved[#content.saved].suffix == "partial", "bounded failure must produce a partial EPUB")

print("download resume behavior ok")
