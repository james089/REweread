-- Small, dependency-free protocol guards for the read-progress helper.
-- They deliberately contain no request construction or account state.
local ReadSyncProtocol = {}

local function continuation(byte)
    return byte and byte >= 0x80 and byte <= 0xBF
end

local function utf8_width(value, index)
    local first = value:byte(index)
    if not first then
        return nil
    end
    if first <= 0x7F then
        return 1
    end
    if first >= 0xC2 and first <= 0xDF and continuation(value:byte(index + 1)) then
        return 2
    end
    local second = value:byte(index + 1)
    if first == 0xE0 and second and second >= 0xA0 and second <= 0xBF
        and continuation(value:byte(index + 2)) then
        return 3
    end
    if first >= 0xE1 and first <= 0xEC
        and continuation(second) and continuation(value:byte(index + 2)) then
        return 3
    end
    if first == 0xED and second and second >= 0x80 and second <= 0x9F
        and continuation(value:byte(index + 2)) then
        return 3
    end
    if first >= 0xEE and first <= 0xEF
        and continuation(second) and continuation(value:byte(index + 2)) then
        return 3
    end
    if first == 0xF0 and second and second >= 0x90 and second <= 0xBF
        and continuation(value:byte(index + 2)) and continuation(value:byte(index + 3)) then
        return 4
    end
    if first >= 0xF1 and first <= 0xF3
        and continuation(second) and continuation(value:byte(index + 2))
        and continuation(value:byte(index + 3)) then
        return 4
    end
    if first == 0xF4 and second and second >= 0x80 and second <= 0x8F
        and continuation(value:byte(index + 2)) and continuation(value:byte(index + 3)) then
        return 4
    end
    return nil
end

-- The upstream payload signer limits `sm` by bytes. Truncate before it gets
-- there so an otherwise valid Chinese or emoji summary never becomes invalid
-- UTF-8 during signing/JSON encoding.
function ReadSyncProtocol.utf8_prefix_bytes(value, maximum)
    value = tostring(value or "")
    maximum = math.max(0, math.floor(tonumber(maximum) or 0))
    if maximum == 0 then
        return ""
    end

    local index = 1
    local last_complete = 0
    while index <= #value do
        local width = utf8_width(value, index)
        if not width or index + width - 1 > #value or index + width - 1 > maximum then
            break
        end
        last_complete = index + width - 1
        index = last_complete + 1
    end
    return value:sub(1, last_complete)
end

function ReadSyncProtocol.elapsed_seconds(value)
    return math.max(0, math.floor(tonumber(value) or 0))
end

-- HTTP success alone is not an acknowledgement of a read write. Only the
-- response form already used by this helper is accepted. A new or unknown
-- response deliberately remains unacknowledged, so the local ledger cannot
-- silently discard duration.
function ReadSyncProtocol.response_succeeded(response)
    if type(response) ~= "table" then
        return false
    end
    -- A server may provide a boolean result together with errCode. Explicit
    -- rejection always wins; errCode=0 alone is the legacy acknowledged form.
    if response.succ == false or response.succ == 0
        or response.success == false or response.success == 0 then
        return false
    end
    if response.errCode ~= nil then
        return tonumber(response.errCode) == 0
    end
    return false
end

return ReadSyncProtocol
