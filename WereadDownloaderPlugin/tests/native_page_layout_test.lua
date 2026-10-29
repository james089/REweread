package.path=assert(arg[1])..'/?.lua;'..package.path
local L=require('native_page_layout')
local function clone(v)
 if type(v)~='table' then return v end
 local out={};for k,x in pairs(v)do out[k]=clone(x)end;return out
end
local function cell(v)return {timestamp='1:1',value=v}end
local legacy={pageCount=3,originalPageCount=3,pages={'a','b','c'},redirectionPageMap={0,1,2}}
local v2={formatVersion=2,pageCount=3,cPages={original=cell(3),pages={}}}
for i=1,3 do v2.cPages.pages[i]={id=string.format('00000000-0000-4000-8000-%012x',i),idx=cell(string.char(96+i)),redir=cell(i-1)}end
assert(L.identityCount(legacy)==3 and L.identityCount(v2)==3)
local mutations={
 function(x)x.pageCount=3.5 end,
 function(x)x.pageCount=0 end,
 function(x)x.pageCount=5001 end,
 function(x)x.formatVersion=3 end,
 function(x)x.cPages.original.value=4 end,
 function(x)x.cPages.pages[3]=nil end,
 function(x)x.cPages.pages[2].id=x.cPages.pages[1].id end,
 function(x)x.cPages.pages[2].idx.value=x.cPages.pages[1].idx.value end,
 function(x)x.cPages.pages[2].idx.value='0' end,
 function(x)x.cPages.pages[2].redir.value=0 end,
 function(x)x.cPages.pages[2].redir=nil end,
 function(x)x.cPages.pages[2].deleted=cell(true) end,
 function(x)x.cPages.pages[2].deleted=true end,
 function(x)x.cPages.pages[2].id='invalid' end,
 function(x)x.cPages.pages[2].idx.value='' end,
 function(x)x.cPages.pages.extra='not-an-array' end,
 function(x)x.cPages.original=3 end,
}
for i,change in ipairs(mutations)do local x=clone(v2);change(x);assert(L.identityCount(x)==nil,'invalid v2 layout accepted: '..i)end
local x=clone(v2);x.cPages.pages[2].deleted=cell(false);assert(L.identityCount(x)==3)
x=clone(legacy);x.redirectionPageMap={1,0,2};assert(L.identityCount(x)==nil)
x=clone(legacy);x.pages=nil;assert(L.identityCount(x)==nil)
x=clone(legacy);x.formatVersion=2;assert(L.identityCount(x)==nil)
x=clone(v2);x.cPages.original.value=2;x.pageCount=2;x.cPages.pages[3]=nil;assert(L.identityCount(x)==2)
assert(L.identityCount(nil)==nil and L.identityCount({})==nil)
print('native layout: legacy/cPages identity mapping and malformed/reordered/deleted page refusal passed')

-- CRDT history: removed records, storage order differing from idx, repeated PDF
-- pages and an omitted original page. Only active native order determines sync.
local history=clone(v2)
history.cPages.pages[3].redir.value=1
local tomb=clone(history.cPages.pages[1]);tomb.id='00000000-0000-4000-8000-000000000004';tomb.deleted=cell(true)
tomb.idx=nil;tomb.redir=nil
history.cPages.pages={history.cPages.pages[3],tomb,history.cPages.pages[1],history.cPages.pages[2]}
local map=assert(L.resolve(history))
assert(map.pageCount==3 and map.originalCount==3)
assert(L.originalPage(map,0)==0 and L.originalPage(map,1)==1 and L.originalPage(map,2)==1)
assert(L.nativePage(map,1,2)==2 and L.nativePage(map,1,0)==1 and L.nativePage(map,2,0)==nil)
assert(L.originalPage(map,-1)==nil and L.originalPage(map,3)==nil)
assert(L.originalPage(map,0.5)==nil and L.nativePage(map,3)==nil)
assert(L.resolve(legacy).originalCount==3)
local reordered=clone(v2);reordered.cPages.pages[2].redir.value=2;reordered.cPages.pages[3].redir.value=1
assert(L.originalPage(assert(L.resolve(reordered)),1)==2)
local blank=clone(v2);blank.cPages.pages[2].redir.value=-1
map=assert(L.resolve(blank));assert(L.originalPage(map,1)==nil and L.nativePage(map,1)==nil)
for _,change in ipairs({
 function(x)x.pageCount=4 end,
 function(x)x.cPages.pages[1].redir.value=3 end,
 function(x)x.cPages.pages[1].redir.value=0.5 end,
 function(x)x.cPages.pages[1].deleted=cell('true') end,
 function(x)x.cPages.pages[1].idx.value=x.cPages.pages[2].idx.value end,
 function(x)x.cPages.pages[1].id=x.cPages.pages[2].id end,
})do local x=clone(v2);change(x);assert(L.resolve(x)==nil)end
assert(L.resolve(history) and history.cPages.pages[1].idx.value=='c','resolver must not reorder input records')
print('native layout: tombstones, active ordering, duplicate/omitted/blank PDF pages and inverse mapping passed')

local tied=clone(v2);tied.cPages.pages[3].idx.value='b';tied.cPages.pages[3].redir.value=1
map=assert(L.resolve(tied));assert(L.originalPage(map,2)==1)
tied.cPages.pages[3].redir.value=2;assert(L.resolve(tied)==nil)
