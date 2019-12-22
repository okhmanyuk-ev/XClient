require "utils"

local visible = false

function FindPlayerEntity(Expression) -- Expression(I) -> bool
	for I = 0, Client.GetEntitiesCount() - 1 do
		if (I ~= Client.GetIndex() + 1) then
			if Client.IsEntityActive(I) then
				if Client.IsPlayerIndex(I) then
					if not Expression then
						return Client.GetEntity(I)
					elseif Expression(I) then
						return Client.GetEntity(I)
					end
				end
			end
		end
	end
	return nil
end

function Move()
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