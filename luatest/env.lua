local prints = print
local pairss = pairs
for key, value in pairss(_G) do
    prints(key,value)
end
_ENV = {}
for key, value in pairss(_G) do
    prints(key,value)
end