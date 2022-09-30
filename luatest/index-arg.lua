a={}
b={__index = function (a,b,c) print(a,b,c) end}
setmetatable(a,b)
print(a.key)