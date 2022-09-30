-- local o={x="hi"}
-- setmetatable(o,{__gc = function (p) print(p.x) end})
-- o = nil

do
    local mt = {__gc = function(o)
        print("new cycle")
        setmetatable({},getmetatable(o))
    end
    }
    setmetatable({},mt)
end

collectgarbage()
collectgarbage()
collectgarbage()