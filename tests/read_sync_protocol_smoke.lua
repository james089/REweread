package.path = "apps/weread-move/lib/?.lua;" .. package.path

local Protocol = require("read_sync_protocol")

local function expect(value, message)
    if not value then
        error(message)
    end
end

local chinese = "春风又绿江南岸"
local chinese_prefix = Protocol.utf8_prefix_bytes(chinese, 20)
expect(chinese_prefix == "春风又绿江南", "Chinese prefix must end on a complete UTF-8 character")
expect(#chinese_prefix == 18, "Chinese prefix must obey the byte limit")

local emoji_prefix = Protocol.utf8_prefix_bytes("ab🙂cd🙂ef", 8)
expect(emoji_prefix == "ab🙂cd", "emoji prefix must not split a four-byte character")
expect(#emoji_prefix == 8, "emoji prefix must obey the byte limit")
expect(Protocol.utf8_prefix_bytes("12345678901234567890x", 20) == "12345678901234567890",
       "ASCII prefix must retain exactly twenty bytes")
expect(Protocol.utf8_prefix_bytes("a" .. string.char(0xE0, 0xA0, 0x80), 4)
           == "a" .. string.char(0xE0, 0xA0, 0x80), "E0 lower valid boundary must remain valid")
expect(Protocol.utf8_prefix_bytes("a" .. string.char(0xF0, 0x90, 0x80, 0x80), 5)
           == "a" .. string.char(0xF0, 0x90, 0x80, 0x80), "F0 lower valid boundary must remain valid")
expect(Protocol.utf8_prefix_bytes("a" .. string.char(0xF4, 0x8F, 0xBF, 0xBF), 5)
           == "a" .. string.char(0xF4, 0x8F, 0xBF, 0xBF), "F4 upper valid boundary must remain valid")
expect(Protocol.utf8_prefix_bytes("a" .. string.char(0xE0, 0x80, 0x80), 20) == "a",
       "overlong E0 sequence must not be copied")
expect(Protocol.utf8_prefix_bytes("a" .. string.char(0xED, 0xA0, 0x80), 20) == "a",
       "UTF-16 surrogate sequence must not be copied")
expect(Protocol.utf8_prefix_bytes("a" .. string.char(0xF4, 0x90, 0x80, 0x80), 20) == "a",
       "out-of-range F4 sequence must not be copied")

expect(Protocol.elapsed_seconds(0) == 0, "zero elapsed seconds must stay zero")
expect(Protocol.elapsed_seconds("3.9") == 3, "elapsed seconds must floor positive values")
expect(Protocol.elapsed_seconds(-4) == 0, "elapsed seconds must never become negative")

expect(Protocol.response_succeeded({ errCode = 0 }), "explicit zero errCode is acknowledged")
expect(not Protocol.response_succeeded({ succ = true }), "unverified succ response must not acknowledge")
expect(not Protocol.response_succeeded({ errCode = 0, succ = false }), "explicit succ rejection must override errCode")
expect(not Protocol.response_succeeded({ errCode = 0, success = 0 }), "explicit success rejection must override errCode")
expect(not Protocol.response_succeeded({ errCode = 2 }), "nonzero errCode must not acknowledge")
expect(not Protocol.response_succeeded({}), "unknown HTTP-success response must not acknowledge")

print("read sync protocol behavior ok")
