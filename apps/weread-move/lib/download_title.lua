local Title = {}

function Title.valid(value, book_id)
    if type(value) ~= "string" then return nil end
    local title = value:gsub("[\r\n]", " "):match("^%s*(.-)%s*$")
    local lower = title:lower()
    if title == "" or title == tostring(book_id) or lower == "untitled"
        or lower == "unknown" or title == "未命名书籍" or title == "未命名" then return nil end
    return title
end

function Title.resolve(book_id, candidates, fetch)
    for _, candidate in ipairs(candidates) do
        local title = Title.valid(candidate, book_id)
        if title then return title end
    end
    local ok, info = pcall(fetch)
    local title = ok and type(info) == "table" and Title.valid(info.title, book_id)
    if not title then error("无法获取真实书名，请联网后重试", 0) end
    return title, info
end

return Title
