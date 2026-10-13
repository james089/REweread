package.path = "apps/weread-move/lib/?.lua;" .. package.path

local reads = 0
local function fake_client()
    return {
        post_json = function() return {} end,
        get_text = function()
            reads = reads + 1
            return '<script>{"psvts":"state-' .. reads .. '"}</script>'
        end,
    }
end
package.preload["lib.client"] = function() return { new = fake_client } end
package.preload["lib.content"] = function() return {} end
package.preload["lib.cookie"] = function() return {} end
package.preload["lib.weread"] = function() return {
    reader_url = function(id) return "https://weread.qq.com/web/reader/" .. id end,
} end
local Manager = require("download_manager")
local base = "https://weread.qq.com/web/reader/book-a"
local client = Manager:new():client({}, "book-a")
local first = client:get_text(base)
assert(client:get_text(base .. "kchapter-2") == first and reads == 1,
    "download chapters must reuse the same book's valid reader state")

local Cache = require("reader_state_cache")
local now = 100
reads = 0
client = Cache.attach(fake_client(), base, function() return now end)
first = client:get_text(base)
now = 129
assert(client:get_text(base .. "kchapter-2") == first and reads == 1, "state must be reusable before expiry")
now = 130
assert(client:get_text(base .. "kchapter-3") ~= first and reads == 2, "state must expire at 30 seconds")
client:invalidateReaderState()
client:get_text(base)
assert(reads == 3, "chapter failure invalidation must force a fresh reader request")
client:get_text("https://weread.qq.com/web/reader/book-b")
client:get_text("https://example.com/web/reader/book-a")
assert(reads == 5, "another book or host must never reuse reader state")
client:get_text(base .. "kchapter-4")
assert(reads == 5, "unrelated requests must not replace this book's state")
now = 99
client:get_text(base)
assert(reads == 6, "a backwards clock must expire reader state")

local invalid_reads = 0
client = Cache.attach({ get_text = function()
    invalid_reads = invalid_reads + 1
    return "login page without reader state"
end }, base)
client:get_text(base)
client:get_text(base)
assert(invalid_reads == 2, "a page without psvts must not be cached")

local attempts = 0
client = Cache.attach({ get_text = function()
    attempts = attempts + 1
    if attempts == 1 then error("network failure") end
    return '<script>{"psvts":"recovered"}</script>'
end }, base)
assert(not pcall(function() client:get_text(base) end), "network errors must propagate")
assert(client:get_text(base):find("recovered", 1, true) and attempts == 2,
    "a failed fetch must not poison the reader cache")
print("reader state cache behavior ok")
