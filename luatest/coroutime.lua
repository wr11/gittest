function func(a,b)
    print("coroutine_start",a,b)
    local f,g = coroutine.yield(3,4)
    print("coroutine_resume0",f,g)
end

co = coroutine.create(func)
local c,d,e = coroutine.resume(co,1,2)
print("coroutine_resume",c,d,e)
coroutine.resume(co,5,6)


local function iterator()
    for i=1,10 do
        coroutine.yield(i)
    end
end

co = coroutine.create(iterator)
for i=1, math.huge do
    local bSuc,ret = coroutine.resume(co)
    print(bSuc,ret)
    if coroutine.status(co) == "dead" then
        break
    end
end