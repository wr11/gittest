function test()
    local o = {object=true}
    print(o)
    local weak = getWeakRef(o)
    print(weak[1].object)
    o = nil
    collectgarbage()
    print(weak[1].object)
end

function getWeakRef(o)
    local tWeakRef = {}
    local tMeta = {__mode = 'v'}
    setmetatable(tWeakRef, tMeta)
    table.insert(tWeakRef, o)
    return tWeakRef
end

test()