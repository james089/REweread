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
function M.upload(progress,id,account,uuid,initialPosition)
 local localRow=load(account..'/local-reading-time-v1.json').books[id]
 if not localRow then return {uploaded=0,remaining=0}end
 need(seconds(localRow.seconds) and localRow.uuid==uuid,'time_cache_unavailable')
 local path=account..'/uploaded-reading-time-v1.json'
 local journal=load(path);journal.version=1
 local row=journal.books[id] or {confirmedSeconds=0}
 need(seconds(row.confirmedSeconds),'time_cache_unavailable')
 need(not row.pending,'time_upload_uncertain')
 local total=localRow.seconds;need(total>=row.confirmedSeconds,'time_cache_unavailable')
 local days=localRow.days or {};need(type(days)=='table','time_cache_unavailable')
 local dated=0
 for date,value in pairs(days)do
  need(type(date)=='string' and date:match('^%d%d%d%d%-%d%d%-%d%d$') and seconds(value),'time_cache_unavailable');dated=dated+value
 end
 need(dated<=total,'time_cache_unavailable')
 row.confirmedDays=row.confirmedDays or {};need(type(row.confirmedDays)=='table','time_cache_unavailable')
 local confirmedDated=0
 for date,value in pairs(row.confirmedDays)do
  need(seconds(value) and value<=(days[date] or 0),'time_cache_unavailable');confirmedDated=confirmedDated+value
 end
 need(confirmedDated<=row.confirmedSeconds and row.confirmedSeconds-confirmedDated<=total-dated,'time_cache_unavailable')
 -- Historical attribution is unverified. Keep previous/undated readings locally;
 -- never silently submit them as activity on the current day.
 local today=readingDay()
 local eligible=(days[today] or 0)-(row.confirmedDays[today] or 0)
 local uploaded=0;local started=os.time()
 for _=1,60 do
  local delta=math.min(60,eligible)
  if delta<=0 or os.time()-started>90 or readingDay()~=today then break end
  local before,html
  if initialPosition then before,html=initialPosition.record,initialPosition.html;initialPosition=nil
  else before,html=progress:fetch(id) end
  need(type(before)=='table' and type(html)=='string' and seconds(before.readingTime),'time_cloud_unavailable')
  local state=require('lib.content').extract_reader_state(html);local W=require('lib.weread')
  need(type(before.chapterUid)=='number' and type(before.chapterOffset)=='number' and type(before.summary)=='string','progress_not_located')
  local payload=W.make_read_payload({book_id=id,chapter_uid=before.chapterUid,chapter_idx=before.chapterIdx,
   chapter_offset=before.chapterOffset,progress=before.progress,summary=before.summary,
   psvts=state.psvts,pclts=state.pclts,token=state.token,elapsed_seconds=delta})
  if readingDay()~=today then break end
  row.pending={date=today,seconds=delta,localThrough=row.confirmedSeconds+delta,cloudBefore=before.readingTime,createdAt=os.time()}
  journal.books[id]=row;save(path,journal)
  local ok,response=pcall(progress.client.report_read,progress.client,payload,W.reader_url(id))
  need(ok,'time_upload_uncertain')
  if not require('read_sync_protocol').response_succeeded(response)then
   row.pending=nil;save(path,journal);error('time_upload_rejected',0)
  end
  -- Like the independent app, a positive business receipt confirms delivery.
  -- Aggregate readingTime can lag; it must not cause an accepted delta to be
  -- resent. A crash before this durable receipt remains conservatively pending.
  row.confirmedDays[today]=(row.confirmedDays[today] or 0)+delta
  row.confirmedSeconds=row.pending.localThrough;row.pending=nil;save(path,journal);uploaded=uploaded+delta;eligible=eligible-delta
 end
 if uploaded>0 then
  local good,after=pcall(function()return progress:fetch(id)end)
  if good then
   local old=progress.cache.books[id] or {}
   progress:saveRecord(id,after,old.displayProgress,{cloudChapter=old.cloudChapter,displayCalculatedAt=old.displayCalculatedAt,
    restorePending=old.restorePending or false,restoreAutomatic=old.restoreAutomatic or false})
  end
 end
 return {uploaded=uploaded,remaining=math.max(0,total-row.confirmedSeconds),deferred=math.max(0,total-row.confirmedSeconds-eligible)}
