local _ENV = moduleDef("HotFixHelper")

sModName = "GbsGuildMod"
tFunc = {"cltHandleApplytoGuild"}
tLocal = {false}
sNewFuncCode = [[
function cltHandleApplytoGuild(nRoleId, nInRoleId, nOperation)
    print("test cltHandleApplytoGuild")
    _addMember(1,1,1,1)
end
]]

function genServerHotFixCode()
    local tMod = _G[sModName]
    if not tMod then
        Logger.error("sModName is not exist", sModName)
        return
    end
    local sHotFixCode = sNewFuncCode
    for i = 1, #tFunc do
        local sFunc = tFunc[i]
        local bLocal = tLocal[i]
        if not bLocal then
            hotfixGlobalFunc(sFunc)
        else
            hotfixLocalFunc(sFunc)
        end
    end

    print(sNewFuncCode)
end

function hotfixGlobalFunc(sFunc)
    local oldFunc = _G[sModName][sFunc]
    if not oldFunc then
        Logger.error("sModName global sFunc is not exist", sModName, sFunc)
        return
    end
    copyGlobalUpValue(oldFunc, sFunc)
    sNewFuncCode = sNewFuncCode .. string.format("\n_G.%s.%s = %s", sModName, sFunc, sFunc)
end

function hotfixLocalFunc(sFunc)
    local oldFunc, sGlobalFuncName, nIndex = getOldLocalFunc(sFunc)
    if not oldFunc then
        Logger.error("sModName local sFunc is not exist", sModName, sFunc)
        return
    end
    copyLocalUpValue(oldFunc, sGlobalFuncName, nIndex, sFunc)

    local tMod = _G[sModName]
    for sName, value in pairs(tMod) do
        if type(value) == "function" then
            local tUpVal = getUpValue(value)
            if tUpVal[sFunc] then
                local nIndex = tUpVal[sFunc][1]
                local oldLocalFunc = tUpVal[sFunc][2]
                replaceGlobalUpValue(sName, nIndex, sFunc)
            end
        end
    end
end

function replaceGlobalUpValue(sGlobalFuncName, nIndex, sFunc)
    sNewFuncCode = sNewFuncCode .. string.format("\ndebug.setupvalue(_G.%s.%s, %d, %s)", sModName, sGlobalFuncName, nIndex, sFunc)
end

function copyGlobalUpValue(oldFunc, sFunc)
    local i = 1
    while true do
        local sName, val = debug.getupvalue(oldFunc, i)
        -- print("sName", sName, sName == nil)
        if sName == nil then
            break;
        end
        if sName == "_ENV" then
            goto continue
        end

        sNewFuncCode = sNewFuncCode .. string.format("\nlocal s,v = debug.getupvalue(_G.%s.%s, %d)", sModName, sFunc, i)
        sNewFuncCode = sNewFuncCode .. string.format("\ndebug.setupvalue(%s, %d, v)", sFunc, i)
        ::continue::
        i = i + 1
    end
end

function copyLocalUpValue(oldFunc, sGlobalFuncName, nIndex, sFunc)
    sNewFuncCode = sNewFuncCode .. string.format("\nlocal s1, v1 = debug.getupvalue(_G.%s.%s, %d)", sModName, sGlobalFuncName, nIndex)
    local i = 1
    while true do
        local sName, val = debug.getupvalue(oldFunc, i)
        -- print("sName", sName, sName == nil)
        if sName == nil then
            break;
        end

        sNewFuncCode = sNewFuncCode .. string.format("\nlocal s2, v2 = debug.getupvalue(v1, %d)", i)
        sNewFuncCode = sNewFuncCode .. string.format("\ndebug.setupvalue(%s, %d, v2)", sFunc, i)
        i = i + 1
    end
end

function getUpValue(func)
    local tUpValue = {}
    local i = 1
    while true do
        local sName, val = debug.getupvalue(func, i)
        -- print("sName", sName, sName == nil)
        if sName == nil then
            break;
        end
        tUpValue[sName] = {i, val}
        i = i + 1
    end
    return tUpValue
end

function getOldLocalFunc(sFunc)
    local tMod = _G[sModName]
    for sName, value in pairs(tMod) do
        if type(value) == "function" then
            local tUpVal = getUpValue(value)
            if tUpVal[sFunc] then
                local nIndex = tUpVal[sFunc][1]
                local oldLocalFunc = tUpVal[sFunc][2]
                return oldLocalFunc, sName, nIndex
            end
        end
    end
    return nil
end