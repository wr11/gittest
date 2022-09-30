local metas = {}
setmetatable(metas, {__mode = "v"})
local function setDefault(t,d)
    local mt = metas[d]
    if mt == nil then
        mt = {__index = function () return d end}
        metas[d] = mt
    end
    setmetatable(t, mt)
end

local a ={}
setDefault(a,11)
print(a.a)
for k,v in pairs(metas) do print(k,v) end