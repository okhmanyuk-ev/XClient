require "utils"

local visible = false
local nav = nil

function InitNav()
	local map = Client.GetMap():match(".+/(.*)$")
	map = map:match("(.+)%..+")
	nav = NAV.New("navigations/" .. map .. ".nav")
	Log("nav loaded")
end

function FindPlayerEntity(Expression) -- Expression(I) -> bool
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

function TestMove()
	local player = FindPlayerEntity()

	if player == nil then
		return
	end

	local origin = Vec3.New(player.origin)

	LookAt(origin)

	if visible and not IsVisible(origin) then
		visible = false
		Log("not visible")
	elseif not visible and IsVisible(origin) then
		visible = true
		Log("visible")
	end

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
	--TestMove()
	TestNavMove()
end