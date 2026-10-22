package.path = 'WereadDownloaderPlugin/tools/?.lua;' .. package.path
local M = require('progress_probe_match')
local text = '';for i=1,120 do text=text..M.utf8(0x4e00+i) end
assert(M.normalize(' a\n b　c ')=='abc')
assert(M.xhtml('<html><head><style>x</style></head><body>A&amp;B&#x4e2d;&#25991;</body></html>')=='A&B中文')
assert(#M.candidates(text,'<p>'..text..'</p>')==1)
assert(#M.candidates(text,'<p>'..text..text..'</p>')==2)
assert(#M.candidates(string.rep('中',64),string.rep('中',65))==2)
assert(#M.candidates('too short',text)==0)
assert(#M.candidates(text,'unrelated')==0)
assert(M.utf8(0x1f600)=='😀' and M.utf8(0xd800)=='')
print('research anchor matching: unique/repeated/short/Unicode cases passed')
local P=require('saved_progress_probe')
local captured
local saved={chapterUid='synthetic-42',chapterOffset=19}
local function decode(s) captured=s;return {reader={progress={book=saved},currentChapter={chapterUid='wrong'}}} end
local parsed=P.extract('before window.__INITIAL_STATE__ = {"x":"escaped\\\" } {","z":{}}; after',decode)
assert(parsed==saved and captured=='{"x":"escaped\\\" } {","z":{}}')
assert(P.extract('window.__INITIAL_STATE__={"unclosed":{',decode)==nil)
assert(P.extract('no assignment',decode)==nil)
assert(P.extract('window.__INITIAL_STATE__={}',function()error('invalid')end)==nil)
assert(P.extract('window.__INITIAL_STATE__={}',function()return {reader={currentChapter=saved}}end)==nil)
print('saved progress parser: quoted braces, truncation, errors and record isolation passed')
local S=require('progress_source_map')
local html='<html><head><style>hidden</style></head><body><p>'..text..'</p></body></html>'
local pos=assert(S.locate(text,html))
assert(pos.coordinateAgrees and pos.codepoints==#'<html><head><style>hidden</style></head><body><p>')
assert(pos.skipped==0)
assert(S.locate(text,'<p>'..text..text..'</p>')==nil)
local emoji=assert(S.locate(text,'<p>😀'..text..'</p>'))
assert(not emoji.coordinateAgrees and emoji.utf16==emoji.codepoints+1)
local entity=assert(S.index('<p>A&amp;&#x4e2d;&#25991;</p>'))
assert(entity.text=='A&中文' and entity.positions[2].codepoints==4)
assert(S.index('<p>&unknown;</p>')==nil)
assert(S.index('<p title="bad>')==nil)
assert(S.index('<script>unfinished')==nil)
local quoted=assert(S.locate(text,'<p title=">">'..text..'</p>'))
assert(quoted.codepoints==#'<p title=">">')
print('original source mapping: markup/entities/ambiguity/UTF16/hidden text passed')
local V=require('progress_cache_verify')
local catalog={{chapterUid=42,chapterIdx=0,title='one'},{chapterUid=901,chapterIdx=1,title='two'}}
assert(V.catalog(catalog)~=V.catalog({catalog[2],catalog[1]}))
assert(V.checksum('test')=='045d01c1')
local calls,closed=0,false
local fakeClient={request=function(_,opts) calls=calls+1;return '{}',200 end,persistentTransport={close=function()closed=true end}}
package.loaded.config_bridge={new=function(_,opts)
    assert(opts.config_path=='/dev/null' and opts.session_path=='/home/root/.local/share/weread-downloader/session.json')
    return {is_cookie_configured=function()return true end,get=function(_,key)assert(key=='cookies');return {wr_vid='123'}end}
end}
package.loaded.sha256={hex=function()return string.rep('a',64)end}
package.loaded['lib.client']={new=function()return fakeClient end}
package.loaded.persistent_http={attach=function()end}
package.loaded['lib.weread']={reader_url=function()return 'https://weread.qq.com/web/reader/test123'end}
package.loaded.json_util={decode=function()return {}end}
package.loaded['lib.content']={fetch_catalog=function(client)
    client:request({method='POST',url='https://weread.qq.com/web/book/chapterInfos'});return catalog
end,fetch_chapter_xhtml=function(client)
    client:request({method='POST',url='https://weread.qq.com/web/book/chapter/e_0'});return '<p>synthetic</p>'
end}
local R=require('read_progress_source').open()
assert(R:catalog('42')==catalog)
assert(R:source('42',catalog[1])=='<p>synthetic</p>')
R:saved('42')
assert(not pcall(function()fakeClient:request({method='POST',url='https://weread.qq.com/web/book/read'})end))
assert(not pcall(function()fakeClient:request({method='GET',url='https://example.org/web/reader/test123'})end))
assert(not pcall(function()fakeClient:request({method='POST',url='https://weread.qq.com/web/book/chapter/t_0'})end))
R:close();assert(closed and calls==3 and R.requests==3)
print('read endpoint guard: isolated session and forbidden write endpoints passed')
