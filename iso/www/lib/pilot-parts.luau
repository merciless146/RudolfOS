--!nocheck
-- Stand-in parts for running Pilot.lua programs (Waste of Space microcontrollers) in Blitzen, Rudolf OS 1.2.5.
-- GPL-2.0-or-later. There is no game here: each part prints what the program asks it to do, events never fire
-- unless the program fires them, Disk keeps values for the run, and task.wait() advances a pretend clock
-- instead of waiting. Enough to check that a program runs and calls its parts the way it means to.
local clock = 0
local function repr(v, depth)
	depth = depth or 0
	local t = typeof(v)
	if t == "string" then return string.format("%q", v) end
	if t ~= "table" then return tostring(v) end
	local mt = getmetatable(v)
	if mt and mt.__tostring then return tostring(v) end
	if depth > 1 then return "{…}" end
	local out, n = {}, 0
	for k, x in v do
		n += 1
		if n > 8 then table.insert(out, "…") break end
		table.insert(out, (typeof(k) == "string" and k .. "=" or "") .. repr(x, depth + 1))
	end
	return "{" .. table.concat(out, ", ") .. "}"
end
local function args(...)
	local a = table.pack(...)
	local s = {}
	for i = 1, a.n do s[i] = repr(a[i]) end
	return table.concat(s, ", ")
end

-- values: small stand-ins for Roblox's Vector3, Vector2, Color3, CFrame, UDim2 and Enum
local function valueType(name, fields, new)
	local mt = {}
	mt.__index = mt
	mt.__tostring = function(v)
		local s = {}
		for i, f in fields do s[i] = string.format("%g", v[f]) end
		return name .. "(" .. table.concat(s, ", ") .. ")"
	end
	mt.__eq = function(a, b) for _, f in fields do if a[f] ~= b[f] then return false end end return true end
	local lib = {}
	lib.new = function(...)
		local a = { ... }
		local v = setmetatable({}, mt)
		for i, f in fields do v[f] = a[i] or 0 end
		if new then new(v) end
		return v
	end
	return lib, mt
end
local Vector3, V3 = valueType("Vector3", { "X", "Y", "Z" }, function(v) v.Magnitude = math.sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z) end)
V3.__add = function(a, b) return Vector3.new(a.X + b.X, a.Y + b.Y, a.Z + b.Z) end
V3.__sub = function(a, b) return Vector3.new(a.X - b.X, a.Y - b.Y, a.Z - b.Z) end
V3.__mul = function(a, b)
	if typeof(a) == "number" then a, b = b, a end
	if typeof(b) == "number" then return Vector3.new(a.X * b, a.Y * b, a.Z * b) end
	return Vector3.new(a.X * b.X, a.Y * b.Y, a.Z * b.Z)
end
V3.__div = function(a, b)
	if typeof(b) == "number" then return Vector3.new(a.X / b, a.Y / b, a.Z / b) end
	return Vector3.new(a.X / b.X, a.Y / b.Y, a.Z / b.Z)
end
V3.__unm = function(a) return Vector3.new(-a.X, -a.Y, -a.Z) end
V3.Dot = function(a, b) return a.X * b.X + a.Y * b.Y + a.Z * b.Z end
V3.Cross = function(a, b) return Vector3.new(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X) end
V3.Lerp = function(a, b, t) return a + (b - a) * t end
V3.__index = function(v, k)
	if k == "Unit" then local m = v.Magnitude return m > 0 and v / m or Vector3.new() end
	return V3[k]
end
Vector3.zero, Vector3.one = Vector3.new(0, 0, 0), Vector3.new(1, 1, 1)
Vector3.xAxis, Vector3.yAxis, Vector3.zAxis = Vector3.new(1, 0, 0), Vector3.new(0, 1, 0), Vector3.new(0, 0, 1)

