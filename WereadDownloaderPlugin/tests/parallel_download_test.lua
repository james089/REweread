print=function()end
local lib,root=assert(arg[1]),assert(arg[2]); package.path=lib..'/?.lua;/home/root/weread-downloader/helper/lib/?.lua;'..package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local http=require('socket.http'); local original=http.request
local fetches,attempts=0,{}
local Content={}
function Content.ensure_reader_state()end
function Content.fetch_catalog() local rows={};for i=1,8 do rows[i]={chapterUid=i,title='synthetic '..i,updateTime=1}end;return rows end
function Content.fetch_single_chapter_content(client,settings,book,chapter,state)
 local index=chapter.chapterUid; fetches=fetches+1; attempts[index]=(attempts[index]or 0)+1
 local text=client:get_text('http://127.0.0.1:18767/task-'..index)
 if index==3 and attempts[index]==1 then error('synthetic transient failure')end
 state.css='body { color: black; }'
 local name='shared.png'; if state.used_asset_names[name]then name='shared-'..index..'.png'end;state.used_asset_names[name]=true
 return '<html><body><p>chapter '..index..'</p><img src="../images/'..name..'"/></body></html>',{{href='images/'..name,data='synthetic image '..index,media_type='image/png'}}
end
function Content.save_book_epub(settings,book,chapters,bodies,kind,assets,css)
 assert(#chapters==8 and #assets==8 and kind=='full'and css:find('color: black',1,true))
 local names={};for _,asset in ipairs(assets)do assert(not names[asset.href]);names[asset.href]=true end
 for index,chapter in ipairs(chapters)do assert(chapter.chapterUid==index and bodies[tostring(index)]:find('chapter '..index,1,true))end
 local path=root..'/books/synthetic/full.epub';local f=assert(io.open(path,'wb'));f:write('synthetic full');f:close();return path
end
package.loaded['lib.content']=Content
local function clone(v)if type(v)~='table'then return v end;local r={};for k,x in pairs(v)do r[k]=clone(x)end;return r end
local config={values={cookies={wr_vid='synthetic',wr_skey='synthetic'},cache={download_book_images=true,show_annotations=false}}}
function config:reload()end
function config:get(k,default)return clone(self.values[k]or default)end
local statuses=require('book_status_store'):new({root=root})
local manager=require('download_manager'):new({config=config,status_store=statuses,cache_dir=root..'/books'})
require('download_io').attach(statuses,manager)
local book={bookId='synthetic',title='synthetic',author='synthetic'}
local job=manager:start_full_download(book)
local events=0
assert(require('parallel_download').run(manager,job,function(done,total)events=events+1;assert(done==events and total==8)end))
assert(job.state=='done'and#job.selected==8 and#job.failed==0 and attempts[3]==2 and fetches==9)
assert(statuses:get('synthetic').downloadState=='full'and http.request==original)
fetches=0
local resumed=manager:start_full_download(book,{force=true})
assert(require('parallel_download').run(manager,resumed))
assert(resumed.state=='done'and fetches==0,'verified cache resume must skip requests')
io.stdout:write('parallel-download=ok ordered chapters, unique images, retry, checkpoints and cache resume\n')
