local a={ss={dd=1,gg=2}, ll={dd=3,gg=4}}
for k,v in pairs(a) do
    if k=="ss" then
        goto continue
    end
    for i,j in pairs(v) do
        if j == 3 and i == "dd" then
            goto continue
        end
        print(i,j)
        ::continue::
    end
    ::continue::
end

function test()
    goto continue

    do
        ::continue::
        print(1)
        return
    end
    ::continue::
    do
        print(2)
        return
    end
end

test()