Bag = {}
Bagmt = {
    put = function(t,item)
        table.insert(t.items, item)
    end,
    list = function(t)
        if not next(t.items) then
            print("no item")
        else
            local result = table.concat(t.items, ",")
            print(result)
        end
    end,
}
Bagmt["__index"] = Bagmt
function  Bag.new()
    local t = {
        items = {}
    }
    setmetatable(t, Bagmt)
    return  t
end

local bag = Bag.new()
bag:list()
bag:put("apple")
bag:put("banana")
bag:list()