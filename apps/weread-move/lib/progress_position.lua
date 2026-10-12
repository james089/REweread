-- A detailed remote position is useful only when every field comes from the
-- same saved-progress record. In particular, never pair currentChapter with
-- another record's summary: those two browser-state objects can be stale at
-- different moments.
local ProgressPosition = {}

function ProgressPosition.from_progress_book(progress_book)
    if type(progress_book) ~= "table" then
        return {}
    end
    local chapter_uid = tostring(progress_book.chapterUid or ""):match("^%s*(.-)%s*$")
    local chapter_idx = tonumber(progress_book.chapterIdx)
    local chapter_offset = tonumber(progress_book.chapterOffset)
    local summary = tostring(progress_book.summary or ""):match("^%s*(.-)%s*$")
    if chapter_uid == "" or chapter_idx == nil or chapter_idx < 0
        or chapter_offset == nil or chapter_offset < 0 or summary == "" then
        return {}
    end
    return {
        chapterUid = chapter_uid,
        chapterIdx = chapter_idx,
        chapterOffset = chapter_offset,
        summary = summary,
    }
end

return ProgressPosition
