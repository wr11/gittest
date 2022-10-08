local function test()
    local list = {"s",2,3,4}
    for i,j,v in pairs(list) do
        print(i,j,v)
    end
end

test()