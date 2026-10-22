-- Exercise the real helper with synthetic network responses, no credentials.
package.path = 'apps/weread-move/lib/?.lua;' .. package.path
local helper_path = (arg and arg[1]) or 'apps/weread-move/tools/sync-progress.lua'
local response, sent
package.preload.koreader_paths = function()return {append=function()end}end
package.preload.config_bridge = function()return {new=function()return {is_cookie_configured=function()return true end}end}end
package.preload['lib.client'] = function()return {new=function()return {
 renew_cookie=function()end,
 get_text=function()return '<synthetic-reader/>' end,
 report_read=function(_,payload)sent=payload;return response end,
}end}end
package.preload['lib.content'] = function()return {extract_reader_state=function()return {}end}end
package.preload['lib.weread'] = function()return {
 reader_url=function()return 'https://example.invalid/synthetic' end,
 make_read_payload=function(opts)return opts end,
}end
local original_print, original_exit, original_arg = print, os.exit, arg
for _, case in ipairs({
 {response={succ=1},ok=true}, {response={succ=true},ok=true},
 {response={errCode=0},ok=true}, {response={succ=1,errCode=-1},ok=false},
 {response={succ=0},ok=false}, {response={},ok=false},
}) do
 response=case.response;sent=nil
 local lines,exit_code={},nil
 print=function(line)lines[#lines+1]=line end
 os.exit=function(code)exit_code=code;error('synthetic exit',0)end
 arg={'synthetic-book','24.50','春风又绿江南岸','0'}
 local ok,err=pcall(dofile,helper_path)
 print,os.exit,arg=original_print,original_exit,original_arg
 assert(sent and sent.elapsed_seconds==0,'zero-duration retry must stay zero')
 assert(sent.progress==24.5 and #sent.summary==18,'helper must preserve percent and valid UTF-8')
 if case.ok then
  assert(ok,err);assert(exit_code==nil and #lines==1 and lines[1]:match('^state=done '))
 else
  assert(not ok and exit_code==1 and #lines==1 and lines[1]=='state=error message=read-sync-unacknowledged')
 end
end
print('sync progress helper behavior ok')
