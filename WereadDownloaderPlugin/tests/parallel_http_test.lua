print=function() end
local root=assert(arg[1]); package.path=root..'/?.lua;/home/root/weread-downloader/helper/lib/?.lua;'..package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local socket=require('socket'); local Persistent=require('persistent_http'); local Http=require('parallel_http')
local http=require('socket.http'); local original=http.request
local base='http://127.0.0.1:18767'
local function request(owner,index)
 local bytes={}; local good,code=owner:request({url=base..'/task-'..index,sink=function(chunk) if chunk then bytes[#bytes+1]=chunk end; return 1 end})
 assert(good and code==200 and table.concat(bytes)=='synthetic:/task-'..index,'response isolation')
end
local serial=assert(Persistent.new()); local started=socket.gettime()
for i=1,8 do request(serial,i) end
local sequential=socket.gettime()-started; serial:close()
local scheduler=assert(Http.new()); local owners,fn={},{}
for lane=1,2 do
 local owner=assert(Persistent.new({perform=function(handle)return scheduler:await(handle)end}))
 scheduler.owners[#scheduler.owners+1]=owner; owners[lane]=owner
 fn[lane]=function() for i=lane,8,2 do request(owner,i) end end
end
started=socket.gettime(); scheduler:run(fn,owners); local parallel=socket.gettime()-started; scheduler:close()
assert(http.request==original,'restore global HTTP dispatcher')
assert(parallel<sequential*.8,'overlap network waits')
-- Real content fetches inline images inside a C string.gsub callback. Such a
-- callback cannot yield; a pending handle must not outlive its coroutine.
local images=assert(Http.new());local imageOwners,imageFunctions={},{}
for lane=1,2 do
 local owner=assert(Persistent.new({perform=function(handle)return images:await(handle)end}))
 images.owners[#images.owners+1]=owner;imageOwners[lane]=owner
 imageFunctions[lane]=function()
  for i=1,4 do
   request(owner,i)
   local replaced=('image'):gsub('image',function()request(owner,9);return 'local image'end)
   assert(replaced=='local image')
  end
 end
end
images:run(imageFunctions,imageOwners);images:close()
assert(http.request==original,'restore dispatcher after inline images')
local failed=assert(Http.new()); local owner=assert(Persistent.new({perform=function(handle)return failed:await(handle)end})); failed.owners[1]=owner
assert(not pcall(function() failed:run({function() request(owner,9); error('synthetic failure') end},{owner}) end),'propagate coroutine failure')
assert(http.request==original and failed.multi==nil,'cleanup after failure')
io.stdout:write(string.format('parallel-http=ok serial=%.3f parallel=%.3f ratio=%.2f\n',sequential,parallel,parallel/sequential))
