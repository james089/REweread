-- Original HTML coordinate mapping. No network or persistence.
-- Returns both original Unicode-codepoint and UTF-16 coordinates, never guesses
-- which unit a server uses when the two differ.
local Text=require('progress_text')
local M={}
local entities={amp='&',lt='<',gt='>',quot='"',apos="'",nbsp=' '}
local function decodeEntity(entity)
    local inner=entity:sub(2,-2);local value=entities[inner]
    if not value then
        local n=inner:match('^#x([%da-fA-F]+)$')
        if n then value=Text.utf8(tonumber(n,16))
        else n=inner:match('^#(%d+)$');if n then value=Text.utf8(tonumber(n)) end end
    end
    return value
end
local function units(s)
    local cp,u16=0,0
    for c in s:gmatch('[%z\1-\127\194-\244][\128-\191]*') do cp=cp+1;u16=u16+(#c==4 and 2 or 1) end
    return cp,u16
end
function M.index(html)
    if type(html)~='string' or #html>2*1024*1024 then return nil,'source-size' end
    local at,cp,u16,blocked=1,0,0,nil
    local pieces,map,total={},{},0
    local function consume(raw,visible)
        if visible and not blocked then
            local c=Text.normalize(visible)
            if #c>0 then map[total+1]={codepoints=cp,utf16=u16};pieces[#pieces+1]=c;total=total+#c end
        end
        local c,u=units(raw);cp=cp+c;u16=u16+u;at=at+#raw
    end
    while at<=#html do
        local c=html:sub(at,at)
        if html:sub(at,at+3)=='<!--' then
            local finish=html:find('-->',at+4,true);if not finish then return nil,'comment' end
            consume(html:sub(at,finish+2))
        elseif c=='<' then
            local i,quote=at+1,nil
            while i<=#html do
                local v=html:sub(i,i)
                if quote then if v==quote then quote=nil end
                elseif v=='"' or v=="'" then quote=v
                elseif v=='>' then break end
                i=i+1
            end
            if i>#html then return nil,'tag' end
            local tag=html:sub(at,i);local lower=tag:lower()
            if lower:match('^<!%[cdata%[') then return nil,'cdata-unsupported' end
            local closing,name=lower:match('^<%s*(/?)%s*([%w:]+)')
            if not blocked and closing=='' and (name=='head' or name=='script' or name=='style') then blocked=name
            elseif blocked and closing=='/' and name==blocked then blocked=nil end
            consume(tag)
        elseif c=='&' then
            local begin,finish=html:find('&[^;%s<>]+;',at)
            local entity=begin==at and html:sub(begin,finish) or nil
            if not entity or #entity>32 then return nil,'entity' end
            local value=decodeEntity(entity)
            if not value or value=='' then return nil,'entity-unsupported' end
            consume(entity,value)
        else
            local begin,finish=html:find('[%z\1-\127\194-\244][\128-\191]*',at)
            local char=begin==at and html:sub(begin,finish) or nil
            if not char then return nil,'utf8' end
            consume(char,char)
        end
    end
    if blocked then return nil,'unclosed-hidden-section' end
    return {text=table.concat(pieces),positions=map}
end
-- The download pipeline stores source spans (original codepoint ranges and
-- their visible raw text) in annotation_store before packaging the EPUB. This
-- reconstructs the same exact source offsets without another chapter request.
function M.indexSpans(spans)
    if type(spans)~='table' then return nil,'spans' end
    local pieces,positions,totalBytes,rawBytes={}, {},0,0
    local previousEnd=0;local supplementary=false
    for _,span in ipairs(spans) do
        if type(span)~='table' or type(span[1])~='number' or type(span[2])~='number'
            or type(span[3])~='string' or span[1]<previousEnd or span[2]<span[1] then return nil,'span-shape' end
        local raw=span[3];local offset=span[1];local at=1;local rawCount=0
        rawBytes=rawBytes+span[2]-span[1]
        if rawBytes>2*1024*1024 then return nil,'source-size' end
        while at<=#raw do
            local token,value
            if raw:sub(at,at)=='&' and not span[4] then
                local begin,finish=raw:find('&[^;%s<>]+;',at)
                token=begin==at and finish and raw:sub(begin,finish) or nil
                if not token or #token>32 then return nil,'entity' end
                value=decodeEntity(token);if not value or value=='' then return nil,'entity-unsupported' end
            else
                local begin,finish=raw:find('[%z\1-\127\194-\244][\128-\191]*',at)
                token=begin==at and raw:sub(begin,finish) or nil
                if not token then return nil,'utf8' end
                value=token
            end
            if #token==4 and token:byte(1)>=240 then supplementary=true end
            local visible=Text.normalize(value)
            if #visible>0 then
                positions[totalBytes+1]={codepoints=offset,utf16=offset}
                pieces[#pieces+1]=visible;totalBytes=totalBytes+#visible
            end
            local units=0
            for _ in token:gmatch('[%z\1-\127\194-\244][\128-\191]*') do units=units+1 end
            offset=offset+units;rawCount=rawCount+units;at=at+#token
        end
        if rawCount~=span[2]-span[1] then return nil,'span-length' end
        previousEnd=span[2]
    end
    return {text=table.concat(pieces),positions=positions,hasSupplementary=supplementary}
end
-- A saved bookmark may point into an HTML tag/whitespace immediately before
-- its summary. Resolve to the next visible character, rather than requiring
-- the raw bookmark to equal the first summary character's source coordinate.
function M.savedPosition(index,summary,offset)
    if type(index)~='table' or type(summary)~='string' or type(offset)~='number'
        or offset~=offset or offset<0 or offset~=math.floor(offset) then return nil end
    summary=Text.normalize(summary)
    if #summary==0 then return nil end
    local cp,u16
    for byte,pos in pairs(index.positions) do
        if pos.codepoints>=offset and (not cp or byte<cp) then cp=byte end
        if pos.utf16>=offset and (not u16 or byte<u16) then u16=byte end
    end
    -- Both interpretations must identify the same visible character. This
    -- handles invisible markup without guessing across an emoji/unit conflict.
    if not cp or cp~=u16 then return nil end
    if index.text:sub(cp,cp+#summary-1)~=summary then return nil end
    return cp
end
function M.locateIndex(page,index)
    if type(index)~='table' or type(index.text)~='string' or type(index.positions)~='table' then return nil,'index' end
    local chars={};for c in Text.normalize(page):gmatch('[%z\1-\127\194-\244][\128-\191]*') do chars[#chars+1]=c end
    for _,skip in ipairs({0,8,16,24,32}) do
        if #chars>=skip+64 then
            local anchor=table.concat(chars,'',skip+1,skip+64)
            local start=index.text:find(anchor,1,true)
            if start then
                if index.text:find(anchor,start+1,true) then return nil,'ambiguous' end
                local position=index.positions[start];if not position then return nil,'boundary' end
                return {codepoints=position.codepoints,utf16=position.utf16,
                    coordinateAgrees=position.codepoints==position.utf16,skipped=skip,anchor=anchor}
            end
        end
    end
    return nil,'no-anchor'
end
function M.locate(page,html)
    local index,err=M.index(html);if not index then return nil,err end
    return M.locateIndex(page,index)
end
-- Compact, text-free map persisted beside the downloaded book. Runs describe
-- visible UTF-8 byte ranges and their first source codepoint offset.
function M.compact(index)
    if type(index)~='table' or type(index.text)~='string' or type(index.positions)~='table' then return nil,'index' end
    local runs={};local startByte,previousByte,sourceStart,previousSource
    for byte=1,#index.text do
        local first=index.text:byte(byte)
        if first<128 or first>=194 then
            local pos=index.positions[byte]
            if not pos or type(pos.codepoints)~='number' then return nil,'position' end
            if startByte and byte==previousByte and pos.codepoints==previousSource+1 then
                previousByte=byte+1;previousSource=pos.codepoints
            else
                if startByte then runs[#runs+1]={b=startByte,e=previousByte,s=sourceStart} end
                startByte=byte;previousByte=byte+1;sourceStart=pos.codepoints;previousSource=pos.codepoints
            end
        end
    end
    if startByte then runs[#runs+1]={b=startByte,e=previousByte,s=sourceStart} end
    return {version=1,textBytes=#index.text,runs=runs}
end
function M.locateMapped(page,text,map)
    if type(text)~='string' or type(map)~='table' or map.version~=1 or type(map.runs)~='table' then return nil,'map' end
    local chars={};for c in Text.normalize(page):gmatch('[%z\1-\127\194-\244][\128-\191]*') do chars[#chars+1]=c end
    for _,skip in ipairs({0,8,16,24,32}) do
        if #chars>=skip+64 then
            local anchor=table.concat(chars,'',skip+1,skip+64)
            local start=text:find(anchor,1,true)
            if start then
                if text:find(anchor,start+1,true) then return nil,'ambiguous' end
                for _,run in ipairs(map.runs) do
                    if start>=run.b and start<run.e then
                        local before=text:sub(run.b,start-1);local count=0
                        for _ in before:gmatch('[%z\1-\127\194-\244][\128-\191]*') do count=count+1 end
                        local offset=run.s+count
                        return {codepoints=offset,utf16=offset,coordinateAgrees=true,skipped=skip,anchor=anchor}
                    end
                end
                return nil,'boundary'
            end
        end
    end
    return nil,'no-anchor'
end
return M
