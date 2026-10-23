-- A bounded coroutine scheduler over the existing verified libcurl transport.
-- One event loop, at most three independent easy handles; no credential logs or threads.
local M = {}
function M.new()
 local ffi = require('ffi')
 local declared = pcall(ffi.typeof, "wrdp_curl_message")
 local ok = declared or pcall(ffi.cdef, [[
  typedef struct { int msg; void *easy_handle; union { void *whatever; int result; } data; } wrdp_curl_message;
  void *curl_multi_init(void);
  int curl_multi_cleanup(void *multi);
  int curl_easy_perform(void *easy);
  int curl_multi_add_handle(void *multi, void *easy);
  int curl_multi_remove_handle(void *multi, void *easy);
  int curl_multi_perform(void *multi, int *running);
  int curl_multi_poll(void *multi, void *extra, unsigned int count, int timeout_ms, int *events);
  wrdp_curl_message *curl_multi_info_read(void *multi, int *remaining);
 ]])
 if not ok then return nil end
 local loaded, curl = pcall(ffi.load, '/usr/lib/libcurl.so.4'); if not loaded then return nil end
 local multi = curl.curl_multi_init(); if multi == nil then return nil end
 local self = {multi=multi, waiting={}, contexts={}, owners={}}
 function self:await(handle)
  local co = coroutine.running(); assert(co and self.contexts[co], 'unmanaged transfer')
  -- Upstream embeds image requests in string.gsub callbacks. Lua cannot yield
  -- across that C callback: do not register a pending multi handle first and
  -- leave it pointing at a coroutine that continues or has already finished.
  -- These requests retain the same verified transport and connection cache.
  if not coroutine.isyieldable or not coroutine.isyieldable() then
   return curl.curl_easy_perform(handle)
  end
  assert(curl.curl_multi_add_handle(self.multi, handle)==0, 'parallel transport setup failed')
  self.waiting[tostring(handle)]={co=co,handle=handle}
  return coroutine.yield()
 end
 function self:close()
  if not self.multi then return end
  for _,pending in pairs(self.waiting) do curl.curl_multi_remove_handle(self.multi,pending.handle) end
  self.waiting={}; curl.curl_multi_cleanup(self.multi); self.multi=nil
  for _,owner in ipairs(self.owners) do owner:close() end
  self.owners={}
 end
 function self:run(functions, transports)
  assert(#functions<=3 and #functions==#transports, "parallelism must be bounded to three")
  local http=require('socket.http'); local original=http.request
  http.request=function(opts,...)
   local owner=self.contexts[coroutine.running()]
   if owner then
    assert(type(opts)=='table' and opts.sink and not opts.proxy and not http.PROXY and not opts.create,
      'unsupported parallel HTTP options')
    return owner:request(opts)
   end
   return original(opts,...)
  end
  local function resume(co,...)
   local success,failure=coroutine.resume(co,...); assert(success,failure)
  end
  local success,failure=pcall(function()
   for index,fn in ipairs(functions) do
    local co=coroutine.create(fn); self.contexts[co]=transports[index]; resume(co)
   end
   local running,remaining,events=ffi.new('int[1]'),ffi.new('int[1]'),ffi.new('int[1]')
   while next(self.waiting) do
    assert(curl.curl_multi_perform(self.multi,running)==0,'parallel transfer failed')
    local completed={}
    while true do
     local message=curl.curl_multi_info_read(self.multi,remaining); if message==nil then break end
     if message.msg==1 then completed[#completed+1]={key=tostring(message.easy_handle),result=tonumber(message.data.result)} end
    end
    for _,done in ipairs(completed) do
     local pending=assert(self.waiting[done.key]); self.waiting[done.key]=nil
     assert(curl.curl_multi_remove_handle(self.multi,pending.handle)==0,'parallel completion failed')
     resume(pending.co,done.result)
    end
    if next(self.waiting) then assert(curl.curl_multi_poll(self.multi,nil,0,100,events)==0,'parallel wait failed') end
   end
  end)
  http.request=original
  if not success then self:close(); error(failure,0) end
 end
 if jit then jit.off(self.run,true); jit.off(self.await,true) end
 return self
end
return M
