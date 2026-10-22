-- Parse only the saved record from the reader state; never mix currentChapter.
local M={}
function M.extract(html,decode)
    if type(html)~='string' or #html>16*1024*1024 then return nil end
    local _,at=html:find('window.__INITIAL_STATE__',1,true)
    if not at then return nil end
    local eq=html:find('=',at+1,true);local begin=eq and html:find('{',eq+1,true)
    if not begin then return nil end
    local depth,quoted,escaped=0,false,false
    for i=begin,#html do
        local c=html:sub(i,i)
        if quoted then
            if escaped then escaped=false elseif c=='\\' then escaped=true elseif c=='"' then quoted=false end
        elseif c=='"' then quoted=true
        elseif c=='{' then depth=depth+1
        elseif c=='}' then
            depth=depth-1
            if depth==0 then
                local ok,state=pcall(decode,html:sub(begin,i))
                if not ok or type(state)~='table' then return nil end
                local reader=type(state.reader)=='table' and state.reader or {}
                local progress=type(reader.progress)=='table' and reader.progress or {}
                return type(progress.book)=='table' and progress.book or nil
            end
        end
    end
end
return M
