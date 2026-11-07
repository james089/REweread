package.path = 'apps/weread-move/lib/?.lua;' .. package.path
local C = require('epub_compatibility')
local original = { ['1'] = '<p>测试&nbsp;正文 &amp; &#160; &lt;nbsp;&amp;nbsp;</p>' }
local image = { href='images/test.png', media_type='image/png', data='\000\255synthetic' }
local bodies, assets = C.prepare(original, {image, image, image})
assert(bodies['1'] == '<p>测试&#160;正文 &amp; &#160; &lt;nbsp;&amp;nbsp;</p>')
assert(original['1']:find('&nbsp;', 1, true), 'cache must remain untouched')
assert(#assets == 1 and assets[1] == image)
assert(C.xhtml(bodies['1']) == bodies['1'], 'normalization is idempotent')
local function conflict(other)
 local ok, err = pcall(C.prepare, original, {image, other})
 assert(not ok and tostring(err):find('Conflicting duplicate EPUB resource',1,true))
end
conflict({href=image.href, media_type=image.media_type, data='different'})
conflict({href=image.href, media_type='image/jpeg', data=image.data})
local path = os.tmpname()
local f = assert(io.open(path,'wb')); f:write(image.data); f:close()
local ok, err = pcall(function()
 local _, disk = C.prepare(original, {image, {href=image.href,media_type=image.media_type,path=path}})
 assert(#disk==1, 'file-backed duplicate must compare actual bytes')
 local _, distinct = C.prepare(original, {image, {href='images/other.png',media_type=image.media_type,path=path}})
 assert(#distinct==2)
end)
os.remove(path); assert(ok, err)
assert(not pcall(C.prepare, {__workspace_text_dir='/not-supported'}, {}))
print('EPUB compatibility behavior ok')
-- The original regression: repeated web image metadata invalidates strict XML.
local source='<div><p>正文</p><img alt="x &gt; y data-w=quoted" src="images/test.png" data-w="10" data-ratio="2" data-w="20" data-ratio="3"/></div>'
local normalized=C.xhtml(source)
assert(normalized:find('data-w="10"',1,true) and not normalized:find('data-w="20"',1,true))
assert(normalized:find('data-ratio="2"',1,true) and not normalized:find('data-ratio="3"',1,true))
assert(normalized:find('alt="x &gt; y data-w=quoted"',1,true),'quoted content must remain intact')
assert(C.xhtml(normalized)==normalized)
local tricky="<p title='a > b &amp; c' data-x='first' data-x='second'>文字</p>"
assert(C.xhtml(tricky)=="<p title='a > b &amp; c' data-x='first'>文字</p>")
local raw='<!-- <img data-w="a" data-w="b"/> --><![CDATA[<img data-w="a" data-w="b"/> &nbsp;]]>'
assert(C.xhtml('<div>'..raw..'</div>')=='<div>'..raw..'</div>','comments/CDATA are not markup to repair')
local doc='<!DOCTYPE html SYSTEM "https://invalid.example/not-fetched.dtd"><html><body>'..source..'</body></html>'
local repaired=C.prepare({['1']=doc},{image},{{chapterUid=1}})
assert(not repaired['1']:find('<!DOCTYPE',1,true))
for _,invalid in ipairs({'<p>broken</div>','<p>&undefined;</p>','<p title="unterminated>text</p>',
 '<div xmlns:a="urn:x" xmlns:b="urn:x" a:x="1" b:x="2"/>',
 '<!DOCTYPE x [<!ENTITY bad "expansion">]><x>&bad;</x>'})do
 local ok,err=pcall(C.prepare,{['1']='<p>valid</p>',['2']=invalid},{},{{chapterUid=1},{chapterUid=2}})
 assert(not ok and tostring(err):find('epub_invalid_xhtml:2',1,true),'strict XML errors must identify the chapter')
end
assert(not pcall(C.prepare,{['1']='<p>valid</p>'},{},{{chapterUid=1},{chapterUid=2}}),'missing chapter must fail')
assert(not pcall(C.prepare,{['1']='<p>valid</p>'},{},{{chapterUid=1},{chapterUid=1}}),'duplicate chapter must fail')
print('EPUB strict XML, duplicate attributes, fragments, chapter completeness and untouched quoted content passed')

local _,_,css=C.prepare({['1']='<p class="used" id="actual">text</p>'},{},nil,
 '.unused{background:url(missing.png)} .used{background:url(keep.png)} #actual{background:url(also.png)} p .nested{background:url(conservative.png)}')
assert(not css:find('missing.png',1,true) and css:find('keep.png',1,true) and css:find('also.png',1,true) and css:find('conservative.png',1,true))
local _,_,encoded=C.prepare({['1']='<p class="u&#115;ed">text</p>'},{},nil,'.used{background:url(keep.png)}')
assert(encoded:find('keep.png',1,true),'encoded selector values must disable pruning')
print('Unused resource CSS pruning preserves active and uncertain selectors')

local pages='<?xml version="1.0"?><html><body><p>第一页</p></body></html><?xml version="1.0"?><html><body><p>第二页</p></body></html>'
local pagesOut=C.prepare({['1']=pages},{},{{chapterUid=1}})
assert(pagesOut['1']:find('第一页',1,true) and pagesOut['1']:find('第二页',1,true),'all fetched page bodies must survive packaging')

-- Real cache shape: outer page wrappers are removed by the EPUB writer.
local wrapped='<?xml version="1.0"?><html class="discarded-head"><head><meta class="discarded-meta"/></head><body class="discarded-body"><p class="kept">page one</p></body></html><?xml version="1.0"?><html><body id="discarded-id"><p>page two</p></body></html>'
local unchanged,_,clean=C.prepare({['1']=wrapped},{},{{chapterUid=1}},
 '.discarded-head{background:url(head.png)} .discarded-meta{background:url(meta.png)} .discarded-body{background:url(body.png)} #discarded-id{background:url(id.png)} .kept{background:url(kept.png)}')
assert(unchanged['1']==wrapped,'cache shape normalization must not discard body content')
assert(not clean:find('head.png',1,true) and not clean:find('meta.png',1,true) and not clean:find('body.png',1,true) and not clean:find('id.png',1,true))
assert(clean:find('kept.png',1,true),'actual body descendant selectors must remain')

local oldAsset={href='images/cached.png',data='\137PNG\r\n\26\nsynthetic'}
local _,mimes=C.prepare({['1']='<p>cached</p>'},{oldAsset,{href=oldAsset.href,data=oldAsset.data,media_type='image/png'}})
assert(#mimes==1 and mimes[1].media_type=='image/png' and oldAsset.media_type==nil,'cached asset metadata must be repaired on an output copy')

local used={classes={},ids={}}
C.xhtml('<p class="real"><!-- <body>not a real wrapper</body> --><![CDATA[<body>example</body>]]></p>',used)
assert(used.classes.real,'body-looking text in comments/CDATA must not change selector ownership')
