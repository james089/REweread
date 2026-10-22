-- Research-only original HTML coordinate mapping. No network or persistence.
-- Returns both original Unicode-codepoint and UTF-16 coordinates, never guesses
-- which unit a server uses when the two differ.
local Text=require('progress_probe_match')
local M={}
local entities={amp='&',lt='<',gt='>',quot='"',apos="'",nbsp=' '}
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
            local inner=entity:sub(2,-2);local value=entities[inner]
            if not value then
                local n=inner:match('^#x([%da-fA-F]+)$')
                if n then value=Text.utf8(tonumber(n,16))
                else n=inner:match('^#(%d+)$');if n then value=Text.utf8(tonumber(n)) end end
            end
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
function M.locate(page,html)
    local index,err=M.index(html);if not index then return nil,err end
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
return M
