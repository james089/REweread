-- Whole-book CONTENT estimate from chapter word weights and an original-HTML
-- bookmark. The service's raw progress field is deliberately never consulted.
local Source=require('progress_source_map')
local Text=require('progress_text')
local M={}
local function count(s)
 local n=0;for _ in s:gmatch('[%z\1-\127\194-\244][\128-\191]*')do n=n+1 end;return n
end
-- Display-only fallback. Saved offsets can use another source serialization;
-- an exact, sufficiently long, unique summary still identifies visible text.
-- Upload/restore must continue to use Source.savedPosition, never this helper.
function M.summaryPosition(text,summary)
 if type(summary)~='string' then return nil end
 summary=Text.normalize(summary)
 if count(summary)<6 then return nil end
 local at=text:find(summary,1,true)
 if not at or text:find(summary,at+1,true) then return nil end
 return at
end
function M.positionKey(record)
 return table.concat({tostring(record.chapterUid or ''),tostring(record.chapterIdx or ''),
  tostring(record.chapterOffset or ''),tostring(record.summary or '')},'\31')
end
function M.calculate(chapters,record,original)
 if type(chapters)~='table' or type(record)~='table' then return nil end
 local total,before,target,seen=0,0,nil,{}
 for _,c in ipairs(chapters)do
  local uid=tostring(c.chapterUid or '');local weight=tonumber(c.wordCount)
  if uid=='' or seen[uid] or not weight or weight~=weight or weight<0 or weight==math.huge then return nil end
  seen[uid]=true
  if uid==tostring(record.chapterUid) then target={before=total,weight=weight} end
  total=total+weight
 end
 if not target or total<=0 or total==math.huge then return nil end
 if target.weight==0 then return target.before*100/total end
 if record.chapterOffset==0 and (record.summary==nil or record.summary=='') then return target.before*100/total end
 local index=Source.index(original);if not index then return nil end
 local at=Source.savedPosition(index,record.summary,record.chapterOffset)
     or M.summaryPosition(index.text,record.summary)
 if not at then return nil end
 local length=count(index.text);if length==0 then return nil end
 local fraction=count(index.text:sub(1,at-1))/length
 return math.max(0,math.min(100,(target.before+target.weight*fraction)*100/total))
end
return M
