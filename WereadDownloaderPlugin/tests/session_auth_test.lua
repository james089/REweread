local repo=assert(arg[1]);package.path=repo..'/apps/weread-move/lib/?.lua;'..package.path
local Json=require('json_util');local calls,mode=0,'ok';local now=10000
local time=os.time;os.time=function()return now end
package.loaded['persistent_http']={attach=function(c)return c end}
package.loaded['lib.client']={new=function(_,settings)
 return {request=function()if mode=='offline'then error('synthetic network failure')end;return '{"name":"synthetic account"}',200,{} end,
 post_json=function()return {errCode=mode=='rejected'and -2012 or 0},200,{} end,
 renew_cookie=function(self)
  calls=calls+1;if mode=='offline'then error('synthetic network failure')end
  self:post_json();settings:set('cookies',{wr_vid='123',wr_skey='synthetic renewed'});settings:flush()
 end}
end}
local Session=dofile(repo..'/WereadDownloaderPlugin/lua/session_auth.lua')
local settings={session={account_name='cached account',auth_renewed_at=0},values={cookies={wr_vid='123',wr_skey='synthetic'}},writes=0}
function settings:get(k,d)local v=self.values[k];if v==nil then return d end;return v end
function settings:set(k,v)self.values[k]=v end
function settings:is_cookie_configured()return self.values.cookies.wr_vid~=nil end
function settings:flush()self.writes=self.writes+1 end
Session.prepare(settings);assert(Session.name(settings)=='cached account'and calls==0)
assert(Session.renew(settings,false)and calls==1 and settings:get('auth_renewed_at')==now)
assert(Session.renew(settings,false)and calls==1)
now=now+1801;mode='offline';local ok,rejected=Session.renew(settings,false)
assert(not ok and not rejected and settings:get('cookies').wr_skey=='synthetic renewed'and settings:get('logged_out')~=true)
mode='rejected';ok,rejected=Session.renew(settings,true);assert(not ok and rejected)
mode='ok';settings:set('account_name','');assert(Session.name(settings)=='synthetic account')
settings:set('cookies',{});assert(not Session.renew(settings,true))
os.time=time
print('session renewal, throttling, offline preservation, rejection and cached profile ok')
