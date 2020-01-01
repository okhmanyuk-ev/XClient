require "buffer"
require "console"
require "vec3"

NAV = {}
NAV.__index = NAV

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
			Log(name) -- TODO: remove this log
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

		area.Connections = {}

		for j = 1, 4 do
			local connections = {}
			local count = MSG:ReadUInt32()
			for k = 0, count - 1 do
				connections[k] = MSG:ReadUInt32()
			end
			area.Connections[j] = connections
		end

		local hs_count = MSG:ReadUInt8()

		Log "here is the progress"


		self.Areas[i] = area
		break -- remove this break when code completed
	end

	--assert(false)

	return self
end 