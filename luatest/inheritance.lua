Bag = {}
function Bag:new(o)
    local obj = o or {}
    self.__index = self
    self.items = {}
    setmetatable(obj, self)
    return obj
end

function Bag:put(item)
    table.insert(self.items, item)
end

function Bag:list()
    return table.concat(self.items, ",")
end

local b = Bag:new()
b:put("apple")
b:put("banana")
print(b:list())



SBag1 = Bag:new()
local sbag1 = SBag1:new({maxsize = 1})
function sbag1:put(item)
    if #self.items >= self.maxsize then
        print("the bag is full,put fail")
    end
    table.insert(self.items, item)
end

sbag1:put("apple")
sbag1:put("banana")
print(sbag1:list())