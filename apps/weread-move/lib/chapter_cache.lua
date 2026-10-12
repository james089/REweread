local Json = require("json_util")

local DEFAULT_ROOT = "/home/root/.local/share/rm-weread/books"
local DOWNLOAD_MANIFEST_VERSION = 1

local ChapterCache = {}
ChapterCache.__index = ChapterCache

local function join_path(left, right)
    if left:sub(-1) == "/" then
        return left .. right
    end
    return left .. "/" .. right
end

local function shell_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function safe_name(value)
    local name = tostring(value or ""):gsub("[^%w%._%-]", "_")
    if name == "" then
        name = "unknown"
    end
    return name
end

local function checksum(data)
    local a = 1
    local b = 0
    for index = 1, #(data or "") do
        a = (a + (data:byte(index) or 0)) % 65521
        b = (b + a) % 65521
    end
    return string.format("%04x%04x", b, a)
end

local function chapter_key(chapter, fallback)
    local key = chapter and (chapter.chapterUid or chapter.chapter_uid or chapter.uid)
    if key == nil or key == "" then
        key = fallback or "chapter"
    end
    return safe_name(key)
end

local function asset_href(href)
    return tostring(href or ""):gsub("[?#].*$", "")
end

local function write_all_atomic(path, text)
    local tmp_path = path .. ".tmp." .. tostring(os.time()) .. "." .. tostring(math.random(1000000))
    local file, err = io.open(tmp_path, "w")
    if not file then
        error("Could not write " .. tmp_path .. ": " .. tostring(err))
    end
    local ok
    ok, err = file:write(text or "")
    if not ok then
        file:close()
        os.remove(tmp_path)
        error("Could not write " .. tmp_path .. ": " .. tostring(err))
    end
    ok, err = file:close()
    if not ok then
        os.remove(tmp_path)
        error("Could not close " .. tmp_path .. ": " .. tostring(err))
    end
    ok, err = os.rename(tmp_path, path)
    if not ok then
        os.remove(tmp_path)
        error("Could not replace " .. path .. ": " .. tostring(err))
    end
end

local function write_binary_atomic(path, data)
    local tmp_path = path .. ".tmp." .. tostring(os.time()) .. "." .. tostring(math.random(1000000))
    local file, err = io.open(tmp_path, "wb")
    if not file then
        error("Could not write " .. tmp_path .. ": " .. tostring(err))
    end
    local ok
    ok, err = file:write(data or "")
    if not ok then
        file:close()
        os.remove(tmp_path)
        error("Could not write " .. tmp_path .. ": " .. tostring(err))
    end
    ok, err = file:close()
    if not ok then
        os.remove(tmp_path)
        error("Could not close " .. tmp_path .. ": " .. tostring(err))
    end
    ok, err = os.rename(tmp_path, path)
    if not ok then
        os.remove(tmp_path)
        error("Could not replace " .. path .. ": " .. tostring(err))
    end
end

local function read_all(path)
    local file = io.open(path, "rb")
    if not file then
        return nil
    end
    local text = file:read("*a")
    file:close()
    return text
end

function ChapterCache:new(opts)
    opts = opts or {}
    return setmetatable({
        root = opts.root or DEFAULT_ROOT,
    }, self)
end

function ChapterCache:book_dir(book_id)
    return join_path(self.root, safe_name(book_id))
end

function ChapterCache:chapters_dir(book_id)
    return join_path(self:book_dir(book_id), "chapters")
end

function ChapterCache:images_dir(book_id)
    return join_path(self:book_dir(book_id), "images")
end

function ChapterCache:ensure(book_id)
    os.execute("mkdir -p " .. shell_quote(self:chapters_dir(book_id)))
    os.execute("mkdir -p " .. shell_quote(self:images_dir(book_id)))
end

function ChapterCache:chapter_path(book_id, chapter_uid)
    return join_path(self:chapters_dir(book_id), safe_name(chapter_uid) .. ".xhtml")
end

function ChapterCache:manifest_path(book_id)
    return join_path(self:book_dir(book_id), "download-manifest.json")
end

function ChapterCache:asset_path(book_id, href)
    local filename = asset_href(href):match("([^/]+)$")
    if not filename or filename == "" then
        return nil
    end
    return join_path(self:images_dir(book_id), safe_name(filename))
