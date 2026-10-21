local helper = arg[1] or "/home/root/weread-qt/helper"
package.path = (arg[2] or helper .. "/lib") .. "/?.lua;" .. helper .. "/lib/?.lua;" .. package.path
require("koreader_paths").append()
local transport = require("persistent_http")
local owner = assert(transport.new(), "existing device libcurl must load")
local base = "http://127.0.0.1:18765"
local function request(path, options)
    options = options or {}; options.url = base .. path
    local chunks = {}
    options.sink = function(chunk) if chunk then chunks[#chunks+1] = chunk end; return 1 end
    local ok, code, headers = owner:request(options)
    return ok, code, headers, table.concat(chunks)
end
local ok, code, headers, body = request("/one")
assert(ok and code == 200 and body == "alpha", "GET response")
local first = headers["x-connection"]
ok, code, headers, body = request("/echo", { method="POST", body="\0payload\255", headers={["Content-Type"]="application/octet-stream"}})
assert(ok and body == "\0payload\255" and headers["x-connection"] == first, "POST binary and connection reuse")
ok, code, headers, body = request("/metadata", {headers={["X-Marker"]="first"}})
assert(ok and body == "GET:first:0", "POST method/body reset")
ok, code, headers, body = request("/metadata")
assert(ok and body == "GET:none:0", "request headers must not leak across requests")
ok, code, headers, body = request("/binary"); assert(ok and body == "\0\1\255binary\0", "binary response")
ok, code, headers, body = request("/chunked"); assert(ok and body == "alpha", "chunk decoding")
ok, code, headers, body = request("/cookies"); assert(ok and type(headers["set-cookie"]) == "table" and #headers["set-cookie"] == 2, "preserve repeated cookies")
ok, code, headers = request("/redirect"); assert(ok and code == 302 and headers.location == "/one", "do not follow redirects")
ok, code, headers, body = request("/status"); assert(ok and code == 403 and body == "denied", "preserve HTTP errors for existing client")
assert(owner.connections == 1, "reuse one connection for healthy requests")
ok = request("/close"); assert(ok, "server close")
ok, code, headers = request("/one"); assert(ok and headers["x-connection"] ~= first, "reconnect after server close")
ok = request("/truncated"); assert(not ok, "reject truncated body")
ok = request("/delay", {timeout={1, 0.05}}); assert(not ok, "total timeout")
ok = owner:request({url=base.."/one", sink=function() return nil end}); assert(not ok, "sink abort")
owner:close(); owner:close()
assert(not owner:request({url=base.."/one"}), "closed owner")
local http = require("socket.http")
local original_http = http.request
local client = {request=function() error("fixture failure") end}
transport.attach(client)
assert(not pcall(client.request, client), "propagate client error")
assert(http.request == original_http, "restore original HTTP on exceptions")
if client.persistentTransport then client.persistentTransport:close() end
print("persistent-http-smoke=ok")
