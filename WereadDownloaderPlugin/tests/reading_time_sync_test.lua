local modules,work=assert(arg[1]),assert(arg[2])
package.path=modules..'/?.lua;/home/root/weread-downloader/helper/lib/?.lua;'..package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local J=require('json_util');local Sync=require('reading_time_sync')
local id='synthetic';local uuid='synthetic-uuid';local today=os.date('%Y-%m-%d')
local function write(name,v)local f=assert(io.open(work..'/'..name,'wb'));assert(f:write(J.encode(v)));f:close()end
local function read(name)local f=assert(io.open(work..'/'..name));local v=J.decode(f:read('*a'));f:close();return v end
os.remove(work..'/uploaded-reading-time-v1.json')
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=150,days={[today]=125,['2000-01-01']=20}}}})
local record={chapterUid=42,chapterOffset=123,chapterIdx=0,progress=37,summary='synthetic summary',readingTime=1000}
local posts=0;local reject=false;local loseReply=false
package.loaded['lib.content']={extract_reader_state=function()return {token='synthetic'}end}
package.loaded['lib.weread']={reader_url=function()return 'synthetic'end,make_read_payload=function(v)return v end}
local p={cache={books={}},fetch=function()local r={};for k,v in pairs(record)do r[k]=v end;return r,'synthetic'end,
 saveRecord=function()end,client={report_read=function(_,payload)
  posts=posts+1;assert(payload.chapter_uid==42 and payload.chapter_offset==123 and payload.progress==37)
  assert(payload.elapsed_seconds>0 and payload.elapsed_seconds<=60)
  if reject then return {errCode=1}end
  record.readingTime=record.readingTime+payload.elapsed_seconds
  if loseReply then error('connection_lost')end
  return {succ=1}
 end}}
local result=Sync.upload(p,id,work,uuid)
assert(result.uploaded==125 and result.remaining==25 and result.deferred==25 and posts==3 and record.readingTime==1125)
assert(read('uploaded-reading-time-v1.json').books[id].confirmedSeconds==125)
assert(Sync.upload(p,id,work,uuid).uploaded==0 and posts==3)
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=170,days={[today]=145,['2000-01-01']=20}}}})
reject=true;local ok,err=pcall(Sync.upload,p,id,work,uuid)
assert(not ok and err=='time_upload_rejected' and not read('uploaded-reading-time-v1.json').books[id].pending)
reject=false;loseReply=true;ok,err=pcall(Sync.upload,p,id,work,uuid)
assert(not ok and err=='time_upload_uncertain')
local before=posts;loseReply=false;ok,err=pcall(Sync.upload,p,id,work,uuid)
assert(not ok and err=='time_upload_uncertain' and posts==before) -- Never blindly resend a lost reply.
assert(read('local-reading-time-v1.json').books[id].seconds==170)
assert(record.chapterOffset==123 and record.progress==37)
-- Undated legacy data and past-day sessions are never sent as today's reading.
os.remove(work..'/uploaded-reading-time-v1.json')
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=80,days={['2000-01-01']=30}}}})
local oldPosts=posts;result=Sync.upload(p,id,work,uuid)
assert(result.uploaded==0 and result.remaining==80 and result.deferred==80 and posts==oldPosts)
-- Crossing midnight while fetching must stop before the POST.
local realDate=os.date;local dateCalls=0
os.date=function(format,...)if format=='%Y-%m-%d' then dateCalls=dateCalls+1;return dateCalls<3 and today or '2099-01-01'end;return realDate(format,...)end
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=10,days={[today]=10}}}})
result=Sync.upload(p,id,work,uuid);assert(result.uploaded==0 and posts==oldPosts);os.date=realDate
-- Combined exit sync places the position and each acknowledged time chunk in
-- the same write callback; it does not fetch cloud state between those writes.
os.remove(work..'/uploaded-reading-time-v1.json')
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=150,days={[today]=125,['2000-01-01']=20}}}})
local combined={}
result=Sync.uploadCombined(p,id,work,uuid,{record=record,html='synthetic'},function(delta)
 combined[#combined+1]=delta
 assert(record.chapterUid==42 and record.chapterOffset==123 and record.progress==37)
 if delta>0 then record.readingTime=record.readingTime+delta end
 return {succ=1}
end)
assert(result.uploaded==125 and result.remaining==25 and #combined==3 and combined[1]==60 and combined[2]==60 and combined[3]==5)
assert(read('uploaded-reading-time-v1.json').books[id].confirmedSeconds==125)
local zeroCalls=0
result=Sync.uploadCombined(p,id,work,uuid,{record=record,html='synthetic'},function(delta)zeroCalls=zeroCalls+1;assert(delta==0);return {succ=1}end)
assert(result.uploaded==0 and result.remaining==25 and zeroCalls==1)
os.remove(work..'/uploaded-reading-time-v1.json')
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=10,days={[today]=10}}}})
local combinedOk,combinedErr=pcall(function()Sync.uploadCombined(p,id,work,uuid,{record=record},function()return {errCode=1}end)end)
assert(not combinedOk and combinedErr=='time_upload_rejected' and not read('uploaded-reading-time-v1.json').books[id].pending)
combinedOk,combinedErr=pcall(function()Sync.uploadCombined(p,id,work,uuid,{record=record},function()error('connection_lost')end)end)
assert(not combinedOk and combinedErr=='time_upload_uncertain')
local noResend=0
combinedOk,combinedErr=pcall(function()Sync.uploadCombined(p,id,work,uuid,{record=record},function()noResend=noResend+1;return {succ=1}end)end)
assert(not combinedOk and combinedErr=='time_upload_uncertain' and noResend==0)
print('reading time sync: standalone and combined position/time writes, chunking, no duplicates, rejection and ambiguous response refusal ok')
