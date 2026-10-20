-- Roblox-lite: the few Roblox types Pilot.lua's definitions use, for Blitzen's Luau checker (Rudolf OS 1.2.5).
-- Written for Rudolf OS from Roblox's public API names; members not listed here are not checked (the classes
-- allow any other name), so the checker never reports a real Roblox member as missing. GPL-2.0-or-later.
declare extern type Vector3 with
	read X: number
	read Y: number
	read Z: number
	read Magnitude: number
	read Unit: Vector3
	function Dot(self, other: Vector3): number
	function Cross(self, other: Vector3): Vector3
	function Lerp(self, goal: Vector3, alpha: number): Vector3
	function FuzzyEq(self, other: Vector3, epsilon: number?): boolean
	function Abs(self): Vector3
	function Floor(self): Vector3
	function Ceil(self): Vector3
	function Max(self, ...: Vector3): Vector3
	function Min(self, ...: Vector3): Vector3
	function __add(self, other: Vector3): Vector3
	function __sub(self, other: Vector3): Vector3
	function __mul(self, other: Vector3 | number): Vector3
	function __div(self, other: Vector3 | number): Vector3
	function __unm(self): Vector3
end
declare Vector3: {
	new: (x: number?, y: number?, z: number?) -> Vector3,
	zero: Vector3, one: Vector3, xAxis: Vector3, yAxis: Vector3, zAxis: Vector3,
}

declare extern type Vector2 with
	read X: number
	read Y: number
	read Magnitude: number
	read Unit: Vector2
	function Dot(self, other: Vector2): number
	function Cross(self, other: Vector2): number
	function Lerp(self, goal: Vector2, alpha: number): Vector2
	function __add(self, other: Vector2): Vector2
	function __sub(self, other: Vector2): Vector2
	function __mul(self, other: Vector2 | number): Vector2
	function __div(self, other: Vector2 | number): Vector2
	function __unm(self): Vector2
end
declare Vector2: {
	new: (x: number?, y: number?) -> Vector2,
	zero: Vector2, one: Vector2, xAxis: Vector2, yAxis: Vector2,
}

declare extern type CFrame with
	read Position: Vector3
	read X: number
	read Y: number
	read Z: number
	read LookVector: Vector3
	read RightVector: Vector3
	read UpVector: Vector3
	read Rotation: CFrame
	function Inverse(self): CFrame
	function Lerp(self, goal: CFrame, alpha: number): CFrame
	function ToWorldSpace(self, cf: CFrame): CFrame
	function ToObjectSpace(self, cf: CFrame): CFrame
	function PointToWorldSpace(self, v: Vector3): Vector3
	function PointToObjectSpace(self, v: Vector3): Vector3
	function VectorToWorldSpace(self, v: Vector3): Vector3
	function VectorToObjectSpace(self, v: Vector3): Vector3
	function ToEulerAnglesXYZ(self): (number, number, number)
	function ToOrientation(self): (number, number, number)
	function GetComponents(self): (number, number, number, number, number, number, number, number, number, number, number, number)
	function __mul(self, other: CFrame | Vector3): any
	function __add(self, other: Vector3): CFrame
	function __sub(self, other: Vector3): CFrame
end
declare CFrame: {
	new: (...number | Vector3) -> CFrame,
	lookAt: (at: Vector3, target: Vector3, up: Vector3?) -> CFrame,
	Angles: (rx: number, ry: number, rz: number) -> CFrame,
	fromEulerAnglesXYZ: (rx: number, ry: number, rz: number) -> CFrame,
	fromOrientation: (rx: number, ry: number, rz: number) -> CFrame,
	fromAxisAngle: (axis: Vector3, angle: number) -> CFrame,
	identity: CFrame,
}

declare extern type Color3 with
	read R: number
	read G: number
	read B: number
	function Lerp(self, goal: Color3, alpha: number): Color3
	function ToHSV(self): (number, number, number)
	function ToHex(self): string
end
declare Color3: {
	new: (r: number?, g: number?, b: number?) -> Color3,
	fromRGB: (r: number?, g: number?, b: number?) -> Color3,
	fromHSV: (h: number, s: number, v: number) -> Color3,
	fromHex: (hex: string) -> Color3,
}

declare extern type Ray with
	read Origin: Vector3
	read Direction: Vector3
	read Unit: Ray
	function ClosestPoint(self, point: Vector3): Vector3
	function Distance(self, point: Vector3): number
