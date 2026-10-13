local ReaderStateCache = {}

-- One download client belongs to one book and one captured login session.
-- Only its reader HTML is reused; chapter bodies are always fetched normally.
function ReaderStateCache.attach(client, reader_url, clock)
    if type(client.get_text) ~= "function" or not reader_url then return client end
    clock = clock or os.time
    local get_text = client.get_text
    local cached_html, fetched_at
    function client:invalidateReaderState()
        cached_html, fetched_at = nil, nil
    end
    function client:get_text(url, opts)
        -- WeRead's encoded IDs contain hex digits and 'g'; 'k' is the
        -- separator between the book and chapter IDs, not part of a book ID.
        local reader_request = url == reader_url
            or (url:sub(1, #reader_url + 1) == reader_url .. "k" and #url > #reader_url + 1)
        local now = clock()
        if reader_request and cached_html and now >= fetched_at and now - fetched_at < 30 then
            return cached_html
        end
        local html = get_text(self, url, opts)
        if reader_request then
            self:invalidateReaderState()
            if type(html) == "string" and html:match('"psvts"%s*:%s*"[^"]+"') then
                cached_html, fetched_at = html, clock()
            end
        end
        return html
    end
    return client
end

return ReaderStateCache