local Vector2, V2 = valueType("Vector2", { "X", "Y" }, function(v) v.Magnitude = math.sqrt(v.X * v.X + v.Y * v.Y) end)
V2.__add = function(a, b) return Vector2.new(a.X + b.X, a.Y + b.Y) end
V2.__sub = function(a, b) return Vector2.new(a.X - b.X, a.Y - b.Y) end
V2.__mul = function(a, b)
	if typeof(a) == "number" then a, b = b, a end
	if typeof(b) == "number" then return Vector2.new(a.X * b, a.Y * b) end
	return Vector2.new(a.X * b.X, a.Y * b.Y)
end
Vector2.zero, Vector2.one = Vector2.new(0, 0), Vector2.new(1, 1)

local Color3 = valueType("Color3", { "R", "G", "B" })
Color3.fromRGB = function(r, g, b) return Color3.new((r or 0) / 255, (g or 0) / 255, (b or 0) / 255) end
Color3.fromHex = function(h)
	h = h:gsub("#", "")
	return Color3.fromRGB(tonumber(h:sub(1, 2), 16), tonumber(h:sub(3, 4), 16), tonumber(h:sub(5, 6), 16))
end
local UDim2 = valueType("UDim2", { "XScale", "XOffset", "YScale", "YOffset" })
UDim2.fromScale = function(x, y) return UDim2.new(x, 0, y, 0) end
UDim2.fromOffset = function(x, y) return UDim2.new(0, x, 0, y) end
local UDim = valueType("UDim", { "Scale", "Offset" })

local CFrame, CF = valueType("CFrame", { "X", "Y", "Z" }, function(v) v.Position = Vector3.new(v.X, v.Y, v.Z) end)
local cfnew = CFrame.new
CFrame.new = function(x, y, z)
	if typeof(x) == "table" then return cfnew(x.X, x.Y, x.Z) end
	return cfnew(x, y, z)
end
CF.__add = function(a, b) return CFrame.new(a.X + b.X, a.Y + b.Y, a.Z + b.Z) end
CF.__sub = function(a, b) return CFrame.new(a.X - b.X, a.Y - b.Y, a.Z - b.Z) end
CF.__mul = function(a, b)
	if getmetatable(b) == V3 then return a.Position + b end
	return CFrame.new(a.X + b.X, a.Y + b.Y, a.Z + b.Z)
end
CFrame.lookAt = function(at) return CFrame.new(at) end
CFrame.Angles = function() return CFrame.new() end
CFrame.fromOrientation = CFrame.Angles
CFrame.identity = CFrame.new()

local Enum = setmetatable({}, { __index = function(_, kind)
	return setmetatable({}, { __index = function(_, item)
		return setmetatable({ Name = item, EnumType = kind, Value = 0 }, { __tostring = function() return "Enum." .. kind .. "." .. item end })
	end })
end })

-- events: :Connect(f) keeps f; the program (or a stand-in) can fire it with :Fire(...)
local function event(owner, name)
	local handlers = {}
	local e = {}
	function e:Connect(f)
		table.insert(handlers, f)
		local c = { Connected = true }
		function c:Disconnect()
			c.Connected = false
			table.remove(handlers, table.find(handlers, f) or 0)
		end
		return c
	end
	e.Once = e.Connect
	function e:Fire(...)
		for _, f in table.clone(handlers) do f(...) end
	end
	function e:Wait() return nil end
	return e
end

