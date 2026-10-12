-- Small self-contained SHA-256 implementation for cache namespace isolation.
--
-- KOReader supplies LuaJIT on the device, but this deliberately avoids an
-- optional native hash module: losing a package must not quietly downgrade an
-- account boundary to a collision-prone checksum.  Account identifiers are
-- short, so the arithmetic implementation is more than fast enough here.

local Sha256 = {}

local MOD = 4294967296
local floor = math.floor

local function normalize(value)
    return value % MOD
end

local function add(...)
    local sum = 0
    for index = 1, select("#", ...) do
        sum = sum + (select(index, ...) or 0)
    end
    return normalize(sum)
end

local function band(left, right)
    local out = 0
    local place = 1
    left = normalize(left)
    right = normalize(right)
    for _ = 1, 32 do
        local left_bit = left % 2
        local right_bit = right % 2
        if left_bit == 1 and right_bit == 1 then
            out = out + place
        end
        left = floor(left / 2)
        right = floor(right / 2)
        place = place * 2
    end
    return out
end

local function bxor(left, right, ...)
    local out = 0
    local place = 1
    local values = { normalize(left), normalize(right) }
    for index = 1, select("#", ...) do
        values[#values + 1] = normalize(select(index, ...))
    end
    for _ = 1, 32 do
        local parity = 0
        for index = 1, #values do
            parity = parity + (values[index] % 2)
            values[index] = floor(values[index] / 2)
        end
        if parity % 2 == 1 then
            out = out + place
        end
        place = place * 2
    end
    return out
end

local function rrotate(value, count)
    value = normalize(value)
    count = count % 32
    if count == 0 then
        return value
    end
    local divisor = 2 ^ count
    return floor(value / divisor) + (value % divisor) * (2 ^ (32 - count))
end

local K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
}

function Sha256.hex(text)
    text = tostring(text or "")
    local bytes = { text:byte(1, #text) }
    local bit_length = #bytes * 8
    bytes[#bytes + 1] = 0x80
    while #bytes % 64 ~= 56 do
        bytes[#bytes + 1] = 0
    end
    local high = floor(bit_length / MOD)
    local low = bit_length % MOD
    for shift = 24, 0, -8 do
        bytes[#bytes + 1] = floor(high / (2 ^ shift)) % 256
    end
    for shift = 24, 0, -8 do
        bytes[#bytes + 1] = floor(low / (2 ^ shift)) % 256
    end

    local h0, h1, h2, h3 = 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a
    local h4, h5, h6, h7 = 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    for offset = 1, #bytes, 64 do
        local w = {}
        for index = 0, 15 do
            local at = offset + index * 4
            w[index + 1] = bytes[at] * 0x1000000 + bytes[at + 1] * 0x10000
                + bytes[at + 2] * 0x100 + bytes[at + 3]
        end
        for index = 17, 64 do
            local x = w[index - 2]
            local y = w[index - 15]
            local small1 = bxor(rrotate(x, 17), rrotate(x, 19), floor(x / 1024))
            local small0 = bxor(rrotate(y, 7), rrotate(y, 18), floor(y / 8))
            w[index] = add(small1, w[index - 7], small0, w[index - 16])
        end

        local a, b, c, d = h0, h1, h2, h3
        local e, f, g, h = h4, h5, h6, h7
        for index = 1, 64 do
            local big1 = bxor(rrotate(e, 6), rrotate(e, 11), rrotate(e, 25))
            local choose = bxor(band(e, f), band(MOD - 1 - e, g))
            local temp1 = add(h, big1, choose, K[index], w[index])
            local big0 = bxor(rrotate(a, 2), rrotate(a, 13), rrotate(a, 22))
            local majority = bxor(band(a, b), band(a, c), band(b, c))
            local temp2 = add(big0, majority)
            h, g, f, e = g, f, e, add(d, temp1)
            d, c, b, a = c, b, a, add(temp1, temp2)
        end
        h0, h1, h2, h3 = add(h0, a), add(h1, b), add(h2, c), add(h3, d)
        h4, h5, h6, h7 = add(h4, e), add(h5, f), add(h6, g), add(h7, h)
    end

    return string.format("%08x%08x%08x%08x%08x%08x%08x%08x", h0, h1, h2, h3, h4, h5, h6, h7)
end

return Sha256
