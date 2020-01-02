require "buffer"
require "console"
require "vec3"

NAV = {}
NAV.__index = NAV

NAV.SpotFlag = {
	NAV_SPOT_IN_COVER = 1 << 0, -- in a corner with good hard cover nearby
	NAV_SPOT_GOOD_SNIPER_SPOT = 1 << 1, -- had at least one decent sniping corridor
	NAV_SPOT_IDEAL_SNIPER_SPOT = 1 << 2, -- can see either very far, or a large area, or both
	NAV_SPOT_EXPOSED = 1 << 3, -- spot in the open, usually on a ledge or cliff (source engine)
}

function NAV.New(FileName)
	local self = setmetatable({}, BSP)
	local file = io.open("assets/" .. FileName, "rb")
	local MSG = Buffer.New(file:read("*all"))
	file:close()

	MSG:ToStart()

	self.Magic = MSG:ReadInt32()
	self.Version = MSG:ReadInt32()
	self.SubVersion = 0
	self.WorldSize = 0
	self.Analyzed = false
	self.Locations = {}

	if self.Version >= 10 then
		self.SubVersion = MSG:ReadUInt32()
	end
	if self.Version >= 4 then
		self.WorldSize = MSG:ReadUInt32()
	end
	if self.Version >= 14 then
		self.Analyzed = MSG:ReadUInt8() > 0
	end
	if self.Version >= 5 then
		local count = MSG:ReadUInt16()
		for i = 0, count - 1 do
			local size = MSG:ReadUInt16()
			local name = MSG:ReadBuf(size)
			self.Locations[i] = name
		end
		if self.Version >= 11 then
			self.HasUnnamedAreas = MSG:ReadUInt8() > 0
		end
	end

	self.Areas = {}

	local areas_count = MSG:ReadUInt32()

	for i = 0, areas_count - 1 do
		local area = {}
		
		area.Index = MSG:ReadUInt32()

		if self.Version <= 8 then
			area.Flags = MSG:ReadUInt8()
		elseif self.Version < 13 then
			area.Flags = MSG:ReadUInt16()
		else
			area.Flags = MSG:ReadUInt32()
		end

		area.Extent = {}
		area.Extent.Hi = MSG:ReadVec3()
		area.Extent.Lo = MSG:ReadVec3()
		area.HeightsX = MSG:ReadFloat()
		area.HeightsY = MSG:ReadFloat()

		-- connections

		area.Connections = {}

		for j = 1, 4 do
			local connections = {}
			local count = MSG:ReadUInt32()
			for k = 0, count - 1 do
				connections[k] = MSG:ReadUInt32()
			end
			area.Connections[j] = connections
		end

		-- hiding spots

		local hs_count = MSG:ReadUInt8()

		for j = 0, hs_count - 1 do
			if self.Version > 1 then
				local index = MSG:ReadUInt32()
				local position = MSG:ReadVec3()
				local flags = MSG:ReadUInt8()
			else
				local position = MSG:ReadVec3()
				local flags = NAV.SpotFlag.NAV_SPOT_IN_COVER
			end
		end

		-- approaches

		if self.Version < 15 then
			local count = MSG:ReadUInt8()

			for j = 0, count - 1 do
				local here = MSG:ReadUInt32()
				local prev = MSG:ReadUInt32()
				local prev_to_here_how = MSG:ReadUInt8()
				local next = MSG:ReadUInt32()
				local here_to_next_how = MSG:ReadUInt8()
			end
		end

		-- encounters

		local enc_count = MSG:ReadUInt32()

		for j = 0, enc_count - 1 do
			if self.Version >= 3 then
				local from = MSG:ReadUInt32()
				local from_dir = MSG:ReadUInt8()
				local dest = MSG:ReadUInt32()
				local dest_dir = MSG:ReadUInt8()

				local spots_count = MSG:ReadUInt8()

				for k = 0, spots_count - 1 do
					local spot = MSG:ReadUInt32()
					local t = MSG:ReadUInt8()
				end
			else
				local from = MSG:ReadUInt32()
				local dest = MSG:ReadUInt32()
				local unk = MSG:ReadBuf(24);
				local unk2 = MSG:ReadBuf(MSG:ReadUInt8() * 16)
			end
		end

		assert(self.Version >= 5) -- TODO: fix

		area.Location = MSG:ReadUInt16() - 1

		assert(self.Version <= 7) -- TODO: fix

		self.Areas[i] = area
	end
	
	return self
end 