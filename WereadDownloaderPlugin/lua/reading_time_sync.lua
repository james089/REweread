-- Only actual UI-recorded deltas are eligible. Local counters and this worker's
-- acknowledgement journal have separate writers. Never resend an ambiguous POST.
local J=require('json_util')
local ffi=require('ffi')
ffi.cdef[[struct wrdp_upload_wait { long tv_sec; long tv_nsec; }; int nanosleep(const struct wrdp_upload_wait*,struct wrdp_upload_wait*);char *realpath(const char*,char*);long readlink(const char*,char*,unsigned long);]]
local M={}
local function need(v,code)if not v then error(code,0)end;return v end
-- Controller/worker set TZ from the user's persisted IANA timezone choice.
local function readingDay(t)return os.date('%Y-%m-%d',t or os.time())end
local function load(path)
 need(ffi.C.readlink(path,ffi.new('char[4096]'),4096)<0,'time_cache_unavailable')
 local b=ffi.new('char[4096]');if ffi.C.realpath(path,b)==nil then return {books={}}end
 need(ffi.string(b)==path,'time_cache_unavailable')
 local f=need(io.open(path,'rb'),'time_cache_unavailable');local size=f:seek('end');f:seek('set')
 if not size or size>8*1024*1024 then f:close();error('time_cache_unavailable',0)end
 local raw=f:read('*a');f:close();local ok,v=pcall(J.decode,raw)
 need(ok and type(v)=='table' and v.version==1 and type(v.books)=='table','time_cache_unavailable');return v
end
local function save(path,v)
 local tmp=path..'.pending';need(ffi.C.readlink(tmp,ffi.new('char[4096]'),4096)<0,'time_cache_unavailable')
 local f=need(io.open(tmp,'wb'),'time_cache_unavailable');local ok=f:write(J.encode(v));local closed=f:close()
 need(ok and closed and os.rename(tmp,path),'time_cache_unavailable')
