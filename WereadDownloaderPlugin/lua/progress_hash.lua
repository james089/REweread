-- Large native metadata files must not use the short-account-ID Lua SHA helper:
-- its byte varargs can exceed LuaJIT's stack. Use the tablet's existing crypto
-- runtime in this isolated process; no keys or credentials are involved.
local ffi=require('ffi')
ffi.cdef[[unsigned char *SHA256(const unsigned char*,size_t,unsigned char*);]]
local crypto=ffi.load('/usr/lib/libcrypto.so.3')
local M={}
function M.hex(text)
    assert(type(text)=='string' and #text<=8*1024*1024,'progress_hash_unavailable')
    local digest=ffi.new('unsigned char[32]')
    assert(crypto.SHA256(text,#text,digest)~=nil,'progress_hash_unavailable')
    return (ffi.string(digest,32):gsub('.',function(c)return string.format('%02x',c:byte())end))
end
return M
