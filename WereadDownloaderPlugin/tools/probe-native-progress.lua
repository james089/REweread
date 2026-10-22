-- Run in a standalone LuaJIT process on Move. Read-only, no authentication or
-- network calls by default; optional cloud-read mode uses read endpoints only. Only aggregate counters are emitted; never titles/text/IDs.
-- Usage: luajit probe-native-progress.lua /absolute/path/to/probe/modules
local modules = assert(arg[1], 'module directory required')
local output=print
if arg[2]=='cloud-read' then print=function()end end
package.path = modules .. '/?.lua;/home/root/weread-downloader/helper/lib/?.lua;' .. package.path
require('koreader_paths').append('/home/root/xovi/exthome/appload/koreader')
local J = require('json_util')
local M = require('progress_probe_match')
local Verify=require('progress_cache_verify')
local ffi = require('ffi')
ffi.cdef[[
char *realpath(const char*,char*);
void FPDF_InitLibrary(void); void FPDF_DestroyLibrary(void);
void* FPDF_LoadDocument(const char*,const char*); void FPDF_CloseDocument(void*);
int FPDF_GetPageCount(void*); void* FPDF_LoadPage(void*,int); void FPDF_ClosePage(void*);
void* FPDFText_LoadPage(void*); void FPDFText_ClosePage(void*);
int FPDFText_CountChars(void*); unsigned int FPDFText_GetUnicode(void*,int);
]]
local root = '/home/root/.local/share/weread-downloader'
local library = '/home/root/.local/share/remarkable/xochitl'
local function canonical(p)
    local buffer = ffi.new('char[4096]')
    return ffi.C.realpath(p,buffer) ~= nil and ffi.string(buffer) == p
end
local function read(p, maximum)
    if not canonical(p) then return nil end
    local f=io.open(p,'rb');if not f then return nil end
    local size=f:seek('end');if not size or size>(maximum or 4*1024*1024) then f:close();return nil end
    f:seek('set');local s=f:read('*a');f:close();return s
end
local function json(p)
    local s=read(p);if not s then return nil end
    local ok,v=pcall(J.decode,s);return ok and type(v)=='table' and v or nil
end
local P=ffi.load('/usr/lib/libpdfium.so')
local result={version=1,readOnly=true,cloudWrites=0,eligible=0,pdfLoaded=0,
    identityPageMaps=0,unsupportedPageMaps=0,pagesWithText=0,uniqueChapterMatches=0,
    ambiguousMatches=0,noAnchorMatch=0,chaptersScanned=0,remoteOffsetUnitResolved=false}
