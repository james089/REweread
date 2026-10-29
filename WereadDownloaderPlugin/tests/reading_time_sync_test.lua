-- Synthetic only: never use a real account or network.
local modules,work=assert(arg[1]),assert(arg[2])
package.path=modules..'/?.lua;apps/weread-move/lib/?.lua;'..package.path
local has,cjson=pcall(require,'cjson');if has then package.loaded.json=cjson end
local J=require('json_util');local T=require('reading_time_sync')
local id='synthetic';local uuid='synthetic-uuid';local token='00000000-0000-4000-8000-000000000001'
local function write(name,v)local f=assert(io.open(work..'/'..name,'wb'));assert(f:write(J.encode(v)));f:close()end
local function read(name)local f=assert(io.open(work..'/'..name));local v=J.decode(f:read('*a'));f:close();return v end
os.remove(work..'/reading-session-v1.json');os.remove(work..'/uploaded-reading-time-v1.json')
local now=100000;local realTime=os.time;os.time=function()return now end
local record={chapterUid=42,chapterIdx=0,chapterOffset=123,progress=37,summary='synthetic',readingTime=1000}
package.loaded['lib.content']={extract_reader_state=function()return {}end}
package.loaded['lib.weread']={e=function()return 'synthetic-pc'end,reader_url=function()return 'synthetic'end,make_enter_read_payload=function(v)v.enter=true;return v end}
local entries,posts,positionOnly,renewals=0,0,0,0;local replies={};local lost=false
local p={cache={books={}},saveRecord=function()end,fetch=function()return record,'synthetic'end,
 client={report_read=function(_,v)assert(v.enter and v.elapsed_seconds==nil);entries=entries+1;return {succ=1}end,
 renew_cookie=function()renewals=renewals+1;return {succ=1}end}}
local function send(delta,fresh)
 if delta==0 then positionOnly=positionOnly+1 else assert(delta>0 and delta<=60);posts=posts+1 end
 if fresh then assert(fresh.record.chapterUid==record.chapterUid and type(fresh.state)=='table')end
 if lost then error('synthetic_disconnect')end
 return table.remove(replies,1) or {succ=1}
