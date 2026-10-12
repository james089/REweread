package.path = "apps/weread-move/lib/?.lua;" .. package.path

local Position = require("progress_position")

local function expect(value, message)
    if not value then
        error(message)
    end
end

local position = Position.from_progress_book({
    chapterUid = " 123 ", chapterIdx = 4, chapterOffset = 56, summary = " 一段正文 ",
})
expect(position.chapterUid == "123" and position.chapterIdx == 4
           and position.chapterOffset == 56 and position.summary == "一段正文",
       "complete saved-progress fields must remain one coherent tuple")

expect(next(Position.from_progress_book({ chapterUid = "123", chapterIdx = 4, chapterOffset = 56 })) == nil,
       "a current-chapter-shaped record without its own summary must not become detailed")
expect(next(Position.from_progress_book({ chapterUid = "123", chapterIdx = -1, chapterOffset = 56, summary = "正文" })) == nil,
       "invalid chapter index must not become detailed")

print("progress position behavior ok")
