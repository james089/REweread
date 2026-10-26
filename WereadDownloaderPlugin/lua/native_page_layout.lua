-- Read-only validation of native EPUB/PDF page identity, including cPages v2.
-- Never rewrite the tablet's CRDT page records or infer cloud positions here.
local M={}
local function integer(v)return type(v)=='number' and v==v and v==math.floor(v)end
local function array(v,n)
 if type(v)~='table' or #v~=n then return false end
 for k in pairs(v)do if not integer(k) or k<1 or k>n then return false end end
 return true
end
local function value(v)if type(v)=='table' then return v.value end end
function M.identityCount(content)
 if type(content)~='table' then return nil end
 local n=content.pageCount
 if not integer(n) or n<1 or n>5000 then return nil end
 if content.formatVersion==2 or content.cPages~=nil then
  local cp=content.cPages
  if content.formatVersion~=2 or type(cp)~='table' or value(cp.original)~=n or not array(cp.pages,n)then return nil end
  local seen,previous={},nil
  for i,page in ipairs(cp.pages)do
   if type(page)~='table' or type(page.id)~='string' or #page.id~=36 or not page.id:match('^[%x-]+$')then return nil end
   local id=page.id:lower();local idx=value(page.idx);local deleted=value(page.deleted)
   if seen[id] or type(idx)~='string' or #idx==0 or #idx>512 or (previous and idx<=previous)
       or value(page.redir)~=i-1 or (page.deleted~=nil and deleted~=false and deleted~=0)then return nil end
   seen[id]=true;previous=idx
  end
  return n
 end
 if content.originalPageCount~=n or not array(content.pages,n) or not array(content.redirectionPageMap,n)then return nil end
 for i,page in ipairs(content.redirectionPageMap)do if page~=i-1 then return nil end end
 return n
end
return M
