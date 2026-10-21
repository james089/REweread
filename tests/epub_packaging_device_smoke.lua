-- Synthetic integration: real DownloadManager + installed Content + ZIP writer.
-- Run from KOReader's working directory, with candidate helper modules first.
local helper = '/home/root/weread-qt/helper'
package.path = '/tmp/reweread-epub-test/?.lua;' .. helper .. '/lib/?.lua;' .. helper .. '/?.lua;' .. package.path
require('koreader_paths').append()
local Content = require('lib.content')
local Manager = require('download_manager')
local Archive = require('ffi/archiver')
local ffi = require('ffi')
ffi.cdef[[void *xmlReadMemory(const char*,int,const char*,const char*,int); void xmlFreeDoc(void*);]]
local xml = ffi.load('/usr/lib/libxml2.so.2')
local root = '/tmp/reweread-epub-test/output'
assert(os.execute('mkdir -p ' .. root) == 0)
-- No account or user annotation state participates in this test.
Content.register_annotation_document = function() end
local chapters = {{chapterUid=1,title='Synthetic body'}, {chapterUid=2,title='Synthetic image'}}
local body = '<p>正文&nbsp;测试 &amp; &#160;</p><img src="../images/probe.png" alt="test"/>'
local png = ('89504e470d0a1a0a0000000d4948445200000001000000010804000000b51c0c020000000b4944415478da6364f80f00010501012718e3660000000049454e44ae426082'):gsub('..', function(h)return string.char(tonumber(h,16))end)
local image = {href='images/probe.png',media_type='image/png',data=png}
local function verify(path)
 local archive = Archive.Reader:new(); assert(archive:open(path))
 local seen, count, image_count = {}, 0, 0
 for entry in archive:iterate() do
  if entry.mode=='file' then
   assert(not seen[entry.path], 'duplicate ZIP member'); seen[entry.path]=true
   local data=assert(archive:extractToMemory(entry.path))
   if entry.path:match('%.xhtml$') or entry.path:match('%.xml$') or entry.path:match('%.opf$') or entry.path:match('%.ncx$') then
    local doc=xml.xmlReadMemory(data,#data,'synthetic.xml',nil,32+64+2048)
    assert(doc~=nil,'strict XML parse failed'); xml.xmlFreeDoc(doc); count=count+1
   end
   if entry.path=='OEBPS/content.opf' then
    local _, n=data:gsub('href="images/probe.png"',''); assert(n==1,'duplicate manifest href')
   end
   if entry.path=='OEBPS/images/probe.png' then assert(data==png);image_count=image_count+1 end
   if entry.path=='OEBPS/text/chapter-001.xhtml' then
    assert(data:find('正文&#160;测试 &amp; &#160;',1,true),'body semantics changed')
   end
  end
 end
 archive:close(); assert(count>=6 and image_count==1)
 assert(os.execute('unzip -t "' .. path .. '" >/dev/null 2>&1')==0)
end
for _, mode in ipairs({'full','partial','opening'}) do
 local manager=setmetatable({},Manager)
 local book={bookId='synthetic-epub-'..mode,title='Synthetic EPUB'}
 local job={book=book,work_book=book,book_id=book.bookId,selected=chapters,chapters=chapters,
  bodies={['1']=body,['2']=body},assets={image,image,image},settings={cache_dir=root},
  content_state={},failed=mode=='partial' and {{}} or {},client={}}
 local path
 if mode=='opening' then path=manager:finish_opening_download(job)
 else path=manager:finish_full_download(job) end
 verify(path)
 assert(job.bodies['1']==body and #job.assets==3,'source cache inputs changed')
 print('EPUB_TEST mode='..mode..' strict_xml=ok resources=ok zip=ok')
end
