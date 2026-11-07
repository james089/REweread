-- Normalize packaging inputs without changing the verified chapter cache.
local Compatibility = {packagingVersion=2}

local function normalize_tag(tag,used)
    local name=tag:match('^<([%a_][%w_.:-]*)')
    assert(name,'Invalid EPUB start tag')
    local out,seen={'<'..name},{};local at=#name+2
    while at<=#tag do
        local start=at
        while tag:sub(at,at):match('%s') do at=at+1 end
        local tail=tag:sub(at)
        if tail=='>' or tail=='/>' then out[#out+1]=tag:sub(start);return table.concat(out) end
        assert(at>start,'Missing EPUB attribute separator')
        local key=tag:sub(at):match('^([%a_:][%w_.:-]*)')
        assert(key,'Invalid EPUB attribute name');at=at+#key
        while tag:sub(at,at):match('%s') do at=at+1 end
        assert(tag:sub(at,at)=='=','Invalid EPUB attribute');at=at+1
        while tag:sub(at,at):match('%s') do at=at+1 end
        local quote=tag:sub(at,at);assert(quote=='"' or quote=="'",'Unquoted EPUB attribute')
        local finish=tag:find(quote,at+1,true);assert(finish,'Unclosed EPUB attribute')
        if not seen[key] then
            if used and (key=='class' or key=='id') then
                local value=tag:sub(at+1,finish-1)
                if value:find('&',1,true) then used.uncertain=true
                elseif key=='id' then used.ids[value]=true
                else for name in value:gmatch('%S+')do used.classes[name]=true end end
            end
            -- Preserve first-value HTML semantics. Never touch quoted content
            -- while looking for duplicate names, including '>' or '<' in values.
            out[#out+1]=(tag:sub(start,finish):gsub('&nbsp;','&#160;'));seen[key]=true
        end
        at=finish+1
    end
    error('Unclosed EPUB tag',0)
end

function Compatibility.xhtml(text,used)
    assert(type(text)=='string','Invalid EPUB chapter input')
    local out,at={},1
    local sawBody,insideBody=false,false
    local allUsed=used and {classes={},ids={}}
    local bodyUsed=used and {classes={},ids={}}
    while at<=#text do
        local first=text:find('<',at,true)
        if not first then out[#out+1]=(text:sub(at):gsub('&nbsp;','&#160;'));break end
        out[#out+1]=(text:sub(at,first-1):gsub('&nbsp;','&#160;'))
        local finish,raw
        if text:sub(first,first+3)=='<!--' then
            finish=assert(text:find('-->',first+4,true),'Unclosed EPUB comment')+2;raw=true
        elseif text:sub(first,first+8)=='<![CDATA[' then
            finish=assert(text:find(']]>',first+9,true),'Unclosed EPUB CDATA')+2;raw=true
        elseif text:sub(first,first+1)=='<?' then
            finish=assert(text:find('?>',first+2,true),'Unclosed EPUB instruction')+1;raw=true
        else
            local quote;finish=first+1
            while finish<=#text do
                local char=text:sub(finish,finish)
                if quote then if char==quote then quote=nil end
                elseif char=='"' or char=="'" then quote=char
                elseif char=='>' then break end
                finish=finish+1
            end
            assert(finish<=#text,'Unclosed EPUB tag')
        end
        local tag=text:sub(first,finish)
        if tag:match('^<!DOCTYPE%s') then
            -- Output is self-contained XML. External XHTML DTDs are unnecessary;
            -- internal subsets/entities are refused rather than expanded.
            assert(not tag:find('[',1,true),'EPUB internal DTD is unsupported')
        elseif raw or tag:sub(1,2)=='</' then
            if tag:match('^</body%s*>$')then insideBody=false end
            out[#out+1]=tag
        else
            local bodyTag=tag:match('^<body[%s>]')~=nil
            -- The writer discards html/head/body wrappers and joins only body
            -- children. Wrapper classes must not keep dead CSS references alive.
            local activeUsed=insideBody and bodyUsed or allUsed
            if bodyTag then activeUsed=nil;sawBody=true end
            out[#out+1]=normalize_tag(tag,activeUsed)
            if bodyTag then insideBody=true end
        end
        at=finish+1
    end
    if used then
        local selected=sawBody and bodyUsed or allUsed
        for name in pairs(selected.classes)do used.classes[name]=true end
        for name in pairs(selected.ids)do used.ids[name]=true end
        if selected.uncertain then used.uncertain=true end
    end
    return table.concat(out)
end

local function prune_unused_css(css,used)
    if not css or used.uncertain then return css end
    return (css:gsub('([^{}]+){([^{}]*)}',function(selector,body)
        if not body:lower():find('url%s*%(') then return selector..'{'..body..'}' end
        local clean=selector:gsub('/%*.-%*/','')
        local count=0
        for part in (clean..','):gmatch('(.-),')do
            part=part:match('^%s*(.-)%s*$')
            local kind,name=part:match('^([.#])([%w_-]+)$')
            if not kind or (kind=='.' and used.classes[name]) or (kind=='#' and used.ids[name]) then
                return selector..'{'..body..'}'
            end
            count=count+1
        end
        if count==0 then return selector..'{'..body..'}' end
        return ''
    end))
end

local function asset_bytes(asset)
    if asset.path then
        local file = assert(io.open(asset.path, "rb"), "EPUB resource is unavailable")
        local data = file:read("*a")
        file:close()
        return assert(data, "Could not read EPUB resource")
    end
    return assert(asset.data, "EPUB resource has no data")
end

local function asset_with_type(asset)
    if type(asset.media_type)=='string' and asset.media_type~='' then return asset end
    -- Old verified chapter caches stored image bytes/href but not MIME type.
    -- Repair packaging metadata on a copy, without redownloading or rewriting cache.
    local bytes=asset_bytes(asset);local mime
    if bytes:sub(1,8)=='\137PNG\r\n\26\n' then mime='image/png'
    elseif bytes:sub(1,3)=='\255\216\255' then mime='image/jpeg'
    elseif bytes:sub(1,3)=='GIF' then mime='image/gif'
    elseif bytes:sub(1,4)=='RIFF' and bytes:sub(9,12)=='WEBP' then mime='image/webp' end
    local ext=tostring(asset.href):lower():match('%.([%w]+)$')
    mime=mime or ({png='image/png',jpg='image/jpeg',jpeg='image/jpeg',gif='image/gif',webp='image/webp',svg='image/svg+xml',
        css='text/css',ttf='font/ttf',otf='font/otf',woff='font/woff',woff2='font/woff2'})[ext] or 'application/octet-stream'
    local result={};for key,value in pairs(asset)do result[key]=value end;result.media_type=mime;return result
end

function Compatibility.prepare(bodies, assets, chapters, css)
    -- Current Qt jobs supply in-memory chapter bodies. Refuse unsupported
    -- streamed inputs rather than silently bypassing normalization.
    assert(not bodies.__workspace_text_dir, "Unsupported streamed EPUB input")
    local expected,count={},0
    if chapters then
        for index,chapter in ipairs(chapters) do
            local uid=tostring(chapter.chapterUid or chapter.chapter_uid or chapter.uid or index)
            assert(not expected[uid],'epub_invalid_chapter_list');expected[uid]=index
        end
    end
    local used={classes={},ids={}}
    local normalized = {}
    for uid, text in pairs(bodies) do
        local ok,value=pcall(function()
            local value=Compatibility.xhtml(text,used)
            require('epub_xml_validate').validate(value)
            return value
        end)
        if not ok then error('epub_invalid_xhtml:'..tostring(expected[tostring(uid)] or 0),0)end
        assert(not chapters or expected[tostring(uid)],'epub_invalid_chapter_list')
        normalized[uid] = value;count=count+1
    end
    if chapters then
        assert(count==#chapters,'epub_invalid_chapter_list')
        for uid in pairs(expected)do assert(normalized[uid]~=nil,'epub_invalid_chapter_list')end
    end
    local unique, seen = {}, {}
    for _, original in ipairs(assets or {}) do
        local asset=asset_with_type(original)
        assert(type(asset.href) == "string" and asset.href ~= "", "EPUB resource has no href")
        local previous = seen[asset.href]
        if previous then
            -- Never discard different images merely because their names match.
            assert(previous.media_type == asset.media_type
                and asset_bytes(previous) == asset_bytes(asset),
                "Conflicting duplicate EPUB resource")
        else
            seen[asset.href] = asset
            unique[#unique + 1] = asset
        end
    end
    return normalized, unique, prune_unused_css(css,used)
end

return Compatibility