end

-- Exit sync combines the exact reading position and eligible time in the same
-- report_read POST. The callback receives each bounded delta and must send the
-- position together with that delta. Its positive receipt advances the same
-- durable no-duplicate journal used by standalone time uploads.
function M.uploadCombined(progress,id,account,uuid,position,send)
 need(type(send)=='function' and type(position)=='table' and type(position.record)=='table','time_cache_unavailable')
 local localRow=load(account..'/local-reading-time-v1.json').books[id]
 local path=account..'/uploaded-reading-time-v1.json'
 local journal=load(path);journal.version=1
 local row=journal.books[id] or {confirmedSeconds=0}
 need(seconds(row.confirmedSeconds),'time_cache_unavailable')
 need(not row.pending,'time_upload_uncertain')
 local total=row.confirmedSeconds;local eligible=0;local today=readingDay()
 if localRow then
  need(seconds(localRow.seconds) and localRow.uuid==uuid,'time_cache_unavailable')
  total=localRow.seconds;need(total>=row.confirmedSeconds,'time_cache_unavailable')
  local days=localRow.days or {};need(type(days)=='table','time_cache_unavailable')
  local dated=0
  for date,value in pairs(days)do
   need(type(date)=='string' and date:match('^%d%d%d%d%-%d%d%-%d%d$') and seconds(value),'time_cache_unavailable');dated=dated+value
  end
  need(dated<=total,'time_cache_unavailable')
  row.confirmedDays=row.confirmedDays or {};need(type(row.confirmedDays)=='table','time_cache_unavailable')
  local confirmedDated=0
  for date,value in pairs(row.confirmedDays)do
   need(seconds(value) and value<=(days[date] or 0),'time_cache_unavailable');confirmedDated=confirmedDated+value
  end
  need(confirmedDated<=row.confirmedSeconds and row.confirmedSeconds-confirmedDated<=total-dated,'time_cache_unavailable')
  eligible=(days[today] or 0)-(row.confirmedDays[today] or 0)
 end
 local uploaded=0;local started=os.time();local sent=false
 for _=1,60 do
  local delta=math.min(60,eligible)
  if delta<=0 or os.time()-started>90 or readingDay()~=today then break end
  need(seconds(position.record.readingTime),'time_cloud_unavailable')
  row.pending={date=today,seconds=delta,localThrough=row.confirmedSeconds+delta,
   cloudBefore=position.record.readingTime,createdAt=os.time()}
  journal.books[id]=row;save(path,journal)
  if readingDay()~=today then row.pending=nil;save(path,journal);break end
  local ok,response=pcall(send,delta)
  if not ok then error('time_upload_uncertain',0) end
  if not require('read_sync_protocol').response_succeeded(response)then
   row.pending=nil;save(path,journal);error('time_upload_rejected',0)
  end
  row.confirmedDays[today]=(row.confirmedDays[today] or 0)+delta
  row.confirmedSeconds=row.pending.localThrough;row.pending=nil;save(path,journal)
  uploaded=uploaded+delta;eligible=eligible-delta;sent=true
 end
 if not sent then
  local ok,response=pcall(send,0)
  if not ok or not require('read_sync_protocol').response_succeeded(response) then error('progress_sync_unconfirmed',0) end
 end
 return {uploaded=uploaded,remaining=math.max(0,total-row.confirmedSeconds),
  deferred=math.max(0,total-row.confirmedSeconds-eligible)}
end
return M
