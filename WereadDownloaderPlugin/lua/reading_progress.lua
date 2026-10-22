-- Independent-account progress reads and explicitly requested single-book sync.
-- Never infer a remote text offset from a native page percentage.
local J=require('json_util')
local Text=require('progress_text')
local Source=require('progress_source_map')
local Verify=require('progress_cache_verify')
local Saved=require('saved_progress')
local Percent=require('progress_percent')
local M={}
local root='/home/root/.local/share/weread-downloader'
local library='/home/root/.local/share/remarkable/xochitl'
local ffi=require('ffi')
ffi.cdef[[char *realpath(const char*,char*);long readlink(const char*,char*,unsigned long); void FPDF_InitLibrary(void);void FPDF_DestroyLibrary(void);
void* FPDF_LoadDocument(const char*,const char*);void FPDF_CloseDocument(void*);
int FPDF_GetPageCount(void*);void* FPDF_LoadPage(void*,int);void FPDF_ClosePage(void*);
void* FPDFText_LoadPage(void*);void FPDFText_ClosePage(void*);
int FPDFText_CountChars(void*);unsigned int FPDFText_GetUnicode(void*,int);]]
local function need(v,code) if not v then error(code,0) end;return v end
local function canonical(p)local b=ffi.new('char[4096]');return ffi.C.realpath(p,b)~=nil and ffi.string(b)==p end
local function read(p,limit)
 if not canonical(p) then return nil end
 local f=io.open(p,'rb');if not f then return nil end
 local n=f:seek('end');if not n or n>(limit or 8*1024*1024) then f:close();return nil end
 f:seek('set');local s=f:read('*a');f:close();return s
end
local function json(p)local s=read(p);if not s then return nil end;local ok,v=pcall(J.decode,s);return ok and type(v)=='table' and v or nil end
local function valid(id)return type(id)=='string' and #id>0 and #id<=128 and id:match('^[%w_-]+$') end
local function percent(v)v=tonumber(v);if v and v==v and v>=0 and v<=100 then return v end end
local function signature(v)
 if type(v)~='table' then return '' end
 return table.concat({tostring(v.chapterUid or ''),tostring(v.chapterIdx or ''),tostring(v.chapterOffset or ''),
 tostring(v.progress or ''),tostring(v.summary or ''),tostring(v.updateTime or '')},'\31')
end
local function atomic(path,data)
 need(canonical(path:match('^(.*)/[^/]+$')),'progress_cache_unavailable')
 local temp=path..'.pending';need(ffi.C.readlink(temp,ffi.new('char[4096]'),4096)<0,'progress_cache_unavailable')
 local f=need(io.open(temp,'wb'),'progress_cache_unavailable')
 local ok=f:write(J.encode(data));local closed=f:close()
 if not ok or not closed then os.remove(temp);error('progress_cache_unavailable',0) end
 need(os.rename(temp,path),'progress_cache_unavailable')