end
local function pos(report)return {record=record,sessionToken=token,reportId=report}end
local function report(n)return string.format('00000000-0000-4000-8000-%012d',n)end
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=7200,days={}}}})
assert(T.begin(p,id,work,uuid,token).state=='reading_session_started' and entries==1)
local r=T.uploadCombined(p,id,work,uuid,pos(report(1)),send);assert(r.uploaded==60 and posts==1)
assert(read('reading-session-v1.json').books[id].lastReportState=='acknowledged')
local ok,err=pcall(T.uploadCombined,p,id,work,uuid,pos(report(1)),send);assert(not ok and err=='time_report_duplicate' and posts==1)
ok,err=pcall(T.uploadCombined,p,id,work,uuid,pos(report(2)),send);assert(not ok and err=='time_report_not_due' and posts==1)
now=now+60;assert(T.begin(p,id,work,uuid,token).state=='reading_session_started' and entries==1)
r=T.uploadCombined(p,id,work,uuid,pos(report(2)),send);assert(r.uploaded==60 and posts==2)
-- With no outstanding local time, exit sends only position.
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=120,days={}}}})
r=T.uploadCombined(p,id,work,uuid,pos(nil),send);assert(r.uploaded==0 and posts==2 and positionOnly==1)
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=7200,days={}}}})
-- Definite rejection refreshes context/entry and retries the interval.
now=now+60;replies={{errCode=1},{succ=1}};local prior=entries
r=T.uploadCombined(p,id,work,uuid,pos(report(3)),send);assert(r.uploaded==60 and posts==4 and entries==prior+1)
-- Then renewal if refreshed context is still rejected, subject to cooldown.
now=now+60;replies={{errCode=1},{errCode=1},{succ=1}}
r=T.uploadCombined(p,id,work,uuid,pos(report(4)),send);assert(r.uploaded==60 and posts==7 and renewals==1)
-- Transport/unknown receipt is not replayed; next interval is a new report.
now=now+60;lost=true;ok,err=pcall(T.uploadCombined,p,id,work,uuid,pos(report(5)),send)
assert(not ok and err=='time_upload_uncertain' and posts==8)
lost=false;ok,err=pcall(T.uploadCombined,p,id,work,uuid,pos(report(5)),send);assert(not ok and err=='time_report_duplicate' and posts==8)
now=now+60;ok,err=pcall(T.uploadCombined,p,id,work,uuid,pos(report(6)),send);assert(not ok and err=='time_upload_uncertain' and posts==8)
-- Synthetic operator reconciliation only; production never clears uncertainty.
local ledger=read('uploaded-reading-time-v1.json');ledger.books[id].pending=nil;write('uploaded-reading-time-v1.json',ledger)
now=now+60;replies={{}};local count=posts;prior=entries
ok,err=pcall(T.uploadCombined,p,id,work,uuid,pos(report(7)),send);assert(not ok and err=='time_upload_uncertain' and posts==count+1 and entries==prior)
-- Stale context is refreshed after 15 minutes, preserving interval intent.
now=now+901;prior=entries;T.begin(p,id,work,uuid,token);assert(entries==prior+1)
local session=read('reading-session-v1.json').books[id];assert(session.lastReportId==report(7))
count=posts;ok,err=pcall(T.uploadCombined,p,id,work,uuid,pos(report(5)),send)
assert(not ok and err=='time_report_duplicate' and posts==count and session.reportIntents[report(5)]=='pending')
-- More than two hours drain in one operation, beyond 60 reports/90 seconds.
os.remove(work..'/uploaded-reading-time-v1.json')
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=7265,days={['1970-01-01']=7200,['1970-01-02']=65}}}})
local sleeps=0;p.timeUploadSleep=function(n)assert(n==1);sleeps=sleeps+1;now=now+1 end
local updates={};p.onTimeChunk=function(done,total)updates[#updates+1]={done,total}end
local calls=0;local fetches=0;p.fetch=function()fetches=fetches+1;error('unexpected fetch')end
local function backfill(n)calls=calls+1;assert(n<=60);return {succ=1}end
r=T.upload(p,id,work,uuid,backfill);assert(r.uploaded==7265 and r.remaining==0 and calls==122 and fetches==0 and sleeps==121)
assert(#updates==123 and updates[1][1]==0 and updates[2][1]==60 and updates[123][1]==7265 and updates[123][2]==7265)
r=T.upload(p,id,work,uuid,backfill);assert(r.uploaded==0 and calls==122)
-- Periodic confirmed seconds are not sent again by a later backfill.
local ledger=read('uploaded-reading-time-v1.json').books[id];assert(ledger.confirmedSeconds==7265 and ledger.confirmedDays['1970-01-01']==7200)
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=7325,days={['1970-01-01']=7200,['1970-01-02']=125}}}})
ok,err=pcall(T.upload,p,id,work,uuid,function()return {succ=0}end);assert(not ok and err=='time_upload_rejected')
assert(not read('uploaded-reading-time-v1.json').books[id].pending)
ok,err=pcall(T.upload,p,id,work,uuid,function()error('disconnect')end);assert(not ok and err=='time_upload_uncertain')
local before=calls;ok,err=pcall(T.upload,p,id,work,uuid,backfill);assert(not ok and err=='time_upload_uncertain' and calls==before)
os.time=realTime
-- Exercise the production nanosleep path once, with synthetic POSTs only.
p.timeUploadSleep=nil;p.onTimeChunk=nil;os.remove(work..'/uploaded-reading-time-v1.json')
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=61,days={}}}})
local realStarted=os.time();local realPosts=0
r=T.upload(p,id,work,uuid,function(n)realPosts=realPosts+1;return {succ=1}end)
assert(r.uploaded==61 and realPosts==2 and os.time()-realStarted>=1)
print('periodic reading: 60-second intervals, shared backlog ledger, rejection recovery, renewal, duplicate and ambiguous guards passed')