end
declare Ray: { new: (origin: Vector3, direction: Vector3) -> Ray }

declare extern type Random with
	function NextNumber(self, min: number?, max: number?): number
	function NextInteger(self, min: number, max: number): number
	function NextUnitVector(self): Vector3
	function Shuffle(self, t: { any }): ()
	function Clone(self): Random
end
declare Random: { new: (seed: number?) -> Random }

declare extern type RBXScriptConnection with
	read Connected: boolean
	function Disconnect(self): ()
end
declare extern type RBXScriptSignal with
	function Connect(self, callback: (...any) -> ...any): RBXScriptConnection
	function Once(self, callback: (...any) -> ...any): RBXScriptConnection
	function Wait(self): ...any
end

declare extern type Instance with
	Name: string
	read ClassName: string
	Parent: Instance?
	function FindFirstChild(self, name: string, recursive: boolean?): Instance?
	function FindFirstChildOfClass(self, className: string): Instance?
	function WaitForChild(self, name: string, timeout: number?): Instance
	function GetChildren(self): { Instance }
	function GetDescendants(self): { Instance }
	function IsA(self, className: string): boolean
	function Clone(self): Instance
	function Destroy(self): ()
	function GetAttribute(self, name: string): any
	function SetAttribute(self, name: string, value: any): ()
	function GetPropertyChangedSignal(self, property: string): RBXScriptSignal
	read Changed: RBXScriptSignal
	read ChildAdded: RBXScriptSignal
	read ChildRemoved: RBXScriptSignal
	read Destroying: RBXScriptSignal
	[string]: any
end
declare extern type BasePart extends Instance with
	Position: Vector3
	Size: Vector3
	CFrame: CFrame
	Color: Color3
	Anchored: boolean
	CanCollide: boolean
	Transparency: number
	read Touched: RBXScriptSignal
end
declare extern type Part extends BasePart with
end
declare extern type Player extends Instance with
	read UserId: number
	DisplayName: string
end

declare extern type Folder extends Instance with
end
declare extern type Model extends Instance with
	function GetPivot(self): CFrame
	function PivotTo(self, cf: CFrame): ()
end
declare extern type WorldModel extends Model with
end
declare extern type Sound extends Instance with
	SoundId: string
	Volume: number
	PlaybackSpeed: number
	Looped: boolean
	read IsPlaying: boolean
	function Play(self): ()
	function Stop(self): ()
	function Pause(self): ()
	function Resume(self): ()
end
declare extern type GuiObject extends Instance with
	Position: UDim2
	Size: UDim2
	AnchorPoint: Vector2
	BackgroundColor3: Color3
	BackgroundTransparency: number
	Visible: boolean
	ZIndex: number
end
declare extern type Frame extends GuiObject with
end
declare extern type InputObject extends Instance with
	read KeyCode: Enum.KeyCode
	read UserInputType: Enum.UserInputType
	read UserInputState: Enum.UserInputState
	read Position: Vector3
	read Delta: Vector3
end

declare extern type UDim with
	read Scale: number
	read Offset: number
end
declare UDim: { new: (scale: number?, offset: number?) -> UDim }
declare extern type UDim2 with
	read X: UDim
	read Y: UDim
	read Width: UDim
	read Height: UDim
	function Lerp(self, goal: UDim2, alpha: number): UDim2
	function __add(self, other: UDim2): UDim2
	function __sub(self, other: UDim2): UDim2
end
declare UDim2: {
	new: (xScale: number?, xOffset: number?, yScale: number?, yOffset: number?) -> UDim2,
	fromScale: (x: number?, y: number?) -> UDim2,
	fromOffset: (x: number?, y: number?) -> UDim2,
}

declare task: {
	wait: (seconds: number?) -> number,
	spawn: (f: ((...any) -> ...any) | thread, ...any) -> thread,
	defer: (f: ((...any) -> ...any) | thread, ...any) -> thread,
	delay: (seconds: number, f: ((...any) -> ...any) | thread, ...any) -> thread,
	cancel: (t: thread) -> (),
}
declare function wait(seconds: number?): (number, number)
declare function delay(seconds: number, f: (...any) -> ...any): ()
declare function spawn(f: (...any) -> ...any): ()
declare function tick(): number
declare function time(): number
declare function warn(...: any): ()
