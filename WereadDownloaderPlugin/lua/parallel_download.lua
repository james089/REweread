-- Reuse DownloadManager's chapter fetch/retry/checkpoint path; assemble strictly
-- in catalog order only after all bounded lanes have finished their assigned chapters.
local M={}
local MAX_LANES=3
local function copy(value)
 if type(value)~='table' then return value end
 local result={}; for key,item in pairs(value) do result[key]=copy(item) end; return result
end
function M.run(manager,job,progress)
 if job.state=='done' or #(job.chapters or {})<4 then return false end
 local scheduler=require('parallel_http').new(); if not scheduler then return false end
 local Persistent=require('persistent_http')
 local lanes,transports,functions={},{},{}
 local results,nextIndex,done={},job.index,0
 local used={}
 for _,cached in pairs(job.cached_chapters or {}) do
  for _,asset in ipairs(cached.assets or {}) do local name=tostring(asset.href or ''):match('([^/]+)$'); if name then used[name]=true end end
 end
 local success,failure=pcall(function()
  for lane=1,MAX_LANES do
   local settings=setmetatable({cache_dir=job.settings.cache_dir,data_dir=job.settings.data_dir,values=copy(job.settings.values)},getmetatable(job.settings))
   local client=manager:client(settings,job.book_id)
   if client.persistentTransport then client.persistentTransport:close() end
   client.request=assert(getmetatable(client).request)
   local owner=assert(Persistent.new({perform=function(handle) return scheduler:await(handle) end}))
   client.persistentTransport=owner; scheduler.owners[#scheduler.owners+1]=owner; transports[lane]=owner
   lanes[lane]={client=client,settings=settings,book=copy(job.work_book),state={css=job.content_state.css,used_asset_names=used}}
   functions[lane]=function()
    local current=lanes[lane]
    while nextIndex<=#job.chapters do
     local index=nextIndex; nextIndex=nextIndex+1
     local part={}; for key,value in pairs(job) do part[key]=value end
     part.index=index; part.client=current.client; part.settings=current.settings; part.work_book=current.book
     part.content_state=current.state; part.selected={}; part.bodies={}; part.assets={}; part.failed={}; part.attempts={}
     repeat manager:step_full_download(part) until part.index~=index
     results[index]=part; done=done+1; if progress then progress(done,#job.chapters) end
    end
   end
  end
  scheduler:run(functions,transports)
 end)
 scheduler:close()
 job.parallelStats={requests=0,connections=0}
 for _,transport in ipairs(transports) do
  job.parallelStats.requests=job.parallelStats.requests+transport.requests
  job.parallelStats.connections=job.parallelStats.connections+transport.connections
 end
 if not success then error(failure,0) end
 job.selected={}; job.bodies={}; job.assets={}; job.failed={}
 for index=job.index,#job.chapters do
  local part=assert(results[index],'chapter result missing')
  for _,chapter in ipairs(part.selected) do job.selected[#job.selected+1]=chapter end
  for uid,body in pairs(part.bodies) do job.bodies[uid]=body end
  for _,asset in ipairs(part.assets) do job.assets[#job.assets+1]=asset end
  for _,errorRow in ipairs(part.failed) do job.failed[#job.failed+1]=errorRow end
 end
 local css={}; for _,lane in ipairs(lanes) do
  if lane.state.css and lane.state.css~='' and css[#css]~=lane.state.css then css[#css+1]=lane.state.css end
 end
 job.content_state.css=table.concat(css,'\n'); job.index=#job.chapters+1
 manager:finish_full_download(job)
 return true
end
return M
