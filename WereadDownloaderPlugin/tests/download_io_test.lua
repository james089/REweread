local module = dofile(assert(arg[1]) .. '/WereadDownloaderPlugin/lua/download_io.lua')
local time, writes, directories, cacheDirectories = 1, 0, 0, 0
local rows = {}
local status = {
 ensure = function() directories = directories + 1 end,
 get = function(_, id) rows[id] = rows[id] or {}; return rows[id] end,
}
function status:update(id, patch)
 self:ensure(); writes = writes + 1
 local row = self:get(id); for key, value in pairs(patch) do row[key] = value end
 return row
end
local cache = {ensure = function() cacheDirectories = cacheDirectories + 1 end}
module.attach(status, {chapter_cache=cache}, function() return time end)
for n=1,100 do status:update('synthetic', {downloadState='downloading', progress=n}); cache:ensure('synthetic') end
assert(writes==1 and directories==1 and cacheDirectories==1 and status:get('synthetic').progress==100)
time=2; status:update('synthetic', {downloadState='downloading', progress=101})
status:update('synthetic', {downloadState='full', fullFile='synthetic.epub'})
assert(writes==3 and status:get('synthetic').downloadState=='full')
cache:ensure('second'); assert(cacheDirectories==2)
print('download progress coalesced; final status durable; directories created once per job/book')
