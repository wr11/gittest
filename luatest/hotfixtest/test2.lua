local a={ss = {sInstance = "ss", value = "1"}}
local b=a.ss
for _,tValue in pairs(a) do
    print(tValue)
end
for _,tValue in pairs(b) do
    print(tValue)
end