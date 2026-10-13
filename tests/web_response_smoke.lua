package.path = "apps/weread-move/lib/?.lua;" .. package.path

-- Exercise the real download client adapter, before an empty catalog can be
-- incorrectly classified as a special or copyright-restricted book.
package.preload["lib.client"] = function()
    return { new = function()
        return { post_json = function() return { errCode = -2012 }, 200, {} end }
    end }
end
package.preload["lib.content"] = function() return {} end
package.preload["lib.cookie"] = function() return {} end
package.preload["lib.weread"] = function() return {} end
local DownloadManager = require("download_manager")
local client = DownloadManager:new():client({})
local ok, err = pcall(function() client:post_json("https://weread.qq.com/web/book/chapterInfos", {}) end)
assert(not ok and tostring(err):find("扫码登录", 1, true), "download client must reject catalog login errors")
local Response = require("web_response")

for _,code in ipairs({ -2012, "-2012", -2013 }) do
    local ok, err = pcall(Response.check, { errCode = code, errMsg = "private server detail" })
    assert(not ok, "HTTP-success login rejection must fail")
    assert(tostring(err):find("扫码登录", 1, true), "login rejection must request QR login")
    assert(not tostring(err):find("private", 1, true), "remote details must not leak")
end
local ok, err = pcall(Response.check, { errCode = -999, errMsg = "private server detail" })
assert(not ok and tostring(err):find("-999", 1, true), "other business errors must retain the code")
assert(not tostring(err):find("private", 1, true), "unknown errors must not leak server details")
for _,response in ipairs({ {}, { errCode = 0 }, { errCode = "0" }, { data = {} } }) do
    assert(Response.check(response) == response, "valid responses must pass through")
end

-- Run the actual renewal helper with a rejected HTTP-success response.
local emitted, exit_code, flushed
package.preload["config_bridge"] = function()
    return { new = function() return {
        is_cookie_configured = function() return true end,
        flush = function() flushed = true end,
        redacted_status = function() return {} end,
    } end }
end
package.preload["koreader_paths"] = function() return { append = function() end } end
package.loaded["lib.client"] = { new = function() return {
    renew_cookie = function() return { errCode = -2013 }, 200, {} end,
} end }
package.loaded["json_util"] = { encode = function(row) emitted = row; return "test-result" end }
local original_exit, original_print = os.exit, print
os.exit = function(code) exit_code = code end
print = function() end
assert(loadfile("apps/weread-move/tools/renew-cookie.lua"))()
os.exit, print = original_exit, original_print
assert(exit_code == 1 and emitted.state == "error", "rejected renewal must not report success")
assert(not flushed, "rejected renewal must not explicitly save success state")
print("web response behavior ok")