local registry=assert(json(root..'/imports.json'),'registry unavailable')
P.FPDF_InitLibrary()
local started=os.time();local bytes=0
for id,row in pairs(registry.books or {}) do
    if os.time()-started>25 or bytes>32*1024*1024 then result.budgetReached=true;break end
    local uuid,scope=row.uuid,row.accountScope
    if row.state=='imported' and type(id)=='string' and #id<=128 and id:match('^[%w_-]+$')
        and type(uuid)=='string' and #uuid==36 and uuid:match('^[%x-]+$')
        and type(scope)=='string' and #scope==64 and scope:match('^%x+$') then
        local base=library..'/'..uuid;local meta=json(base..'.metadata');local content=json(base..'.content')
        if meta and content and not meta.deleted and not meta.trashed and meta.parent~='trash' and content.fileType=='epub' then
            result.eligible=result.eligible+1
            local ix=meta.lastOpenedPage;local map=content.redirectionPageMap;local identity=type(map)=='table'
                and #map==content.pageCount and type(content.pages)=='table' and #content.pages==content.pageCount
            if identity then for k,v in ipairs(map) do if v~=k-1 then identity=false end end end
            -- Reject edited/nonidentity layouts in this prototype.
            if not identity then result.unsupportedPageMaps=result.unsupportedPageMaps+1
            elseif canonical(base..'.pdf') then
                result.identityPageMaps=result.identityPageMaps+1
                local d=P.FPDF_LoadDocument(base..'.pdf',nil)
                if d~=nil then
                    result.pdfLoaded=result.pdfLoaded+1
                    local count=P.FPDF_GetPageCount(d)
                    if type(ix)=='number' and ix==math.floor(ix) and ix>=0 and ix<count
                        and content.pageCount==count and content.originalPageCount==count then
                        local p=P.FPDF_LoadPage(d,ix)
                        if p~=nil then
                            local t=P.FPDFText_LoadPage(p);local chars={}
                            if t~=nil then
                                local n=P.FPDFText_CountChars(t)
                                for i=0,math.min(n,8192)-1 do chars[#chars+1]=M.utf8(P.FPDFText_GetUnicode(t,i)) end
                                P.FPDFText_ClosePage(t)
                            end
                            P.FPDF_ClosePage(p)
                            local text=M.normalize(table.concat(chars))
                            if #text>0 then
                                result.pagesWithText=result.pagesWithText+1
                                local book=root..'/accounts/'..scope..'/books/'..id
                                local manifest=json(book..'/download-manifest.json')
                                local matches=0;local scanned=0;local matchedUid
                                for uid,record in pairs(manifest and manifest.chapters or {}) do
                                    if os.time()-started>25 or bytes>32*1024*1024 or scanned>=2000 then result.budgetReached=true;break end
                                    if type(uid)=='string' and #uid<=128 and uid:match('^[%w_-]+$') then
                                        local x=read(book..'/chapters/'..uid..'.xhtml',2*1024*1024)
                                        if x and type(record)=='table' and #x==record.bytes and Verify.checksum(x)==record.checksum then
                                            bytes=bytes+#x;scanned=scanned+1
                                            result.chaptersScanned=result.chaptersScanned+1
                                            local found=#M.candidates(text,x)
                                            matches=matches+found
                                            if found>0 then matchedUid=uid end
                                        end
                                    end
                                end
                                local afterMeta=json(base..'.metadata');local afterContent=json(base..'.content')
                                local stable=afterMeta and afterContent and J.encode(afterMeta)==J.encode(meta) and J.encode(afterContent)==J.encode(content)
                                if not stable then result.changedDuringScan=(result.changedDuringScan or 0)+1
                                elseif matches==1 and not result.budgetReached then
                                    result.uniqueChapterMatches=result.uniqueChapterMatches+1
                                    if arg[2]=='cloud-read' and not result.cloudReadAttempted then
                                        result.cloudReadAttempted=true
                                        local remote
                                        local ok=pcall(function()
                                            remote=require('read_progress_source').open()
                                            assert(remote.scope==scope and manifest.accountScope==remote.cacheScope,'account-mismatch')
                                            local chapters=remote:catalog(id)
                                            result.catalogFingerprintMatches=Verify.catalog(chapters)==manifest.catalogFingerprint
                                            assert(result.catalogFingerprintMatches,'catalog-changed')
                                            local chapter
                                            for i,c in ipairs(chapters) do
                                                if tostring(c.chapterUid)==matchedUid then
                                                    assert(not chapter,'duplicate-chapter')
                                                    assert(manifest.chapters[matchedUid].chapterFingerprint==Verify.chapter(c,i),'chapter-changed')
                                                    chapter=c
                                                end
                                            end
                                            assert(chapter and type(chapter.chapterIdx)=='number','chapter-mapping-unavailable')
                                            result.chapterIdentityVerified=true
                                            local original=remote:source(id,chapter)
                                            local Source=require('progress_source_map')
                                            local position=Source.locate(text,original)
                                            result.nativePageMappedToOriginal=position~=nil
                                            result.nativePageCoordinateAgrees=position and position.coordinateAgrees or false
                                            local saved=remote:saved(id)
                                            result.cloudRecordPresent=type(saved)=='table'
                                            if saved then
                                                result.cloudTimestampPresent=type(saved.updateTime)=='number' and saved.updateTime>0
                                                local savedChapter
                                                for _,c in ipairs(chapters) do if tostring(c.chapterUid)==tostring(saved.chapterUid) then savedChapter=c end end
                                                if savedChapter and type(saved.summary)=='string' and #saved.summary>0 then
                                                    local cloudSource=tostring(savedChapter.chapterUid)==matchedUid and original or remote:source(id,savedChapter)
                                                    local indexed=Source.index(cloudSource)
                                                    local summary=M.normalize(saved.summary)
                                                    local first=indexed and #summary>0 and indexed.text:find(summary,1,true)
                                                    if first and not indexed.text:find(summary,first+1,true) then
                                                        local p=indexed.positions[first]
                                                        result.cloudOffsetVerifiedAtSummary=p~=nil and (p.codepoints==saved.chapterOffset or p.utf16==saved.chapterOffset)
                                                    end
                                                end
                                            end
                                            result.previewMappingReady=result.nativePageCoordinateAgrees and result.cloudOffsetVerifiedAtSummary or false
                                            local lastMeta=json(base..'.metadata');local lastContent=json(base..'.content')
                                            if not lastMeta or not lastContent or J.encode(lastMeta)~=J.encode(meta) or J.encode(lastContent)~=J.encode(content) then
                                                result.previewMappingReady=false;result.changedDuringScan=(result.changedDuringScan or 0)+1
                                            end
                                        end)
                                        if remote then result.readEndpointRequests=remote.requests;pcall(function()remote:close()end) end
                                        result.cloudReadSucceeded=ok
                                    end
                                elseif matches>1 then result.ambiguousMatches=result.ambiguousMatches+1
                                else result.noAnchorMatch=result.noAnchorMatch+1 end
                            end
                        end
                    else result.pageBoundsMismatch=(result.pageBoundsMismatch or 0)+1 end
                    P.FPDF_CloseDocument(d)
                end
            end
        end
    end
end
P.FPDF_DestroyLibrary()
output(J.encode(result))