end

function ChapterCache:first_cached_chapter_path(book_id)
    local dir = self:chapters_dir(book_id)
    local handle = io.popen("find " .. shell_quote(dir) .. " -type f -name '*.xhtml' 2>/dev/null | sort | sed -n '1p'")
    if not handle then
        return nil
    end
    local path = handle:read("*l")
    handle:close()
    if path and path ~= "" then
        return path
    end
    return nil
end

function ChapterCache:read_chapter(path)
    return read_all(path)
end

function ChapterCache:write_chapter(book_id, chapter, xhtml)
    local chapter_uid = chapter_key(chapter)
    self:ensure(book_id)
    local path = self:chapter_path(book_id, chapter_uid)
    write_all_atomic(path, xhtml or "")
    return path
end

function ChapterCache:write_chapter_assets(book_id, assets)
    self:ensure(book_id)
    local count = 0
    for _, asset in ipairs(assets or {}) do
        local path = self:asset_path(book_id, asset.href)
        if path and asset.data then
            write_binary_atomic(path, asset.data)
            count = count + 1
        end
    end
    return count
end

function ChapterCache:load_download_manifest(book_id)
    local text = read_all(self:manifest_path(book_id))
    if not text or text == "" then
        return nil
    end
    local ok, manifest = pcall(Json.decode, text)
    if not ok or type(manifest) ~= "table" or manifest.version ~= DOWNLOAD_MANIFEST_VERSION
        or type(manifest.chapters) ~= "table" then
        return nil
    end
    return manifest
end

function ChapterCache:save_download_manifest(book_id, manifest)
    self:ensure(book_id)
    manifest = manifest or {}
    manifest.version = DOWNLOAD_MANIFEST_VERSION
    manifest.updatedAt = os.time()
    write_all_atomic(self:manifest_path(book_id), Json.encode(manifest))
end

function ChapterCache:write_download_chapter(book_id, chapter, xhtml, assets, chapter_fingerprint)
    local key = chapter_key(chapter)
    local xhtml_path = self:write_chapter(book_id, chapter, xhtml)
    self:write_chapter_assets(book_id, assets)

    local asset_records = {}
    for _, asset in ipairs(assets or {}) do
        local href = asset_href(asset.href)
        local path = self:asset_path(book_id, href)
        if href ~= "" and path and asset.data then
            local cached = read_all(path)
            if not cached or checksum(cached) ~= checksum(asset.data) then
                error("Could not verify cached chapter asset")
            end
            table.insert(asset_records, {
                href = href,
                bytes = #cached,
                checksum = checksum(cached),
            })
        end
    end

    local cached_xhtml = read_all(xhtml_path)
    if not cached_xhtml or checksum(cached_xhtml) ~= checksum(xhtml or "") then
        error("Could not verify cached chapter")
    end
    return key, {
        chapterFingerprint = chapter_fingerprint,
        bytes = #cached_xhtml,
        checksum = checksum(cached_xhtml),
        assets = asset_records,
    }
end

function ChapterCache:load_verified_download_chapter(book_id, chapter, chapter_fingerprint, record)
    local key = chapter_key(chapter)
    if type(record) ~= "table" or record.chapterFingerprint ~= chapter_fingerprint
        or tonumber(record.bytes or 0) <= 0 or type(record.checksum) ~= "string" then
        return nil
    end

    local xhtml = read_all(self:chapter_path(book_id, key))
    if not xhtml or #xhtml ~= tonumber(record.bytes) or checksum(xhtml) ~= record.checksum then
        return nil
    end

    local assets = {}
    for _, asset in ipairs(record.assets or {}) do
        local href = asset_href(asset.href)
        local path = self:asset_path(book_id, href)
        local cached = path and read_all(path) or nil
        if href == "" or not cached or #cached ~= tonumber(asset.bytes or -1)
            or checksum(cached) ~= asset.checksum then
            return nil
        end
        table.insert(assets, { href = href, data = cached })
    end
    return xhtml, assets
end

function ChapterCache:chapter_key(chapter, fallback)
    return chapter_key(chapter, fallback)
end

function ChapterCache:checksum(data)
    return checksum(data)
end

return ChapterCache
