local modules,work=assert(arg[1]),assert(arg[2])
package.path=modules..'/?.lua;/home/root/weread-downloader/helper/lib/?.lua;'..package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local J=require('json_util');local T=require('reading_time_sync')
local id='synthetic';local uuid='synthetic-uuid';local token='00000000-0000-4000-8000-000000000001'
local function write(name,v)local f=assert(io.open(work..'/'..name,'wb'));assert(f:write(J.encode(v)));f:close()end
local function read(name)local f=assert(io.open(work..'/'..name));local v=J.decode(f:read('*a'));f:close();return v end
for _,name in ipairs({'uploaded-reading-time-v1.json','reading-session-v1.json'})do os.remove(work..'/'..name)end
local today=os.date('%Y-%m-%d');local realTime=os.time;local now=realTime();os.time=function()return now end
local record={chapterUid=42,chapterIdx=0,chapterOffset=123,progress=37,summary='synthetic',readingTime=1000}
package.loaded['lib.content']={extract_reader_state=function()return {}end}
package.loaded['lib.weread']={reader_url=function()return 'synthetic'end,make_enter_read_payload=function(v)v.enter=true;return v end}
local entries,posts,positionOnly=0,0,0;local reject=false;local lost=false
local p={cache={books={}},saveRecord=function()end,fetch=function()return record,'synthetic'end,client={report_read=function(_,v)assert(v.enter and v.elapsed_seconds==nil);entries=entries+1;return {succ=1}end}}
local function send(delta)
 if delta==0 then positionOnly=positionOnly+1 else posts=posts+1 end
 if lost then error('synthetic_disconnect')end
 if reject then return {errCode=1}end
 return {succ=1}
end
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=150,days={[today]=125,['2000-01-01']=20}}}})
local ok,err=pcall(T.upload);assert(not ok and err=='manual_time_upload_disabled')
assert(T.begin(p,id,work,uuid,token).state=='reading_session_started' and entries==1)
-- A short read can end before the entry baseline is saved. Valid zero delta
-- is a normal completion, not an entry failure, and must not consume a session.
local empty=T.uploadCombined(p,id,work,uuid,{record=record,sessionToken=token},send)
assert(empty.uploaded==0 and not empty.skipped and empty.noNewTime and posts==0)
assert(not read('reading-session-v1.json').books[id].finished)

now=now+20
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=170,days={[today]=145,['2000-01-01']=20}}}})
local position={record=record,sessionToken=token}
local result=T.uploadCombined(p,id,work,uuid,position,send)
assert(result.uploaded==20 and result.remaining==150 and posts==1)
local sent=read('uploaded-reading-time-v1.json').books[id]
assert(sent.confirmedSeconds==20 and sent.confirmedDays[today]==20 and not sent.pending)
assert(T.uploadCombined(p,id,work,uuid,position,send).uploaded==0 and posts==1)
-- Manual progress never drains older pending duration.
assert(T.uploadCombined(p,id,work,uuid,{record=record},send).uploaded==0 and posts==1)
-- A second live session only includes its newly observed time, not old backlogs.
local token2='00000000-0000-4000-8000-000000000002';T.begin(p,id,work,uuid,token2);now=now+10
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=180,days={[today]=155,['2000-01-01']=20}}}})
reject=true;ok,err=pcall(T.uploadCombined,p,id,work,uuid,{record=record,sessionToken=token2},send)
assert(not ok and err=='time_upload_rejected' and not read('uploaded-reading-time-v1.json').books[id].pending)
reject=false
assert(T.uploadCombined(p,id,work,uuid,{record=record,sessionToken=token2},send).uploaded==0)
-- An ambiguous POST is never retried; the receipt journal remains pending.
local token3='00000000-0000-4000-8000-000000000003';T.begin(p,id,work,uuid,token3);now=now+15
write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=195,days={[today]=170,['2000-01-01']=20}}}})
lost=true;ok,err=pcall(T.uploadCombined,p,id,work,uuid,{record=record,sessionToken=token3},send)
assert(not ok and err=='time_upload_uncertain' and read('uploaded-reading-time-v1.json').books[id].pending.seconds==15)
lost=false;local beforePosts=posts
assert(T.uploadCombined(p,id,work,uuid,{record=record,sessionToken=token3},send).uploaded==0 and posts==beforePosts)
assert(T.uploadCombined(p,id,work,uuid,{record=record,sessionToken='missing'},send).uploaded==0 and posts==beforePosts)
-- If entry was acknowledged but its baseline fetch failed, retry must resume
-- the read without another enter report or resetting the duration start.
local token4='00000000-0000-4000-8000-000000000004';local fetches=0
local fetch=p.fetch;p.fetch=function()fetches=fetches+1;if fetches==2 then error('cloud_progress_unavailable',0)end;return record,'synthetic'end
local priorEntries=entries;local entered=now
ok,err=pcall(T.begin,p,id,work,uuid,token4)
assert(not ok and err=='cloud_progress_unavailable' and entries==priorEntries+1)
local pendingEntry=read('reading-session-v1.json').books[id]
assert(pendingEntry.entryAcknowledged and not pendingEntry.baselineReady and pendingEntry.startTotal==195)
now=now+10;write('local-reading-time-v1.json',{version=1,books={[id]={uuid=uuid,seconds=205,days={[today]=180,['2000-01-01']=20}}}})
p.fetch=fetch;assert(T.begin(p,id,work,uuid,token4).state=='reading_session_started')
local resumed=read('reading-session-v1.json').books[id]
assert(entries==priorEntries+1 and resumed.baselineReady and resumed.startTotal==195 and resumed.enteredAt==entered)
assert(T.begin(p,id,work,uuid,token4).state=='reading_session_started' and entries==priorEntries+1)
os.time=realTime
print('reading time: manual disabled; live session single delta, no backlog replay, no duplicates and ambiguous receipt preservation passed')
