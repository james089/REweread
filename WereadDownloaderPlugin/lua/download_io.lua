-- One foreground download owns these objects. Keep final status writes durable,
-- but avoid spawning mkdir subprocesses and rewriting JSON for every progress tick.
local M = {}
function M.attach(statuses, manager, clock)
    clock = clock or os.time
    if statuses.ensure then
        local ensure = statuses.ensure
        local ensured = false
        statuses.ensure = function(self)
            if not ensured then ensure(self); ensured = true end
        end
    end
    if statuses.update and statuses.get then
        local update, last = statuses.update, nil
        statuses.update = function(self, id, patch)
            local now = clock()
            if patch and patch.downloadState == 'downloading' and last == now then
                local current = self:get(id)
                for key, value in pairs(patch) do current[key] = value end
                return current
            end
            local result = update(self, id, patch)
            last = now
            return result
        end
    end
    local cache = manager.chapter_cache
    if cache and cache.ensure then
        local ensure, ready = cache.ensure, {}
        cache.ensure = function(self, id)
            if not ready[id] then ensure(self, id); ready[id] = true end
        end
    end
end
return M
