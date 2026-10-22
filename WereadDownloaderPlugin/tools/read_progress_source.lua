-- Research-only read endpoints using ONLY the downloader's independent session.
-- No renewal, credential persistence, annotations, or read-progress reporting.
local M={}
function M.open()
    local root='/home/root/.local/share/weread-downloader'
    local settings=require('config_bridge'):new({config_path='/dev/null',session_path=root..'/session.json'})
    settings.flush=function()end
    assert(settings:is_cookie_configured(),'login-required')
    local vid=settings:get('cookies',{}).wr_vid
    assert(type(vid)=='string' and vid:match('^%d+$'),'identity-unavailable')
    local self={scope=require('sha256').hex('weread-downloader-account-v1\31'..vid),
        cacheScope='sha256-'..require('sha256').hex('rm-weread-cache-v1\31'..vid),requests=0}
    local client=require('lib.client'):new(settings)
    require('persistent_http').attach(client)
    local request=client.request
    client.request=function(current,opts)
        local page=opts.method=='GET' and opts.url:match('^https://weread%.qq%.com/web/reader/[%w]+$')
        local shard=opts.method=='POST' and opts.url:match('^https://weread%.qq%.com/web/book/chapter/e_[013]$')
        local catalog=opts.method=='POST' and opts.url=='https://weread.qq.com/web/book/chapterInfos'
        assert(page or shard or catalog,'unapproved-read-endpoint')
        self.requests=self.requests+1
        return request(current,opts)
    end
    function self:saved(id)
        local url=require('lib.weread').reader_url(id)
        local html,code=client:request({url=url,method='GET',timeout=20,headers={Referer=url}})
        assert(code and code>=200 and code<300,'reader-unavailable')
        return require('saved_progress_probe').extract(html,require('json_util').decode)
    end
    function self:catalog(id)
        return require('lib.content').fetch_catalog(client,{bookId=id})
    end
    function self:source(id,chapter)
        local content=require('lib.content')
        content.cache_annotation_source=function()end
        return content.fetch_chapter_xhtml(client,settings,{bookId=id,_content_format='epub'},chapter)
    end
    function self:close()
        if client.persistentTransport then client.persistentTransport:close()end
    end
    return self
end
return M
