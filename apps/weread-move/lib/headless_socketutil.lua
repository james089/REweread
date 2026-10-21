-- Network helpers run outside KOReader's display process.
local socket = require("socket")
local http = require("socket.http")
local https = require("ssl.https")
local ltn12 = require("ltn12")
local M = { block_timeout = 60, total_timeout = -1 }
function M:set_timeout(block_timeout, total_timeout)
    self.block_timeout = block_timeout or 5
    self.total_timeout = total_timeout or 15
    http.TIMEOUT, https.TIMEOUT = self.block_timeout, self.block_timeout
end
function M:reset_timeout()
    self.block_timeout, self.total_timeout = 60, -1
    http.TIMEOUT, https.TIMEOUT = 60, 60
end
local original_tcp = socket.tcp
socket.tcp = function()
    local connection, err = original_tcp()
    if not connection then return nil, err end
    connection:settimeout(M.block_timeout, "b")
    connection:settimeout(M.total_timeout, "t")
    return connection
end
function M.table_sink(target)
    if M.total_timeout < 0 then return ltn12.sink.table(target) end
    target = target or {}
    local started = socket.gettime()
    return function(chunk)
        if socket.gettime() - started > M.total_timeout then return nil, "sink timeout" end
        if chunk then target[#target + 1] = chunk end
        return 1
    end, target
end
return M
