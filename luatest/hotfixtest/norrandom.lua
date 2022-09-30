local c_NormalFactor = math.sqrt( math.pi * 2)

local function formula(x, u, o)
	return math.exp(-((x - u) ^ 2) / (2 * o * o)) / (c_NormalFactor * o)
end

local function initSumDist(u, o)
	local t = {}
	local vSum = 0
	local n = u + 4 * o
	for x = 1, n do
		local v = formula(x, u, o)
		vSum = vSum + v
		table.insert(t, vSum)
	end
	return t
end

local function getX(nVal, tNorSumDist)
	local nBegin = 1
	local nEnd = #tNorSumDist
	local x
	while true do
		x = nBegin + (nEnd - nBegin) // 2
		local sum = tNorSumDist[x]
		if x <= nBegin then
			return nVal <= sum and x or nEnd
		else
			if nVal < sum then
				nEnd = x
			else
				nBegin = x
			end
		end
	end
end

function newRandom(u, o, seed)
	local nor = {}
	assert(type(seed) == "number")
	nor.handler = ltc.newrandom(seed) -- 临时
	nor.tNorSumDist = initSumDist(u, o)
	return nor
end

function getRandomSeed(nor)
	return ltc.randomseed(nor.handler)
end

function random(nor)
	local nVal = ltc.randomvc(nor.handler)
	return getX(nVal, nor.tNorSumDist)
end

a = initSumDist(50,10)
print(#a)
print(table.concat(a, ","))