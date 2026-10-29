-- Host adapter only. Payload construction stays in the external Lua module
-- used by the plugin; hashing is delegated over private child-process pipes.
package.path = 'apps/weread-move/lib/?.lua;' .. package.path
local Json = require('json_util')
local function encode(value)
    local kind = type(value)
    if kind == 'string' then
        return '"' .. value:gsub('[%z\1-\31\\"]', function(c)
            if c == '\\' then return '\\\\' end
            if c == '"' then return '\\"' end
            return string.format('\\u%04x', c:byte())
        end) .. '"'
    elseif kind == 'number' or kind == 'boolean' then return tostring(value)
    elseif kind == 'table' then
        local parts = {}
        for key, item in pairs(value) do parts[#parts+1] = encode(tostring(key)) .. ':' .. encode(item) end
        return '{' .. table.concat(parts, ',') .. '}'
    end
    return 'null'
end
local function emit(value) io.write(encode(value), '\n'); io.flush() end
package.preload.bit = function()
    return {
        band = function(a,b) return (math.tointeger(a) & math.tointeger(b)) & 0xffffffff end,
        bxor = function(a,b) return (math.tointeger(a) ~ math.tointeger(b)) & 0xffffffff end,
        lshift = function(a,b) return (math.tointeger(a) << math.tointeger(b)) & 0xffffffff end,
    }
end
local function hash(algorithm, text)
    emit({hash=algorithm,text=text})
    return assert(Json.decode(assert(io.read('*l'))).digest)
end
package.preload['weread.lib.crypto'] = function()
    return { md5_hex=function(text)return hash('md5',text)end,
        sha256_hex=function(text)return hash('sha256',text)end }
end
local request = Json.decode(assert(io.read('*l')))
local W = assert(loadfile(arg[1]))()
local Protocol = require('read_sync_protocol')
if request.action == 'acknowledged' then emit({result=Protocol.response_succeeded(request.response)})
elseif request.action == 'readerPath' then emit({result=W.reader_url(request.bookId):gsub('^https://weread.qq.com','')})
else
    local constructor = request.opts.elapsed_seconds == nil and W.make_enter_read_payload or W.make_read_payload
    emit({result=constructor(request.opts)})
end
