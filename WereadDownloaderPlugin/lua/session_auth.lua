-- Private session maintenance. No tokens, profile responses or URLs are logged.
local M={}
function M.prepare(settings)
 local saved=settings.session or {}
 settings:set('account_name',type(saved.account_name)=='string'and saved.account_name or '')
 settings:set('auth_renewed_at',tonumber(saved.auth_renewed_at)or 0)
end
function M.renew(settings,force)
 if not settings:is_cookie_configured()then return false,false end
 local now=os.time();local last=settings:get('auth_renewed_at',0)
 if not force and now>=last and now-last<1800 then return true,false end
 local client=require('lib.client'):new(settings)
 require('persistent_http').attach(client)
 local rejected=false;local request=client.request
 client.request=function(self,...)
  local text,code,headers,status=request(self,...)
  if code==401 then rejected=true end
  return text,code,headers,status
 end
 local post=client.post_json
 client.post_json=function(self,...)
  local row,code,headers=post(self,...)
  local errorCode=type(row)=='table'and tonumber(row.errCode or row.errcode)
  if errorCode==-2012 or errorCode==-2013 then rejected=true;error('authentication_required',0)end
  return row,code,headers
 end
 local ok=pcall(function()client:renew_cookie()end)
 if client.persistentTransport then client.persistentTransport:close()end
 if ok then settings:set('auth_renewed_at',now);settings:flush()end
 return ok,rejected
end
function M.name(settings)
 local saved=settings:get('account_name','');if saved~=''then return saved end
 local client=require('lib.client'):new(settings);require('persistent_http').attach(client)
 local ok,name=pcall(function()
  local vid=settings:get('cookies',{}).wr_vid
  assert(type(vid)=='string'and vid:match('^%d+$'))
  local text,code=client:request({url='https://weread.qq.com/api/userInfo?userVid='..vid,method='GET',timeout=10,headers={Accept='application/json',Referer='https://weread.qq.com/'}})
  assert(code and code>=200 and code<300)
  local row=require('json_util').decode(text);local user=row.userInfo or row
  local value=user.name or user.nickname or user.nickName
  assert(type(value)=='string'and#value>0 and#value<=512 and not value:find('[%z\1-\31]'))
  return value
 end)
 if client.persistentTransport then client.persistentTransport:close()end
 if ok then settings:set('account_name',name);settings:flush();return name end
 return ''
end
return M
