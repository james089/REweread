-- Normalize packaging inputs without changing the verified chapter cache.
local Compatibility = {}

function Compatibility.xhtml(text)
    -- HTML's named nbsp is not predefined in XML. Numeric references preserve
    -- its non-breaking semantics without requiring external XHTML DTDs.
    return (text:gsub("&nbsp;", "&#160;"))
end

local function asset_bytes(asset)
    if asset.path then
        local file = assert(io.open(asset.path, "rb"), "EPUB resource is unavailable")
        local data = file:read("*a")
        file:close()
        return assert(data, "Could not read EPUB resource")
    end
    return assert(asset.data, "EPUB resource has no data")
end

function Compatibility.prepare(bodies, assets)
    -- Current Qt jobs supply in-memory chapter bodies. Refuse unsupported
    -- streamed inputs rather than silently bypassing normalization.
    assert(not bodies.__workspace_text_dir, "Unsupported streamed EPUB input")
    local normalized = {}
    for uid, text in pairs(bodies) do
        normalized[uid] = Compatibility.xhtml(text)
    end
    local unique, seen = {}, {}
    for _, asset in ipairs(assets or {}) do
        assert(type(asset.href) == "string" and asset.href ~= "", "EPUB resource has no href")
        local previous = seen[asset.href]
        if previous then
            -- Never discard different images merely because their names match.
            assert(previous.media_type == asset.media_type
                and asset_bytes(previous) == asset_bytes(asset),
                "Conflicting duplicate EPUB resource")
        else
            seen[asset.href] = asset
            unique[#unique + 1] = asset
        end
    end
    return normalized, unique
end

return Compatibility
