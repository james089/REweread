local KoreaderPaths = {}

function KoreaderPaths.append(koreader_dir)
    koreader_dir = koreader_dir or os.getenv("KO_DIR") or "/home/root/xovi/exthome/appload/koreader"
    package.path =
        koreader_dir .. "/plugins/weread.koplugin/weread/?.lua;" ..
        koreader_dir .. "/plugins/weread.koplugin/?.lua;" ..
        koreader_dir .. "/?.lua;" ..
        koreader_dir .. "/common/?.lua;" ..
        koreader_dir .. "/frontend/?.lua;" ..
        koreader_dir .. "/plugins/exporter.koplugin/?.lua;" ..
        package.path
    package.cpath =
        koreader_dir .. "/?.so;" ..
        koreader_dir .. "/common/?.so;" ..
        koreader_dir .. "/common/?.dll;" ..
        "/usr/lib/lua/?.so;" ..
        package.cpath
    -- New KOReader builds resolve their native libraries relative to KO_DIR.
    local ok_lfs, lfs = pcall(require, "libs/libkoreader-lfs")
    if ok_lfs then
        assert(lfs.chdir(koreader_dir))
        require("ffi/loadlib")
    end
    if not package.loaded.socketutil then
        package.preload.socketutil = function() return require("headless_socketutil") end
    end
    -- The current plugin renamed lib.weread to weread.lib.protocol.
    if not package.preload["lib.weread"] then
        package.preload["lib.weread"] = function()
            local ok, protocol = pcall(require, "weread.lib.protocol")
            if ok then return protocol end
            return dofile(koreader_dir .. "/plugins/weread.koplugin/lib/weread.lua")
        end
    end
    return koreader_dir
end

return KoreaderPaths
