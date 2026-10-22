-- Research-only matching. Byte positions in normalized XHTML are NOT WeRead
-- chapterOffset values. No network, configuration, or persistent writes.
local M = {}
function M.utf8(cp)
    if cp <= 0 or cp > 0x10ffff or (cp >= 0xd800 and cp <= 0xdfff) then return '' end
    if cp < 128 then return string.char(cp) end
    if cp < 2048 then return string.char(192 + math.floor(cp/64), 128 + cp%64) end
    if cp < 65536 then return string.char(224 + math.floor(cp/4096),128 + math.floor(cp/64)%64,128 + cp%64) end
    return string.char(240 + math.floor(cp/262144),128 + math.floor(cp/4096)%64,128 + math.floor(cp/64)%64,128 + cp%64)
end
function M.normalize(s)
    return (s:gsub('%s+', ''):gsub('\194\160', ''):gsub('\227\128\128', ''))
end
function M.xhtml(s)
    s = s:gsub('<head[%s>].-</head>', ''):gsub('<style[%s>].-</style>', ''):gsub('<script[%s>].-</script>', '')
    s = s:gsub('<[^>]*>', '')
    s = s:gsub('&#x([%da-fA-F]+);', function(n) return M.utf8(tonumber(n,16)) end)
         :gsub('&#(%d+);', function(n) return M.utf8(tonumber(n)) end)
    local entities = {amp='&',lt='<',gt='>',quot='"',apos="'",nbsp=' '}
    s = s:gsub('&(%a+);', function(n) return entities[n] or '&'..n..';' end)
    return M.normalize(s)
end
local function characters(s)
    local a = {}; for c in s:gmatch('[%z\1-\127\194-\244][\128-\191]*') do a[#a+1]=c end; return a
end
-- Two consecutive 32-codepoint anchors, after a short possible page header.
-- Ambiguity within one chapter is also rejected. Caller scans all chapters.
function M.candidates(page, chapter)
    local p = characters(M.normalize(page)); local text = M.xhtml(chapter)
    local result, seen = {}, {}
    for _, skip in ipairs({0, 8, 16, 24, 32}) do
        if #p >= skip + 64 then
            local needle = table.concat(p, '', skip+1, skip+64)
            local from = 1
            while true do
                local start, finish = text:find(needle, from, true)
                if not start then break end
                -- Subtract skipped header length only for deduplicating anchors;
                -- never expose this as a remote coordinate.
                local prefixBytes = #table.concat(p, '', 1, skip)
                local key = start-prefixBytes
                if not seen[key] then result[#result+1]=key; seen[key]=true end
                from = start + 1
                if #result > 8 then return result end
            end
        end
    end
    return result
end
return M
