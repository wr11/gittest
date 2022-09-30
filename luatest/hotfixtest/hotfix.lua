local hotfix = {}

local function collect_uv(f, uv)
    local i = 1
    while true do
        local name, value = debug.getupvalue(f, i)
        if name == nil then -- 当所有上值收集完时，跳出循环
            break
        end
        if not uv[name] then
            uv[name] = { func = f, index = i } -- 这里就会收集到旧函数 print_index 所有的上值，包括变量 index
            if type(value) == "function" then
                collect_uv(value, uv)
            end
        end

        i = i + 1
    end
end

local function update_func(f, uv)
    local i = 1
    while true do
        local name, value = debug.getupvalue(f, i)
        if name == nil then -- 当所有上值收集完时，跳出循环
            break
        end
        -- value 值为空，并且这个 name 在 旧的函数中存在
        if not value and uv[name] then
            local desc = uv[name]
            -- 将新函数 f 的第 i 个上值引用旧模块 func 的第 index 个上值
            debug.upvaluejoin(f, i, desc.func, desc.index)
        end

         -- 只对 function 类型进行递归更新，对基本数据类型（number、boolean、string） 不管
        if type(value) == "function" then
            update_func(value, uv)
        end

        i = i + 1
    end
end

function hotfix.update(old, new)
    local uv = {}
    collect_uv(old, uv)
    update_func(new, uv)
end

return hotfix