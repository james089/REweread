local helper = arg[1] or "/home/root/weread-qt/helper"
package.path = helper .. "/lib/?.lua;" .. package.path
require("koreader_paths").append()
local socket = require("socket")
local server = assert(socket.bind("127.0.0.1", 18765)); server:settimeout(0)
local clients, buffers, ids, serial = {}, {}, {}, 0
local deadline = socket.gettime() + 90
local function remove(c)
    buffers[c], ids[c] = nil, nil
    for i, value in ipairs(clients) do if value == c then table.remove(clients, i); break end end
    c:close()
end
while socket.gettime() < deadline do
    local reads = { server }; for _, c in ipairs(clients) do reads[#reads+1] = c end
    for _, c in ipairs(socket.select(reads, nil, 0.05)) do
        if c == server then
            local connection = server:accept()
            if connection then
                connection:settimeout(0); serial = serial + 1
                clients[#clients+1], buffers[connection], ids[connection] = connection, "", serial
            end
        else
            local text, err, partial = c:receive(8192)
            buffers[c] = buffers[c] .. (text or partial or "")
            local head_end = buffers[c]:find("\r\n\r\n", 1, true)
            if head_end then
                local header = buffers[c]:sub(1, head_end+3)
                local length = tonumber(header:lower():match("content%-length:%s*(%d+)")) or 0
                if #buffers[c] >= head_end + 3 + length then
                    local method, path = header:match("^(%S+)%s+(%S+)")
                    local body = buffers[c]:sub(head_end+4, head_end+3+length)
                    buffers[c] = buffers[c]:sub(head_end+4+length)
                    local response = "alpha"
                    local code, extra, close = "200 OK", "", false
                    if path == "/echo" then response = body
                    elseif path == "/binary" then response = "\0\1\255binary\0"
                    elseif path == "/metadata" then response = method .. ":" .. (header:lower():match("x%-marker:%s*([^\r\n]+)") or "none") .. ":" .. #body
                    elseif path == "/redirect" then code, extra = "302 Found", "Location: /one\r\n"
                    elseif path == "/cookies" then extra = "Set-Cookie: fixture_a=one; Path=/\r\nSet-Cookie: fixture_b=two; Path=/\r\n"
                    elseif path == "/status" then code, response = "403 Forbidden", "denied"
                    elseif path == "/close" then close = true
                    elseif path == "/truncated" then close = true
                    elseif path == "/delay" then socket.sleep(0.2)
                    elseif path == "/chunked" then extra = "Transfer-Encoding: chunked\r\n"; response = "5\r\nalpha\r\n0\r\n\r\n" end
                    if path ~= "/chunked" then extra = extra .. "Content-Length: " .. (#response + (path == "/truncated" and 10 or 0)) .. "\r\n" end
                    c:settimeout(1)
                    c:send("HTTP/1.1 " .. code .. "\r\nX-Connection: " .. ids[c] .. "\r\n" .. extra .. "Connection: " .. (close and "close" or "keep-alive") .. "\r\n\r\n" .. response)
                    c:settimeout(0)
                    if close then remove(c) end
                end
            end
            if err == "closed" and buffers[c] then remove(c) end
        end
    end
end
for _, c in ipairs(clients) do c:close() end
server:close()
