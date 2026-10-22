-- Public CDN images only; deliberately no account/config/session is loaded.
print = function() end
package.path = '/home/root/weread-downloader/helper/lib/?.lua;' .. package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local function allowed(url)
 local host = tostring(url):match('^https://([^/:?#]+)')
 return host and not host:find('@',1,true) and (host == 'cdn.weread.qq.com' or host == 'res.weread.qq.com'
   or host:match('%.qq%.com$') or host:match('%.qpic%.cn$') or host:match('%.file%.myqcloud%.com$') or host:match('%.image%.myqcloud%.com$'))
end
local ok = pcall(function()
 local url = assert(arg[1]); assert(#url <= 2048 and allowed(url))
 local settings = {get = function(_, key, fallback) if key == 'cookies' then return {} end; return fallback end,
   set = function() end, flush = function() end}
 local client = require('lib.client'):new(settings)
 require('persistent_http').attach(client)
 assert(client.persistentTransport, 'image transport unavailable')
 for attempt=1,4 do
  local chunks, length = {},0
  local _,code,headers = client:request({url=url,method='GET',timeout=15,
   headers={Accept='image/jpeg,image/png,image/webp,image/*;q=0.8'},
   sink=function(chunk)
    if chunk then length=length+#chunk; if length>4194304 then return nil,'image too large' end; chunks[#chunks+1]=chunk end
    return 1
   end})
  if code==200 then
   local image=table.concat(chunks); assert(#image>0)
   io.stdout:write('WRDP-COVER\n'..tostring(#image)..'\n'..image); io.stdout:flush()
   client.persistentTransport:close(); return
  end
  assert(code and code>=300 and code<400 and attempt<4)
  local target
  for name,value in pairs(headers or {}) do if tostring(name):lower()=='location' then target=value end end
  assert(type(target)=='string' and #target<=2048 and allowed(target)); url=target
 end
end)
if not ok then os.exit(1) end
