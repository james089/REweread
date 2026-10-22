-- Large native metadata files must not use the short-account-ID Lua SHA helper:
-- its byte varargs can exceed LuaJIT's stack. Use the tablet's existing crypto
-- runtime in this isolated process; no keys or credentials are involved.
local ffi=require('ffi')
ffi.cdef[[unsigned char *SHA256(const unsigned char*,size_t,unsigned char*);]]
ffi.cdef[[void *EVP_MD_CTX_new(void);void EVP_MD_CTX_free(void*);const void *EVP_sha256(void);
int EVP_DigestInit_ex(void*,const void*,void*);int EVP_DigestUpdate(void*,const void*,size_t);
int EVP_DigestFinal_ex(void*,unsigned char*,unsigned int*);]]
local crypto=ffi.load('/usr/lib/libcrypto.so.3')
local M={}
function M.hex(text)
    assert(type(text)=='string' and #text<=8*1024*1024,'progress_hash_unavailable')
    local digest=ffi.new('unsigned char[32]')
    assert(crypto.SHA256(text,#text,digest)~=nil,'progress_hash_unavailable')
    return (ffi.string(digest,32):gsub('.',function(c)return string.format('%02x',c:byte())end))
end
function M.file(path)
    local f=io.open(path,'rb');if not f then return nil end
    local ctx=crypto.EVP_MD_CTX_new();if ctx==nil then f:close();return nil end
    local ok=crypto.EVP_DigestInit_ex(ctx,crypto.EVP_sha256(),nil)==1
    local total=0
    while ok do
        local chunk,err=f:read(65536);if not chunk then if err then ok=false end;break end
        total=total+#chunk
        ok=total<=128*1024*1024 and crypto.EVP_DigestUpdate(ctx,chunk,#chunk)==1
    end
    local digest=ffi.new('unsigned char[32]');local length=ffi.new('unsigned int[1]')
    ok=ok and crypto.EVP_DigestFinal_ex(ctx,digest,length)==1 and length[0]==32
    crypto.EVP_MD_CTX_free(ctx);f:close()
    if not ok then return nil end
    return (ffi.string(digest,32):gsub('.',function(c)return string.format('%02x',c:byte())end))
end
return M