end
local function seconds(n)return type(n)=='number' and n>=0 and n<=315360000 and n==math.floor(n)end
-- A missing receipt consumes only that local slice as skipped. It must never
-- freeze future progress/time reports or masquerade as acknowledged duration.
local function consumed(row)return row.confirmedSeconds+(row.skippedSeconds or 0)end
local function allocations(l,n)
 local result,dates={},{}
 for date in pairs(l.source.days or {})do dates[#dates+1]=date end;table.sort(dates)
 for _,date in ipairs(dates)do
  local take=math.min(n,l.source.days[date]-(l.row.confirmedDays[date] or 0)-(l.row.skippedDays[date] or 0))
  if take>0 then result[date]=take;n=n-take end
 end
 return result
end
local function settle(l,ack)
 local pending=l.row.pending
 local days=ack and l.row.confirmedDays or l.row.skippedDays
 for date,n in pairs(pending.days)do days[date]=(days[date] or 0)+n end
 local field=ack and 'confirmedSeconds' or 'skippedSeconds'
 l.row[field]=(l.row[field] or 0)+pending.seconds
 l.row.pending=nil;save(l.path,l.journal)
end
local function ledger(account,id,uuid)
 local source=load(account..'/local-reading-time-v1.json').books[id]
 if not source then return nil end
 need(source.uuid==uuid and seconds(source.seconds),'time_cache_unavailable')
 local path=account..'/uploaded-reading-time-v1.json';local journal=load(path);journal.version=1
 local row=journal.books[id] or {confirmedSeconds=0}
 row.skippedSeconds=row.skippedSeconds or 0;row.confirmedDays=row.confirmedDays or {};row.skippedDays=row.skippedDays or {}
 need(seconds(row.confirmedSeconds) and seconds(row.skippedSeconds) and consumed(row)<=source.seconds,'time_cache_unavailable')
 local days=source.days or {};need(type(days)=='table' and type(row.confirmedDays)=='table' and type(row.skippedDays)=='table','time_cache_unavailable')
 local dated,processed=0,0
 for date,n in pairs(days)do
  need(type(date)=='string' and date:match('^%d%d%d%d%-%d%d%-%d%d$') and seconds(n),'time_cache_unavailable');dated=dated+n
 end
 for _,record in ipairs({row.confirmedDays,row.skippedDays})do for date,n in pairs(record)do
  need(seconds(n) and (row.confirmedDays[date] or 0)+(row.skippedDays[date] or 0)<=(days[date] or 0),'time_cache_unavailable');processed=processed+n
 end end
 need(dated<=source.seconds and processed<=consumed(row) and consumed(row)-processed<=source.seconds-dated,'time_cache_unavailable')
 journal.books[id]=row;local l={path=path,journal=journal,row=row,source=source}
 if row.pending then
  local n=row.pending.seconds
  need(seconds(n) and n>0 and n<=60 and row.pending.localThrough==consumed(row)+n and consumed(row)+n<=source.seconds,'time_cache_unavailable')
  row.pending.days=allocations(l,n);settle(l,false)
 end
 return l
end
function M.recoverPending(account,id,uuid)
 local l=ledger(account,id,uuid);return l and l.row.skippedSeconds or 0
end
local function reserve(l,n)
 l.row.pending={seconds=n,localThrough=consumed(l.row)+n,createdAt=os.time(),days=allocations(l,n)}
 save(l.path,l.journal)
end
local function acknowledge(l)settle(l,true)end
local function release(l)l.row.pending=nil;save(l.path,l.journal)end
function M.upload(progress,id,account,uuid,send)
 local l=ledger(account,id,uuid)
 if not l then return {uploaded=0,remaining=0}end
 local uploaded,skipped=0,0;local target=l.source.seconds-consumed(l.row)
 local function notify()if progress.onTimeChunk then progress.onTimeChunk(uploaded,target,l.source.seconds-consumed(l.row),skipped)end end
 notify()
 -- Reuse cloud context. Space successful reports by one real second; never
 -- fabricate ct or drain bursts sharing the same integer timestamp.
 while true do
  local n=math.min(60,l.source.seconds-consumed(l.row))
  if n<=0 then break end
  if uploaded>0 then
   if progress.timeUploadSleep then progress.timeUploadSleep(1)else
    local remaining=ffi.new('struct wrdp_upload_wait',{1,0})
    while ffi.C.nanosleep(remaining,remaining)~=0 do need(ffi.errno()==4,'time_upload_interrupted')end
   end
  end
  reserve(l,n)
  local ok,response=pcall(send,n)
  if not ok then settle(l,false);skipped=skipped+n;notify();break end
  local definite=type(response)=='table' and (response.success==false or response.success==0 or response.succ==false or response.succ==0 or response.errCode~=nil and tonumber(response.errCode)~=0)
  if definite then release(l);error('time_upload_rejected',0)end
  if not (require('read_sync_protocol').response_succeeded(response) or type(response)=='table' and response.synckey~=nil)then settle(l,false);skipped=skipped+n;notify();break end
  acknowledge(l);uploaded=uploaded+n;notify()
 end
 return {uploaded=uploaded,skippedSeconds=skipped,remaining=l.source.seconds-consumed(l.row),deferred=0,skipped=false}
end
local function totalAt(account,id)
 local r=load(account..'/local-reading-time-v1.json').books[id]
 if not r then return 0,{} end
 need(seconds(r.seconds),'time_cache_unavailable');return r.seconds,r.days or {}
end
function M.begin(progress,id,account,uuid,token,force)
 need(type(token)=='string' and #token==36 and token:match('^[%x-]+$'),'time_cache_unavailable')
 local path=account..'/reading-session-v1.json';local sessions=load(path);sessions.version=1
 local session=sessions.books[id]
 local previous=session
 local resume=not force and session and session.uuid==uuid and session.token==token and session.entryAcknowledged==true and not session.finished
  and os.time()-(session.enteredAt or 0)<900
 if resume and session.baselineReady and type(session.context)=='table' and type(session.context.record)=='table' then
  progress.reportContext=session.context;return {state='reading_session_started',bookId=id}
 end
 if not resume then
  local before,html=progress:fetch(id);local state=require('lib.content').extract_reader_state(html);local W=require('lib.weread')
  if state.pclts==nil or state.pclts=='' or tonumber(state.pclts)==0 then state.pclts=W.e(os.time())end
  local payload=W.make_enter_read_payload({book_id=id,chapter_uid=before.chapterUid,chapter_idx=before.chapterIdx,
   chapter_offset=before.chapterOffset,progress=before.progress,summary=before.summary,psvts=state.psvts,pclts=state.pclts})
  need(require('read_sync_protocol').response_succeeded(progress.client:report_read(payload,W.reader_url(id))),'time_upload_rejected')
  local total,days=totalAt(account,id)
  session={uuid=uuid,token=token,enteredAt=os.time(),startTotal=total,startDays=days,entryAcknowledged=true,baselineReady=false,
   context={record=before,psvts=state.psvts,pclts=state.pclts,token=state.token}}
  if previous and previous.token==token then
   session.lastReportId=previous.lastReportId;session.lastReportAt=previous.lastReportAt;session.lastReportState=previous.lastReportState
   session.reportCount=previous.reportCount;session.renewAttemptAt=previous.renewAttemptAt
   session.reportIntents=previous.reportIntents
  end
  sessions.books[id]=session;save(path,sessions)
 end
 -- Persist only the small private report context, not the reader HTML.
 if not session.context then
  local record,html=progress:fetch(id);local state=require('lib.content').extract_reader_state(html)
  session.context={record=record,psvts=state.psvts,pclts=state.pclts,token=state.token}
 end
 progress.reportContext=session.context
 local old=(progress.cache.books or {})[id] or {}
 progress:saveRecord(id,session.context.record,old.displayProgress,{cloudChapter=old.cloudChapter,displayCalculatedAt=old.displayCalculatedAt,
  restorePending=old.restorePending or false,restoreAutomatic=old.restoreAutomatic or false})
 session.baselineReady=true;save(path,sessions)
 return {state='reading_session_started',bookId=id}
end

-- The periodic reader reports exactly one configured interval, as KOReader
-- does. Explicit statistics backfill shares its duration journal; position sync does not.
local function rejected(v)
 return type(v)=='table' and (v.success==false or v.success==0 or v.succ==false or v.succ==0
  or v.errCode~=nil and tonumber(v.errCode)~=0)
end
local function accepted(v)
 return not rejected(v) and (require('read_sync_protocol').response_succeeded(v)
  or type(v)=='table' and v.synckey~=nil)
end
function M.uploadCombined(progress,id,account,uuid,position,send)
 need(type(send)=='function' and type(position)=='table','time_cache_unavailable')
 local reportId=position.reportId
 if not reportId then
  -- Position sync never drains duration. Backfill belongs exclusively to the
  -- explicit statistics action; scheduled reader reports still send 60 seconds.
  local ok,response=pcall(send,0)
  need(ok and accepted(response),'progress_sync_unconfirmed')
  return {uploaded=0,remaining=0,deferred=0,skipped=false,noNewTime=true}
 end
 local token=position.sessionToken
 need(type(reportId)=='string' and #reportId==36 and reportId:match('^[%x-]+$'),'time_cache_unavailable')
 local path=account..'/reading-session-v1.json';local sessions=load(path);local session=sessions.books[id]
 need(session and session.uuid==uuid and session.token==token and session.entryAcknowledged and session.baselineReady,'time_cache_unavailable')
 session.reportIntents=session.reportIntents or {}
 need(session.lastReportId~=reportId and not session.reportIntents[reportId],'time_report_duplicate')
 local l=ledger(account,id,uuid)
 need(l and l.source.seconds-consumed(l.row)>=60,'time_report_not_due')
 local now=os.time()
 need(not session.lastReportAt or now-session.lastReportAt>=60,'time_report_not_due')
 session.lastReportId=reportId;session.lastReportAt=now;session.lastReportState='pending';session.reportIntents[reportId]='pending';save(path,sessions)
 reserve(l,60)
 local attempt=0
 while true do
  attempt=attempt+1
  local fresh
  if attempt>1 then
   if attempt==3 then
    local saved=load(path);local current=saved.books[id]
    need(not current.renewAttemptAt or now-current.renewAttemptAt>=600,'time_upload_rejected')
    current.renewAttemptAt=now;save(path,saved)
    local vid=progress.settings and progress.settings:get('cookies',{}).wr_vid
    local renewed,reply=pcall(progress.client.renew_cookie,progress.client)
    need(renewed and accepted(reply),'time_upload_rejected')
    if vid then need(progress.settings:get('cookies',{}).wr_vid==vid,'account_changed')end
   end
   M.begin(progress,id,account,uuid,token,true)
   fresh={record=progress.reportContext.record,state=progress.reportContext}
  end
  local ok,response=pcall(send,60,fresh)
  if not ok or not accepted(response) and not rejected(response) then
   settle(l,false);sessions=load(path);session=sessions.books[id]
   session.lastReportState='skipped';session.reportIntents[reportId]='skipped';save(path,sessions)
   return {uploaded=0,skippedSeconds=60,remaining=l.source.seconds-consumed(l.row),skipped=false}
  end
  if accepted(response) then
   acknowledge(l)
   sessions=load(path);session=sessions.books[id]
   session.lastReportState='acknowledged';session.reportIntents[reportId]='acknowledged';session.reportCount=(session.reportCount or 0)+1;save(path,sessions)
   return {uploaded=60,remaining=0,deferred=0,skipped=false}
  end
  -- Only a definite server rejection can be retried. Unknown receipts keep
  -- their persisted intent; later ticks report new intervals, never this one.
  need(rejected(response),'time_upload_uncertain')
  if attempt>=3 then
   release(l)
   sessions=load(path);sessions.books[id].lastReportState='rejected';sessions.books[id].reportIntents[reportId]='rejected';save(path,sessions)
   error('time_upload_rejected',0)
  end
 end
end
return M
