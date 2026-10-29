local modules,work=assert(arg[1]),assert(arg[2])
package.path=modules..'/?.lua;/home/root/weread-downloader/helper/lib/?.lua;'..package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local J=require('json_util');local M=require('cloud_reading_stats')
local epoch=os.time({year=2026,month=10,day=26,hour=12});local day=os.date('%Y-%m-%d',epoch)
local snapshot=M.normalize({totalReadTime=5000},{readTimes={[tostring(epoch)]=120}}, {days={['2000-01-01']=35}})
assert(snapshot.totalSeconds==5000 and snapshot.days[day]==120 and snapshot.days['2000-01-01']==35)
assert(not pcall(M.normalize,{totalReadTime='5000'},{readTimes={}}))
assert(not pcall(M.normalize,{totalReadTime=5000},{readTimes={bad=20}}))
local function write(name,v)local f=assert(io.open(work..'/'..name,'wb'));assert(f:write(J.encode(v)));f:close()end
local calls=0;local fixtureKey='synthetic-statistics-key'
package.loaded['lib.client']={new=function(_,settings)
 assert(settings:get('api_key')=='synthetic-statistics-key')
 return {gateway=function(_,api,params)
  calls=calls+1;assert(api=='/readdata/detail' and params.skill_version=='1.0.5')
  if params.mode=='overall' then return {totalReadTime=5000}end
  assert(params.mode=='monthly');return {readTimes={[tostring(epoch)]=120}}
 end}
end}
assert(M.fetch({},'synthetic-scope',work,0).code=='statistics_not_configured' and calls==0)
write('statistics-api-v1.json',{version=1,accountScope='other-scope',apiKey=fixtureKey})
assert(M.fetch({},'synthetic-scope',work,0).code=='statistics_not_configured' and calls==0)
write('statistics-api-v1.json',{version=1,accountScope='synthetic-scope',apiKey=fixtureKey})
assert(M.fetch({get=function(_,_,d)return d end},'synthetic-scope',work,epoch).state=='cloud_time_ready' and calls==2)
local f=assert(io.open(work..'/cloud-reading-time-v1.json'));local saved=J.decode(f:read('*a'));f:close()
assert(saved.accountScope=='synthetic-scope' and saved.totalSeconds==5000 and saved.days[day]==120)
assert(saved.apiKey==nil and saved.keyFingerprint~=nil)
package.loaded['lib.client']={new=function()return {gateway=function()error('HTTP 401, redacted',0)end}end}
local authOk,authError=pcall(M.fetch,{get=function(_,_,d)return d end},'synthetic-scope',work,epoch)
assert(not authOk and authError=='statistics_auth_failed')
print('cloud statistics: independent config, account binding, cloud-only daily/total cache and malformed data rejection passed')
