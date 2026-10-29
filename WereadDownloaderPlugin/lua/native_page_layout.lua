-- Read-only mapping between native page order and original PDF pages.
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
-- cPages is a CRDT history, not an ordered array of visible pages. Deleted
-- records remain stored; idx orders the active records and redir addresses PDF.
function M.resolve(content)
 if type(content)~='table' then return nil end
 if content.formatVersion~=2 and content.cPages==nil then
  local n=M.identityCount(content);if not n then return nil end
  local map={pageCount=n,originalCount=n,pdfPages={}}
  for i=1,n do map.pdfPages[i]=i-1 end
  return map
 end
 local n=content.pageCount;local cp=content.cPages
 if content.formatVersion~=2 or not integer(n) or n<1 or n>5000 or type(cp)~='table' then return nil end
 local original=value(cp.original);local records=cp.pages
 if not integer(original) or original<1 or original>5000 or type(records)~='table'
     or #records>20000 or not array(records,#records)then return nil end
 local active,seen={},{}
 for _,page in ipairs(records)do
  if type(page)~='table' or type(page.id)~='string' or #page.id~=36 or not page.id:match('^[%x-]+$')then return nil end
  local id=page.id:lower();if seen[id]then return nil end;seen[id]=true
  local deleted=value(page.deleted)
  if page.deleted~=nil and deleted~=false and deleted~=0 and deleted~=true and deleted~=1 then return nil end
  if deleted~=true and deleted~=1 then
   local idx=value(page.idx);local redir=value(page.redir)
   if type(idx)~='string' or #idx==0 or #idx>512 or not integer(redir) or redir< -1 or redir>=original then return nil end
   active[#active+1]={idx=idx,pdf=redir}
  end
 end
 if #active~=n then return nil end
 table.sort(active,function(a,b)return a.idx<b.idx end)
 local map={pageCount=n,originalCount=original,pdfPages={}}
 for i,page in ipairs(active)do
  -- Equal CRDT positions are safe only when every tied record addresses
  -- the same PDF page, so their tie-break order cannot change the mapping.
  if i>1 and active[i-1].idx==page.idx and active[i-1].pdf~=page.pdf then return nil end
  map.pdfPages[i]=page.pdf
 end
 return map
end
function M.originalPage(map,page)
 if type(map)~='table' or not integer(page) or page<0 or page>=map.pageCount then return nil end
 local pdf=map.pdfPages[page+1];if pdf and pdf>=0 then return pdf end
end
-- Repeated native pages share the same source text. Prefer the occurrence
-- nearest the current native page; never jump to a deleted/absent PDF page.
function M.nativePage(map,pdf,current)
 if type(map)~='table' or not integer(pdf) or pdf<0 or pdf>=map.originalCount then return nil end
 current=integer(current) and current>=0 and current<map.pageCount and current or 0
 local best,distance
 for i,v in ipairs(map.pdfPages)do
  if v==pdf then local d=math.abs(i-1-current)
   if not distance or d<distance then best=i-1;distance=d end
  end
 end
 return best
end
return M
