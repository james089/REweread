-- Private, account-scoped source-coordinate maps captured during chapter
-- download. The XHTML itself remains in the verified chapter cache.
local Json=require('json_util')
local Source=require('progress_source_map')
local Hash=require('progress_hash')
local ffi=require('ffi')
ffi.cdef[[char *realpath(const char*,char*); long readlink(const char*,char*,unsigned long);]]
local M={}
local function valid(v)return type(v)=='string' and #v>0 and #v<=128 and v:match('^[%w_-]+$') end
local function canonical(path)
    local buffer=ffi.new('char[4096]')
    return ffi.C.realpath(path,buffer)~=nil and ffi.string(buffer)==path
end
local function path(root,id,uid)return root..'/books/'..id..'/source-indexes/'..uid..'.json'end
local function read(file)
    if not canonical(file) then return nil end
    local handle=io.open(file,'rb');if not handle then return nil end
    local size=handle:seek('end');if not size or size>2*1024*1024 then handle:close();return nil end
    handle:seek('set');local bytes=handle:read('*a');handle:close()
    local ok,value=pcall(Json.decode,bytes)
    return ok and type(value)=='table' and value or nil
end
function M.save(root,id,uid,html)
    if not valid(id) or not valid(uid) or type(html)~='string' then return false end
    local index=Source.index(html);if not index then return false end
    local compact=Source.compact(index);if not compact then return false end
    compact.bookId=id;compact.chapterUid=uid;compact.textHash=Hash.hex(index.text)
    local directory=root..'/books/'..id..'/source-indexes'
    local quoted="'"..directory:gsub("'","'\\''").."'"
    if os.execute('mkdir -p '..quoted)~=0 or not canonical(directory) then return false end
    local target=path(root,id,uid);local temporary=target..'.pending'
    local link=ffi.new('char[4096]')
    if ffi.C.readlink(temporary,link,4096)>=0 then return false end
    local file=io.open(temporary,'wb');if not file then return false end
    local ok=file:write(Json.encode(compact));local closed=file:close()
    if not ok or not closed or not os.rename(temporary,target) then os.remove(temporary);return false end
    return true
end
function M.locate(root,id,uid,html,page)
    if not valid(id) or not valid(uid) or type(html)~='string' then return nil,'invalid' end
    local map=read(path(root,id,uid))
    if not map or map.version~=1 or map.bookId~=id or map.chapterUid~=uid
        or type(map.textHash)~='string' then return nil,'missing' end
    local index=Source.index(html)
    if not index then return nil,'cache-mismatch' end
    local visible=index.text
    if #visible~=map.textBytes or Hash.hex(visible)~=map.textHash then return nil,'cache-mismatch' end
    local position,err=Source.locateMapped(page,visible,map)
    if not position then return nil,err end
    return position,{text=visible}
end
return M
