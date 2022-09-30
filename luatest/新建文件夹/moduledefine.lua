function moduleDef(sModName, tMod)
	if not _G[sModName] then
		if type(tMod) == "table" then
			_G[sModName] = tMod
		elseif type(tMod) == "function" then
			_G[sModName] = tMod()
		else
			assert(type(tMod) == "nil")
			tMod = {}
			_G[sModName] = tMod
		end

		_G[sModName].__sModName__ = sModName

		setmetatable(_G[sModName], {__index = _G})
	end

	return _G[sModName]
end