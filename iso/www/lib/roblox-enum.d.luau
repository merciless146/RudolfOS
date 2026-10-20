-- Roblox-lite enums for Blitzen's Luau checker (Rudolf OS 1.2.5): loaded as the type namespace "Enum", so
-- Enum.Material and the others work as types. Items are not listed: any item name is accepted. GPL-2.0-or-later.
declare extern type EnumItem with
	read Name: string
	read Value: number
	read EnumType: any
end
export type Material = EnumItem
export type KeyCode = EnumItem
export type UserInputType = EnumItem
export type UserInputState = EnumItem
export type CameraType = EnumItem
export type FieldOfViewMode = EnumItem
export type HttpCompression = EnumItem
export type HttpContentType = EnumItem
export type EasingStyle = EnumItem
export type EasingDirection = EnumItem
export type PartType = EnumItem
export type Font = EnumItem
declare Enum: {
	Material: { [string]: EnumItem },
	KeyCode: { [string]: EnumItem },
	UserInputType: { [string]: EnumItem },
	UserInputState: { [string]: EnumItem },
	CameraType: { [string]: EnumItem },
	FieldOfViewMode: { [string]: EnumItem },
	HttpCompression: { [string]: EnumItem },
	HttpContentType: { [string]: EnumItem },
	EasingStyle: { [string]: EnumItem },
	EasingDirection: { [string]: EnumItem },
	PartType: { [string]: EnumItem },
	Font: { [string]: EnumItem },
}
