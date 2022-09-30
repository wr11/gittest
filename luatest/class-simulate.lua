local function search(k, classList)
    for i=1,#classList do
        local v = rawget(classList[i], k)
        if v then return v end
    end
end

local function InList(t, v)
    -- just for list
    for i=1,#t do
        if t[i] == v then return true end
    end
    return false
end

local function getAttr(o,k)
    if #o.parents == 0 then return end
    local getType = o.parentMode
    local record = {o,}
    local function dfs(par, res)
        if #par.parents == 0 then return res end
        for i=1,#par.parents do
            if not InList(res,par.parents[i]) then
                table.insert(res, par.parents[i])
                dfs(par.parents[i], res)
            end
        end
    end
    local function bfs(par, res)
        if #par.parents == 0 then return end
        for i=1,#par.parents do
            if not InList(res, par.parents[i]) then
                table.insert(res, par.parents[i])
            end
        end
        for i=1,#par.parents do
            bfs(par.parents[i], res)
        end
    end
    if getType == "dfs" then
        dfs(o, record)
    else
        bfs(o, record)
    end
    return search(k,record)
end

-- e={parents = {},name="e"}
-- a={parents = {},name="a"}
-- b={parents = {a,e},name="b"}
-- d={parents = {},name="d"}
-- c={parents = {b,d},name="c"}
-- local list = getAttr(c,"","dfs")
-- local name = ""
-- for i=1, #list do
--     name = name..list[i].name
-- end
-- print(name)

BaseClass = {parents = {}, name = "BaseClass", parentMode = "bfs"}
function BaseClass:new(t)
    local o = t or {}
    self.__index = self
    setmetatable(o, self)
    return o
end
function BaseClass:SetParentMode(sMode)
    assert(sMode=="dfs" or sMode=="bfs", "parentmode must be dfs or bfs")
    self.parentMode = sMode
end

function NewClass(sClassName, tAttr, listParent, sParentMode)
    local listTemp = listParent or {}
    local tTempAttr = tAttr or {}
    local sTempMode = sParentMode or "bfs"
    table.insert(listTemp, BaseClass)

    local newClass = BaseClass:new(tTempAttr)

    newClass.name = sClassName
    newClass.parents = listTemp
    newClass.parentMode = sTempMode

    local tClassMt = {__index = getAttr, __gc = function(t) t.parents = nil end}
    setmetatable(newClass, tClassMt)
    return newClass
end

local class1 = NewClass("class1", {m_A=true,})
local class2 = NewClass("class2", {m_ID=2,m_B=true,})
local class3 = NewClass("class3", {m_C=true,}, {class1,class2,})
local class4 = NewClass("class4", {m_D=true,})
local class5 = NewClass("class5", {m_E=true,}, {class3,class4,})

local obj = class5:new({m_T = 1})
print(obj,obj.name,obj.m_ID,obj.m_A,obj.m_B,obj.m_C,obj.m_D,obj.m_E)
print(obj.m_T)