local function search(k, plist)
    --k 属性名 plist 父类列表
    for i=1,#plist do
        local v = plist[i][k]
        if v then return v end
    end
end

local function createClass(...)
    local c = {}
    local parents = {...}

    setmetatable(c, {__index = function(_,k) return search(k,parents) end })

    c.__index = c

    function c.new(o)
        o = o or {}
        setmetatable(o,c)
        return o
    end

    return c
end


local a = {}
local o = {}
setmetatable(a, {__index = {s=1,a=2}})

a.__index = a
setmetatable(o,a)

print(o.s)