require "utils"

local nav = nil

function InitNav()
	local map = Client.GetMap():match(".+/(.*)$")
	map = map:match("(.+)%..+")
	nav = NAV.New("navigations/" .. map .. ".nav")
	Log("nav loaded")
end

function FindFirstPlayerEntity(Expression) -- Expression(I) -> bool
	for I = 0, Client.GetEntitiesCount() - 1 do
		if I ~= Client.GetIndex() + 1 then
			if Client.IsEntityActive(I) then
				if Client.IsPlayerIndex(I) then
					if not Expression or Expression(I) then
						return Client.GetEntity(I)
					end
				end
			end
		end
	end
	return nil
end

function FindNearestEntity(Expression)
	local result = nil
	local min_distance = 9999.0
	for I = 0, Client.GetEntitiesCount() - 1 do	
		if not Expression or Expression(I) then
			local entity = Client.GetEntity(I)
			local origin = Vec3.New(entity.origin)
			local distance = GetDistance(origin)
			if distance < min_distance then
				min_distance = distance
				result = entity
			end
		end
	end
	return result
end

function FindNearestVisiblePlayerEntity(Expression) -- Expression(I) -> bool
	return FindNearestEntity(function(I)
		if I == Client.GetIndex() + 1 then
			return false
		end
		if not Client.IsEntityActive(I) then
			return false
		end
		if not Client.IsPlayerIndex(I) then
			return false
		end
		if Expression and not Expression(I) then
			return false
		end
		local entity = Client.GetEntity(I)
		local origin = Vec3.New(entity.origin)
		if not IsVisible(origin) then
			return false
		end
		return true
	end)
end

function TestMove()
	local player = FindNearestVisiblePlayerEntity()

	if player == nil then
		return
	end

	--[[
	if visible and not IsVisible(origin) then
		visible = false
		Log("not visible")
	elseif not visible and IsVisible(origin) then
		visible = true
		Log("visible")
	end]]

	local origin = Vec3.New(player.origin)

	LookAt(origin)

	local distance = GetDistance(origin)

	if distance > 200 then
		MoveTo(origin)
	elseif distance < 150 then
		MoveFrom(origin)
	end
end

function TestNavMove()
	local area = nil
	for i = 0, #nav.Areas - 1 do
		if nav.Areas[i].Index == 15 then
			area = nav.Areas[i]
			break
		end
	end

	if area == nil then
		return
	end

	local dest = (area.Extent.Hi + area.Extent.Lo) / 2.0

	MoveTo(dest)
end

function Move()
	TestMove()
	--TestNavMove()
end