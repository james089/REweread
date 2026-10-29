-- Independent API configuration; never borrow credentials from another app.
local J=require('json_util');local M={};local ffi=require('ffi')
ffi.cdef[[char *realpath(const char*,char*);long readlink(const char*,char*,unsigned long);]]
local function canonical(path)local b=ffi.new('char[4096]');return ffi.C.realpath(path,b)~=nil and ffi.string(b)==path end
local function read(path)if not canonical(path)then return nil end;local f=io.open(path,'rb');if not f then return nil end;local s=f:read(8*1024*1024+1);f:close();if #s>8*1024*1024 then return nil end;local ok,v=pcall(J.decode,s);if ok and type(v)=='table' then return v end end
local function seconds(v)return type(v)=='number' and v>=0 and v<=315360000 and v==math.floor(v)end
function M.normalize(overall,month,old)
 if type(overall)~='table' or not seconds(overall.totalReadTime) or type(month)~='table' or type(month.readTimes)~='table' then error('statistics_unavailable',0)end
 local days={};for day,n in pairs((old or {}).days or {})do if type(day)=='string' and seconds(n) then days[day]=n end end
 for stamp,n in pairs(month.readTimes)do
  local t=tonumber(stamp);if not t or t~=math.floor(t) or not seconds(n) then error('statistics_unavailable',0)end
  local day=os.date('%Y-%m-%d',t);days[day]=n
 end
 return {version=1,totalSeconds=overall.totalReadTime,days=days,fetchedAt=os.time()}
end
function M.fetch(settings,scope,account,baseTime)
 local cfg=read(account..'/statistics-api-v1.json')
 if not cfg or cfg.accountScope~=scope or type(cfg.apiKey)~='string' or #cfg.apiKey<8 then return {state='statistics_error',code='statistics_not_configured',accountScope=scope}end
 local fingerprint=require('sha256').hex(cfg.apiKey)
 local old=read(account..'/cloud-reading-time-v1.json')
 local zone=os.getenv('WEREAD_TIME_ZONE') or 'UTC'
 if old and (old.accountScope~=scope or old.keyFingerprint~=fingerprint or old.timeZone~=zone)then old=nil end
 local wrapped={get=function(_,key,default)if key=='api_key' then return cfg.apiKey end;return settings:get(key,default)end}
 local client=require('lib.client'):new(wrapped)
 local function request(mode,time)
  local ok,value=pcall(client.gateway,client,'/readdata/detail',{mode=mode,baseTime=time,skill_version='1.0.5'})
  if not ok then
   local status=tostring(value):match('HTTP (%d+)')
   error((status=='401' or status=='403') and 'statistics_auth_failed' or 'statistics_unavailable',0)
  end
  return value
 end
 local overall=request('overall',0)
 local month=request('monthly',baseTime or 0)
 if overall.upgrade_info or month.upgrade_info then error('statistics_unavailable',0)end
 local snapshot=M.normalize(overall.data or overall,month.data or month,old)
 snapshot.accountScope=scope;snapshot.keyFingerprint=fingerprint;snapshot.timeZone=zone
 local path=account..'/cloud-reading-time-v1.json';local temporary=path..'.pending'
 assert(canonical(account) and ffi.C.readlink(temporary,ffi.new('char[4096]'),4096)<0);local f=assert(io.open(temporary,'wb'));assert(f:write(J.encode(snapshot)));assert(f:close());assert(os.rename(temporary,path))
 return {state='cloud_time_ready',accountScope=scope}
end
return M
