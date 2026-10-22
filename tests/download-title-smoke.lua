package.path = ((arg and arg[1]) or 'apps/weread-move/lib') .. '/?.lua;' .. package.path
local T = require('download_title')
local calls = 0
local function fetch() calls = calls + 1; return {title='真实书名'} end
assert(T.resolve('123', {'Untitled', '', '123'}, fetch) == '真实书名')
assert(calls == 1)
assert(T.resolve('123', {'已有书名'}, fetch) == '已有书名')
assert(calls == 1)
assert(not pcall(T.resolve, '123', {'untitled'}, function() return {title='Untitled'} end))
assert(not pcall(T.resolve, '123', {}, function() error('offline') end))
print('download title resolution ok')