-- parts: every method prints "[Class 1] Method(arguments)"; a few return something useful
local counts = {}
local parts = {}
local Returns = {
	Disk = function(p)
		local store = {}
		return {
			Write = function(_, k, v) store[k] = v end,
			Read = function(_, k) return store[k] end,
			ReadAll = function() return table.clone(store) end,
			ClearDisk = function() table.clear(store) end,
		}
	end,
	Screen = function(p)
		return {
			CreateElement = function(_, class, props)
				local el = { ClassName = class }
				for k, v in props or {} do el[k] = v end
				el.Destroy = function() end
				el.AddChild = function() end
				return el
			end,
			GetCursor = function() return nil end,
			GetCursors = function() return {} end,
		}
	end,
	Microphone = function(p) return { Chatted = event(p, "Chatted") } end,
	Keyboard = function(p) return { TextInputted = event(p, "TextInputted"), KeyPressed = event(p, "KeyPressed") } end,
	Button = function(p) return { OnClick = event(p, "OnClick") } end,
	Port = function(p) return { Triggered = event(p, "Triggered") } end,
	Modem = function(p)
		return { MessageSent = event(p, "MessageSent"), GetAsync = function() return "" end, RealPostRequest = function() return "" end }
	end,
	Gyro = function(p) return { PointAt = function() end } end,
	Sign = function(p) return { SetSignText = function(_, t) print("[" .. p.__name .. "] shows: " .. tostring(t)) end } end,
}
local function makePart(class)
	counts[class] = (counts[class] or 0) + 1
	local p = { ClassName = class, GUID = class .. "-" .. counts[class], Position = Vector3.new(), Temperature = 20, Durability = 100 }
	p.__name = class .. " " .. counts[class]
	local special = Returns[class] and Returns[class](p) or {}
	for k, v in special do
		if typeof(v) == "function" then
			p[k] = function(self, ...)
				print("[" .. p.__name .. "] " .. k .. "(" .. args(...) .. ")")
				return v(self, ...)
			end
		else
			p[k] = v
		end
	end
	p.Configured = p.Configured or event(p, "Configured")
	p.Triggered = p.Triggered or event(p, "Triggered")
	return setmetatable(p, { __index = function(_, k)
		if typeof(k) == "string" and k:match("^%u") then
			return function(self, ...)
				print("[" .. p.__name .. "] " .. k .. "(" .. args(...) .. ")")
				return nil
			end
		end
		return nil
	end, __tostring = function() return p.__name end })
end
local function part(class)
	class = class or "Part"
	parts[class] = parts[class] or { makePart(class) }
	return parts[class][1]
end

GetPart = function(class) return part(class) end
GetParts = function(class) return { part(class) } end
GetPartFromPort = function(_, class) return part(class) end
GetPartsFromPort = function(_, class) return { part(class) } end
GetPort = function(id) return part("Port") end
GetPorts = function() return { part("Port") } end
TriggerPort = function(port) print("[Port] triggered") end
Beep = function(f) print("[Beep] " .. tostring(f or 1)) end
logError = function(m) print("[Error] " .. tostring(m)) end
GetCPUTime = function() return os.clock() end
JSONEncode = function(v) return repr(v) end
JSONDecode = function(s) return {} end
Microcontroller = makePart("Microcontroller")
SandboxID, SandboxRunID = "rudolf", "1"
pilot = {
	setTimeout = function() end, getTimeout = function() return 0 end, hasRing = function() return true end,
	setInterrupt = function() return function() end end, getCPUTime = function() return os.clock() end,
	getRing = function() return 0 end, setRing = function() end, claimThread = function() return true end,
	getThreadParent = function() return nil end, saveRing = function() return function() end end,
}
FileSystem = { Read = function() return nil end, Write = function() end }
Network = { GetSubnet = function() return nil end }

task = {
	wait = function(s) clock += (s or 0.03) return s or 0.03 end,
	spawn = function(f, ...) if typeof(f) == "function" then f(...) end end,
	defer = function(f, ...) if typeof(f) == "function" then f(...) end end,
	delay = function(s, f, ...) clock += s if typeof(f) == "function" then f(...) end end,
	cancel = function() end,
}
wait = function(s) clock += (s or 0.03) return s or 0.03, clock end
tick = function() return clock end
time = function() return clock end
warn = function(...) print("[warning] " .. args(...)) end
_G.Vector3, _G.Vector2, _G.Color3, _G.UDim2, _G.UDim, _G.CFrame, _G.Enum = Vector3, Vector2, Color3, UDim2, UDim, CFrame, Enum
