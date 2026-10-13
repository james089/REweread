local WebResponse = {}

-- HTTP 200 can carry a business error. Do not pass it to catalog parsing
-- (which would turn it into an empty book) or report renewal as successful.
function WebResponse.check(response)
    if type(response) ~= "table" or response.errCode == nil then
        return response
    end
    local code = tonumber(response.errCode)
    if code == 0 then return response end
    if code == -2012 or code == -2013 then
        error("微信读书登录已失效，请在微信读书 App 的账号页使用扫码登录后重试。", 0)
    end
    -- Server messages may include private details; expose only a numeric code.
    error("微信读书请求失败（错误码：" .. tostring(code or "未知") .. "），请稍后重试。", 0)
end

return WebResponse
