local _ENV = moduleDef("lua1", {A={}})
function Test()
    print(A)
    print(_ENV["A"])
end
Test()