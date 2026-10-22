-- Private device-only sample; never outputs IDs, paths, titles or content.
print=function()end
local lib,root,mode=assert(arg[1]),assert(arg[2]),assert(arg[3])
assert(root:match('^/tmp/wrdp%-real%-speed%.[%w]+$'));assert(mode=='serial'or mode=='parallel')
package.path=lib..'/?.lua;/home/root/weread-downloader/helper/lib/?.lua;'..package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local ffi=require('ffi');ffi.cdef('unsigned int umask(unsigned int mask);');ffi.C.umask(63)
local Json=require('json_util');local socket=require('socket')
local function read(path)local f=assert(io.open(path,'rb'));local row=Json.decode(f:read('*a'));f:close();return row end
local registry=read('/home/root/.local/share/weread-downloader/imports.json')
local chosen,latest
for id,row in pairs(registry.books)do if row.state=='imported'and(not latest or row.updatedAt>latest)then chosen,latest=id,row.updatedAt end end
assert(chosen)
local config=require('config_bridge'):new({config_path='/dev/null',session_path='/home/root/.local/share/weread-downloader/session.json'})
assert(config:is_cookie_configured())
local scope=require('sha256').hex('weread-downloader-account-v1\31'..config:get('cookies',{}).wr_vid)
local shelf=read('/home/root/.local/share/weread-downloader/accounts/'..scope..'/shelf.json')
local book;for _,row in ipairs(shelf.books)do if row.bookId==chosen then book=row;break end end;assert(book)
local statuses=require('book_status_store'):new({root=root})
local manager=require('download_manager'):new({config=config,status_store=statuses,cache_dir=root..'/books'})
require('download_io').attach(statuses,manager)
local job=manager:start_full_download(book)
assert(#job.chapters>=8)
local sample={};for i=math.min(5,#job.chapters-7),math.min(5,#job.chapters-7)+7 do sample[#sample+1]=job.chapters[i]end
job.chapters=sample;job.work_book.chapters=sample;job.cached_chapters={};job.manifest.chapters={}
local started=socket.gettime()
if mode=='parallel'then assert(require('parallel_download').run(manager,job))
else while job.state~='done'do manager:step_full_download(job)end end
local elapsed=socket.gettime()-started
local status=statuses:get(chosen)
assert(status.downloadState=='full'and status.failedChapterCount==0 and#job.selected==8)
local file=assert(io.open(status.fullFile,'rb'));local size=file:seek('end');file:close()
local out=assert(io.open(root..'/result-path','wb'));out:write(status.fullFile);out:close()
io.stdout:write(Json.encode({mode=mode,seconds=elapsed,chapters=#job.selected,bytes=size,failures=status.failedChapterCount})..'\n')