end
function M.new(settings,scope,account,environment)
 local root=(environment and environment.root) or root
 local library=(environment and environment.library) or library
 local function pdf()return environment and environment.pdf or ffi.load('/usr/lib/libpdfium.so')end
 local self={settings=settings,scope=scope,account=account}
 local client=require('lib.client'):new(settings);require('persistent_http').attach(client)
 self.client=client;self.cache=json(account..'/reading-progress-v1.json') or {version=1,books={}}
 if self.cache.version~=1 or type(self.cache.books)~='table' then self.cache={version=1,books={}} end
 function self:close() if client.persistentTransport then client.persistentTransport:close()end end
 function self:save()atomic(account..'/reading-progress-v1.json',self.cache)end
 function self:fetch(id)
  need(valid(id),'invalid_book_id')
  local url=require('lib.weread').reader_url(id)
  local html,code=client:request({url=url,method='GET',timeout=12,headers={Referer=url,Accept='text/html',['Cache-Control']='no-cache'}})
  need(code and code>=200 and code<300,'progress_network_failed')
  local record=Saved.extract(html,J.decode)
  need(record and percent(record.progress)~=nil,'cloud_progress_unavailable')
  return record,html
 end
 function self:displayCatalog(id)
  need(valid(id),'invalid_book_id')
  local manifest=json(account..'/books/'..id..'/download-manifest.json')
  local trusted=manifest and manifest.accountScope=='sha256-'..require('sha256').hex('rm-weread-cache-v1\31'..settings:get('cookies',{}).wr_vid)
  self.cache.catalogs=self.cache.catalogs or {}
  local saved=self.cache.catalogs[id]
  if trusted and type(saved)=='table' and type(saved.chapters)=='table'
      and Verify.catalog(saved.chapters)==manifest.catalogFingerprint then return saved.chapters end
  local catalog=require('lib.content').fetch_catalog(client,{bookId=id})
  if trusted and Verify.catalog(catalog)==manifest.catalogFingerprint then
   self.cache.catalogs[id]={chapters=catalog};self:save()
  end
  return catalog
 end
 function self:displayPercent(id,record,catalog,original)
  local old=self.cache.books[id]
  if not catalog and old and old.displayMethod==1 and old.displayPosition==Percent.positionKey(record)
      and type(old.cloudChapter)=='string' and percent(old.displayProgress) and os.time()-(old.displayCalculatedAt or 0)>=0
      and os.time()-(old.displayCalculatedAt or 0)<300 then return old.displayProgress,old.displayCalculatedAt,old.cloudChapter end
  local Content=require('lib.content')
  catalog=catalog or self:displayCatalog(id)
  local chapter
  for _,c in ipairs(catalog)do if tostring(c.chapterUid)==tostring(record.chapterUid) then need(not chapter,'progress_not_located');chapter=c end end
  if not chapter then
   -- Some clients save a summary with chapterUid=0. Resolve display only from
   -- the complete, verified local download; incomplete/ambiguous data fails.
   local book=account..'/books/'..id
   local manifest=need(json(book..'/download-manifest.json'),'progress_not_located')
   need(manifest.accountScope=='sha256-'..require('sha256').hex('rm-weread-cache-v1\31'..settings:get('cookies',{}).wr_vid)
       and manifest.catalogFingerprint==Verify.catalog(catalog),'progress_not_located')
   local bytes,started=0,os.time()
   for i,c in ipairs(catalog)do
    local uid=tostring(c.chapterUid or '');need(valid(uid),'progress_not_located')
    local entry=manifest.chapters and manifest.chapters[uid]
    local text=need(read(book..'/chapters/'..uid..'.xhtml',2*1024*1024),'progress_not_located')
    bytes=bytes+#text;need(bytes<=40*1024*1024 and os.time()-started<=15,'progress_not_located')
    need(entry and entry.bytes==#text and entry.checksum==Verify.checksum(text)
        and entry.chapterFingerprint==Verify.chapter(c,i),'progress_not_located')
    local visible=Text.xhtml(text)
    if Percent.summaryPosition(visible,record.summary) then need(not chapter,'progress_not_located');chapter=c end
    -- A repeated summary even inside one chapter cannot prove uniqueness.
    local summary=Text.normalize(record.summary or '')
    local at=#summary>0 and visible:find(summary,1,true)
    need(not at or not visible:find(summary,at+1,true),'progress_not_located')
   end
   if chapter then local copy={};for k,v in pairs(record)do copy[k]=v end;copy.chapterUid=chapter.chapterUid;record=copy end
  end
  need(chapter,'progress_not_located')
  if original==nil and not (record.chapterOffset==0 and (record.summary==nil or record.summary=='')) then
   local put=Content.cache_annotation_source;Content.cache_annotation_source=function()end
   local ok,text=pcall(Content.fetch_chapter_xhtml,client,settings,{bookId=id,_content_format='epub'},chapter)
   Content.cache_annotation_source=put;need(ok,'progress_network_failed');original=text
  end
  return Percent.calculate(catalog,record,original),os.time(),tostring(chapter.title or ''),catalog
 end
 function self:localChapter(id,row,catalog,cacheOnly)
  need(valid(id) and type(row.uuid)=='string' and #row.uuid==36 and row.uuid:match('^[%x-]+$'),'progress_not_located')
  local base=library..'/'..row.uuid
  local metadata=need(read(base..'.metadata'),'progress_not_located')
  local contentBytes=need(read(base..'.content'),'progress_not_located')
  local meta,content=J.decode(metadata),J.decode(contentBytes)
  local page=tonumber(meta.lastOpenedPage);local count=tonumber(content.pageCount)
  need(not meta.deleted and meta.parent~='trash' and content.fileType=='epub' and page and count
      and page==math.floor(page) and page>=0 and page<count and canonical(base..'.pdf'),'progress_not_located')
  local Hash=require('progress_hash')
  local manifestBytes=need(read(account..'/books/'..id..'/download-manifest.json'),'progress_not_located')
  local manifest=J.decode(manifestBytes)
  need(manifest.accountScope=='sha256-'..require('sha256').hex('rm-weread-cache-v1\31'..settings:get('cookies',{}).wr_vid),'account_changed')
  local pdfHash=need(Hash.file(base..'.pdf'),'progress_not_located')
  local key=table.concat({'1',row.uuid,tostring(page),Hash.hex(contentBytes),Hash.hex(manifestBytes),pdfHash},':')
  self.cache.localChapters=self.cache.localChapters or {}
  local cached=self.cache.localChapters[id]
  if cached and cached.key==key and type(cached.title)=='string' then
   need(read(base..'.metadata')==metadata and read(base..'.content')==contentBytes,'progress_not_located')
   return cached.title
  end
  if cacheOnly then return nil end
  local function remember(title)
   need(read(base..'.metadata')==metadata and read(base..'.content')==contentBytes
       and read(account..'/books/'..id..'/download-manifest.json')==manifestBytes
       and Hash.file(base..'.pdf')==pdfHash,'progress_not_located')
   self.cache.localChapters[id]={key=key,title=title};self:save();return title
  end
  local P=pdf();P.FPDF_InitLibrary();local doc=P.FPDF_LoadDocument(base..'.pdf',nil);local text
  if doc~=nil then
   if P.FPDF_GetPageCount(doc)==count then
    local p=P.FPDF_LoadPage(doc,page)
    if p~=nil then
     local t=P.FPDFText_LoadPage(p)
     if t~=nil then local chars={};for i=0,math.min(P.FPDFText_CountChars(t),8192)-1 do chars[#chars+1]=Text.utf8(P.FPDFText_GetUnicode(t,i))end
      text=Text.normalize(table.concat(chars));P.FPDFText_ClosePage(t)end
     P.FPDF_ClosePage(p)
    end
   end
   P.FPDF_CloseDocument(doc)
  end
  P.FPDF_DestroyLibrary();need(text~=nil,'progress_not_located')
  if text=='' then
   need(read(base..'.metadata')==metadata and read(base..'.content')==contentBytes,'progress_not_located')
   return remember('当前页无正文')
  end
  local book=account..'/books/'..id
  need(manifest.accountScope=='sha256-'..require('sha256').hex('rm-weread-cache-v1\31'..settings:get('cookies',{}).wr_vid),'account_changed')
  catalog=catalog or self:displayCatalog(id)
  need(Verify.catalog(catalog)==manifest.catalogFingerprint,'progress_cache_changed')
  local matches,title,bytes,started=0,nil,0,os.time()
  for i,c in ipairs(catalog)do
   local uid=tostring(c.chapterUid or '');need(valid(uid),'progress_not_located')
   local record=manifest.chapters and manifest.chapters[uid]
   local x=need(read(book..'/chapters/'..uid..'.xhtml',2*1024*1024),'progress_not_located')
   bytes=bytes+#x;need(bytes<=40*1024*1024 and os.time()-started<=15,'progress_not_located')
   need(record and record.bytes==#x and record.checksum==Verify.checksum(x)
       and record.chapterFingerprint==Verify.chapter(c,i),'progress_cache_changed')
   local n=#Text.candidates(text,x);matches=matches+n;if n>0 then title=tostring(c.title or '')end
  end
  need(matches==1 and read(base..'.metadata')==metadata and read(base..'.content')==contentBytes,'progress_not_located')
  return remember(title)
 end
 function self:saveRecord(id,record,display,flags)
  local entry={record=record,fetchedAt=os.time(),displayProgress=display,displayMethod=1,
   displayPosition=Percent.positionKey(record),displayCalculatedAt=os.time()}
  for k,v in pairs(flags or {})do entry[k]=v end
  self.cache.books[id]=entry;self:save()
 end
 function self:refresh(books,emit)
  local registry=json(root..'/imports.json') or {}
  -- Send unchanged local positions for the whole shelf before network reads.
  for _,book in ipairs(books)do
   local row=registry.books and registry.books[book.bookId]
   if valid(book.bookId) and row and row.state=='imported' and row.accountScope==scope then
    local ok,title=pcall(function()return self:localChapter(book.bookId,row,nil,true)end)
    if ok and title then emit({state='reading_progress',bookId=book.bookId,accountScope=scope,localChapter=title})end
   end
  end
  for _,book in ipairs(books) do
   local row=registry.books and registry.books[book.bookId]
   if valid(book.bookId) and row and row.state=='imported' and row.accountScope==scope then
    local ok,record=pcall(function()return self:fetch(book.bookId)end)
    local calculated=false
    local catalog
    if ok then
     local previous=self.cache.books[book.bookId] or {}
     local good,display,calculatedAt,title,chapters=pcall(function()return self:displayPercent(book.bookId,record)end)
     catalog=good and chapters or nil
     calculated=good and percent(display)~=nil
     self:saveRecord(book.bookId,record,good and display or nil,{cloudChapter=good and title or nil,restorePending=previous.restorePending or false,restoreAutomatic=previous.restoreAutomatic or false,displayCalculatedAt=good and calculatedAt or nil})
    end
    local localOk,localTitle=pcall(function()return self:localChapter(book.bookId,row,catalog)end)
    local old=self.cache.books[book.bookId]
    emit({state='reading_progress',bookId=book.bookId,accountScope=scope,
      localChapter=localOk and localTitle or '',cloudChapter=old and old.cloudChapter or '',cloudProgress=old and old.displayMethod==1 and percent(old.displayProgress) or -1,cloudProgressStale=not (ok and calculated),restorePending=old and old.restorePending or false,restoreAutomatic=old and old.restoreAutomatic or false})
   end
  end
 end
 function self:prepareRestore(id)
  need(valid(id),'invalid_book_id')
  local entry=self.cache.books[id] or {};entry.restorePending=true;entry.restoreAutomatic=true
  self.cache.books[id]=entry;self:save();return {state='restore_prepared'}
 end
 function self:restore(id)
  need(valid(id),'invalid_book_id')
  local row=need((json(root..'/imports.json') or {}).books,'progress_not_located')[id]
  need(row and row.state=='imported' and row.accountScope==scope and type(row.uuid)=='string' and #row.uuid==36 and row.uuid:match('^[%x-]+$'),'progress_not_located')
  local old=self.cache.books[id] or {};old.restorePending=true;old.restoreAutomatic=false;self.cache.books[id]=old;self:save()
  local record=self:fetch(id)
  self.cache.books[id]={record=record,fetchedAt=os.time(),restorePending=true,restoreAutomatic=false};self:save()
  local base=library..'/'..row.uuid
  local mb=need(read(base..'.metadata'),'progress_not_located');local cb=need(read(base..'.content'),'progress_not_located')
  local meta,content=J.decode(mb),J.decode(cb)
  need(not meta.deleted and meta.parent~='trash' and content.fileType=='epub','progress_not_located')
  local count=content.pageCount;local map=content.redirectionPageMap
  need(type(count)=='number' and count>0 and count<5001 and type(map)=='table' and #map==count and content.originalPageCount==count and #content.pages==count,'progress_layout_unsupported')
  for i,v in ipairs(map) do need(v==i-1,'progress_layout_unsupported')end
  local target
  local display
  if tonumber(record.progress)==0 and (tonumber(record.chapterOffset) or 0)==0 then target=0
  else
   local Content=require('lib.content');local catalog=Content.fetch_catalog(client,{bookId=id});local chapter
   for _,c in ipairs(catalog) do if tostring(c.chapterUid)==tostring(record.chapterUid) then need(not chapter,'progress_not_located');chapter=c end end
   need(chapter and type(record.summary)=='string' and #record.summary>0,'progress_not_located')
   local put=Content.cache_annotation_source;Content.cache_annotation_source=function()end
   local ok,html=pcall(Content.fetch_chapter_xhtml,client,settings,{bookId=id,_content_format='epub'},chapter)
   Content.cache_annotation_source=put;need(ok,'progress_network_failed')
   local indexed=need(Source.index(html),'progress_not_located');local summary=Text.normalize(record.summary)
   -- A saved offset may describe a different HTML serialization. Downloading
   -- a native page can instead prove the bookmark with a unique exact summary
   -- in this known chapter, followed by unique whole-PDF anchor matching below.
   -- This fallback is never used to construct an upload offset.
   local at=need(Source.savedPosition(indexed,record.summary,record.chapterOffset)
       or Percent.summaryPosition(indexed.text,record.summary),'progress_offset_unsupported')
   display=Percent.calculate(catalog,record,html)
   local a={};for c in indexed.text:sub(at):gmatch('[%z\1-\127\194-\244][\128-\191]*') do a[#a+1]=c;if #a>=32 then break end end
   need(#a>=16,'progress_not_located');local needle=table.concat(a,'',1,math.min(32,#a));local short=table.concat(a,'',1,16)
   need(canonical(base..'.pdf'),'progress_not_located')
   local P=pdf();P.FPDF_InitLibrary();local doc=P.FPDF_LoadDocument(base..'.pdf',nil)
   local matches,shortMatches={},{};local started=os.time();local complete=false
   if doc~=nil then
    if P.FPDF_GetPageCount(doc)==count then
     complete=true
     for page=0,count-1 do
      if os.time()-started>45 then complete=false;break end
      local p=P.FPDF_LoadPage(doc,page)
      if p~=nil then local t=P.FPDFText_LoadPage(p)
       if t~=nil then local chars={};for i=0,math.min(P.FPDFText_CountChars(t),16384)-1 do chars[#chars+1]=Text.utf8(P.FPDFText_GetUnicode(t,i))end
        local text=Text.normalize(table.concat(chars));if text:find(needle,1,true) then matches[#matches+1]=page end
        if text:find(short,1,true) then shortMatches[#shortMatches+1]=page end
        P.FPDFText_ClosePage(t)
       end;P.FPDF_ClosePage(p)
      end
     end
    end;P.FPDF_CloseDocument(doc)
   end
   P.FPDF_DestroyLibrary();need(complete,'progress_not_located')
   if #matches==1 then target=matches[1] elseif #matches==0 and #shortMatches==1 then target=shortMatches[1] end
  end
  need(target~=nil,'progress_not_located')
  need(mb==read(base..'.metadata') and cb==read(base..'.content'),'progress_local_changed')
  self:saveRecord(id,record,display,{restorePending=true,restoreAutomatic=false})
  return {state='restore_position',bookId=id,uuid=row.uuid,page=target,pageCount=count,accountScope=scope,
   localProgress=count>1 and target*100/(count-1) or 0,cloudProgress=percent(display) or -1,
   metadataHash=require('progress_hash').hex(mb),contentHash=require('progress_hash').hex(cb)}
 end
 function self:restored(id)
  need(self.cache.books[id]~=nil,'cloud_progress_unavailable')
  self.cache.books[id].restorePending=false;self.cache.books[id].restoreAutomatic=false;self:save()
  return {state='progress_restored',bookId=id,accountScope=scope,restorePending=false,restoreAutomatic=false,
    cloudProgress=percent(self.cache.books[id].displayProgress) or -1,cloudProgressStale=self.cache.books[id].displayProgress==nil}
 end
 function self:markRecent(id)
  local row=need((json(root..'/imports.json') or {}).books,'progress_not_located')[id]
  need(row and row.state=='imported' and row.accountScope==scope,'progress_not_located')
  local before,html=self:fetch(id)
  need(type(before.chapterUid)=='number' and type(before.chapterOffset)=='number'
      and before.chapterOffset>=0 and type(before.summary)=='string','progress_not_located')
  local state=require('lib.content').extract_reader_state(html)
  local W=require('lib.weread')
  local payload=W.make_enter_read_payload({book_id=id,chapter_uid=before.chapterUid,chapter_idx=before.chapterIdx,
   chapter_offset=before.chapterOffset,progress=before.progress,summary=before.summary,psvts=state.psvts,pclts=state.pclts})
  need(payload.rt==nil,'progress_sync_unconfirmed')
  need(require('read_sync_protocol').response_succeeded(client:report_read(payload,W.reader_url(id))),'progress_sync_unconfirmed')
  local after=self:fetch(id)
  need(after.chapterUid==before.chapterUid and after.chapterOffset==before.chapterOffset
      and after.progress==before.progress and after.readingTime==before.readingTime,'progress_sync_unconfirmed')
  local old=self.cache.books[id] or {}
  self:saveRecord(id,after,old.displayProgress,{cloudChapter=old.cloudChapter,displayCalculatedAt=old.displayCalculatedAt,
   restorePending=old.restorePending or false,restoreAutomatic=old.restoreAutomatic or false})
  return {state='recent_marked',bookId=id,uuid=row.uuid,accountScope=scope}
 end
 function self:sync(id)
  need(valid(id),'invalid_book_id')
  local registered=need(json(root..'/imports.json'),'progress_not_located')
  local row=need(registered.books and registered.books[id],'progress_not_located')
  need(row.state=='imported' and row.accountScope==scope and type(row.uuid)=='string' and #row.uuid==36 and row.uuid:match('^[%x-]+$'),'progress_not_located')
  local base=library..'/'..row.uuid
  local metaBytes=need(read(base..'.metadata'),'progress_not_located');local contentBytes=need(read(base..'.content'),'progress_not_located')
  local meta,content=J.decode(metaBytes),J.decode(contentBytes)
  need(not meta.deleted and meta.parent~='trash' and content.fileType=='epub','progress_not_located')
  local map=content.redirectionPageMap;local count=tonumber(content.pageCount)
  need(type(map)=='table' and count and #map==count and type(content.pages)=='table' and #content.pages==count and content.originalPageCount==count,'progress_layout_unsupported')
  for i,v in ipairs(map) do need(v==i-1,'progress_layout_unsupported')end
  local ix=tonumber(meta.lastOpenedPage);need(ix and ix==math.floor(ix) and ix>=0 and ix<count,'progress_not_located')
  need(canonical(base..'.pdf'),'progress_not_located')
  local P=pdf();P.FPDF_InitLibrary()
  local doc=P.FPDF_LoadDocument(base..'.pdf',nil);local text
  if doc~=nil then
   if P.FPDF_GetPageCount(doc)==count then
    local p=P.FPDF_LoadPage(doc,ix)
    if p~=nil then
     local t=P.FPDFText_LoadPage(p)
     if t~=nil then
      local a={};for i=0,math.min(P.FPDFText_CountChars(t),8192)-1 do a[#a+1]=Text.utf8(P.FPDFText_GetUnicode(t,i))end
      text=Text.normalize(table.concat(a));P.FPDFText_ClosePage(t)
     end;P.FPDF_ClosePage(p)
    end
   end;P.FPDF_CloseDocument(doc)
  end
  P.FPDF_DestroyLibrary();need(text and #text>0,'progress_not_located')
  local book=account..'/books/'..id;local manifest=need(json(book..'/download-manifest.json'),'progress_not_located')
  local vid=settings:get('cookies',{}).wr_vid
  need(manifest.accountScope=='sha256-'..require('sha256').hex('rm-weread-cache-v1\31'..vid),'account_changed')
  local matches,uid,bytes,chaptersChecked=0,nil,0,0;local cachedWeights={};local started=os.time()
  for key,record in pairs(manifest.chapters or {}) do
   need(os.time()-started<=30 and bytes<=32*1024*1024 and chaptersChecked<2000,'progress_not_located')
   need(valid(key),'progress_not_located')
   local x=need(read(book..'/chapters/'..key..'.xhtml',2*1024*1024),'progress_not_located')
   need(#x==record.bytes and Verify.checksum(x)==record.checksum,'progress_cache_changed')
   bytes=bytes+#x;chaptersChecked=chaptersChecked+1
   cachedWeights[key]=math.max(1,#Text.xhtml(x));
   local n=#Text.candidates(text,x);matches=matches+n;if n>0 then uid=key end
  end
  need(matches==1,'progress_not_located')
  local Content=require('lib.content');local catalog=Content.fetch_catalog(client,{bookId=id})
  need(Verify.catalog(catalog)==manifest.catalogFingerprint,'progress_cache_changed')
  local chapter
  for i,c in ipairs(catalog) do if tostring(c.chapterUid)==uid then need(not chapter,'progress_not_located');chapter=c
   need(manifest.chapters[uid].chapterFingerprint==Verify.chapter(c,i),'progress_cache_changed')end end
  need(chapter and type(chapter.chapterIdx)=='number','progress_not_located')
  -- Download helpers normally store annotation source as a side effect. This
  -- operation needs only raw text, and must not modify annotation databases.
  local storeSource=Content.cache_annotation_source;Content.cache_annotation_source=function()end
  local ok,original=pcall(Content.fetch_chapter_xhtml,client,settings,{bookId=id,_content_format='epub'},chapter)
  Content.cache_annotation_source=storeSource;need(ok and type(original)=='string','progress_network_failed')
  local position=need(Source.locate(text,original),'progress_not_located')
  need(position.coordinateAgrees,'progress_offset_unsupported')
  local baseline=self.cache.books[id]
  need(baseline and baseline.record,'cloud_progress_unavailable')
  local before,html=self:fetch(id)
  need(signature(before)==signature(baseline.record),'cloud_progress_changed')
  need(metaBytes==read(base..'.metadata') and contentBytes==read(base..'.content'),'progress_local_changed')
  local latest=need(json(root..'/imports.json'),'progress_local_changed').books[id]
  need(latest and latest.uuid==row.uuid and latest.state=='imported' and latest.accountScope==scope,'progress_local_changed')
  -- Whole-book percentage uses chapter word weights, not native page fraction.
  local total,preceding=0,0;local targetIndex
  for i,c in ipairs(catalog) do
   local w=tonumber(c.wordCount);if not w or w<=0 then w=cachedWeights[tostring(c.chapterUid)] end;need(w and w>0,'progress_weight_unavailable')
   c._progressWeight=w;total=total+w;if tostring(c.chapterUid)==uid then targetIndex=i end
  end
  need(targetIndex and total>0,'progress_weight_unavailable')
  for i=1,targetIndex-1 do preceding=preceding+catalog[i]._progressWeight end
  local index=need(Source.index(original),'progress_not_located')
  local anchor=need(index.text:find(position.anchor,1,true),'progress_not_located')
  local function chars(s)local n=0;for _ in s:gmatch('[%z\1-\127\194-\244][\128-\191]*')do n=n+1 end;return n end
  local fraction=chars(index.text:sub(1,anchor-1))/math.max(1,chars(index.text))
  local progress=math.max(0,math.min(100,(preceding+catalog[targetIndex]._progressWeight*fraction)*100/total))
  local state=Content.extract_reader_state(html)
  local Read=require('read_sync_protocol')
  local payload=require('lib.weread').make_read_payload({book_id=id,chapter_uid=chapter.chapterUid,chapter_idx=chapter.chapterIdx,
   chapter_offset=position.codepoints,progress=progress,summary=Read.utf8_prefix_bytes(position.anchor,20),
   psvts=state.psvts,pclts=state.pclts,token=state.token,elapsed_seconds=0})
  local response=client:report_read(payload,require('lib.weread').reader_url(id))
  need(Read.response_succeeded(response),'progress_sync_unconfirmed')
  local after=self:fetch(id)
  need(tostring(after.chapterUid)==tostring(chapter.chapterUid) and tonumber(after.chapterOffset)==position.codepoints,'progress_sync_unconfirmed')
  local display=Percent.calculate(catalog,after,original)
  self:saveRecord(id,after,display,{restorePending=false,restoreAutomatic=false})
  local recent=pcall(function()self:markRecent(id)end)
  return {state='progress_synced',bookId=id,uuid=row.uuid,recentMarked=recent,accountScope=scope,cloudProgress=percent(display) or -1,cloudProgressStale=display==nil,restorePending=false,restoreAutomatic=false}
 end
 return self
end
return M
