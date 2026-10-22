-- Same manifest checksums/fingerprint inputs as the source-owned download cache.
local M={}
function M.checksum(data)
    local a,b=1,0
    for i=1,#data do a=(a+data:byte(i))%65521;b=(b+a)%65521 end
    return string.format('%04x%04x',b,a)
end
function M.chapter(chapter,index)
    return M.checksum(table.concat({tostring(index or ''),tostring(chapter.chapterUid or chapter.chapter_uid or chapter.uid or ''),
        tostring(chapter.title or ''),tostring(chapter.updateTime or chapter.update_time or chapter.version or chapter.modifiedTime or '')},'\31'))
end
function M.catalog(chapters)
    local parts={tostring(#chapters)}
    for i,c in ipairs(chapters) do parts[#parts+1]=M.chapter(c,i) end
    return M.checksum(table.concat(parts,'\30'))
end
return M
