-- Source-owned adapter for the device's existing libcurl. No cookies, payloads
-- or responses are logged or cached here. One owner serves one download job.
local M = {}
local ok_ffi, ffi = pcall(require, "ffi")
local curl
if ok_ffi then
    local ok = pcall(ffi.cdef, [[
        struct rm_curl_slist { char *data; struct rm_curl_slist *next; };
        int curl_global_init(long flags);
        void *curl_easy_init(void);
        void curl_easy_cleanup(void *handle);
        void curl_easy_reset(void *handle);
        int curl_easy_setopt(void *handle, int option, ...);
        int curl_easy_perform(void *handle);
        int curl_easy_getinfo(void *handle, int info, ...);
        struct rm_curl_slist *curl_slist_append(struct rm_curl_slist *, const char *);
        void curl_slist_free_all(struct rm_curl_slist *);
    ]])
    if ok then
        for _, name in ipairs({ "/usr/lib/libcurl.so.4", "libcurl.so.4" }) do
            local loaded, library = pcall(ffi.load, name)
            if loaded then curl = library; break end
        end
    end
end

function M.new(options)
    if not curl or curl.curl_global_init(3) ~= 0 then return nil end
    local handle = curl.curl_easy_init()
    if handle == nil then return nil end
    handle = ffi.gc(handle, curl.curl_easy_cleanup)
    local ctx = {}
    local writer = ffi.cast("size_t (*)(char *, size_t, size_t, void *)", function(data, size, count)
        local length = tonumber(size * count)
        local ok, accepted = pcall(ctx.sink, ffi.string(data, length))
        if not ok or not accepted then ctx.failed = true; return 0 end
        return length
    end)
    local header = ffi.cast("size_t (*)(char *, size_t, size_t, void *)", function(data, size, count)
        local length = tonumber(size * count)
        local line = ffi.string(data, length):gsub("[\r\n]+$", "")
        if line:match("^HTTP/") then
            ctx.status, ctx.headers = line, {}
        else
            local name, value = line:match("^([^:]+):%s*(.*)$")
            if name then
                name = name:lower()
                local previous = ctx.headers[name]
                if previous and name == "set-cookie" then
                    if type(previous) ~= "table" then previous = { previous }; ctx.headers[name] = previous end
                    previous[#previous + 1] = value
                elseif previous then ctx.headers[name] = previous .. ", " .. value
                else ctx.headers[name] = value end
            end
        end
        return length
    end)
    -- Reclaim callback slots as well as the connection handle if a caller
    -- releases a client without explicitly closing it.
    ffi.gc(handle, function(pointer)
        curl.curl_easy_cleanup(pointer)
        writer:free(); header:free()
    end)
    local owner = { error_buffer = ffi.new("char[256]"), handle = handle, writer = writer, header = header, ctx = ctx, requests = 0, connections = 0 }
    function owner:close()
        if not self.handle then return end
        curl.curl_easy_cleanup(ffi.gc(self.handle, nil))
        self.handle = nil
        self.writer:free(); self.header:free()
        self.ctx.sink, self.ctx.headers = nil, nil
    end
    function owner:request(opts)
        if not self.handle then return nil, "transport closed" end
        ctx.sink, ctx.headers, ctx.status, ctx.failed = opts.sink, {}, nil, false
        curl.curl_easy_reset(self.handle) -- retains connections, DNS and TLS sessions
        local list
        local function long(option, value)
            assert(curl.curl_easy_setopt(self.handle, option, ffi.new("long", value)) == 0, "transport option failed")
        end
        local function pointer(option, value)
            assert(curl.curl_easy_setopt(self.handle, option, value) == 0, "transport option failed")
        end
        local ok, a, b, c, d = pcall(function()
            local body = opts.body
            if body == nil and opts.source then
                local chunks, bytes = {}, 0
                while true do
                    local chunk, err = opts.source()
                    if not chunk then if err then error("request source failed") end; break end
                    bytes = bytes + #chunk
                    if bytes > 8 * 1024 * 1024 then error("request body too large") end
                    chunks[#chunks + 1] = chunk
                end
                body = table.concat(chunks)
            end
            self.error_buffer[0] = 0
            pointer(10010, self.error_buffer)
            pointer(10002, opts.url) -- URL
            pointer(10004, "") -- use direct networking, never implicit proxy environment
            long(181, 3) -- HTTP/HTTPS only
            long(52, 0) -- redirects stay with the client's credential-aware handler
            long(64, 1); long(81, 2) -- TLS peer and hostname verification
            -- This device's default OpenSSL cipher offer is rejected by the
            -- reader endpoint. Use standard ECDHE/AES-GCM suites; no weak TLS
            -- suites or disabled certificate verification as a fallback.
            pointer(10083, "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256")
            pointer(10065, "/etc/ssl/certs/ca-certificates.crt")
            long(99, 1) -- no signals
            long(71, 4) -- bounded connection cache
            local timeout = opts.timeout
            local block = type(timeout) == "table" and timeout[1] or tonumber(timeout) or 15
            local total = type(timeout) == "table" and timeout[2] or nil
            long(156, math.max(1, block) * 1000)
            long(19, 1); long(20, math.max(1, block))
            if total and total > 0 then long(155, total * 1000) end
            pointer(20011, self.writer); pointer(20079, self.header)
            for name, value in pairs(opts.headers or {}) do
                if tostring(name):find("[\r\n]") or tostring(value):find("[\r\n]") then error("invalid header") end
                local appended = curl.curl_slist_append(list, tostring(name) .. ": " .. tostring(value))
                if appended == nil then error("header allocation failed") end
                list = appended
            end
            -- LuaSocket doesn't add Expect: 100-continue; match that behavior.
            local appended = curl.curl_slist_append(list, "Expect:")
            if appended == nil then error("header allocation failed") end
            list = appended
            pointer(10023, list)
            if body ~= nil then pointer(10015, body); long(60, #body) end
            if opts.method == "HEAD" then long(44, 1) end
            pointer(10036, opts.method or (body and "POST" or "GET"))
            local result
            if options and options.perform then result = options.perform(self.handle)
            else result = curl.curl_easy_perform(self.handle) end
            self.last_result = result
            self.requests = self.requests + 1
            local connected = ffi.new("long[1]")
            curl.curl_easy_getinfo(self.handle, 0x20001a, connected)
            self.connections = self.connections + tonumber(connected[0])
            if result ~= 0 then return nil, ctx.failed and "response sink failed" or ("transport error " .. result) end
            local code = ffi.new("long[1]")
            if curl.curl_easy_getinfo(self.handle, 0x200002, code) ~= 0 then return nil, "missing response status" end
            local done, accepted = pcall(ctx.sink, nil)
            if not done or not accepted then return nil, "response sink failed" end
            return 1, tonumber(code[0]), ctx.headers, ctx.status
        end)
        if list ~= nil then curl.curl_slist_free_all(list) end
        ctx.sink, ctx.headers, ctx.status = nil, nil, nil
        if not ok then return nil, "transport setup failed" end
        return a, b, c, d
    end
    if jit then jit.off(owner.request, true) end
    return owner
end

-- Let the existing Client retain all authentication, redirect and error logic.
-- Lua helpers are single-threaded and do not yield in request(). Restore the
-- original function even on an error; never retry a sent request implicitly.
function M.attach(client)
    if client.persistentTransport or type(client.request) ~= "function" then return client end
    if os.getenv("RM_WEREAD_PERSISTENT_HTTP") == "0" then return client end
    local owner = M.new()
    if not owner then return client end
    local http = require("socket.http")
    local original = client.request
    client.persistentTransport = owner
    function client:request(...)
        local original_http = http.request
        http.request = function(opts, ...)
            if type(opts) ~= "table" or not opts.sink or opts.proxy or http.PROXY
                or opts.create or not tostring(opts.url):match("^https?://") then
                return original_http(opts, ...)
            end
            return owner:request(opts)
        end
        local ok, a, b, c, d = pcall(original, self, ...)
        http.request = original_http
        if not ok then error(a, 0) end
        return a, b, c, d
    end
    return client
end
return M
