-- Only actual UI-recorded deltas are eligible. Local counters and this worker's
-- acknowledgement journal have separate writers. Never resend an ambiguous POST.
local J=require('json_util')
local ffi=require('ffi')
ffi.cdef[[char *realpath(const char*,char*);long readlink(const char*,char*,unsigned long);]]
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
function M.upload()error('manual_time_upload_disabled',0)end
local function totalAt(account,id)
 local r=load(account..'/local-reading-time-v1.json').books[id]
 if not r then return 0,{} end
 need(seconds(r.seconds),'time_cache_unavailable');return r.seconds,r.days or {}
end
function M.begin(progress,id,account,uuid,token)
 need(type(token)=='string' and #token==36 and token:match('^[%x-]+$'),'time_cache_unavailable')
 local path=account..'/reading-session-v1.json';local sessions=load(path);sessions.version=1
 local session=sessions.books[id]
 local resume=session and session.uuid==uuid and session.token==token and session.entryAcknowledged==true and not session.finished
 if resume and session.baselineReady then return {state='reading_session_started',bookId=id}end
 if not resume then
  local before,html=progress:fetch(id);local state=require('lib.content').extract_reader_state(html);local W=require('lib.weread')
  local payload=W.make_enter_read_payload({book_id=id,chapter_uid=before.chapterUid,chapter_idx=before.chapterIdx,
   chapter_offset=before.chapterOffset,progress=before.progress,summary=before.summary,psvts=state.psvts,pclts=state.pclts})
  need(require('read_sync_protocol').response_succeeded(progress.client:report_read(payload,W.reader_url(id))),'time_upload_rejected')
  local total,days=totalAt(account,id)
  session={uuid=uuid,token=token,enteredAt=os.time(),startTotal=total,startDays=days,entryAcknowledged=true,baselineReady=false}
  sessions.books[id]=session;save(path,sessions)
 end
 -- A retry after acknowledged entry resumes only this baseline read. It must
 -- not resend enter-read or reset the duration's starting counter/timestamp.
 local after=progress:fetch(id);local old=(progress.cache.books or {})[id] or {}
 progress:saveRecord(id,after,old.displayProgress,{cloudChapter=old.cloudChapter,displayCalculatedAt=old.displayCalculatedAt,
  restorePending=old.restorePending or false,restoreAutomatic=old.restoreAutomatic or false})
 session.baselineReady=true;save(path,sessions)
 return {state='reading_session_started',bookId=id}
end

-- Exit sync combines the exact reading position and eligible time in the same
-- report_read POST. The callback receives each bounded delta and must send the
-- position together with that delta. Its positive receipt advances the same
-- durable no-duplicate journal used by standalone time uploads.
function M.uploadCombined(progress,id,account,uuid,position,send)
 need(type(send)=='function' and type(position)=='table' and type(position.record)=='table','time_cache_unavailable')
 local total,days=totalAt(account,id)
 local path=account..'/uploaded-reading-time-v1.json';local journal=load(path);journal.version=1
 local row=journal.books[id] or {confirmedSeconds=0,confirmedDays={}}
 need(seconds(row.confirmedSeconds) and total>=row.confirmedSeconds,'time_cache_unavailable')
 local sessionPath=account..'/reading-session-v1.json';local sessions=load(sessionPath);local session=sessions.books[id]
 local token=position.sessionToken
 local eligible=0;local allocations={};local liveSession=false
 if type(token)=='string' and #token==36 and session and session.token==token and session.uuid==uuid
     and not session.finished and not row.pending and seconds(session.startTotal) and total>=session.startTotal
     and type(session.enteredAt)=='number' and session.enteredAt>=0 and session.enteredAt==math.floor(session.enteredAt) and os.time()>=session.enteredAt then
  liveSession=true
  eligible=math.min(total-session.startTotal,os.time()-session.enteredAt)
  local keys={};for day in pairs(days)do keys[#keys+1]=day end;table.sort(keys)
  local remaining=eligible;row.confirmedDays=row.confirmedDays or {}
  for _,day in ipairs(keys)do
   local value=days[day];local start=(session.startDays or {})[day] or 0;local sent=row.confirmedDays[day] or 0
   need(seconds(value) and seconds(start) and seconds(sent),'time_cache_unavailable')
   local n=math.min(remaining,math.max(0,value-start),math.max(0,value-sent))
   if n>0 then allocations[day]=n;remaining=remaining-n end
  end
  eligible=eligible-remaining
 end
 -- Manual progress uploads and sessions without a successful entry never send
 -- a duration. Old offline backlogs and ambiguous receipts are not replayed.
 if eligible<=0 then
  local ok,response=pcall(send,0)
  need(ok and require('read_sync_protocol').response_succeeded(response),'progress_sync_unconfirmed')
  return {uploaded=0,remaining=total-row.confirmedSeconds,deferred=total-row.confirmedSeconds,skipped=token~=nil and token~='' and not liveSession,noNewTime=liveSession}
 end
 -- Consume this session before the POST. Unknown outcomes stay pending and
 -- cannot be retried as another duration report.
 session.finished=true;save(sessionPath,sessions)
 row.pending={seconds=eligible,allocations=allocations,localThrough=row.confirmedSeconds+eligible,
  cloudBefore=position.record.readingTime,createdAt=os.time()}
 journal.books[id]=row;save(path,journal)
 local ok,response=pcall(send,eligible)
 need(ok,'time_upload_uncertain')
 if not require('read_sync_protocol').response_succeeded(response)then row.pending=nil;save(path,journal);error('time_upload_rejected',0)end
 for day,n in pairs(allocations)do row.confirmedDays[day]=(row.confirmedDays[day] or 0)+n end
 row.confirmedSeconds=row.pending.localThrough;row.pending=nil;save(path,journal)
 return {uploaded=eligible,remaining=total-row.confirmedSeconds,deferred=total-row.confirmedSeconds}
end
return M
