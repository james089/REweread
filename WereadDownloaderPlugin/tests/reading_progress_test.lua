-- Device LuaJIT behavior test. All files are synthetic and live in a temporary
-- directory supplied by the test caller. No real account or network access.
local modules,work=assert(arg[1]),assert(arg[2])
package.path=modules..'/?.lua;/home/root/weread-downloader/helper/lib/?.lua;'..package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local Hash=require('progress_hash')
assert(Hash.hex('abc')=='ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
assert(Hash.hex(string.rep('a',100000))=='6d1cf22d7cc09b085dfc25ee1a1f3ae0265804c607bc2074ad253bcc82fd81ee')
local J=require('json_util');local V=require('progress_cache_verify');local Source=require('progress_source_map')
local scope=string.rep('a',64);local uuid='12345678-1234-1234-1234-123456789abc';local id='synthetic-42'
local text='';for i=1,120 do text=text..require('progress_text').utf8(0x4e00+i)end
local html='<html><body><p>😀'..text..'</p></body></html>';local position=assert(Source.locate(text,html))
local prefix='<html><body><p>';local sourceSpans={{#prefix,#prefix+121,'😀'..text}}
local plainHtml='<html><body><p>'..text..'</p></body></html>'
local chapter={chapterUid=42,chapterIdx=0,title='synthetic',wordCount=120}
local base=work..'/library/'..uuid;local account=work..'/account'
os.remove(account..'/reading-progress-v1.json') -- Reset this synthetic test's persisted display cache.
os.remove(account..'/local-reading-time-v1.json');os.remove(account..'/uploaded-reading-time-v1.json')
local function write(path,data)local f=assert(io.open(path,'wb'));assert(f:write(data));assert(f:close())end
local function json(path,data)write(path,J.encode(data))end
local function read(path)local f=assert(io.open(path,'rb'));local s=f:read('*a');f:close();return s end
write(work..'/hash-stream-fixture',string.rep('a',100000))
assert(Hash.file(work..'/hash-stream-fixture')=='6d1cf22d7cc09b085dfc25ee1a1f3ae0265804c607bc2074ad253bcc82fd81ee')
os.remove(work..'/hash-stream-fixture')
json(work..'/imports.json',{version=1,books={[id]={state='imported',uuid=uuid,accountScope=scope}}})
json(base..'.metadata',{lastOpenedPage=1,parent='',type='DocumentType'})
json(base..'.content',{fileType='epub',pageCount=2,originalPageCount=2,pages={'a','b'},redirectionPageMap={0,1}})
write(base..'.pdf','synthetic')
assert(Hash.file(base..'.pdf')==Hash.hex('synthetic'))
write(account..'/books/'..id..'/chapters/42.xhtml',html)
assert(require('progress_source_store').save(account,id,'42',html))
json(account..'/books/'..id..'/download-manifest.json',{version=1,accountScope='sha256-'..scope,catalogFingerprint=V.catalog({chapter}),chapters={['42']={bytes=#html,checksum=V.checksum(html),chapterFingerprint=V.chapter(chapter,1)}}})
local saved={chapterUid=42,chapterIdx=0,chapterOffset=position.codepoints,progress=20,summary=text:sub(1,18),updateTime=10,readingTime=0}
local Percent=require('progress_percent')
local weighted={{chapterUid=17,wordCount=100},{chapterUid=901,wordCount=900}}
local middle=text:sub(181) -- 60 Chinese codepoints into 120
local indexed=assert(Source.index(plainHtml))
local at=assert(indexed.text:find(middle,1,true))
local rec={chapterUid=901,chapterOffset=indexed.positions[at].codepoints,summary=middle:sub(1,18),progress=99}
assert(math.abs(assert(Percent.calculate(weighted,rec,plainHtml))-55)<0.001)
rec.progress=1;assert(math.abs(assert(Percent.calculate(weighted,rec,plainHtml))-55)<0.001)
local offset=rec.chapterOffset;rec.chapterOffset=0
assert(math.abs(assert(Percent.calculate(weighted,rec,plainHtml))-55)<0.001)
rec.chapterOffset=offset
assert(Percent.summaryPosition('abcdefabcdef','abcdef')==nil)
assert(Percent.summaryPosition('abcde','abcde')==nil)
assert(Percent.summaryPosition('xabcdefy','abcdef')==2)
assert(Percent.calculate({{chapterUid=17,wordCount=100}},rec,plainHtml)==nil)
assert(Percent.calculate({{chapterUid=901,wordCount=0}},rec,plainHtml)==nil)
assert(Percent.calculate({{chapterUid=901,wordCount=1},{chapterUid=901,wordCount=1}},rec,plainHtml)==nil)
assert(Percent.calculate({{chapterUid=901}},rec,plainHtml)==nil)
local writes=0;local timeWrites=0;local enters=0;local cloudReads=0;local rejectEnter=false;local rejected=false;local changed=false;local lastPayload
local chapterFetches=0;local catalogFetches=0
package.loaded.sha256={hex=function()return scope end}
package.loaded.config_bridge=nil
local client={request=function()cloudReads=cloudReads+1;return 'window.__INITIAL_STATE__='..J.encode({reader={progress={book=saved}}}),200 end,
 report_read=function(_,payload)
  if payload.enter then enters=enters+1;assert(payload.rt==nil and payload.elapsed_seconds==nil)
   if rejectEnter then return {errCode=1}end
   saved.updateTime=saved.updateTime+1;return {succ=1}
  end
  writes=writes+1;lastPayload=payload
  if rejected then return {errCode=1} end
  local elapsed=payload.elapsed_seconds or 0
  if elapsed>0 then timeWrites=timeWrites+1 end
  saved={chapterUid=payload.chapter_uid,chapterIdx=payload.chapter_idx,chapterOffset=payload.chapter_offset,summary=payload.summary,progress=payload.progress,updateTime=20,readingTime=saved.readingTime+elapsed}
  return {succ=1}
 end}
package.loaded['lib.client']={new=function()return client end}
package.loaded.persistent_http={attach=function()end}
package.loaded['lib.weread']={reader_url=function()return 'https://weread.qq.com/web/reader/synthetic'end,make_read_payload=function(v)return v end,
 make_enter_read_payload=function(v)v.enter=true;return v end}
package.loaded['lib.content']={fetch_catalog=function()catalogFetches=catalogFetches+1;return {chapter}end,cache_annotation_source=function()error('no-annotation-writes')end,
 fetch_chapter_xhtml=function()chapterFetches=chapterFetches+1;return plainHtml end,extract_reader_state=function()return {psvts=1,pclts=1,token='synthetic'}end}
local currentPage=0;local pdfLoads=0
local P={FPDF_InitLibrary=function()end,FPDF_DestroyLibrary=function()end,FPDF_LoadDocument=function()pdfLoads=pdfLoads+1;return {}end,FPDF_CloseDocument=function()end,
 FPDF_GetPageCount=function()return 2 end,FPDF_LoadPage=function(_,p)currentPage=p;return {}end,FPDF_ClosePage=function()end,
 FPDFText_LoadPage=function()return {}end,FPDFText_ClosePage=function()end,FPDFText_CountChars=function()return currentPage==1 and 120 or 0 end,
 FPDFText_GetUnicode=function(_,i)return 0x4e00+i+1 end}
local settings={get=function(_,key)assert(key=='cookies');return {wr_vid='123'}end}
local M=require('reading_progress').new(settings,scope,account,{root=work,library=work..'/library',pdf=P})
local rows={};M:refresh({{bookId=id},{bookId='not-imported'}},function(row)rows[#rows+1]=row end)
assert(#rows==1 and rows[1].cloudProgress==0 and writes==0) -- raw 20 is ignored; content position is chapter start
assert(M.cache.books[id].record.progress==20)
local beforeRecent=J.encode({saved.chapterUid,saved.chapterOffset,saved.progress,saved.readingTime})
assert(M:markRecent(id).state=='recent_marked' and enters==1 and writes==0)
assert(J.encode({saved.chapterUid,saved.chapterOffset,saved.progress,saved.readingTime})==beforeRecent)
assert(rows[1].cloudChapter=='synthetic' and rows[1].localChapter=='synthetic')
assert(M:localChapter(id,{uuid=uuid},{chapter})=='synthetic')
local beforeLoads=pdfLoads
assert(M:localChapter(id,{uuid=uuid},{chapter})=='synthetic' and pdfLoads==beforeLoads)
local reopened=require('reading_progress').new(settings,scope,account,{root=work,library=work..'/library',pdf=P})
assert(reopened:localChapter(id,{uuid=uuid},{chapter})=='synthetic' and pdfLoads==beforeLoads)
reopened:close()
write(base..'.pdf','different')
assert(M:localChapter(id,{uuid=uuid},{chapter})=='synthetic' and pdfLoads==beforeLoads+1)
write(base..'.pdf','synthetic')
json(base..'.metadata',{lastOpenedPage=0,parent='',type='DocumentType'})
assert(M:localChapter(id,{uuid=uuid},{chapter})=='当前页无正文')
json(base..'.metadata',{lastOpenedPage=1,parent='',type='DocumentType'})
local missingUid={chapterUid=0,chapterOffset=0,summary=middle:sub(1,18),progress=99}
assert(math.abs(assert(M:displayPercent(id,missingUid))-50)<0.001)
local unchanged=read(account..'/books/'..id..'/chapters/42.xhtml')
write(account..'/books/'..id..'/chapters/42.xhtml',unchanged..'changed')
assert(not pcall(function()M:displayPercent(id,missingUid)end))
write(account..'/books/'..id..'/chapters/42.xhtml',unchanged)
M:prepareRestore(id)
assert(J.decode(read(account..'/reading-progress-v1.json')).books[id].restoreAutomatic)
local fetchesBeforeSync=chapterFetches
local result=M:sync(id) -- explicit upload keeps its direction despite pending restore
assert(not J.decode(read(account..'/reading-progress-v1.json')).books[id].restorePending)
assert(result.state=='progress_synced' and writes==1 and lastPayload.elapsed_seconds==0)
assert(result.recentMarked and result.uuid==uuid and enters==2)
assert(result.timings.sourceMode=='download-map' and result.timings.chapterFetchMs==0 and chapterFetches==fetchesBeforeSync)
assert(lastPayload.chapter_uid==42 and lastPayload.chapter_offset==position.codepoints)
local sourceMapPath=account..'/books/'..id..'/source-indexes/42.json'
os.remove(sourceMapPath)
local missingOk,missingErr=pcall(function()M:sync(id)end)
assert(not missingOk and missingErr=='progress_source_map_missing' and chapterFetches==fetchesBeforeSync)
assert(require('progress_source_store').save(account,id,'42',html))
-- Concurrent cloud movement wins, while current live-session time still sends.
local originalSaved=J.decode(J.encode(saved));local beforeConflictEnters=enters
saved.updateTime=30;saved.chapterOffset=0;saved.summary=middle:sub(1,18);saved.progress=50
local expectedCloud=J.encode({saved.chapterUid,saved.chapterOffset,saved.summary,saved.progress})
local liveToken='00000000-0000-4000-8000-000000000001';local day=os.date('%Y-%m-%d')
json(account..'/local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=20,days={[day]=20}}}})
json(account..'/reading-session-v1.json',{version=1,books={[id]={uuid=uuid,token=liveToken,enteredAt=os.time()-20,startTotal=0,startDays={}}}})
local oldGetenv=os.getenv;os.getenv=function(key)if key=='WEREAD_READING_SESSION' then return liveToken end;return oldGetenv(key)end
result=M:sync(id);os.getenv=oldGetenv
assert(result.state=='progress_synced' and result.cloudPositionPreserved and result.timeUploadedSeconds==20)
assert(writes==2 and timeWrites==1 and enters==beforeConflictEnters)
assert(lastPayload.elapsed_seconds==20 and lastPayload.chapter_offset==0 and lastPayload.summary==middle:sub(1,18))
assert(J.encode({saved.chapterUid,saved.chapterOffset,saved.summary,saved.progress})==expectedCloud)
assert(J.decode(read(account..'/uploaded-reading-time-v1.json')).books[id].confirmedSeconds==20)
-- Restore independent fixtures so subsequent rejection/layout scenarios retain their counters.
for _,name in ipairs({'local-reading-time-v1.json','uploaded-reading-time-v1.json','reading-session-v1.json'})do os.remove(account..'/'..name)end
saved=originalSaved;writes=1;timeWrites=0
local ok,err
-- Refresh establishes the new baseline, then a business rejection never reports success.
M:refresh({{bookId=id}},function()end);rejected=true
ok,err=pcall(function()M:sync(id)end);assert(not ok and err=='progress_sync_unconfirmed' and writes==2)
rejected=false
-- Saved offset is in markup before the first visible summary character.
-- This is a real-world server representation, not the visible summary index.
saved.chapterOffset=0;saved.progress=25
local indexed=assert(Source.index(plainHtml))
assert(Source.savedPosition(indexed,saved.summary,saved.chapterOffset)==1)
assert(Source.savedPosition(indexed,'unrelated summary',0)==nil)
assert(Source.savedPosition(indexed,saved.summary,-1)==nil)
local withEmoji=assert(Source.index('<p>😀'..text..'</p>'))
assert(Source.savedPosition(withEmoji,text:sub(1,18),5)==nil)
local before=read(base..'.metadata')
result=M:restore(id)
assert(result.state=='restore_position' and result.page==1 and writes==2)
assert(read(base..'.metadata')==before) -- module never edits native files itself
saved.chapterOffset=999999 -- Incompatible source serialization; exact unique saved text still proves the page.
json(base..'.metadata',{lastOpenedPage=0,parent='',type='DocumentType'})
local coverMetadata=read(base..'.metadata')
result=M:restore(id)
assert(result.state=='restore_position' and result.page==1 and writes==2 and read(base..'.metadata')==coverMetadata)
local oldSummary=saved.summary;saved.summary='no matching saved text'
ok,err=pcall(function()M:restore(id)end);assert(not ok and err=='progress_offset_unsupported' and writes==2)
saved.summary=oldSummary;saved.chapterOffset=0
local chapterFetchesBeforeSnapshot=chapterFetches
local catalogFetchesBeforeSnapshot=catalogFetches
local snapshotRecord=J.decode(J.encode(saved))
local readsBeforeDownloadSnapshot=cloudReads
local prepared=M:prepareDownloadedRestore(id,{chapter},snapshotRecord)
assert(prepared.snapshot and cloudReads==readsBeforeDownloadSnapshot,'download-start reader state should be reused')
local readsBeforeCachedRestore=cloudReads
result=M:restore(id)
assert(result.state=='restore_position' and result.page==1)
assert(cloudReads==readsBeforeCachedRestore and chapterFetches==chapterFetchesBeforeSnapshot
 and catalogFetches==catalogFetchesBeforeSnapshot,'fresh download snapshot, catalog and XHTML should restore locally')
prepared=M:prepareDownloadedRestore(id,{chapter},J.decode(J.encode(saved)))
assert(prepared.snapshot)
M.cache.books[id].downloadSnapshotAt=os.time()-3600;M:save()
local readsBeforeOldSnapshot=cloudReads
result=M:restore(id)
assert(result.state=='restore_position' and cloudReads==readsBeforeOldSnapshot,'the download-time snapshot is used even if cloud position changes during download')
local readsAfterConsumedSnapshot=cloudReads
result=M:restore(id)
assert(result.state=='restore_position' and cloudReads==readsAfterConsumedSnapshot+1,'a consumed snapshot is never reused for later manual retries')
write(base..'.metadata',before)
local cache=J.decode(read(account..'/reading-progress-v1.json'));assert(cache.books[id].restorePending and not cache.books[id].restoreAutomatic)
result=M:restored(id);assert(result.state=='progress_restored')
cache=J.decode(read(account..'/reading-progress-v1.json'));assert(not cache.books[id].restorePending)
-- Edited page layouts must fail before submitting cloud progress.
json(base..'.content',{fileType='epub',pageCount=2,originalPageCount=2,pages={'a','b'},redirectionPageMap={1,0}})
ok,err=pcall(function()M:sync(id)end);assert(not ok and err=='progress_layout_unsupported' and writes==2)
json(base..'.content',{fileType='epub',pageCount=2,originalPageCount=2,pages={'a','b'},redirectionPageMap={0,1}})
M:refresh({{bookId=id}},function()end);rejectEnter=true
local getenv=os.getenv;local entersBeforeExit=enters
os.getenv=function(name)if name=='WEREAD_READING_SESSION' then return '00000000-0000-0000-0000-000000000000' end;return getenv(name)end
result=M:sync(id);assert(result.state=='progress_synced' and not result.recentMarked and writes==3)
assert(enters==entersBeforeExit,'native exit must not send a second enter-read')
os.getenv=getenv
rejectEnter=false
json(account..'/local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=5,days={[os.date('%Y-%m-%d')]=5}}}})
M:refresh({{bookId=id}},function()end)
local readsBeforeCombined=cloudReads
result=M:sync(id);assert(result.state=='progress_synced' and result.timeUploaded and writes==4 and timeWrites==0 and lastPayload.elapsed_seconds==0,
 'manual progress must not upload accumulated reading time')
assert(cloudReads-readsBeforeCombined==2,'combined sync should perform one preflight cloud read and one final verification')
-- Native cPages v2 must retain exact sync/restore without rewriting CRDT/annotations.
local function cell(v)return {timestamp='1:1',value=v}end
local v2={fileType='epub',formatVersion=2,pageCount=2,cPages={original=cell(2),lastOpened=cell('00000000-0000-4000-8000-000000000002'),pages={
 {id='00000000-0000-4000-8000-000000000001',idx=cell('a'),redir=cell(0)},
 {id='00000000-0000-4000-8000-000000000002',idx=cell('b'),redir=cell(1)},
}}}
json(base..'.content',v2)
local preservedContent=read(base..'.content');local preservedMetadata=read(base..'.metadata')
M:refresh({{bookId=id}},function()end)
local v2Writes=writes;local v2Fetches=chapterFetches
result=M:sync(id)
assert(result.state=='progress_synced' and writes==v2Writes+1 and lastPayload.chapter_uid==42
 and lastPayload.chapter_offset==position.codepoints and chapterFetches==v2Fetches)
assert(read(base..'.content')==preservedContent and read(base..'.metadata')==preservedMetadata)
result=M:restore(id)
assert(result.state=='restore_position' and result.page==1 and writes==v2Writes+1)
assert(read(base..'.content')==preservedContent and read(base..'.metadata')==preservedMetadata)
-- Nonidentity visible mapping with a tombstone and two occurrences of PDF 1.
local first,second=v2.cPages.pages[1],v2.cPages.pages[2]
local repeated={id='00000000-0000-4000-8000-000000000003',idx=cell('b'),redir=cell(1)}
local deleted={id='00000000-0000-4000-8000-000000000004',deleted=cell(true)}
v2.pageCount=3;v2.cPages.pages={repeated,deleted,first,second}
json(base..'.metadata',{lastOpenedPage=2,parent='',type='DocumentType'})
json(base..'.content',v2)
preservedContent=read(base..'.content');preservedMetadata=read(base..'.metadata')
local historyWrites=writes
assert(M:localChapter(id,{uuid=uuid},{chapter})=='synthetic')
result=M:sync(id)
assert(result.state=='progress_synced' and currentPage==1 and writes==historyWrites+1
 and lastPayload.chapter_offset==position.codepoints)
result=M:restore(id)
assert(result.state=='restore_position' and result.page==2 and result.pageCount==3 and result.localProgress==100)
assert(read(base..'.content')==preservedContent and read(base..'.metadata')==preservedMetadata)
-- A PDF page omitted from native layout cannot be restored to a guessed page.
first.redir.value=1;json(base..'.content',v2)
local old=saved;saved={chapterUid=42,chapterIdx=0,chapterOffset=0,progress=0,summary='',updateTime=10}
ok,err=pcall(function()M:restore(id)end);assert(not ok and err=='progress_not_located')
saved=old;first.redir.value=0
local rejectedWrites=writes
second.redir.value=2;json(base..'.content',v2)
ok,err=pcall(function()M:sync(id)end)
assert(not ok and err=='progress_layout_unsupported' and writes==rejectedWrites)
ok,err=pcall(function()M:restore(id)end)
assert(not ok and err=='progress_layout_unsupported' and writes==rejectedWrites)
M:close();print('synthetic progress: imported-only reads, exact sync/zero time, conflict, rejection, restore and layout refusal passed')
