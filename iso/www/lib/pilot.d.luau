-- Pilot.lua (Waste of Space microcontrollers) type definitions, from github.com/ArvidSilverlock/Pilot.lua-Luau-LSP
-- (MIT, see pilot-LICENSE.txt), release workspace types/global.d.luau of 2026-08-11. Used by Blitzen's Luau checker.
declare FileSystem: FileSystem
declare RawFileSystem: RawFileSystem
declare Microcontroller: Microcontroller
declare SandboxRunID: string
declare SandboxID: string
declare Network: Network
declare pilot: {
	setTimeout: (timeout: number?, thread: thread?) -> (),
	hasRing: (ring: number, thread: thread?) -> (boolean),
	setInterrupt: (period: number, callback: () -> ()) -> (() -> ()),
	getTimeout: (thread: thread?) -> (number),
	setRing: (ring: number, thread: thread?) -> (),
	claimThread: (thread: thread) -> (boolean),
	getCPUTime: () -> (number),
	getRing: (thread: thread?) -> (number),
	getThreadParent: (thread: thread?) -> (thread?),
	saveRing: (ring: number?) -> (() -> ()),
}
declare function GetPorts(id: number?): ({ Port })
declare function GetPort(id: number?): (Port?)
declare function TriggerPort(port: PortLike): ()
declare function JSONEncode(data: JSON): (string)
declare function JSONDecode(data: string): (JSON)
declare GetPartFromPort: ((port: PortLike?, class: "Wood") -> (Wood?)) & ((port: PortLike?, class: "DelayWire") -> (DelayWire?)) & ((port: PortLike?, class: "Hatch") -> (Hatch?)) & ((port: PortLike?, class: "Screen") -> (Screen?)) & ((port: PortLike?, class: "Extractor") -> (Extractor?)) & ((port: PortLike?, class: "DevGenerator") -> (DevGenerator?)) & ((port: PortLike?, class: "BurstLaser") -> (BurstLaser?)) & ((port: PortLike?, class: "Apparel") -> (Apparel?)) & ((port: PortLike?, class: "Cooler") -> (Cooler?)) & ((port: PortLike?, class: "CloningBay") -> (CloningBay?)) & ((port: PortLike?, class: "Food") -> (Food?)) & ((port: PortLike?, class: "Lead") -> (Lead?)) & ((port: PortLike?, class: "BlastingCap") -> (BlastingCap?)) & ((port: PortLike?, class: "HeatPipe") -> (HeatPipe?)) & ((port: PortLike?, class: "Filter") -> (Filter?)) & ((port: PortLike?, class: "Antenna") -> (Antenna?)) & ((port: PortLike?, class: "Instrument") -> (Instrument?)) & ((port: PortLike?, class: "ARController") -> (ARController?)) & ((port: PortLike?, class: "Teleporter") -> (Teleporter?)) & ((port: PortLike?, class: "Water") -> (Water?)) & ((port: PortLike?, class: "Stick") -> (Stick?)) & ((port: PortLike?, class: "Laser") -> (Laser?)) & ((port: PortLike?, class: "DevHeatStorage") -> (DevHeatStorage?)) & ((port: PortLike?, class: "WaterCooler") -> (WaterCooler?)) & ((port: PortLike?, class: "Cone") -> (Cone?)) & ((port: PortLike?, class: "ReinforcedGlass") -> (ReinforcedGlass?)) & ((port: PortLike?, class: "BurnerGenerator") -> (BurnerGenerator?)) & ((port: PortLike?, class: "TestStarMap") -> (TestStarMap?)) & ((port: PortLike?, class: "DevGravityGenerator") -> (DevGravityGenerator?)) & ((port: PortLike?, class: "Fence") -> (Fence?)) & ((port: PortLike?, class: "Servo") -> (Servo?)) & ((port: PortLike?, class: "DriveBox") -> (DriveBox?)) & ((port: PortLike?, class: "FluidProjector") -> (FluidProjector?)) & ((port: PortLike?, class: "ScubaMask") -> (ScubaMask?)) & ((port: PortLike?, class: "Frog") -> (Frog?)) & ((port: PortLike?, class: "MonsterMashPotion") -> (MonsterMashPotion?)) & ((port: PortLike?, class: "TimeSensor") -> (TimeSensor?)) & ((port: PortLike?, class: "ElectricFence") -> (ElectricFence?)) & ((port: PortLike?, class: "ARGlasses") -> (ARGlasses?)) & ((port: PortLike?, class: "Pump") -> (Pump?)) & ((port: PortLike?, class: "Torch") -> (Torch?)) & ((port: PortLike?, class: "Assembler") -> (Assembler?)) & ((port: PortLike?, class: "Diode") -> (Diode?)) & ((port: PortLike?, class: "Neon") -> (Neon?)) & ((port: PortLike?, class: "Keyboard") -> (Keyboard?)) & ((port: PortLike?, class: "Cement") -> (Cement?)) & ((port: PortLike?, class: "StasisField") -> (StasisField?)) & ((port: PortLike?, class: "EnergyGun") -> (EnergyGun?)) & ((port: PortLike?, class: "LightBridge") -> (LightBridge?)) & ((port: PortLike?, class: "Ball") -> (Ball?)) & ((port: PortLike?, class: "ProximityButton") -> (ProximityButton?)) & ((port: PortLike?, class: "Block") -> (Block?)) & ((port: PortLike?, class: "WirelessButton") -> (WirelessButton?)) & ((port: PortLike?, class: "DarkConverter") -> (DarkConverter?)) & ((port: PortLike?, class: "Disk") -> (Disk?)) & ((port: PortLike?, class: "LightTube") -> (LightTube?)) & ((port: PortLike?, class: "LifeSensor") -> (LifeSensor?)) & ((port: PortLike?, class: "SteamTurbine") -> (SteamTurbine?)) & ((port: PortLike?, class: "PilotObject") -> (PilotObject?)) & ((port: PortLike?, class: "Component") -> (Component?)) & ((port: PortLike?, class: "Brick") -> (Brick?)) & ((port: PortLike?, class: "Warhead") -> (Warhead?)) & ((port: PortLike?, class: "AdBoard") -> (AdBoard?)) & ((port: PortLike?, class: "Transformer") -> (Transformer?)) & ((port: PortLike?, class: "Fireworks") -> (Fireworks?)) & ((port: PortLike?, class: "Cleat") -> (Cleat?)) & ((port: PortLike?, class: "Pistol") -> (Pistol?)) & ((port: PortLike?, class: "FactionHub") -> (FactionHub?)) & ((port: PortLike?, class: "TriggerRelay") -> (TriggerRelay?)) & ((port: PortLike?, class: "PowerCell") -> (PowerCell?)) & ((port: PortLike?, class: "Transistor") -> (Transistor?)) & ((port: PortLike?, class: "Piston") -> (Piston?)) & ((port: PortLike?, class: "Treads") -> (Treads?)) & ((port: PortLike?, class: "Marble") -> (Marble?)) & ((port: PortLike?, class: "CrudeWing") -> (CrudeWing?)) & ((port: PortLike?, class: "Quartz") -> (Quartz?)) & ((port: PortLike?, class: "SteamEngine") -> (SteamEngine?)) & ((port: PortLike?, class: "Microcontroller") -> (Microcontroller?)) & ((port: PortLike?, class: "Plutonium") -> (Plutonium?)) & ((port: PortLike?, class: "HeatPump") -> (HeatPump?)) & ((port: PortLike?, class: "Speaker") -> (Speaker?)) & ((port: PortLike?, class: "Cloth") -> (Cloth?)) & ((port: PortLike?, class: "Magnesium") -> (Magnesium?)) & ((port: PortLike?, class: "SoundMuffler") -> (SoundMuffler?)) & ((port: PortLike?, class: "Hull") -> (Hull?)) & ((port: PortLike?, class: "VehicleSeat") -> (VehicleSeat?)) & ((port: PortLike?, class: "RoundWedge") -> (RoundWedge?)) & ((port: PortLike?, class: "Winch") -> (Winch?)) & ((port: PortLike?, class: "Insulation") -> (Insulation?)) & ((port: PortLike?, class: "RTG") -> (RTG?)) & ((port: PortLike?, class: "Rotor") -> (Rotor?)) & ((port: PortLike?, class: "Controller") -> (Controller?)) & ((port: PortLike?, class: "Obelisk") -> (Obelisk?)) & ((port: PortLike?, class: "Motor") -> (Motor?)) & ((port: PortLike?, class: "Aluminum") -> (Aluminum?)) & ((port: PortLike?, class: "Telescope") -> (Telescope?)) & ((port: PortLike?, class: "BlackBox") -> (BlackBox?)) & ((port: PortLike?, class: "Reactor") -> (Reactor?)) & ((port: PortLike?, class: "Prosthetic") -> (Prosthetic?)) & ((port: PortLike?, class: "RoundWedge2") -> (RoundWedge2?)) & ((port: PortLike?, class: "Decoupler") -> (Decoupler?)) & ((port: PortLike?, class: "Solenoid") -> (Solenoid?)) & ((port: PortLike?, class: "DevSink") -> (DevSink?)) & ((port: PortLike?, class: "HyperDrive") -> (HyperDrive?)) & ((port: PortLike?, class: "Tile") -> (Tile?)) & ((port: PortLike?, class: "TriggerSwitch") -> (TriggerSwitch?)) & ((port: PortLike?, class: "SolarPanel") -> (SolarPanel?)) & ((port: PortLike?, class: "Boombox") -> (Boombox?)) & ((port: PortLike?, class: "RepairKit") -> (RepairKit?)) & ((port: PortLike?, class: "Igniter") -> (Igniter?)) & ((port: PortLike?, class: "FactionSpawn") -> (FactionSpawn?)) & ((port: PortLike?, class: "DeleteSwitch") -> (DeleteSwitch?)) & ((port: PortLike?, class: "Electromagnet") -> (Electromagnet?)) & ((port: PortLike?, class: "SpawnPoint") -> (SpawnPoint?)) & ((port: PortLike?, class: "Rocket") -> (Rocket?)) & ((port: PortLike?, class: "Scrapper") -> (Scrapper?)) & ((port: PortLike?, class: "TemperatureGate") -> (TemperatureGate?)) & ((port: PortLike?, class: "RemoteControl") -> (RemoteControl?)) & ((port: PortLike?, class: "Flamethrower") -> (Flamethrower?)) & ((port: PortLike?, class: "Light") -> (Light?)) & ((port: PortLike?, class: "Balloon") -> (Balloon?)) & ((port: PortLike?, class: "VitalScanner") -> (VitalScanner?)) & ((port: PortLike?, class: "BeamRifle") -> (BeamRifle?)) & ((port: PortLike?, class: "DarkReactor") -> (DarkReactor?)) & ((port: PortLike?, class: "Extinguisher") -> (Extinguisher?)) & ((port: PortLike?, class: "Sail") -> (Sail?)) & ((port: PortLike?, class: "Rail") -> (Rail?)) & ((port: PortLike?, class: "RepairLaser") -> (RepairLaser?)) & ((port: PortLike?, class: "Polysilicon") -> (Polysilicon?)) & ((port: PortLike?, class: "Scanner") -> (Scanner?)) & ((port: PortLike?, class: "CornerTetra") -> (CornerTetra?)) & ((port: PortLike?, class: "Gear") -> (Gear?)) & ((port: PortLike?, class: "Furnace") -> (Furnace?)) & ((port: PortLike?, class: "Cannon") -> (Cannon?)) & ((port: PortLike?, class: "DevSource") -> (DevSource?)) & ((port: PortLike?, class: "CornerRoundWedge2") -> (CornerRoundWedge2?)) & ((port: PortLike?, class: "Tetrahedron") -> (Tetrahedron?)) & ((port: PortLike?, class: "Pipe") -> (Pipe?)) & ((port: PortLike?, class: "EthernetCable") -> (EthernetCable?)) & ((port: PortLike?, class: "AutomaticLaser") -> (AutomaticLaser?)) & ((port: PortLike?, class: "Faucet") -> (Faucet?)) & ((port: PortLike?, class: "NightVisionGoggles") -> (NightVisionGoggles?)) & ((port: PortLike?, class: "Thruster") -> (Thruster?)) & ((port: PortLike?, class: "Billboardium") -> (Billboardium?)) & ((port: PortLike?, class: "CombustionTurbine") -> (CombustionTurbine?)) & ((port: PortLike?, class: "TouchScreen") -> (TouchScreen?)) & ((port: PortLike?, class: "GeigerCounter") -> (GeigerCounter?)) & ((port: PortLike?, class: "HeatCell") -> (HeatCell?)) & ((port: PortLike?, class: "Camera") -> (Camera?)) & ((port: PortLike?, class: "Gasoline") -> (Gasoline?)) & ((port: PortLike?, class: "SubspaceTripmine") -> (SubspaceTripmine?)) & ((port: PortLike?, class: "Modem") -> (Modem?)) & ((port: PortLike?, class: "TemperatureSensor") -> (TemperatureSensor?)) & ((port: PortLike?, class: "StanSword") -> (StanSword?)) & ((port: PortLike?, class: "Engine") -> (Engine?)) & ((port: PortLike?, class: "ImpactPlate") -> (ImpactPlate?)) & ((port: PortLike?, class: "Radar") -> (Radar?)) & ((port: PortLike?, class: "FireWood") -> (FireWood?)) & ((port: PortLike?, class: "AdminTool") -> (AdminTool?)) & ((port: PortLike?, class: "RepairPlate") -> (RepairPlate?)) & ((port: PortLike?, class: "Chute") -> (Chute?)) & ((port: PortLike?, class: "MiningLaser") -> (MiningLaser?)) & ((port: PortLike?, class: "Sulfur") -> (Sulfur?)) & ((port: PortLike?, class: "Stone") -> (Stone?)) & ((port: PortLike?, class: "TintedGlass") -> (TintedGlass?)) & ((port: PortLike?, class: "Steam") -> (Steam?)) & ((port: PortLike?, class: "IonRocket") -> (IonRocket?)) & ((port: PortLike?, class: "Stanlium") -> (Stanlium?)) & ((port: PortLike?, class: "Snow") -> (Snow?)) & ((port: PortLike?, class: "BallastTank") -> (BallastTank?)) & ((port: PortLike?, class: "Petroleum") -> (Petroleum?)) & ((port: PortLike?, class: "Claymore") -> (Claymore?)) & ((port: PortLike?, class: "Sand") -> (Sand?)) & ((port: PortLike?, class: "Ruby") -> (Ruby?)) & ((port: PortLike?, class: "CrossBow") -> (CrossBow?)) & ((port: PortLike?, class: "Router") -> (Router?)) & ((port: PortLike?, class: "Explosive") -> (Explosive?)) & ((port: PortLike?, class: "Generator") -> (Generator?)) & ((port: PortLike?, class: "ObjectDetector") -> (ObjectDetector?)) & ((port: PortLike?, class: "Ice") -> (Ice?)) & ((port: PortLike?, class: "StarMap") -> (StarMap?)) & ((port: PortLike?, class: "Perfectium") -> (Perfectium?)) & ((port: PortLike?, class: "TractorBeam") -> (TractorBeam?)) & ((port: PortLike?, class: "Obamium") -> (Obamium?)) & ((port: PortLike?, class: "Turbofan") -> (Turbofan?)) & ((port: PortLike?, class: "NitrogenOxide") -> (NitrogenOxide?)) & ((port: PortLike?, class: "Battery") -> (Battery?)) & ((port: PortLike?, class: "Neutronium") -> (Neutronium?)) & ((port: PortLike?, class: "Coupler") -> (Coupler?)) & ((port: PortLike?, class: "Iron") -> (Iron?)) & ((port: PortLike?, class: "Jade") -> (Jade?)) & ((port: PortLike?, class: "DevTeleporter") -> (DevTeleporter?)) & ((port: PortLike?, class: "Lava") -> (Lava?)) & ((port: PortLike?, class: "Mandrillium") -> (Mandrillium?)) & ((port: PortLike?, class: "Rubber") -> (Rubber?)) & ((port: PortLike?, class: "Helium") -> (Helium?)) & ((port: PortLike?, class: "Heatshield") -> (Heatshield?)) & ((port: PortLike?, class: "HeatSink") -> (HeatSink?)) & ((port: PortLike?, class: "Grass") -> (Grass?)) & ((port: PortLike?, class: "PlutoniumCore") -> (PlutoniumCore?)) & ((port: PortLike?, class: "Seat") -> (Seat?)) & ((port: PortLike?, class: "ImpulseCannon") -> (ImpulseCannon?)) & ((port: PortLike?, class: "Glass") -> (Glass?)) & ((port: PortLike?, class: "Part100k") -> (Part100k?)) & ((port: PortLike?, class: "Wedge") -> (Wedge?)) & ((port: PortLike?, class: "Valve") -> (Valve?)) & ((port: PortLike?, class: "ExoticMatter") -> (ExoticMatter?)) & ((port: PortLike?, class: "CornerRoundWedge") -> (CornerRoundWedge?)) & ((port: PortLike?, class: "Diamond") -> (Diamond?)) & ((port: PortLike?, class: "Freezer") -> (Freezer?)) & ((port: PortLike?, class: "DarkMatter") -> (DarkMatter?)) & ((port: PortLike?, class: "Melter") -> (Melter?)) & ((port: PortLike?, class: "Coal") -> (Coal?)) & ((port: PortLike?, class: "ChemicalSynthiser") -> (ChemicalSynthiser?)) & ((port: PortLike?, class: "Beryllium") -> (Beryllium?)) & ((port: PortLike?, class: "MustardGas") -> (MustardGas?)) & ((port: PortLike?, class: "AlienCore") -> (AlienCore?)) & ((port: PortLike?, class: "Railgun") -> (Railgun?)) & ((port: PortLike?, class: "Dispenser") -> (Dispenser?)) & ((port: PortLike?, class: "RustedMetal") -> (RustedMetal?)) & ((port: PortLike?, class: "ZapWire") -> (ZapWire?)) & ((port: PortLike?, class: "Wing") -> (Wing?)) & ((port: PortLike?, class: "Wire") -> (Wire?)) & ((port: PortLike?, class: "Flint") -> (Flint?)) & ((port: PortLike?, class: "Anchor") -> (Anchor?)) & ((port: PortLike?, class: "Rifle") -> (Rifle?)) & ((port: PortLike?, class: "HalfSphere") -> (HalfSphere?)) & ((port: PortLike?, class: "Tire") -> (Tire?)) & ((port: PortLike?, class: "Artillery") -> (Artillery?)) & ((port: PortLike?, class: "StudAligner") -> (StudAligner?)) & ((port: PortLike?, class: "Truss") -> (Truss?)) & ((port: PortLike?, class: "TriggerWire") -> (TriggerWire?)) & ((port: PortLike?, class: "HeatValve") -> (HeatValve?)) & ((port: PortLike?, class: "Spheroid") -> (Spheroid?)) & ((port: PortLike?, class: "AirSupply") -> (AirSupply?)) & ((port: PortLike?, class: "Constructor") -> (Constructor?)) & ((port: PortLike?, class: "tinnitus") -> (tinnitus?)) & ((port: PortLike?, class: "Primer") -> (Primer?)) & ((port: PortLike?, class: "SmoothGlass") -> (SmoothGlass?)) & ((port: PortLike?, class: "EnergyBomb") -> (EnergyBomb?)) & ((port: PortLike?, class: "Hydrogen") -> (Hydrogen?)) & ((port: PortLike?, class: "Rice") -> (Rice?)) & ((port: PortLike?, class: "RegionCloaker") -> (RegionCloaker?)) & ((port: PortLike?, class: "Pulverizer") -> (Pulverizer?)) & ((port: PortLike?, class: "SmoothReinforcedGlass") -> (SmoothReinforcedGlass?)) & ((port: PortLike?, class: "EnergyShield") -> (EnergyShield?)) & ((port: PortLike?, class: "PlasmaCannon") -> (PlasmaCannon?)) & ((port: PortLike?, class: "TouchSensor") -> (TouchSensor?)) & ((port: PortLike?, class: "PDWX44") -> (PDWX44?)) & ((port: PortLike?, class: "StorageSensor") -> (StorageSensor?)) & ((port: PortLike?, class: "Beaker") -> (Beaker?)) & ((port: PortLike?, class: "Lantern") -> (Lantern?)) & ((port: PortLike?, class: "Katana") -> (Katana?)) & ((port: PortLike?, class: "Aerogel") -> (Aerogel?)) & ((port: PortLike?, class: "Microphone") -> (Microphone?)) & ((port: PortLike?, class: "Gold") -> (Gold?)) & ((port: PortLike?, class: "Hotdog") -> (Hotdog?)) & ((port: PortLike?, class: "Heater") -> (Heater?)) & ((port: PortLike?, class: "VintagePlasmaPistol") -> (VintagePlasmaPistol?)) & ((port: PortLike?, class: "Gun") -> (Gun?)) & ((port: PortLike?, class: "Blade") -> (Blade?)) & ((port: PortLike?, class: "Sorter") -> (Sorter?)) & ((port: PortLike?, class: "Goo") -> (Goo?)) & ((port: PortLike?, class: "Framewire") -> (Framewire?)) & ((port: PortLike?, class: "Hydroponic") -> (Hydroponic?)) & ((port: PortLike?, class: "FloatDevice") -> (FloatDevice?)) & ((port: PortLike?, class: "GravityGenerator") -> (GravityGenerator?)) & ((port: PortLike?, class: "Flashlight") -> (Flashlight?)) & ((port: PortLike?, class: "Gyro") -> (Gyro?)) & ((port: PortLike?, class: "WindTurbine") -> (WindTurbine?)) & ((port: PortLike?, class: "EnergySword") -> (EnergySword?)) & ((port: PortLike?, class: "Cylinder") -> (Cylinder?)) & ((port: PortLike?, class: "Sign") -> (Sign?)) & ((port: PortLike?, class: "Radiator") -> (Radiator?)) & ((port: PortLike?, class: "ConveyorBelt") -> (ConveyorBelt?)) & ((port: PortLike?, class: "Plastic") -> (Plastic?)) & ((port: PortLike?, class: "Egg") -> (Egg?)) & ((port: PortLike?, class: "Shotgun") -> (Shotgun?)) & ((port: PortLike?, class: "Titanium") -> (Titanium?)) & ((port: PortLike?, class: "Hologram") -> (Hologram?)) & ((port: PortLike?, class: "Button") -> (Button?)) & ((port: PortLike?, class: "FourthOfJuly") -> (FourthOfJuly?)) & ((port: PortLike?, class: "Relay") -> (Relay?)) & ((port: PortLike?, class: "Transporter") -> (Transporter?)) & ((port: PortLike?, class: "Oil") -> (Oil?)) & ((port: PortLike?, class: "DevBattery") -> (DevBattery?)) & ((port: PortLike?, class: "Uranium") -> (Uranium?)) & ((port: PortLike?, class: "Door") -> (Door?)) & ((port: PortLike?, class: "Handle") -> (Handle?)) & ((port: PortLike?, class: "SolarScoop") -> (SolarScoop?)) & ((port: PortLike?, class: "Refinery") -> (Refinery?)) & ((port: PortLike?, class: "Bin") -> (Bin?)) & ((port: PortLike?, class: "Propeller") -> (Propeller?)) & ((port: PortLike?, class: "CornerWedge") -> (CornerWedge?)) & ((port: PortLike?, class: "Copper") -> (Copper?)) & ((port: PortLike?, class: "Kiln") -> (Kiln?)) & ((port: PortLike?, class: "Plasma") -> (Plasma?)) & ((port: PortLike?, class: "SMG") -> (SMG?)) & ((port: PortLike?, class: "Boiler") -> (Boiler?)) & ((port: PortLike?, class: "Asphalt") -> (Asphalt?)) & ((port: PortLike?, class: "HyperspaceRadar") -> (HyperspaceRadar?)) & ((port: PortLike?, class: "NuclearWaste") -> (NuclearWaste?)) & ((port: PortLike?, class: "Tank") -> (Tank?)) & ((port: PortLike?, class: "Switch") -> (Switch?)) & ((port: PortLike?, class: "Port") -> (Port?)) & ((port: PortLike?, class: "Beacon") -> (Beacon?)) & ((port: PortLike?, class: "Silicon") -> (Silicon?)) & ((port: PortLike?, class: "Spotlight") -> (Spotlight?))
declare GetParts: ((class: "Wood") -> ({ Wood })) & ((class: "DelayWire") -> ({ DelayWire })) & ((class: "Hatch") -> ({ Hatch })) & ((class: "Screen") -> ({ Screen })) & ((class: "Extractor") -> ({ Extractor })) & ((class: "DevGenerator") -> ({ DevGenerator })) & ((class: "BurstLaser") -> ({ BurstLaser })) & ((class: "Apparel") -> ({ Apparel })) & ((class: "Cooler") -> ({ Cooler })) & ((class: "CloningBay") -> ({ CloningBay })) & ((class: "Food") -> ({ Food })) & ((class: "Lead") -> ({ Lead })) & ((class: "BlastingCap") -> ({ BlastingCap })) & ((class: "HeatPipe") -> ({ HeatPipe })) & ((class: "Filter") -> ({ Filter })) & ((class: "Antenna") -> ({ Antenna })) & ((class: "Instrument") -> ({ Instrument })) & ((class: "ARController") -> ({ ARController })) & ((class: "Teleporter") -> ({ Teleporter })) & ((class: "Water") -> ({ Water })) & ((class: "Stick") -> ({ Stick })) & ((class: "Laser") -> ({ Laser })) & ((class: "DevHeatStorage") -> ({ DevHeatStorage })) & ((class: "WaterCooler") -> ({ WaterCooler })) & ((class: "Cone") -> ({ Cone })) & ((class: "ReinforcedGlass") -> ({ ReinforcedGlass })) & ((class: "BurnerGenerator") -> ({ BurnerGenerator })) & ((class: "TestStarMap") -> ({ TestStarMap })) & ((class: "DevGravityGenerator") -> ({ DevGravityGenerator })) & ((class: "Fence") -> ({ Fence })) & ((class: "Servo") -> ({ Servo })) & ((class: "DriveBox") -> ({ DriveBox })) & ((class: "FluidProjector") -> ({ FluidProjector })) & ((class: "ScubaMask") -> ({ ScubaMask })) & ((class: "Frog") -> ({ Frog })) & ((class: "MonsterMashPotion") -> ({ MonsterMashPotion })) & ((class: "TimeSensor") -> ({ TimeSensor })) & ((class: "ElectricFence") -> ({ ElectricFence })) & ((class: "ARGlasses") -> ({ ARGlasses })) & ((class: "Pump") -> ({ Pump })) & ((class: "Torch") -> ({ Torch })) & ((class: "Assembler") -> ({ Assembler })) & ((class: "Diode") -> ({ Diode })) & ((class: "Neon") -> ({ Neon })) & ((class: "Keyboard") -> ({ Keyboard })) & ((class: "Cement") -> ({ Cement })) & ((class: "StasisField") -> ({ StasisField })) & ((class: "EnergyGun") -> ({ EnergyGun })) & ((class: "LightBridge") -> ({ LightBridge })) & ((class: "Ball") -> ({ Ball })) & ((class: "ProximityButton") -> ({ ProximityButton })) & ((class: "Block") -> ({ Block })) & ((class: "WirelessButton") -> ({ WirelessButton })) & ((class: "DarkConverter") -> ({ DarkConverter })) & ((class: "Disk") -> ({ Disk })) & ((class: "LightTube") -> ({ LightTube })) & ((class: "LifeSensor") -> ({ LifeSensor })) & ((class: "SteamTurbine") -> ({ SteamTurbine })) & ((class: "PilotObject") -> ({ PilotObject })) & ((class: "Component") -> ({ Component })) & ((class: "Brick") -> ({ Brick })) & ((class: "Warhead") -> ({ Warhead })) & ((class: "AdBoard") -> ({ AdBoard })) & ((class: "Transformer") -> ({ Transformer })) & ((class: "Fireworks") -> ({ Fireworks })) & ((class: "Cleat") -> ({ Cleat })) & ((class: "Pistol") -> ({ Pistol })) & ((class: "FactionHub") -> ({ FactionHub })) & ((class: "TriggerRelay") -> ({ TriggerRelay })) & ((class: "PowerCell") -> ({ PowerCell })) & ((class: "Transistor") -> ({ Transistor })) & ((class: "Piston") -> ({ Piston })) & ((class: "Treads") -> ({ Treads })) & ((class: "Marble") -> ({ Marble })) & ((class: "CrudeWing") -> ({ CrudeWing })) & ((class: "Quartz") -> ({ Quartz })) & ((class: "SteamEngine") -> ({ SteamEngine })) & ((class: "Microcontroller") -> ({ Microcontroller })) & ((class: "Plutonium") -> ({ Plutonium })) & ((class: "HeatPump") -> ({ HeatPump })) & ((class: "Speaker") -> ({ Speaker })) & ((class: "Cloth") -> ({ Cloth })) & ((class: "Magnesium") -> ({ Magnesium })) & ((class: "SoundMuffler") -> ({ SoundMuffler })) & ((class: "Hull") -> ({ Hull })) & ((class: "VehicleSeat") -> ({ VehicleSeat })) & ((class: "RoundWedge") -> ({ RoundWedge })) & ((class: "Winch") -> ({ Winch })) & ((class: "Insulation") -> ({ Insulation })) & ((class: "RTG") -> ({ RTG })) & ((class: "Rotor") -> ({ Rotor })) & ((class: "Controller") -> ({ Controller })) & ((class: "Obelisk") -> ({ Obelisk })) & ((class: "Motor") -> ({ Motor })) & ((class: "Aluminum") -> ({ Aluminum })) & ((class: "Telescope") -> ({ Telescope })) & ((class: "BlackBox") -> ({ BlackBox })) & ((class: "Reactor") -> ({ Reactor })) & ((class: "Prosthetic") -> ({ Prosthetic })) & ((class: "RoundWedge2") -> ({ RoundWedge2 })) & ((class: "Decoupler") -> ({ Decoupler })) & ((class: "Solenoid") -> ({ Solenoid })) & ((class: "DevSink") -> ({ DevSink })) & ((class: "HyperDrive") -> ({ HyperDrive })) & ((class: "Tile") -> ({ Tile })) & ((class: "TriggerSwitch") -> ({ TriggerSwitch })) & ((class: "SolarPanel") -> ({ SolarPanel })) & ((class: "Boombox") -> ({ Boombox })) & ((class: "RepairKit") -> ({ RepairKit })) & ((class: "Igniter") -> ({ Igniter })) & ((class: "FactionSpawn") -> ({ FactionSpawn })) & ((class: "DeleteSwitch") -> ({ DeleteSwitch })) & ((class: "Electromagnet") -> ({ Electromagnet })) & ((class: "SpawnPoint") -> ({ SpawnPoint })) & ((class: "Rocket") -> ({ Rocket })) & ((class: "Scrapper") -> ({ Scrapper })) & ((class: "TemperatureGate") -> ({ TemperatureGate })) & ((class: "RemoteControl") -> ({ RemoteControl })) & ((class: "Flamethrower") -> ({ Flamethrower })) & ((class: "Light") -> ({ Light })) & ((class: "Balloon") -> ({ Balloon })) & ((class: "VitalScanner") -> ({ VitalScanner })) & ((class: "BeamRifle") -> ({ BeamRifle })) & ((class: "DarkReactor") -> ({ DarkReactor })) & ((class: "Extinguisher") -> ({ Extinguisher })) & ((class: "Sail") -> ({ Sail })) & ((class: "Rail") -> ({ Rail })) & ((class: "RepairLaser") -> ({ RepairLaser })) & ((class: "Polysilicon") -> ({ Polysilicon })) & ((class: "Scanner") -> ({ Scanner })) & ((class: "CornerTetra") -> ({ CornerTetra })) & ((class: "Gear") -> ({ Gear })) & ((class: "Furnace") -> ({ Furnace })) & ((class: "Cannon") -> ({ Cannon })) & ((class: "DevSource") -> ({ DevSource })) & ((class: "CornerRoundWedge2") -> ({ CornerRoundWedge2 })) & ((class: "Tetrahedron") -> ({ Tetrahedron })) & ((class: "Pipe") -> ({ Pipe })) & ((class: "EthernetCable") -> ({ EthernetCable })) & ((class: "AutomaticLaser") -> ({ AutomaticLaser })) & ((class: "Faucet") -> ({ Faucet })) & ((class: "NightVisionGoggles") -> ({ NightVisionGoggles })) & ((class: "Thruster") -> ({ Thruster })) & ((class: "Billboardium") -> ({ Billboardium })) & ((class: "CombustionTurbine") -> ({ CombustionTurbine })) & ((class: "TouchScreen") -> ({ TouchScreen })) & ((class: "GeigerCounter") -> ({ GeigerCounter })) & ((class: "HeatCell") -> ({ HeatCell })) & ((class: "Camera") -> ({ Camera })) & ((class: "Gasoline") -> ({ Gasoline })) & ((class: "SubspaceTripmine") -> ({ SubspaceTripmine })) & ((class: "Modem") -> ({ Modem })) & ((class: "TemperatureSensor") -> ({ TemperatureSensor })) & ((class: "StanSword") -> ({ StanSword })) & ((class: "Engine") -> ({ Engine })) & ((class: "ImpactPlate") -> ({ ImpactPlate })) & ((class: "Radar") -> ({ Radar })) & ((class: "FireWood") -> ({ FireWood })) & ((class: "AdminTool") -> ({ AdminTool })) & ((class: "RepairPlate") -> ({ RepairPlate })) & ((class: "Chute") -> ({ Chute })) & ((class: "MiningLaser") -> ({ MiningLaser })) & ((class: "Sulfur") -> ({ Sulfur })) & ((class: "Stone") -> ({ Stone })) & ((class: "TintedGlass") -> ({ TintedGlass })) & ((class: "Steam") -> ({ Steam })) & ((class: "IonRocket") -> ({ IonRocket })) & ((class: "Stanlium") -> ({ Stanlium })) & ((class: "Snow") -> ({ Snow })) & ((class: "BallastTank") -> ({ BallastTank })) & ((class: "Petroleum") -> ({ Petroleum })) & ((class: "Claymore") -> ({ Claymore })) & ((class: "Sand") -> ({ Sand })) & ((class: "Ruby") -> ({ Ruby })) & ((class: "CrossBow") -> ({ CrossBow })) & ((class: "Router") -> ({ Router })) & ((class: "Explosive") -> ({ Explosive })) & ((class: "Generator") -> ({ Generator })) & ((class: "ObjectDetector") -> ({ ObjectDetector })) & ((class: "Ice") -> ({ Ice })) & ((class: "StarMap") -> ({ StarMap })) & ((class: "Perfectium") -> ({ Perfectium })) & ((class: "TractorBeam") -> ({ TractorBeam })) & ((class: "Obamium") -> ({ Obamium })) & ((class: "Turbofan") -> ({ Turbofan })) & ((class: "NitrogenOxide") -> ({ NitrogenOxide })) & ((class: "Battery") -> ({ Battery })) & ((class: "Neutronium") -> ({ Neutronium })) & ((class: "Coupler") -> ({ Coupler })) & ((class: "Iron") -> ({ Iron })) & ((class: "Jade") -> ({ Jade })) & ((class: "DevTeleporter") -> ({ DevTeleporter })) & ((class: "Lava") -> ({ Lava })) & ((class: "Mandrillium") -> ({ Mandrillium })) & ((class: "Rubber") -> ({ Rubber })) & ((class: "Helium") -> ({ Helium })) & ((class: "Heatshield") -> ({ Heatshield })) & ((class: "HeatSink") -> ({ HeatSink })) & ((class: "Grass") -> ({ Grass })) & ((class: "PlutoniumCore") -> ({ PlutoniumCore })) & ((class: "Seat") -> ({ Seat })) & ((class: "ImpulseCannon") -> ({ ImpulseCannon })) & ((class: "Glass") -> ({ Glass })) & ((class: "Part100k") -> ({ Part100k })) & ((class: "Wedge") -> ({ Wedge })) & ((class: "Valve") -> ({ Valve })) & ((class: "ExoticMatter") -> ({ ExoticMatter })) & ((class: "CornerRoundWedge") -> ({ CornerRoundWedge })) & ((class: "Diamond") -> ({ Diamond })) & ((class: "Freezer") -> ({ Freezer })) & ((class: "DarkMatter") -> ({ DarkMatter })) & ((class: "Melter") -> ({ Melter })) & ((class: "Coal") -> ({ Coal })) & ((class: "ChemicalSynthiser") -> ({ ChemicalSynthiser })) & ((class: "Beryllium") -> ({ Beryllium })) & ((class: "MustardGas") -> ({ MustardGas })) & ((class: "AlienCore") -> ({ AlienCore })) & ((class: "Railgun") -> ({ Railgun })) & ((class: "Dispenser") -> ({ Dispenser })) & ((class: "RustedMetal") -> ({ RustedMetal })) & ((class: "ZapWire") -> ({ ZapWire })) & ((class: "Wing") -> ({ Wing })) & ((class: "Wire") -> ({ Wire })) & ((class: "Flint") -> ({ Flint })) & ((class: "Anchor") -> ({ Anchor })) & ((class: "Rifle") -> ({ Rifle })) & ((class: "HalfSphere") -> ({ HalfSphere })) & ((class: "Tire") -> ({ Tire })) & ((class: "Artillery") -> ({ Artillery })) & ((class: "StudAligner") -> ({ StudAligner })) & ((class: "Truss") -> ({ Truss })) & ((class: "TriggerWire") -> ({ TriggerWire })) & ((class: "HeatValve") -> ({ HeatValve })) & ((class: "Spheroid") -> ({ Spheroid })) & ((class: "AirSupply") -> ({ AirSupply })) & ((class: "Constructor") -> ({ Constructor })) & ((class: "tinnitus") -> ({ tinnitus })) & ((class: "Primer") -> ({ Primer })) & ((class: "SmoothGlass") -> ({ SmoothGlass })) & ((class: "EnergyBomb") -> ({ EnergyBomb })) & ((class: "Hydrogen") -> ({ Hydrogen })) & ((class: "Rice") -> ({ Rice })) & ((class: "RegionCloaker") -> ({ RegionCloaker })) & ((class: "Pulverizer") -> ({ Pulverizer })) & ((class: "SmoothReinforcedGlass") -> ({ SmoothReinforcedGlass })) & ((class: "EnergyShield") -> ({ EnergyShield })) & ((class: "PlasmaCannon") -> ({ PlasmaCannon })) & ((class: "TouchSensor") -> ({ TouchSensor })) & ((class: "PDWX44") -> ({ PDWX44 })) & ((class: "StorageSensor") -> ({ StorageSensor })) & ((class: "Beaker") -> ({ Beaker })) & ((class: "Lantern") -> ({ Lantern })) & ((class: "Katana") -> ({ Katana })) & ((class: "Aerogel") -> ({ Aerogel })) & ((class: "Microphone") -> ({ Microphone })) & ((class: "Gold") -> ({ Gold })) & ((class: "Hotdog") -> ({ Hotdog })) & ((class: "Heater") -> ({ Heater })) & ((class: "VintagePlasmaPistol") -> ({ VintagePlasmaPistol })) & ((class: "Gun") -> ({ Gun })) & ((class: "Blade") -> ({ Blade })) & ((class: "Sorter") -> ({ Sorter })) & ((class: "Goo") -> ({ Goo })) & ((class: "Framewire") -> ({ Framewire })) & ((class: "Hydroponic") -> ({ Hydroponic })) & ((class: "FloatDevice") -> ({ FloatDevice })) & ((class: "GravityGenerator") -> ({ GravityGenerator })) & ((class: "Flashlight") -> ({ Flashlight })) & ((class: "Gyro") -> ({ Gyro })) & ((class: "WindTurbine") -> ({ WindTurbine })) & ((class: "EnergySword") -> ({ EnergySword })) & ((class: "Cylinder") -> ({ Cylinder })) & ((class: "Sign") -> ({ Sign })) & ((class: "Radiator") -> ({ Radiator })) & ((class: "ConveyorBelt") -> ({ ConveyorBelt })) & ((class: "Plastic") -> ({ Plastic })) & ((class: "Egg") -> ({ Egg })) & ((class: "Shotgun") -> ({ Shotgun })) & ((class: "Titanium") -> ({ Titanium })) & ((class: "Hologram") -> ({ Hologram })) & ((class: "Button") -> ({ Button })) & ((class: "FourthOfJuly") -> ({ FourthOfJuly })) & ((class: "Relay") -> ({ Relay })) & ((class: "Transporter") -> ({ Transporter })) & ((class: "Oil") -> ({ Oil })) & ((class: "DevBattery") -> ({ DevBattery })) & ((class: "Uranium") -> ({ Uranium })) & ((class: "Door") -> ({ Door })) & ((class: "Handle") -> ({ Handle })) & ((class: "SolarScoop") -> ({ SolarScoop })) & ((class: "Refinery") -> ({ Refinery })) & ((class: "Bin") -> ({ Bin })) & ((class: "Propeller") -> ({ Propeller })) & ((class: "CornerWedge") -> ({ CornerWedge })) & ((class: "Copper") -> ({ Copper })) & ((class: "Kiln") -> ({ Kiln })) & ((class: "Plasma") -> ({ Plasma })) & ((class: "SMG") -> ({ SMG })) & ((class: "Boiler") -> ({ Boiler })) & ((class: "Asphalt") -> ({ Asphalt })) & ((class: "HyperspaceRadar") -> ({ HyperspaceRadar })) & ((class: "NuclearWaste") -> ({ NuclearWaste })) & ((class: "Tank") -> ({ Tank })) & ((class: "Switch") -> ({ Switch })) & ((class: "Port") -> ({ Port })) & ((class: "Beacon") -> ({ Beacon })) & ((class: "Silicon") -> ({ Silicon })) & ((class: "Spotlight") -> ({ Spotlight }))
declare GetPartsFromPort: ((port: PortLike?, class: "Wood") -> ({ Wood })) & ((port: PortLike?, class: "DelayWire") -> ({ DelayWire })) & ((port: PortLike?, class: "Hatch") -> ({ Hatch })) & ((port: PortLike?, class: "Screen") -> ({ Screen })) & ((port: PortLike?, class: "Extractor") -> ({ Extractor })) & ((port: PortLike?, class: "DevGenerator") -> ({ DevGenerator })) & ((port: PortLike?, class: "BurstLaser") -> ({ BurstLaser })) & ((port: PortLike?, class: "Apparel") -> ({ Apparel })) & ((port: PortLike?, class: "Cooler") -> ({ Cooler })) & ((port: PortLike?, class: "CloningBay") -> ({ CloningBay })) & ((port: PortLike?, class: "Food") -> ({ Food })) & ((port: PortLike?, class: "Lead") -> ({ Lead })) & ((port: PortLike?, class: "BlastingCap") -> ({ BlastingCap })) & ((port: PortLike?, class: "HeatPipe") -> ({ HeatPipe })) & ((port: PortLike?, class: "Filter") -> ({ Filter })) & ((port: PortLike?, class: "Antenna") -> ({ Antenna })) & ((port: PortLike?, class: "Instrument") -> ({ Instrument })) & ((port: PortLike?, class: "ARController") -> ({ ARController })) & ((port: PortLike?, class: "Teleporter") -> ({ Teleporter })) & ((port: PortLike?, class: "Water") -> ({ Water })) & ((port: PortLike?, class: "Stick") -> ({ Stick })) & ((port: PortLike?, class: "Laser") -> ({ Laser })) & ((port: PortLike?, class: "DevHeatStorage") -> ({ DevHeatStorage })) & ((port: PortLike?, class: "WaterCooler") -> ({ WaterCooler })) & ((port: PortLike?, class: "Cone") -> ({ Cone })) & ((port: PortLike?, class: "ReinforcedGlass") -> ({ ReinforcedGlass })) & ((port: PortLike?, class: "BurnerGenerator") -> ({ BurnerGenerator })) & ((port: PortLike?, class: "TestStarMap") -> ({ TestStarMap })) & ((port: PortLike?, class: "DevGravityGenerator") -> ({ DevGravityGenerator })) & ((port: PortLike?, class: "Fence") -> ({ Fence })) & ((port: PortLike?, class: "Servo") -> ({ Servo })) & ((port: PortLike?, class: "DriveBox") -> ({ DriveBox })) & ((port: PortLike?, class: "FluidProjector") -> ({ FluidProjector })) & ((port: PortLike?, class: "ScubaMask") -> ({ ScubaMask })) & ((port: PortLike?, class: "Frog") -> ({ Frog })) & ((port: PortLike?, class: "MonsterMashPotion") -> ({ MonsterMashPotion })) & ((port: PortLike?, class: "TimeSensor") -> ({ TimeSensor })) & ((port: PortLike?, class: "ElectricFence") -> ({ ElectricFence })) & ((port: PortLike?, class: "ARGlasses") -> ({ ARGlasses })) & ((port: PortLike?, class: "Pump") -> ({ Pump })) & ((port: PortLike?, class: "Torch") -> ({ Torch })) & ((port: PortLike?, class: "Assembler") -> ({ Assembler })) & ((port: PortLike?, class: "Diode") -> ({ Diode })) & ((port: PortLike?, class: "Neon") -> ({ Neon })) & ((port: PortLike?, class: "Keyboard") -> ({ Keyboard })) & ((port: PortLike?, class: "Cement") -> ({ Cement })) & ((port: PortLike?, class: "StasisField") -> ({ StasisField })) & ((port: PortLike?, class: "EnergyGun") -> ({ EnergyGun })) & ((port: PortLike?, class: "LightBridge") -> ({ LightBridge })) & ((port: PortLike?, class: "Ball") -> ({ Ball })) & ((port: PortLike?, class: "ProximityButton") -> ({ ProximityButton })) & ((port: PortLike?, class: "Block") -> ({ Block })) & ((port: PortLike?, class: "WirelessButton") -> ({ WirelessButton })) & ((port: PortLike?, class: "DarkConverter") -> ({ DarkConverter })) & ((port: PortLike?, class: "Disk") -> ({ Disk })) & ((port: PortLike?, class: "LightTube") -> ({ LightTube })) & ((port: PortLike?, class: "LifeSensor") -> ({ LifeSensor })) & ((port: PortLike?, class: "SteamTurbine") -> ({ SteamTurbine })) & ((port: PortLike?, class: "PilotObject") -> ({ PilotObject })) & ((port: PortLike?, class: "Component") -> ({ Component })) & ((port: PortLike?, class: "Brick") -> ({ Brick })) & ((port: PortLike?, class: "Warhead") -> ({ Warhead })) & ((port: PortLike?, class: "AdBoard") -> ({ AdBoard })) & ((port: PortLike?, class: "Transformer") -> ({ Transformer })) & ((port: PortLike?, class: "Fireworks") -> ({ Fireworks })) & ((port: PortLike?, class: "Cleat") -> ({ Cleat })) & ((port: PortLike?, class: "Pistol") -> ({ Pistol })) & ((port: PortLike?, class: "FactionHub") -> ({ FactionHub })) & ((port: PortLike?, class: "TriggerRelay") -> ({ TriggerRelay })) & ((port: PortLike?, class: "PowerCell") -> ({ PowerCell })) & ((port: PortLike?, class: "Transistor") -> ({ Transistor })) & ((port: PortLike?, class: "Piston") -> ({ Piston })) & ((port: PortLike?, class: "Treads") -> ({ Treads })) & ((port: PortLike?, class: "Marble") -> ({ Marble })) & ((port: PortLike?, class: "CrudeWing") -> ({ CrudeWing })) & ((port: PortLike?, class: "Quartz") -> ({ Quartz })) & ((port: PortLike?, class: "SteamEngine") -> ({ SteamEngine })) & ((port: PortLike?, class: "Microcontroller") -> ({ Microcontroller })) & ((port: PortLike?, class: "Plutonium") -> ({ Plutonium })) & ((port: PortLike?, class: "HeatPump") -> ({ HeatPump })) & ((port: PortLike?, class: "Speaker") -> ({ Speaker })) & ((port: PortLike?, class: "Cloth") -> ({ Cloth })) & ((port: PortLike?, class: "Magnesium") -> ({ Magnesium })) & ((port: PortLike?, class: "SoundMuffler") -> ({ SoundMuffler })) & ((port: PortLike?, class: "Hull") -> ({ Hull })) & ((port: PortLike?, class: "VehicleSeat") -> ({ VehicleSeat })) & ((port: PortLike?, class: "RoundWedge") -> ({ RoundWedge })) & ((port: PortLike?, class: "Winch") -> ({ Winch })) & ((port: PortLike?, class: "Insulation") -> ({ Insulation })) & ((port: PortLike?, class: "RTG") -> ({ RTG })) & ((port: PortLike?, class: "Rotor") -> ({ Rotor })) & ((port: PortLike?, class: "Controller") -> ({ Controller })) & ((port: PortLike?, class: "Obelisk") -> ({ Obelisk })) & ((port: PortLike?, class: "Motor") -> ({ Motor })) & ((port: PortLike?, class: "Aluminum") -> ({ Aluminum })) & ((port: PortLike?, class: "Telescope") -> ({ Telescope })) & ((port: PortLike?, class: "BlackBox") -> ({ BlackBox })) & ((port: PortLike?, class: "Reactor") -> ({ Reactor })) & ((port: PortLike?, class: "Prosthetic") -> ({ Prosthetic })) & ((port: PortLike?, class: "RoundWedge2") -> ({ RoundWedge2 })) & ((port: PortLike?, class: "Decoupler") -> ({ Decoupler })) & ((port: PortLike?, class: "Solenoid") -> ({ Solenoid })) & ((port: PortLike?, class: "DevSink") -> ({ DevSink })) & ((port: PortLike?, class: "HyperDrive") -> ({ HyperDrive })) & ((port: PortLike?, class: "Tile") -> ({ Tile })) & ((port: PortLike?, class: "TriggerSwitch") -> ({ TriggerSwitch })) & ((port: PortLike?, class: "SolarPanel") -> ({ SolarPanel })) & ((port: PortLike?, class: "Boombox") -> ({ Boombox })) & ((port: PortLike?, class: "RepairKit") -> ({ RepairKit })) & ((port: PortLike?, class: "Igniter") -> ({ Igniter })) & ((port: PortLike?, class: "FactionSpawn") -> ({ FactionSpawn })) & ((port: PortLike?, class: "DeleteSwitch") -> ({ DeleteSwitch })) & ((port: PortLike?, class: "Electromagnet") -> ({ Electromagnet })) & ((port: PortLike?, class: "SpawnPoint") -> ({ SpawnPoint })) & ((port: PortLike?, class: "Rocket") -> ({ Rocket })) & ((port: PortLike?, class: "Scrapper") -> ({ Scrapper })) & ((port: PortLike?, class: "TemperatureGate") -> ({ TemperatureGate })) & ((port: PortLike?, class: "RemoteControl") -> ({ RemoteControl })) & ((port: PortLike?, class: "Flamethrower") -> ({ Flamethrower })) & ((port: PortLike?, class: "Light") -> ({ Light })) & ((port: PortLike?, class: "Balloon") -> ({ Balloon })) & ((port: PortLike?, class: "VitalScanner") -> ({ VitalScanner })) & ((port: PortLike?, class: "BeamRifle") -> ({ BeamRifle })) & ((port: PortLike?, class: "DarkReactor") -> ({ DarkReactor })) & ((port: PortLike?, class: "Extinguisher") -> ({ Extinguisher })) & ((port: PortLike?, class: "Sail") -> ({ Sail })) & ((port: PortLike?, class: "Rail") -> ({ Rail })) & ((port: PortLike?, class: "RepairLaser") -> ({ RepairLaser })) & ((port: PortLike?, class: "Polysilicon") -> ({ Polysilicon })) & ((port: PortLike?, class: "Scanner") -> ({ Scanner })) & ((port: PortLike?, class: "CornerTetra") -> ({ CornerTetra })) & ((port: PortLike?, class: "Gear") -> ({ Gear })) & ((port: PortLike?, class: "Furnace") -> ({ Furnace })) & ((port: PortLike?, class: "Cannon") -> ({ Cannon })) & ((port: PortLike?, class: "DevSource") -> ({ DevSource })) & ((port: PortLike?, class: "CornerRoundWedge2") -> ({ CornerRoundWedge2 })) & ((port: PortLike?, class: "Tetrahedron") -> ({ Tetrahedron })) & ((port: PortLike?, class: "Pipe") -> ({ Pipe })) & ((port: PortLike?, class: "EthernetCable") -> ({ EthernetCable })) & ((port: PortLike?, class: "AutomaticLaser") -> ({ AutomaticLaser })) & ((port: PortLike?, class: "Faucet") -> ({ Faucet })) & ((port: PortLike?, class: "NightVisionGoggles") -> ({ NightVisionGoggles })) & ((port: PortLike?, class: "Thruster") -> ({ Thruster })) & ((port: PortLike?, class: "Billboardium") -> ({ Billboardium })) & ((port: PortLike?, class: "CombustionTurbine") -> ({ CombustionTurbine })) & ((port: PortLike?, class: "TouchScreen") -> ({ TouchScreen })) & ((port: PortLike?, class: "GeigerCounter") -> ({ GeigerCounter })) & ((port: PortLike?, class: "HeatCell") -> ({ HeatCell })) & ((port: PortLike?, class: "Camera") -> ({ Camera })) & ((port: PortLike?, class: "Gasoline") -> ({ Gasoline })) & ((port: PortLike?, class: "SubspaceTripmine") -> ({ SubspaceTripmine })) & ((port: PortLike?, class: "Modem") -> ({ Modem })) & ((port: PortLike?, class: "TemperatureSensor") -> ({ TemperatureSensor })) & ((port: PortLike?, class: "StanSword") -> ({ StanSword })) & ((port: PortLike?, class: "Engine") -> ({ Engine })) & ((port: PortLike?, class: "ImpactPlate") -> ({ ImpactPlate })) & ((port: PortLike?, class: "Radar") -> ({ Radar })) & ((port: PortLike?, class: "FireWood") -> ({ FireWood })) & ((port: PortLike?, class: "AdminTool") -> ({ AdminTool })) & ((port: PortLike?, class: "RepairPlate") -> ({ RepairPlate })) & ((port: PortLike?, class: "Chute") -> ({ Chute })) & ((port: PortLike?, class: "MiningLaser") -> ({ MiningLaser })) & ((port: PortLike?, class: "Sulfur") -> ({ Sulfur })) & ((port: PortLike?, class: "Stone") -> ({ Stone })) & ((port: PortLike?, class: "TintedGlass") -> ({ TintedGlass })) & ((port: PortLike?, class: "Steam") -> ({ Steam })) & ((port: PortLike?, class: "IonRocket") -> ({ IonRocket })) & ((port: PortLike?, class: "Stanlium") -> ({ Stanlium })) & ((port: PortLike?, class: "Snow") -> ({ Snow })) & ((port: PortLike?, class: "BallastTank") -> ({ BallastTank })) & ((port: PortLike?, class: "Petroleum") -> ({ Petroleum })) & ((port: PortLike?, class: "Claymore") -> ({ Claymore })) & ((port: PortLike?, class: "Sand") -> ({ Sand })) & ((port: PortLike?, class: "Ruby") -> ({ Ruby })) & ((port: PortLike?, class: "CrossBow") -> ({ CrossBow })) & ((port: PortLike?, class: "Router") -> ({ Router })) & ((port: PortLike?, class: "Explosive") -> ({ Explosive })) & ((port: PortLike?, class: "Generator") -> ({ Generator })) & ((port: PortLike?, class: "ObjectDetector") -> ({ ObjectDetector })) & ((port: PortLike?, class: "Ice") -> ({ Ice })) & ((port: PortLike?, class: "StarMap") -> ({ StarMap })) & ((port: PortLike?, class: "Perfectium") -> ({ Perfectium })) & ((port: PortLike?, class: "TractorBeam") -> ({ TractorBeam })) & ((port: PortLike?, class: "Obamium") -> ({ Obamium })) & ((port: PortLike?, class: "Turbofan") -> ({ Turbofan })) & ((port: PortLike?, class: "NitrogenOxide") -> ({ NitrogenOxide })) & ((port: PortLike?, class: "Battery") -> ({ Battery })) & ((port: PortLike?, class: "Neutronium") -> ({ Neutronium })) & ((port: PortLike?, class: "Coupler") -> ({ Coupler })) & ((port: PortLike?, class: "Iron") -> ({ Iron })) & ((port: PortLike?, class: "Jade") -> ({ Jade })) & ((port: PortLike?, class: "DevTeleporter") -> ({ DevTeleporter })) & ((port: PortLike?, class: "Lava") -> ({ Lava })) & ((port: PortLike?, class: "Mandrillium") -> ({ Mandrillium })) & ((port: PortLike?, class: "Rubber") -> ({ Rubber })) & ((port: PortLike?, class: "Helium") -> ({ Helium })) & ((port: PortLike?, class: "Heatshield") -> ({ Heatshield })) & ((port: PortLike?, class: "HeatSink") -> ({ HeatSink })) & ((port: PortLike?, class: "Grass") -> ({ Grass })) & ((port: PortLike?, class: "PlutoniumCore") -> ({ PlutoniumCore })) & ((port: PortLike?, class: "Seat") -> ({ Seat })) & ((port: PortLike?, class: "ImpulseCannon") -> ({ ImpulseCannon })) & ((port: PortLike?, class: "Glass") -> ({ Glass })) & ((port: PortLike?, class: "Part100k") -> ({ Part100k })) & ((port: PortLike?, class: "Wedge") -> ({ Wedge })) & ((port: PortLike?, class: "Valve") -> ({ Valve })) & ((port: PortLike?, class: "ExoticMatter") -> ({ ExoticMatter })) & ((port: PortLike?, class: "CornerRoundWedge") -> ({ CornerRoundWedge })) & ((port: PortLike?, class: "Diamond") -> ({ Diamond })) & ((port: PortLike?, class: "Freezer") -> ({ Freezer })) & ((port: PortLike?, class: "DarkMatter") -> ({ DarkMatter })) & ((port: PortLike?, class: "Melter") -> ({ Melter })) & ((port: PortLike?, class: "Coal") -> ({ Coal })) & ((port: PortLike?, class: "ChemicalSynthiser") -> ({ ChemicalSynthiser })) & ((port: PortLike?, class: "Beryllium") -> ({ Beryllium })) & ((port: PortLike?, class: "MustardGas") -> ({ MustardGas })) & ((port: PortLike?, class: "AlienCore") -> ({ AlienCore })) & ((port: PortLike?, class: "Railgun") -> ({ Railgun })) & ((port: PortLike?, class: "Dispenser") -> ({ Dispenser })) & ((port: PortLike?, class: "RustedMetal") -> ({ RustedMetal })) & ((port: PortLike?, class: "ZapWire") -> ({ ZapWire })) & ((port: PortLike?, class: "Wing") -> ({ Wing })) & ((port: PortLike?, class: "Wire") -> ({ Wire })) & ((port: PortLike?, class: "Flint") -> ({ Flint })) & ((port: PortLike?, class: "Anchor") -> ({ Anchor })) & ((port: PortLike?, class: "Rifle") -> ({ Rifle })) & ((port: PortLike?, class: "HalfSphere") -> ({ HalfSphere })) & ((port: PortLike?, class: "Tire") -> ({ Tire })) & ((port: PortLike?, class: "Artillery") -> ({ Artillery })) & ((port: PortLike?, class: "StudAligner") -> ({ StudAligner })) & ((port: PortLike?, class: "Truss") -> ({ Truss })) & ((port: PortLike?, class: "TriggerWire") -> ({ TriggerWire })) & ((port: PortLike?, class: "HeatValve") -> ({ HeatValve })) & ((port: PortLike?, class: "Spheroid") -> ({ Spheroid })) & ((port: PortLike?, class: "AirSupply") -> ({ AirSupply })) & ((port: PortLike?, class: "Constructor") -> ({ Constructor })) & ((port: PortLike?, class: "tinnitus") -> ({ tinnitus })) & ((port: PortLike?, class: "Primer") -> ({ Primer })) & ((port: PortLike?, class: "SmoothGlass") -> ({ SmoothGlass })) & ((port: PortLike?, class: "EnergyBomb") -> ({ EnergyBomb })) & ((port: PortLike?, class: "Hydrogen") -> ({ Hydrogen })) & ((port: PortLike?, class: "Rice") -> ({ Rice })) & ((port: PortLike?, class: "RegionCloaker") -> ({ RegionCloaker })) & ((port: PortLike?, class: "Pulverizer") -> ({ Pulverizer })) & ((port: PortLike?, class: "SmoothReinforcedGlass") -> ({ SmoothReinforcedGlass })) & ((port: PortLike?, class: "EnergyShield") -> ({ EnergyShield })) & ((port: PortLike?, class: "PlasmaCannon") -> ({ PlasmaCannon })) & ((port: PortLike?, class: "TouchSensor") -> ({ TouchSensor })) & ((port: PortLike?, class: "PDWX44") -> ({ PDWX44 })) & ((port: PortLike?, class: "StorageSensor") -> ({ StorageSensor })) & ((port: PortLike?, class: "Beaker") -> ({ Beaker })) & ((port: PortLike?, class: "Lantern") -> ({ Lantern })) & ((port: PortLike?, class: "Katana") -> ({ Katana })) & ((port: PortLike?, class: "Aerogel") -> ({ Aerogel })) & ((port: PortLike?, class: "Microphone") -> ({ Microphone })) & ((port: PortLike?, class: "Gold") -> ({ Gold })) & ((port: PortLike?, class: "Hotdog") -> ({ Hotdog })) & ((port: PortLike?, class: "Heater") -> ({ Heater })) & ((port: PortLike?, class: "VintagePlasmaPistol") -> ({ VintagePlasmaPistol })) & ((port: PortLike?, class: "Gun") -> ({ Gun })) & ((port: PortLike?, class: "Blade") -> ({ Blade })) & ((port: PortLike?, class: "Sorter") -> ({ Sorter })) & ((port: PortLike?, class: "Goo") -> ({ Goo })) & ((port: PortLike?, class: "Framewire") -> ({ Framewire })) & ((port: PortLike?, class: "Hydroponic") -> ({ Hydroponic })) & ((port: PortLike?, class: "FloatDevice") -> ({ FloatDevice })) & ((port: PortLike?, class: "GravityGenerator") -> ({ GravityGenerator })) & ((port: PortLike?, class: "Flashlight") -> ({ Flashlight })) & ((port: PortLike?, class: "Gyro") -> ({ Gyro })) & ((port: PortLike?, class: "WindTurbine") -> ({ WindTurbine })) & ((port: PortLike?, class: "EnergySword") -> ({ EnergySword })) & ((port: PortLike?, class: "Cylinder") -> ({ Cylinder })) & ((port: PortLike?, class: "Sign") -> ({ Sign })) & ((port: PortLike?, class: "Radiator") -> ({ Radiator })) & ((port: PortLike?, class: "ConveyorBelt") -> ({ ConveyorBelt })) & ((port: PortLike?, class: "Plastic") -> ({ Plastic })) & ((port: PortLike?, class: "Egg") -> ({ Egg })) & ((port: PortLike?, class: "Shotgun") -> ({ Shotgun })) & ((port: PortLike?, class: "Titanium") -> ({ Titanium })) & ((port: PortLike?, class: "Hologram") -> ({ Hologram })) & ((port: PortLike?, class: "Button") -> ({ Button })) & ((port: PortLike?, class: "FourthOfJuly") -> ({ FourthOfJuly })) & ((port: PortLike?, class: "Relay") -> ({ Relay })) & ((port: PortLike?, class: "Transporter") -> ({ Transporter })) & ((port: PortLike?, class: "Oil") -> ({ Oil })) & ((port: PortLike?, class: "DevBattery") -> ({ DevBattery })) & ((port: PortLike?, class: "Uranium") -> ({ Uranium })) & ((port: PortLike?, class: "Door") -> ({ Door })) & ((port: PortLike?, class: "Handle") -> ({ Handle })) & ((port: PortLike?, class: "SolarScoop") -> ({ SolarScoop })) & ((port: PortLike?, class: "Refinery") -> ({ Refinery })) & ((port: PortLike?, class: "Bin") -> ({ Bin })) & ((port: PortLike?, class: "Propeller") -> ({ Propeller })) & ((port: PortLike?, class: "CornerWedge") -> ({ CornerWedge })) & ((port: PortLike?, class: "Copper") -> ({ Copper })) & ((port: PortLike?, class: "Kiln") -> ({ Kiln })) & ((port: PortLike?, class: "Plasma") -> ({ Plasma })) & ((port: PortLike?, class: "SMG") -> ({ SMG })) & ((port: PortLike?, class: "Boiler") -> ({ Boiler })) & ((port: PortLike?, class: "Asphalt") -> ({ Asphalt })) & ((port: PortLike?, class: "HyperspaceRadar") -> ({ HyperspaceRadar })) & ((port: PortLike?, class: "NuclearWaste") -> ({ NuclearWaste })) & ((port: PortLike?, class: "Tank") -> ({ Tank })) & ((port: PortLike?, class: "Switch") -> ({ Switch })) & ((port: PortLike?, class: "Port") -> ({ Port })) & ((port: PortLike?, class: "Beacon") -> ({ Beacon })) & ((port: PortLike?, class: "Silicon") -> ({ Silicon })) & ((port: PortLike?, class: "Spotlight") -> ({ Spotlight }))
declare GetPart: ((class: "Wood") -> (Wood?)) & ((class: "DelayWire") -> (DelayWire?)) & ((class: "Hatch") -> (Hatch?)) & ((class: "Screen") -> (Screen?)) & ((class: "Extractor") -> (Extractor?)) & ((class: "DevGenerator") -> (DevGenerator?)) & ((class: "BurstLaser") -> (BurstLaser?)) & ((class: "Apparel") -> (Apparel?)) & ((class: "Cooler") -> (Cooler?)) & ((class: "CloningBay") -> (CloningBay?)) & ((class: "Food") -> (Food?)) & ((class: "Lead") -> (Lead?)) & ((class: "BlastingCap") -> (BlastingCap?)) & ((class: "HeatPipe") -> (HeatPipe?)) & ((class: "Filter") -> (Filter?)) & ((class: "Antenna") -> (Antenna?)) & ((class: "Instrument") -> (Instrument?)) & ((class: "ARController") -> (ARController?)) & ((class: "Teleporter") -> (Teleporter?)) & ((class: "Water") -> (Water?)) & ((class: "Stick") -> (Stick?)) & ((class: "Laser") -> (Laser?)) & ((class: "DevHeatStorage") -> (DevHeatStorage?)) & ((class: "WaterCooler") -> (WaterCooler?)) & ((class: "Cone") -> (Cone?)) & ((class: "ReinforcedGlass") -> (ReinforcedGlass?)) & ((class: "BurnerGenerator") -> (BurnerGenerator?)) & ((class: "TestStarMap") -> (TestStarMap?)) & ((class: "DevGravityGenerator") -> (DevGravityGenerator?)) & ((class: "Fence") -> (Fence?)) & ((class: "Servo") -> (Servo?)) & ((class: "DriveBox") -> (DriveBox?)) & ((class: "FluidProjector") -> (FluidProjector?)) & ((class: "ScubaMask") -> (ScubaMask?)) & ((class: "Frog") -> (Frog?)) & ((class: "MonsterMashPotion") -> (MonsterMashPotion?)) & ((class: "TimeSensor") -> (TimeSensor?)) & ((class: "ElectricFence") -> (ElectricFence?)) & ((class: "ARGlasses") -> (ARGlasses?)) & ((class: "Pump") -> (Pump?)) & ((class: "Torch") -> (Torch?)) & ((class: "Assembler") -> (Assembler?)) & ((class: "Diode") -> (Diode?)) & ((class: "Neon") -> (Neon?)) & ((class: "Keyboard") -> (Keyboard?)) & ((class: "Cement") -> (Cement?)) & ((class: "StasisField") -> (StasisField?)) & ((class: "EnergyGun") -> (EnergyGun?)) & ((class: "LightBridge") -> (LightBridge?)) & ((class: "Ball") -> (Ball?)) & ((class: "ProximityButton") -> (ProximityButton?)) & ((class: "Block") -> (Block?)) & ((class: "WirelessButton") -> (WirelessButton?)) & ((class: "DarkConverter") -> (DarkConverter?)) & ((class: "Disk") -> (Disk?)) & ((class: "LightTube") -> (LightTube?)) & ((class: "LifeSensor") -> (LifeSensor?)) & ((class: "SteamTurbine") -> (SteamTurbine?)) & ((class: "PilotObject") -> (PilotObject?)) & ((class: "Component") -> (Component?)) & ((class: "Brick") -> (Brick?)) & ((class: "Warhead") -> (Warhead?)) & ((class: "AdBoard") -> (AdBoard?)) & ((class: "Transformer") -> (Transformer?)) & ((class: "Fireworks") -> (Fireworks?)) & ((class: "Cleat") -> (Cleat?)) & ((class: "Pistol") -> (Pistol?)) & ((class: "FactionHub") -> (FactionHub?)) & ((class: "TriggerRelay") -> (TriggerRelay?)) & ((class: "PowerCell") -> (PowerCell?)) & ((class: "Transistor") -> (Transistor?)) & ((class: "Piston") -> (Piston?)) & ((class: "Treads") -> (Treads?)) & ((class: "Marble") -> (Marble?)) & ((class: "CrudeWing") -> (CrudeWing?)) & ((class: "Quartz") -> (Quartz?)) & ((class: "SteamEngine") -> (SteamEngine?)) & ((class: "Microcontroller") -> (Microcontroller?)) & ((class: "Plutonium") -> (Plutonium?)) & ((class: "HeatPump") -> (HeatPump?)) & ((class: "Speaker") -> (Speaker?)) & ((class: "Cloth") -> (Cloth?)) & ((class: "Magnesium") -> (Magnesium?)) & ((class: "SoundMuffler") -> (SoundMuffler?)) & ((class: "Hull") -> (Hull?)) & ((class: "VehicleSeat") -> (VehicleSeat?)) & ((class: "RoundWedge") -> (RoundWedge?)) & ((class: "Winch") -> (Winch?)) & ((class: "Insulation") -> (Insulation?)) & ((class: "RTG") -> (RTG?)) & ((class: "Rotor") -> (Rotor?)) & ((class: "Controller") -> (Controller?)) & ((class: "Obelisk") -> (Obelisk?)) & ((class: "Motor") -> (Motor?)) & ((class: "Aluminum") -> (Aluminum?)) & ((class: "Telescope") -> (Telescope?)) & ((class: "BlackBox") -> (BlackBox?)) & ((class: "Reactor") -> (Reactor?)) & ((class: "Prosthetic") -> (Prosthetic?)) & ((class: "RoundWedge2") -> (RoundWedge2?)) & ((class: "Decoupler") -> (Decoupler?)) & ((class: "Solenoid") -> (Solenoid?)) & ((class: "DevSink") -> (DevSink?)) & ((class: "HyperDrive") -> (HyperDrive?)) & ((class: "Tile") -> (Tile?)) & ((class: "TriggerSwitch") -> (TriggerSwitch?)) & ((class: "SolarPanel") -> (SolarPanel?)) & ((class: "Boombox") -> (Boombox?)) & ((class: "RepairKit") -> (RepairKit?)) & ((class: "Igniter") -> (Igniter?)) & ((class: "FactionSpawn") -> (FactionSpawn?)) & ((class: "DeleteSwitch") -> (DeleteSwitch?)) & ((class: "Electromagnet") -> (Electromagnet?)) & ((class: "SpawnPoint") -> (SpawnPoint?)) & ((class: "Rocket") -> (Rocket?)) & ((class: "Scrapper") -> (Scrapper?)) & ((class: "TemperatureGate") -> (TemperatureGate?)) & ((class: "RemoteControl") -> (RemoteControl?)) & ((class: "Flamethrower") -> (Flamethrower?)) & ((class: "Light") -> (Light?)) & ((class: "Balloon") -> (Balloon?)) & ((class: "VitalScanner") -> (VitalScanner?)) & ((class: "BeamRifle") -> (BeamRifle?)) & ((class: "DarkReactor") -> (DarkReactor?)) & ((class: "Extinguisher") -> (Extinguisher?)) & ((class: "Sail") -> (Sail?)) & ((class: "Rail") -> (Rail?)) & ((class: "RepairLaser") -> (RepairLaser?)) & ((class: "Polysilicon") -> (Polysilicon?)) & ((class: "Scanner") -> (Scanner?)) & ((class: "CornerTetra") -> (CornerTetra?)) & ((class: "Gear") -> (Gear?)) & ((class: "Furnace") -> (Furnace?)) & ((class: "Cannon") -> (Cannon?)) & ((class: "DevSource") -> (DevSource?)) & ((class: "CornerRoundWedge2") -> (CornerRoundWedge2?)) & ((class: "Tetrahedron") -> (Tetrahedron?)) & ((class: "Pipe") -> (Pipe?)) & ((class: "EthernetCable") -> (EthernetCable?)) & ((class: "AutomaticLaser") -> (AutomaticLaser?)) & ((class: "Faucet") -> (Faucet?)) & ((class: "NightVisionGoggles") -> (NightVisionGoggles?)) & ((class: "Thruster") -> (Thruster?)) & ((class: "Billboardium") -> (Billboardium?)) & ((class: "CombustionTurbine") -> (CombustionTurbine?)) & ((class: "TouchScreen") -> (TouchScreen?)) & ((class: "GeigerCounter") -> (GeigerCounter?)) & ((class: "HeatCell") -> (HeatCell?)) & ((class: "Camera") -> (Camera?)) & ((class: "Gasoline") -> (Gasoline?)) & ((class: "SubspaceTripmine") -> (SubspaceTripmine?)) & ((class: "Modem") -> (Modem?)) & ((class: "TemperatureSensor") -> (TemperatureSensor?)) & ((class: "StanSword") -> (StanSword?)) & ((class: "Engine") -> (Engine?)) & ((class: "ImpactPlate") -> (ImpactPlate?)) & ((class: "Radar") -> (Radar?)) & ((class: "FireWood") -> (FireWood?)) & ((class: "AdminTool") -> (AdminTool?)) & ((class: "RepairPlate") -> (RepairPlate?)) & ((class: "Chute") -> (Chute?)) & ((class: "MiningLaser") -> (MiningLaser?)) & ((class: "Sulfur") -> (Sulfur?)) & ((class: "Stone") -> (Stone?)) & ((class: "TintedGlass") -> (TintedGlass?)) & ((class: "Steam") -> (Steam?)) & ((class: "IonRocket") -> (IonRocket?)) & ((class: "Stanlium") -> (Stanlium?)) & ((class: "Snow") -> (Snow?)) & ((class: "BallastTank") -> (BallastTank?)) & ((class: "Petroleum") -> (Petroleum?)) & ((class: "Claymore") -> (Claymore?)) & ((class: "Sand") -> (Sand?)) & ((class: "Ruby") -> (Ruby?)) & ((class: "CrossBow") -> (CrossBow?)) & ((class: "Router") -> (Router?)) & ((class: "Explosive") -> (Explosive?)) & ((class: "Generator") -> (Generator?)) & ((class: "ObjectDetector") -> (ObjectDetector?)) & ((class: "Ice") -> (Ice?)) & ((class: "StarMap") -> (StarMap?)) & ((class: "Perfectium") -> (Perfectium?)) & ((class: "TractorBeam") -> (TractorBeam?)) & ((class: "Obamium") -> (Obamium?)) & ((class: "Turbofan") -> (Turbofan?)) & ((class: "NitrogenOxide") -> (NitrogenOxide?)) & ((class: "Battery") -> (Battery?)) & ((class: "Neutronium") -> (Neutronium?)) & ((class: "Coupler") -> (Coupler?)) & ((class: "Iron") -> (Iron?)) & ((class: "Jade") -> (Jade?)) & ((class: "DevTeleporter") -> (DevTeleporter?)) & ((class: "Lava") -> (Lava?)) & ((class: "Mandrillium") -> (Mandrillium?)) & ((class: "Rubber") -> (Rubber?)) & ((class: "Helium") -> (Helium?)) & ((class: "Heatshield") -> (Heatshield?)) & ((class: "HeatSink") -> (HeatSink?)) & ((class: "Grass") -> (Grass?)) & ((class: "PlutoniumCore") -> (PlutoniumCore?)) & ((class: "Seat") -> (Seat?)) & ((class: "ImpulseCannon") -> (ImpulseCannon?)) & ((class: "Glass") -> (Glass?)) & ((class: "Part100k") -> (Part100k?)) & ((class: "Wedge") -> (Wedge?)) & ((class: "Valve") -> (Valve?)) & ((class: "ExoticMatter") -> (ExoticMatter?)) & ((class: "CornerRoundWedge") -> (CornerRoundWedge?)) & ((class: "Diamond") -> (Diamond?)) & ((class: "Freezer") -> (Freezer?)) & ((class: "DarkMatter") -> (DarkMatter?)) & ((class: "Melter") -> (Melter?)) & ((class: "Coal") -> (Coal?)) & ((class: "ChemicalSynthiser") -> (ChemicalSynthiser?)) & ((class: "Beryllium") -> (Beryllium?)) & ((class: "MustardGas") -> (MustardGas?)) & ((class: "AlienCore") -> (AlienCore?)) & ((class: "Railgun") -> (Railgun?)) & ((class: "Dispenser") -> (Dispenser?)) & ((class: "RustedMetal") -> (RustedMetal?)) & ((class: "ZapWire") -> (ZapWire?)) & ((class: "Wing") -> (Wing?)) & ((class: "Wire") -> (Wire?)) & ((class: "Flint") -> (Flint?)) & ((class: "Anchor") -> (Anchor?)) & ((class: "Rifle") -> (Rifle?)) & ((class: "HalfSphere") -> (HalfSphere?)) & ((class: "Tire") -> (Tire?)) & ((class: "Artillery") -> (Artillery?)) & ((class: "StudAligner") -> (StudAligner?)) & ((class: "Truss") -> (Truss?)) & ((class: "TriggerWire") -> (TriggerWire?)) & ((class: "HeatValve") -> (HeatValve?)) & ((class: "Spheroid") -> (Spheroid?)) & ((class: "AirSupply") -> (AirSupply?)) & ((class: "Constructor") -> (Constructor?)) & ((class: "tinnitus") -> (tinnitus?)) & ((class: "Primer") -> (Primer?)) & ((class: "SmoothGlass") -> (SmoothGlass?)) & ((class: "EnergyBomb") -> (EnergyBomb?)) & ((class: "Hydrogen") -> (Hydrogen?)) & ((class: "Rice") -> (Rice?)) & ((class: "RegionCloaker") -> (RegionCloaker?)) & ((class: "Pulverizer") -> (Pulverizer?)) & ((class: "SmoothReinforcedGlass") -> (SmoothReinforcedGlass?)) & ((class: "EnergyShield") -> (EnergyShield?)) & ((class: "PlasmaCannon") -> (PlasmaCannon?)) & ((class: "TouchSensor") -> (TouchSensor?)) & ((class: "PDWX44") -> (PDWX44?)) & ((class: "StorageSensor") -> (StorageSensor?)) & ((class: "Beaker") -> (Beaker?)) & ((class: "Lantern") -> (Lantern?)) & ((class: "Katana") -> (Katana?)) & ((class: "Aerogel") -> (Aerogel?)) & ((class: "Microphone") -> (Microphone?)) & ((class: "Gold") -> (Gold?)) & ((class: "Hotdog") -> (Hotdog?)) & ((class: "Heater") -> (Heater?)) & ((class: "VintagePlasmaPistol") -> (VintagePlasmaPistol?)) & ((class: "Gun") -> (Gun?)) & ((class: "Blade") -> (Blade?)) & ((class: "Sorter") -> (Sorter?)) & ((class: "Goo") -> (Goo?)) & ((class: "Framewire") -> (Framewire?)) & ((class: "Hydroponic") -> (Hydroponic?)) & ((class: "FloatDevice") -> (FloatDevice?)) & ((class: "GravityGenerator") -> (GravityGenerator?)) & ((class: "Flashlight") -> (Flashlight?)) & ((class: "Gyro") -> (Gyro?)) & ((class: "WindTurbine") -> (WindTurbine?)) & ((class: "EnergySword") -> (EnergySword?)) & ((class: "Cylinder") -> (Cylinder?)) & ((class: "Sign") -> (Sign?)) & ((class: "Radiator") -> (Radiator?)) & ((class: "ConveyorBelt") -> (ConveyorBelt?)) & ((class: "Plastic") -> (Plastic?)) & ((class: "Egg") -> (Egg?)) & ((class: "Shotgun") -> (Shotgun?)) & ((class: "Titanium") -> (Titanium?)) & ((class: "Hologram") -> (Hologram?)) & ((class: "Button") -> (Button?)) & ((class: "FourthOfJuly") -> (FourthOfJuly?)) & ((class: "Relay") -> (Relay?)) & ((class: "Transporter") -> (Transporter?)) & ((class: "Oil") -> (Oil?)) & ((class: "DevBattery") -> (DevBattery?)) & ((class: "Uranium") -> (Uranium?)) & ((class: "Door") -> (Door?)) & ((class: "Handle") -> (Handle?)) & ((class: "SolarScoop") -> (SolarScoop?)) & ((class: "Refinery") -> (Refinery?)) & ((class: "Bin") -> (Bin?)) & ((class: "Propeller") -> (Propeller?)) & ((class: "CornerWedge") -> (CornerWedge?)) & ((class: "Copper") -> (Copper?)) & ((class: "Kiln") -> (Kiln?)) & ((class: "Plasma") -> (Plasma?)) & ((class: "SMG") -> (SMG?)) & ((class: "Boiler") -> (Boiler?)) & ((class: "Asphalt") -> (Asphalt?)) & ((class: "HyperspaceRadar") -> (HyperspaceRadar?)) & ((class: "NuclearWaste") -> (NuclearWaste?)) & ((class: "Tank") -> (Tank?)) & ((class: "Switch") -> (Switch?)) & ((class: "Port") -> (Port?)) & ((class: "Beacon") -> (Beacon?)) & ((class: "Silicon") -> (Silicon?)) & ((class: "Spotlight") -> (Spotlight?))
declare function logError(message: string, level: number?): ()
declare function Beep(frequency: number?): ()
declare function GetCPUTime(): (number)
type JSONValue = string | number | boolean | buffer
type JSON = {
	[JSONValue]: JSON,
} | { JSON } | JSONValue
export type ComponentName = "Blade" | "KeyButton" | "ClickButton" | "Hull" | "Door"
export type Permission = "Modify" | "Unlock" | "Paint" | "Configure" | "Interact" | "Attach"
export type TemperatureUnit = "K" | "F" | "C"
export type CanvasContext = "2D" | "3D"
export type HandleTriggerMode = "MouseDown" | "MouseUp" | "Both"
export type VehicleSeatMode = "Horizontal" | "Yaw/Pitch" | "Full" | "Mouse"
export type PolysiliconMode = "Activate" | "Deactivate" | "FlipFlop"
export type RelayMode = "Send" | "Receive"
export type InstrumentType = "Speed" | "AngularSpeed" | "Temperature" | "Time" | "Power" | "Size" | "Position" | "TemperatureF" | "Orientation" | "TemperatureC" | "AirTemperatureF" | "AirTemperatureC"
export type PlayerLimb = "Head" | "Torso" | "Left Arm" | "Right Arm" | "Left Leg" | "Right Leg" | "HumanoidRootPart"
export type BladeShape = "Block" | "Spheroid" | "Cone"
export type HandleSwingMode = "None" | "Swing" | "Point"
export type RemoteControlMode = "EmitFromTarget" | "SendDirectly"
export type RegionLog = {
	TimeAgo: number,
	Event: RegionLogType,
	Desc: string,
}
export type RegionLogType = "HyperDrive is warping to" | "Aliens were spawned from an obelisk located at" | "Spawned" | "Death" | "ExitRegion" | "Poison" | "Irradiated" | "Suffocating" | "Freezing" | "Melting"
export type Network = {
	GetPort: (self: Network, port: PortLike?) -> (Port),
	GetPartsFromPort: ((self: Network, port: PortLike?, class: "Wood") -> ({ Wood })) & ((self: Network, port: PortLike?, class: "DelayWire") -> ({ DelayWire })) & ((self: Network, port: PortLike?, class: "Hatch") -> ({ Hatch })) & ((self: Network, port: PortLike?, class: "Screen") -> ({ Screen })) & ((self: Network, port: PortLike?, class: "Extractor") -> ({ Extractor })) & ((self: Network, port: PortLike?, class: "DevGenerator") -> ({ DevGenerator })) & ((self: Network, port: PortLike?, class: "BurstLaser") -> ({ BurstLaser })) & ((self: Network, port: PortLike?, class: "Apparel") -> ({ Apparel })) & ((self: Network, port: PortLike?, class: "Cooler") -> ({ Cooler })) & ((self: Network, port: PortLike?, class: "CloningBay") -> ({ CloningBay })) & ((self: Network, port: PortLike?, class: "Food") -> ({ Food })) & ((self: Network, port: PortLike?, class: "Lead") -> ({ Lead })) & ((self: Network, port: PortLike?, class: "BlastingCap") -> ({ BlastingCap })) & ((self: Network, port: PortLike?, class: "HeatPipe") -> ({ HeatPipe })) & ((self: Network, port: PortLike?, class: "Filter") -> ({ Filter })) & ((self: Network, port: PortLike?, class: "Antenna") -> ({ Antenna })) & ((self: Network, port: PortLike?, class: "Instrument") -> ({ Instrument })) & ((self: Network, port: PortLike?, class: "ARController") -> ({ ARController })) & ((self: Network, port: PortLike?, class: "Teleporter") -> ({ Teleporter })) & ((self: Network, port: PortLike?, class: "Water") -> ({ Water })) & ((self: Network, port: PortLike?, class: "Stick") -> ({ Stick })) & ((self: Network, port: PortLike?, class: "Laser") -> ({ Laser })) & ((self: Network, port: PortLike?, class: "DevHeatStorage") -> ({ DevHeatStorage })) & ((self: Network, port: PortLike?, class: "WaterCooler") -> ({ WaterCooler })) & ((self: Network, port: PortLike?, class: "Cone") -> ({ Cone })) & ((self: Network, port: PortLike?, class: "ReinforcedGlass") -> ({ ReinforcedGlass })) & ((self: Network, port: PortLike?, class: "BurnerGenerator") -> ({ BurnerGenerator })) & ((self: Network, port: PortLike?, class: "TestStarMap") -> ({ TestStarMap })) & ((self: Network, port: PortLike?, class: "DevGravityGenerator") -> ({ DevGravityGenerator })) & ((self: Network, port: PortLike?, class: "Fence") -> ({ Fence })) & ((self: Network, port: PortLike?, class: "Servo") -> ({ Servo })) & ((self: Network, port: PortLike?, class: "DriveBox") -> ({ DriveBox })) & ((self: Network, port: PortLike?, class: "FluidProjector") -> ({ FluidProjector })) & ((self: Network, port: PortLike?, class: "ScubaMask") -> ({ ScubaMask })) & ((self: Network, port: PortLike?, class: "Frog") -> ({ Frog })) & ((self: Network, port: PortLike?, class: "MonsterMashPotion") -> ({ MonsterMashPotion })) & ((self: Network, port: PortLike?, class: "TimeSensor") -> ({ TimeSensor })) & ((self: Network, port: PortLike?, class: "ElectricFence") -> ({ ElectricFence })) & ((self: Network, port: PortLike?, class: "ARGlasses") -> ({ ARGlasses })) & ((self: Network, port: PortLike?, class: "Pump") -> ({ Pump })) & ((self: Network, port: PortLike?, class: "Torch") -> ({ Torch })) & ((self: Network, port: PortLike?, class: "Assembler") -> ({ Assembler })) & ((self: Network, port: PortLike?, class: "Diode") -> ({ Diode })) & ((self: Network, port: PortLike?, class: "Neon") -> ({ Neon })) & ((self: Network, port: PortLike?, class: "Keyboard") -> ({ Keyboard })) & ((self: Network, port: PortLike?, class: "Cement") -> ({ Cement })) & ((self: Network, port: PortLike?, class: "StasisField") -> ({ StasisField })) & ((self: Network, port: PortLike?, class: "EnergyGun") -> ({ EnergyGun })) & ((self: Network, port: PortLike?, class: "LightBridge") -> ({ LightBridge })) & ((self: Network, port: PortLike?, class: "Ball") -> ({ Ball })) & ((self: Network, port: PortLike?, class: "ProximityButton") -> ({ ProximityButton })) & ((self: Network, port: PortLike?, class: "Block") -> ({ Block })) & ((self: Network, port: PortLike?, class: "WirelessButton") -> ({ WirelessButton })) & ((self: Network, port: PortLike?, class: "DarkConverter") -> ({ DarkConverter })) & ((self: Network, port: PortLike?, class: "Disk") -> ({ Disk })) & ((self: Network, port: PortLike?, class: "LightTube") -> ({ LightTube })) & ((self: Network, port: PortLike?, class: "LifeSensor") -> ({ LifeSensor })) & ((self: Network, port: PortLike?, class: "SteamTurbine") -> ({ SteamTurbine })) & ((self: Network, port: PortLike?, class: "PilotObject") -> ({ PilotObject })) & ((self: Network, port: PortLike?, class: "Component") -> ({ Component })) & ((self: Network, port: PortLike?, class: "Brick") -> ({ Brick })) & ((self: Network, port: PortLike?, class: "Warhead") -> ({ Warhead })) & ((self: Network, port: PortLike?, class: "AdBoard") -> ({ AdBoard })) & ((self: Network, port: PortLike?, class: "Transformer") -> ({ Transformer })) & ((self: Network, port: PortLike?, class: "Fireworks") -> ({ Fireworks })) & ((self: Network, port: PortLike?, class: "Cleat") -> ({ Cleat })) & ((self: Network, port: PortLike?, class: "Pistol") -> ({ Pistol })) & ((self: Network, port: PortLike?, class: "FactionHub") -> ({ FactionHub })) & ((self: Network, port: PortLike?, class: "TriggerRelay") -> ({ TriggerRelay })) & ((self: Network, port: PortLike?, class: "PowerCell") -> ({ PowerCell })) & ((self: Network, port: PortLike?, class: "Transistor") -> ({ Transistor })) & ((self: Network, port: PortLike?, class: "Piston") -> ({ Piston })) & ((self: Network, port: PortLike?, class: "Treads") -> ({ Treads })) & ((self: Network, port: PortLike?, class: "Marble") -> ({ Marble })) & ((self: Network, port: PortLike?, class: "CrudeWing") -> ({ CrudeWing })) & ((self: Network, port: PortLike?, class: "Quartz") -> ({ Quartz })) & ((self: Network, port: PortLike?, class: "SteamEngine") -> ({ SteamEngine })) & ((self: Network, port: PortLike?, class: "Microcontroller") -> ({ Microcontroller })) & ((self: Network, port: PortLike?, class: "Plutonium") -> ({ Plutonium })) & ((self: Network, port: PortLike?, class: "HeatPump") -> ({ HeatPump })) & ((self: Network, port: PortLike?, class: "Speaker") -> ({ Speaker })) & ((self: Network, port: PortLike?, class: "Cloth") -> ({ Cloth })) & ((self: Network, port: PortLike?, class: "Magnesium") -> ({ Magnesium })) & ((self: Network, port: PortLike?, class: "SoundMuffler") -> ({ SoundMuffler })) & ((self: Network, port: PortLike?, class: "Hull") -> ({ Hull })) & ((self: Network, port: PortLike?, class: "VehicleSeat") -> ({ VehicleSeat })) & ((self: Network, port: PortLike?, class: "RoundWedge") -> ({ RoundWedge })) & ((self: Network, port: PortLike?, class: "Winch") -> ({ Winch })) & ((self: Network, port: PortLike?, class: "Insulation") -> ({ Insulation })) & ((self: Network, port: PortLike?, class: "RTG") -> ({ RTG })) & ((self: Network, port: PortLike?, class: "Rotor") -> ({ Rotor })) & ((self: Network, port: PortLike?, class: "Controller") -> ({ Controller })) & ((self: Network, port: PortLike?, class: "Obelisk") -> ({ Obelisk })) & ((self: Network, port: PortLike?, class: "Motor") -> ({ Motor })) & ((self: Network, port: PortLike?, class: "Aluminum") -> ({ Aluminum })) & ((self: Network, port: PortLike?, class: "Telescope") -> ({ Telescope })) & ((self: Network, port: PortLike?, class: "BlackBox") -> ({ BlackBox })) & ((self: Network, port: PortLike?, class: "Reactor") -> ({ Reactor })) & ((self: Network, port: PortLike?, class: "Prosthetic") -> ({ Prosthetic })) & ((self: Network, port: PortLike?, class: "RoundWedge2") -> ({ RoundWedge2 })) & ((self: Network, port: PortLike?, class: "Decoupler") -> ({ Decoupler })) & ((self: Network, port: PortLike?, class: "Solenoid") -> ({ Solenoid })) & ((self: Network, port: PortLike?, class: "DevSink") -> ({ DevSink })) & ((self: Network, port: PortLike?, class: "HyperDrive") -> ({ HyperDrive })) & ((self: Network, port: PortLike?, class: "Tile") -> ({ Tile })) & ((self: Network, port: PortLike?, class: "TriggerSwitch") -> ({ TriggerSwitch })) & ((self: Network, port: PortLike?, class: "SolarPanel") -> ({ SolarPanel })) & ((self: Network, port: PortLike?, class: "Boombox") -> ({ Boombox })) & ((self: Network, port: PortLike?, class: "RepairKit") -> ({ RepairKit })) & ((self: Network, port: PortLike?, class: "Igniter") -> ({ Igniter })) & ((self: Network, port: PortLike?, class: "FactionSpawn") -> ({ FactionSpawn })) & ((self: Network, port: PortLike?, class: "DeleteSwitch") -> ({ DeleteSwitch })) & ((self: Network, port: PortLike?, class: "Electromagnet") -> ({ Electromagnet })) & ((self: Network, port: PortLike?, class: "SpawnPoint") -> ({ SpawnPoint })) & ((self: Network, port: PortLike?, class: "Rocket") -> ({ Rocket })) & ((self: Network, port: PortLike?, class: "Scrapper") -> ({ Scrapper })) & ((self: Network, port: PortLike?, class: "TemperatureGate") -> ({ TemperatureGate })) & ((self: Network, port: PortLike?, class: "RemoteControl") -> ({ RemoteControl })) & ((self: Network, port: PortLike?, class: "Flamethrower") -> ({ Flamethrower })) & ((self: Network, port: PortLike?, class: "Light") -> ({ Light })) & ((self: Network, port: PortLike?, class: "Balloon") -> ({ Balloon })) & ((self: Network, port: PortLike?, class: "VitalScanner") -> ({ VitalScanner })) & ((self: Network, port: PortLike?, class: "BeamRifle") -> ({ BeamRifle })) & ((self: Network, port: PortLike?, class: "DarkReactor") -> ({ DarkReactor })) & ((self: Network, port: PortLike?, class: "Extinguisher") -> ({ Extinguisher })) & ((self: Network, port: PortLike?, class: "Sail") -> ({ Sail })) & ((self: Network, port: PortLike?, class: "Rail") -> ({ Rail })) & ((self: Network, port: PortLike?, class: "RepairLaser") -> ({ RepairLaser })) & ((self: Network, port: PortLike?, class: "Polysilicon") -> ({ Polysilicon })) & ((self: Network, port: PortLike?, class: "Scanner") -> ({ Scanner })) & ((self: Network, port: PortLike?, class: "CornerTetra") -> ({ CornerTetra })) & ((self: Network, port: PortLike?, class: "Gear") -> ({ Gear })) & ((self: Network, port: PortLike?, class: "Furnace") -> ({ Furnace })) & ((self: Network, port: PortLike?, class: "Cannon") -> ({ Cannon })) & ((self: Network, port: PortLike?, class: "DevSource") -> ({ DevSource })) & ((self: Network, port: PortLike?, class: "CornerRoundWedge2") -> ({ CornerRoundWedge2 })) & ((self: Network, port: PortLike?, class: "Tetrahedron") -> ({ Tetrahedron })) & ((self: Network, port: PortLike?, class: "Pipe") -> ({ Pipe })) & ((self: Network, port: PortLike?, class: "EthernetCable") -> ({ EthernetCable })) & ((self: Network, port: PortLike?, class: "AutomaticLaser") -> ({ AutomaticLaser })) & ((self: Network, port: PortLike?, class: "Faucet") -> ({ Faucet })) & ((self: Network, port: PortLike?, class: "NightVisionGoggles") -> ({ NightVisionGoggles })) & ((self: Network, port: PortLike?, class: "Thruster") -> ({ Thruster })) & ((self: Network, port: PortLike?, class: "Billboardium") -> ({ Billboardium })) & ((self: Network, port: PortLike?, class: "CombustionTurbine") -> ({ CombustionTurbine })) & ((self: Network, port: PortLike?, class: "TouchScreen") -> ({ TouchScreen })) & ((self: Network, port: PortLike?, class: "GeigerCounter") -> ({ GeigerCounter })) & ((self: Network, port: PortLike?, class: "HeatCell") -> ({ HeatCell })) & ((self: Network, port: PortLike?, class: "Camera") -> ({ Camera })) & ((self: Network, port: PortLike?, class: "Gasoline") -> ({ Gasoline })) & ((self: Network, port: PortLike?, class: "SubspaceTripmine") -> ({ SubspaceTripmine })) & ((self: Network, port: PortLike?, class: "Modem") -> ({ Modem })) & ((self: Network, port: PortLike?, class: "TemperatureSensor") -> ({ TemperatureSensor })) & ((self: Network, port: PortLike?, class: "StanSword") -> ({ StanSword })) & ((self: Network, port: PortLike?, class: "Engine") -> ({ Engine })) & ((self: Network, port: PortLike?, class: "ImpactPlate") -> ({ ImpactPlate })) & ((self: Network, port: PortLike?, class: "Radar") -> ({ Radar })) & ((self: Network, port: PortLike?, class: "FireWood") -> ({ FireWood })) & ((self: Network, port: PortLike?, class: "AdminTool") -> ({ AdminTool })) & ((self: Network, port: PortLike?, class: "RepairPlate") -> ({ RepairPlate })) & ((self: Network, port: PortLike?, class: "Chute") -> ({ Chute })) & ((self: Network, port: PortLike?, class: "MiningLaser") -> ({ MiningLaser })) & ((self: Network, port: PortLike?, class: "Sulfur") -> ({ Sulfur })) & ((self: Network, port: PortLike?, class: "Stone") -> ({ Stone })) & ((self: Network, port: PortLike?, class: "TintedGlass") -> ({ TintedGlass })) & ((self: Network, port: PortLike?, class: "Steam") -> ({ Steam })) & ((self: Network, port: PortLike?, class: "IonRocket") -> ({ IonRocket })) & ((self: Network, port: PortLike?, class: "Stanlium") -> ({ Stanlium })) & ((self: Network, port: PortLike?, class: "Snow") -> ({ Snow })) & ((self: Network, port: PortLike?, class: "BallastTank") -> ({ BallastTank })) & ((self: Network, port: PortLike?, class: "Petroleum") -> ({ Petroleum })) & ((self: Network, port: PortLike?, class: "Claymore") -> ({ Claymore })) & ((self: Network, port: PortLike?, class: "Sand") -> ({ Sand })) & ((self: Network, port: PortLike?, class: "Ruby") -> ({ Ruby })) & ((self: Network, port: PortLike?, class: "CrossBow") -> ({ CrossBow })) & ((self: Network, port: PortLike?, class: "Router") -> ({ Router })) & ((self: Network, port: PortLike?, class: "Explosive") -> ({ Explosive })) & ((self: Network, port: PortLike?, class: "Generator") -> ({ Generator })) & ((self: Network, port: PortLike?, class: "ObjectDetector") -> ({ ObjectDetector })) & ((self: Network, port: PortLike?, class: "Ice") -> ({ Ice })) & ((self: Network, port: PortLike?, class: "StarMap") -> ({ StarMap })) & ((self: Network, port: PortLike?, class: "Perfectium") -> ({ Perfectium })) & ((self: Network, port: PortLike?, class: "TractorBeam") -> ({ TractorBeam })) & ((self: Network, port: PortLike?, class: "Obamium") -> ({ Obamium })) & ((self: Network, port: PortLike?, class: "Turbofan") -> ({ Turbofan })) & ((self: Network, port: PortLike?, class: "NitrogenOxide") -> ({ NitrogenOxide })) & ((self: Network, port: PortLike?, class: "Battery") -> ({ Battery })) & ((self: Network, port: PortLike?, class: "Neutronium") -> ({ Neutronium })) & ((self: Network, port: PortLike?, class: "Coupler") -> ({ Coupler })) & ((self: Network, port: PortLike?, class: "Iron") -> ({ Iron })) & ((self: Network, port: PortLike?, class: "Jade") -> ({ Jade })) & ((self: Network, port: PortLike?, class: "DevTeleporter") -> ({ DevTeleporter })) & ((self: Network, port: PortLike?, class: "Lava") -> ({ Lava })) & ((self: Network, port: PortLike?, class: "Mandrillium") -> ({ Mandrillium })) & ((self: Network, port: PortLike?, class: "Rubber") -> ({ Rubber })) & ((self: Network, port: PortLike?, class: "Helium") -> ({ Helium })) & ((self: Network, port: PortLike?, class: "Heatshield") -> ({ Heatshield })) & ((self: Network, port: PortLike?, class: "HeatSink") -> ({ HeatSink })) & ((self: Network, port: PortLike?, class: "Grass") -> ({ Grass })) & ((self: Network, port: PortLike?, class: "PlutoniumCore") -> ({ PlutoniumCore })) & ((self: Network, port: PortLike?, class: "Seat") -> ({ Seat })) & ((self: Network, port: PortLike?, class: "ImpulseCannon") -> ({ ImpulseCannon })) & ((self: Network, port: PortLike?, class: "Glass") -> ({ Glass })) & ((self: Network, port: PortLike?, class: "Part100k") -> ({ Part100k })) & ((self: Network, port: PortLike?, class: "Wedge") -> ({ Wedge })) & ((self: Network, port: PortLike?, class: "Valve") -> ({ Valve })) & ((self: Network, port: PortLike?, class: "ExoticMatter") -> ({ ExoticMatter })) & ((self: Network, port: PortLike?, class: "CornerRoundWedge") -> ({ CornerRoundWedge })) & ((self: Network, port: PortLike?, class: "Diamond") -> ({ Diamond })) & ((self: Network, port: PortLike?, class: "Freezer") -> ({ Freezer })) & ((self: Network, port: PortLike?, class: "DarkMatter") -> ({ DarkMatter })) & ((self: Network, port: PortLike?, class: "Melter") -> ({ Melter })) & ((self: Network, port: PortLike?, class: "Coal") -> ({ Coal })) & ((self: Network, port: PortLike?, class: "ChemicalSynthiser") -> ({ ChemicalSynthiser })) & ((self: Network, port: PortLike?, class: "Beryllium") -> ({ Beryllium })) & ((self: Network, port: PortLike?, class: "MustardGas") -> ({ MustardGas })) & ((self: Network, port: PortLike?, class: "AlienCore") -> ({ AlienCore })) & ((self: Network, port: PortLike?, class: "Railgun") -> ({ Railgun })) & ((self: Network, port: PortLike?, class: "Dispenser") -> ({ Dispenser })) & ((self: Network, port: PortLike?, class: "RustedMetal") -> ({ RustedMetal })) & ((self: Network, port: PortLike?, class: "ZapWire") -> ({ ZapWire })) & ((self: Network, port: PortLike?, class: "Wing") -> ({ Wing })) & ((self: Network, port: PortLike?, class: "Wire") -> ({ Wire })) & ((self: Network, port: PortLike?, class: "Flint") -> ({ Flint })) & ((self: Network, port: PortLike?, class: "Anchor") -> ({ Anchor })) & ((self: Network, port: PortLike?, class: "Rifle") -> ({ Rifle })) & ((self: Network, port: PortLike?, class: "HalfSphere") -> ({ HalfSphere })) & ((self: Network, port: PortLike?, class: "Tire") -> ({ Tire })) & ((self: Network, port: PortLike?, class: "Artillery") -> ({ Artillery })) & ((self: Network, port: PortLike?, class: "StudAligner") -> ({ StudAligner })) & ((self: Network, port: PortLike?, class: "Truss") -> ({ Truss })) & ((self: Network, port: PortLike?, class: "TriggerWire") -> ({ TriggerWire })) & ((self: Network, port: PortLike?, class: "HeatValve") -> ({ HeatValve })) & ((self: Network, port: PortLike?, class: "Spheroid") -> ({ Spheroid })) & ((self: Network, port: PortLike?, class: "AirSupply") -> ({ AirSupply })) & ((self: Network, port: PortLike?, class: "Constructor") -> ({ Constructor })) & ((self: Network, port: PortLike?, class: "tinnitus") -> ({ tinnitus })) & ((self: Network, port: PortLike?, class: "Primer") -> ({ Primer })) & ((self: Network, port: PortLike?, class: "SmoothGlass") -> ({ SmoothGlass })) & ((self: Network, port: PortLike?, class: "EnergyBomb") -> ({ EnergyBomb })) & ((self: Network, port: PortLike?, class: "Hydrogen") -> ({ Hydrogen })) & ((self: Network, port: PortLike?, class: "Rice") -> ({ Rice })) & ((self: Network, port: PortLike?, class: "RegionCloaker") -> ({ RegionCloaker })) & ((self: Network, port: PortLike?, class: "Pulverizer") -> ({ Pulverizer })) & ((self: Network, port: PortLike?, class: "SmoothReinforcedGlass") -> ({ SmoothReinforcedGlass })) & ((self: Network, port: PortLike?, class: "EnergyShield") -> ({ EnergyShield })) & ((self: Network, port: PortLike?, class: "PlasmaCannon") -> ({ PlasmaCannon })) & ((self: Network, port: PortLike?, class: "TouchSensor") -> ({ TouchSensor })) & ((self: Network, port: PortLike?, class: "PDWX44") -> ({ PDWX44 })) & ((self: Network, port: PortLike?, class: "StorageSensor") -> ({ StorageSensor })) & ((self: Network, port: PortLike?, class: "Beaker") -> ({ Beaker })) & ((self: Network, port: PortLike?, class: "Lantern") -> ({ Lantern })) & ((self: Network, port: PortLike?, class: "Katana") -> ({ Katana })) & ((self: Network, port: PortLike?, class: "Aerogel") -> ({ Aerogel })) & ((self: Network, port: PortLike?, class: "Microphone") -> ({ Microphone })) & ((self: Network, port: PortLike?, class: "Gold") -> ({ Gold })) & ((self: Network, port: PortLike?, class: "Hotdog") -> ({ Hotdog })) & ((self: Network, port: PortLike?, class: "Heater") -> ({ Heater })) & ((self: Network, port: PortLike?, class: "VintagePlasmaPistol") -> ({ VintagePlasmaPistol })) & ((self: Network, port: PortLike?, class: "Gun") -> ({ Gun })) & ((self: Network, port: PortLike?, class: "Blade") -> ({ Blade })) & ((self: Network, port: PortLike?, class: "Sorter") -> ({ Sorter })) & ((self: Network, port: PortLike?, class: "Goo") -> ({ Goo })) & ((self: Network, port: PortLike?, class: "Framewire") -> ({ Framewire })) & ((self: Network, port: PortLike?, class: "Hydroponic") -> ({ Hydroponic })) & ((self: Network, port: PortLike?, class: "FloatDevice") -> ({ FloatDevice })) & ((self: Network, port: PortLike?, class: "GravityGenerator") -> ({ GravityGenerator })) & ((self: Network, port: PortLike?, class: "Flashlight") -> ({ Flashlight })) & ((self: Network, port: PortLike?, class: "Gyro") -> ({ Gyro })) & ((self: Network, port: PortLike?, class: "WindTurbine") -> ({ WindTurbine })) & ((self: Network, port: PortLike?, class: "EnergySword") -> ({ EnergySword })) & ((self: Network, port: PortLike?, class: "Cylinder") -> ({ Cylinder })) & ((self: Network, port: PortLike?, class: "Sign") -> ({ Sign })) & ((self: Network, port: PortLike?, class: "Radiator") -> ({ Radiator })) & ((self: Network, port: PortLike?, class: "ConveyorBelt") -> ({ ConveyorBelt })) & ((self: Network, port: PortLike?, class: "Plastic") -> ({ Plastic })) & ((self: Network, port: PortLike?, class: "Egg") -> ({ Egg })) & ((self: Network, port: PortLike?, class: "Shotgun") -> ({ Shotgun })) & ((self: Network, port: PortLike?, class: "Titanium") -> ({ Titanium })) & ((self: Network, port: PortLike?, class: "Hologram") -> ({ Hologram })) & ((self: Network, port: PortLike?, class: "Button") -> ({ Button })) & ((self: Network, port: PortLike?, class: "FourthOfJuly") -> ({ FourthOfJuly })) & ((self: Network, port: PortLike?, class: "Relay") -> ({ Relay })) & ((self: Network, port: PortLike?, class: "Transporter") -> ({ Transporter })) & ((self: Network, port: PortLike?, class: "Oil") -> ({ Oil })) & ((self: Network, port: PortLike?, class: "DevBattery") -> ({ DevBattery })) & ((self: Network, port: PortLike?, class: "Uranium") -> ({ Uranium })) & ((self: Network, port: PortLike?, class: "Door") -> ({ Door })) & ((self: Network, port: PortLike?, class: "Handle") -> ({ Handle })) & ((self: Network, port: PortLike?, class: "SolarScoop") -> ({ SolarScoop })) & ((self: Network, port: PortLike?, class: "Refinery") -> ({ Refinery })) & ((self: Network, port: PortLike?, class: "Bin") -> ({ Bin })) & ((self: Network, port: PortLike?, class: "Propeller") -> ({ Propeller })) & ((self: Network, port: PortLike?, class: "CornerWedge") -> ({ CornerWedge })) & ((self: Network, port: PortLike?, class: "Copper") -> ({ Copper })) & ((self: Network, port: PortLike?, class: "Kiln") -> ({ Kiln })) & ((self: Network, port: PortLike?, class: "Plasma") -> ({ Plasma })) & ((self: Network, port: PortLike?, class: "SMG") -> ({ SMG })) & ((self: Network, port: PortLike?, class: "Boiler") -> ({ Boiler })) & ((self: Network, port: PortLike?, class: "Asphalt") -> ({ Asphalt })) & ((self: Network, port: PortLike?, class: "HyperspaceRadar") -> ({ HyperspaceRadar })) & ((self: Network, port: PortLike?, class: "NuclearWaste") -> ({ NuclearWaste })) & ((self: Network, port: PortLike?, class: "Tank") -> ({ Tank })) & ((self: Network, port: PortLike?, class: "Switch") -> ({ Switch })) & ((self: Network, port: PortLike?, class: "Port") -> ({ Port })) & ((self: Network, port: PortLike?, class: "Beacon") -> ({ Beacon })) & ((self: Network, port: PortLike?, class: "Silicon") -> ({ Silicon })) & ((self: Network, port: PortLike?, class: "Spotlight") -> ({ Spotlight })),
	GetPart: ((self: Network, class: "Wood") -> (Wood?)) & ((self: Network, class: "DelayWire") -> (DelayWire?)) & ((self: Network, class: "Hatch") -> (Hatch?)) & ((self: Network, class: "Screen") -> (Screen?)) & ((self: Network, class: "Extractor") -> (Extractor?)) & ((self: Network, class: "DevGenerator") -> (DevGenerator?)) & ((self: Network, class: "BurstLaser") -> (BurstLaser?)) & ((self: Network, class: "Apparel") -> (Apparel?)) & ((self: Network, class: "Cooler") -> (Cooler?)) & ((self: Network, class: "CloningBay") -> (CloningBay?)) & ((self: Network, class: "Food") -> (Food?)) & ((self: Network, class: "Lead") -> (Lead?)) & ((self: Network, class: "BlastingCap") -> (BlastingCap?)) & ((self: Network, class: "HeatPipe") -> (HeatPipe?)) & ((self: Network, class: "Filter") -> (Filter?)) & ((self: Network, class: "Antenna") -> (Antenna?)) & ((self: Network, class: "Instrument") -> (Instrument?)) & ((self: Network, class: "ARController") -> (ARController?)) & ((self: Network, class: "Teleporter") -> (Teleporter?)) & ((self: Network, class: "Water") -> (Water?)) & ((self: Network, class: "Stick") -> (Stick?)) & ((self: Network, class: "Laser") -> (Laser?)) & ((self: Network, class: "DevHeatStorage") -> (DevHeatStorage?)) & ((self: Network, class: "WaterCooler") -> (WaterCooler?)) & ((self: Network, class: "Cone") -> (Cone?)) & ((self: Network, class: "ReinforcedGlass") -> (ReinforcedGlass?)) & ((self: Network, class: "BurnerGenerator") -> (BurnerGenerator?)) & ((self: Network, class: "TestStarMap") -> (TestStarMap?)) & ((self: Network, class: "DevGravityGenerator") -> (DevGravityGenerator?)) & ((self: Network, class: "Fence") -> (Fence?)) & ((self: Network, class: "Servo") -> (Servo?)) & ((self: Network, class: "DriveBox") -> (DriveBox?)) & ((self: Network, class: "FluidProjector") -> (FluidProjector?)) & ((self: Network, class: "ScubaMask") -> (ScubaMask?)) & ((self: Network, class: "Frog") -> (Frog?)) & ((self: Network, class: "MonsterMashPotion") -> (MonsterMashPotion?)) & ((self: Network, class: "TimeSensor") -> (TimeSensor?)) & ((self: Network, class: "ElectricFence") -> (ElectricFence?)) & ((self: Network, class: "ARGlasses") -> (ARGlasses?)) & ((self: Network, class: "Pump") -> (Pump?)) & ((self: Network, class: "Torch") -> (Torch?)) & ((self: Network, class: "Assembler") -> (Assembler?)) & ((self: Network, class: "Diode") -> (Diode?)) & ((self: Network, class: "Neon") -> (Neon?)) & ((self: Network, class: "Keyboard") -> (Keyboard?)) & ((self: Network, class: "Cement") -> (Cement?)) & ((self: Network, class: "StasisField") -> (StasisField?)) & ((self: Network, class: "EnergyGun") -> (EnergyGun?)) & ((self: Network, class: "LightBridge") -> (LightBridge?)) & ((self: Network, class: "Ball") -> (Ball?)) & ((self: Network, class: "ProximityButton") -> (ProximityButton?)) & ((self: Network, class: "Block") -> (Block?)) & ((self: Network, class: "WirelessButton") -> (WirelessButton?)) & ((self: Network, class: "DarkConverter") -> (DarkConverter?)) & ((self: Network, class: "Disk") -> (Disk?)) & ((self: Network, class: "LightTube") -> (LightTube?)) & ((self: Network, class: "LifeSensor") -> (LifeSensor?)) & ((self: Network, class: "SteamTurbine") -> (SteamTurbine?)) & ((self: Network, class: "PilotObject") -> (PilotObject?)) & ((self: Network, class: "Component") -> (Component?)) & ((self: Network, class: "Brick") -> (Brick?)) & ((self: Network, class: "Warhead") -> (Warhead?)) & ((self: Network, class: "AdBoard") -> (AdBoard?)) & ((self: Network, class: "Transformer") -> (Transformer?)) & ((self: Network, class: "Fireworks") -> (Fireworks?)) & ((self: Network, class: "Cleat") -> (Cleat?)) & ((self: Network, class: "Pistol") -> (Pistol?)) & ((self: Network, class: "FactionHub") -> (FactionHub?)) & ((self: Network, class: "TriggerRelay") -> (TriggerRelay?)) & ((self: Network, class: "PowerCell") -> (PowerCell?)) & ((self: Network, class: "Transistor") -> (Transistor?)) & ((self: Network, class: "Piston") -> (Piston?)) & ((self: Network, class: "Treads") -> (Treads?)) & ((self: Network, class: "Marble") -> (Marble?)) & ((self: Network, class: "CrudeWing") -> (CrudeWing?)) & ((self: Network, class: "Quartz") -> (Quartz?)) & ((self: Network, class: "SteamEngine") -> (SteamEngine?)) & ((self: Network, class: "Microcontroller") -> (Microcontroller?)) & ((self: Network, class: "Plutonium") -> (Plutonium?)) & ((self: Network, class: "HeatPump") -> (HeatPump?)) & ((self: Network, class: "Speaker") -> (Speaker?)) & ((self: Network, class: "Cloth") -> (Cloth?)) & ((self: Network, class: "Magnesium") -> (Magnesium?)) & ((self: Network, class: "SoundMuffler") -> (SoundMuffler?)) & ((self: Network, class: "Hull") -> (Hull?)) & ((self: Network, class: "VehicleSeat") -> (VehicleSeat?)) & ((self: Network, class: "RoundWedge") -> (RoundWedge?)) & ((self: Network, class: "Winch") -> (Winch?)) & ((self: Network, class: "Insulation") -> (Insulation?)) & ((self: Network, class: "RTG") -> (RTG?)) & ((self: Network, class: "Rotor") -> (Rotor?)) & ((self: Network, class: "Controller") -> (Controller?)) & ((self: Network, class: "Obelisk") -> (Obelisk?)) & ((self: Network, class: "Motor") -> (Motor?)) & ((self: Network, class: "Aluminum") -> (Aluminum?)) & ((self: Network, class: "Telescope") -> (Telescope?)) & ((self: Network, class: "BlackBox") -> (BlackBox?)) & ((self: Network, class: "Reactor") -> (Reactor?)) & ((self: Network, class: "Prosthetic") -> (Prosthetic?)) & ((self: Network, class: "RoundWedge2") -> (RoundWedge2?)) & ((self: Network, class: "Decoupler") -> (Decoupler?)) & ((self: Network, class: "Solenoid") -> (Solenoid?)) & ((self: Network, class: "DevSink") -> (DevSink?)) & ((self: Network, class: "HyperDrive") -> (HyperDrive?)) & ((self: Network, class: "Tile") -> (Tile?)) & ((self: Network, class: "TriggerSwitch") -> (TriggerSwitch?)) & ((self: Network, class: "SolarPanel") -> (SolarPanel?)) & ((self: Network, class: "Boombox") -> (Boombox?)) & ((self: Network, class: "RepairKit") -> (RepairKit?)) & ((self: Network, class: "Igniter") -> (Igniter?)) & ((self: Network, class: "FactionSpawn") -> (FactionSpawn?)) & ((self: Network, class: "DeleteSwitch") -> (DeleteSwitch?)) & ((self: Network, class: "Electromagnet") -> (Electromagnet?)) & ((self: Network, class: "SpawnPoint") -> (SpawnPoint?)) & ((self: Network, class: "Rocket") -> (Rocket?)) & ((self: Network, class: "Scrapper") -> (Scrapper?)) & ((self: Network, class: "TemperatureGate") -> (TemperatureGate?)) & ((self: Network, class: "RemoteControl") -> (RemoteControl?)) & ((self: Network, class: "Flamethrower") -> (Flamethrower?)) & ((self: Network, class: "Light") -> (Light?)) & ((self: Network, class: "Balloon") -> (Balloon?)) & ((self: Network, class: "VitalScanner") -> (VitalScanner?)) & ((self: Network, class: "BeamRifle") -> (BeamRifle?)) & ((self: Network, class: "DarkReactor") -> (DarkReactor?)) & ((self: Network, class: "Extinguisher") -> (Extinguisher?)) & ((self: Network, class: "Sail") -> (Sail?)) & ((self: Network, class: "Rail") -> (Rail?)) & ((self: Network, class: "RepairLaser") -> (RepairLaser?)) & ((self: Network, class: "Polysilicon") -> (Polysilicon?)) & ((self: Network, class: "Scanner") -> (Scanner?)) & ((self: Network, class: "CornerTetra") -> (CornerTetra?)) & ((self: Network, class: "Gear") -> (Gear?)) & ((self: Network, class: "Furnace") -> (Furnace?)) & ((self: Network, class: "Cannon") -> (Cannon?)) & ((self: Network, class: "DevSource") -> (DevSource?)) & ((self: Network, class: "CornerRoundWedge2") -> (CornerRoundWedge2?)) & ((self: Network, class: "Tetrahedron") -> (Tetrahedron?)) & ((self: Network, class: "Pipe") -> (Pipe?)) & ((self: Network, class: "EthernetCable") -> (EthernetCable?)) & ((self: Network, class: "AutomaticLaser") -> (AutomaticLaser?)) & ((self: Network, class: "Faucet") -> (Faucet?)) & ((self: Network, class: "NightVisionGoggles") -> (NightVisionGoggles?)) & ((self: Network, class: "Thruster") -> (Thruster?)) & ((self: Network, class: "Billboardium") -> (Billboardium?)) & ((self: Network, class: "CombustionTurbine") -> (CombustionTurbine?)) & ((self: Network, class: "TouchScreen") -> (TouchScreen?)) & ((self: Network, class: "GeigerCounter") -> (GeigerCounter?)) & ((self: Network, class: "HeatCell") -> (HeatCell?)) & ((self: Network, class: "Camera") -> (Camera?)) & ((self: Network, class: "Gasoline") -> (Gasoline?)) & ((self: Network, class: "SubspaceTripmine") -> (SubspaceTripmine?)) & ((self: Network, class: "Modem") -> (Modem?)) & ((self: Network, class: "TemperatureSensor") -> (TemperatureSensor?)) & ((self: Network, class: "StanSword") -> (StanSword?)) & ((self: Network, class: "Engine") -> (Engine?)) & ((self: Network, class: "ImpactPlate") -> (ImpactPlate?)) & ((self: Network, class: "Radar") -> (Radar?)) & ((self: Network, class: "FireWood") -> (FireWood?)) & ((self: Network, class: "AdminTool") -> (AdminTool?)) & ((self: Network, class: "RepairPlate") -> (RepairPlate?)) & ((self: Network, class: "Chute") -> (Chute?)) & ((self: Network, class: "MiningLaser") -> (MiningLaser?)) & ((self: Network, class: "Sulfur") -> (Sulfur?)) & ((self: Network, class: "Stone") -> (Stone?)) & ((self: Network, class: "TintedGlass") -> (TintedGlass?)) & ((self: Network, class: "Steam") -> (Steam?)) & ((self: Network, class: "IonRocket") -> (IonRocket?)) & ((self: Network, class: "Stanlium") -> (Stanlium?)) & ((self: Network, class: "Snow") -> (Snow?)) & ((self: Network, class: "BallastTank") -> (BallastTank?)) & ((self: Network, class: "Petroleum") -> (Petroleum?)) & ((self: Network, class: "Claymore") -> (Claymore?)) & ((self: Network, class: "Sand") -> (Sand?)) & ((self: Network, class: "Ruby") -> (Ruby?)) & ((self: Network, class: "CrossBow") -> (CrossBow?)) & ((self: Network, class: "Router") -> (Router?)) & ((self: Network, class: "Explosive") -> (Explosive?)) & ((self: Network, class: "Generator") -> (Generator?)) & ((self: Network, class: "ObjectDetector") -> (ObjectDetector?)) & ((self: Network, class: "Ice") -> (Ice?)) & ((self: Network, class: "StarMap") -> (StarMap?)) & ((self: Network, class: "Perfectium") -> (Perfectium?)) & ((self: Network, class: "TractorBeam") -> (TractorBeam?)) & ((self: Network, class: "Obamium") -> (Obamium?)) & ((self: Network, class: "Turbofan") -> (Turbofan?)) & ((self: Network, class: "NitrogenOxide") -> (NitrogenOxide?)) & ((self: Network, class: "Battery") -> (Battery?)) & ((self: Network, class: "Neutronium") -> (Neutronium?)) & ((self: Network, class: "Coupler") -> (Coupler?)) & ((self: Network, class: "Iron") -> (Iron?)) & ((self: Network, class: "Jade") -> (Jade?)) & ((self: Network, class: "DevTeleporter") -> (DevTeleporter?)) & ((self: Network, class: "Lava") -> (Lava?)) & ((self: Network, class: "Mandrillium") -> (Mandrillium?)) & ((self: Network, class: "Rubber") -> (Rubber?)) & ((self: Network, class: "Helium") -> (Helium?)) & ((self: Network, class: "Heatshield") -> (Heatshield?)) & ((self: Network, class: "HeatSink") -> (HeatSink?)) & ((self: Network, class: "Grass") -> (Grass?)) & ((self: Network, class: "PlutoniumCore") -> (PlutoniumCore?)) & ((self: Network, class: "Seat") -> (Seat?)) & ((self: Network, class: "ImpulseCannon") -> (ImpulseCannon?)) & ((self: Network, class: "Glass") -> (Glass?)) & ((self: Network, class: "Part100k") -> (Part100k?)) & ((self: Network, class: "Wedge") -> (Wedge?)) & ((self: Network, class: "Valve") -> (Valve?)) & ((self: Network, class: "ExoticMatter") -> (ExoticMatter?)) & ((self: Network, class: "CornerRoundWedge") -> (CornerRoundWedge?)) & ((self: Network, class: "Diamond") -> (Diamond?)) & ((self: Network, class: "Freezer") -> (Freezer?)) & ((self: Network, class: "DarkMatter") -> (DarkMatter?)) & ((self: Network, class: "Melter") -> (Melter?)) & ((self: Network, class: "Coal") -> (Coal?)) & ((self: Network, class: "ChemicalSynthiser") -> (ChemicalSynthiser?)) & ((self: Network, class: "Beryllium") -> (Beryllium?)) & ((self: Network, class: "MustardGas") -> (MustardGas?)) & ((self: Network, class: "AlienCore") -> (AlienCore?)) & ((self: Network, class: "Railgun") -> (Railgun?)) & ((self: Network, class: "Dispenser") -> (Dispenser?)) & ((self: Network, class: "RustedMetal") -> (RustedMetal?)) & ((self: Network, class: "ZapWire") -> (ZapWire?)) & ((self: Network, class: "Wing") -> (Wing?)) & ((self: Network, class: "Wire") -> (Wire?)) & ((self: Network, class: "Flint") -> (Flint?)) & ((self: Network, class: "Anchor") -> (Anchor?)) & ((self: Network, class: "Rifle") -> (Rifle?)) & ((self: Network, class: "HalfSphere") -> (HalfSphere?)) & ((self: Network, class: "Tire") -> (Tire?)) & ((self: Network, class: "Artillery") -> (Artillery?)) & ((self: Network, class: "StudAligner") -> (StudAligner?)) & ((self: Network, class: "Truss") -> (Truss?)) & ((self: Network, class: "TriggerWire") -> (TriggerWire?)) & ((self: Network, class: "HeatValve") -> (HeatValve?)) & ((self: Network, class: "Spheroid") -> (Spheroid?)) & ((self: Network, class: "AirSupply") -> (AirSupply?)) & ((self: Network, class: "Constructor") -> (Constructor?)) & ((self: Network, class: "tinnitus") -> (tinnitus?)) & ((self: Network, class: "Primer") -> (Primer?)) & ((self: Network, class: "SmoothGlass") -> (SmoothGlass?)) & ((self: Network, class: "EnergyBomb") -> (EnergyBomb?)) & ((self: Network, class: "Hydrogen") -> (Hydrogen?)) & ((self: Network, class: "Rice") -> (Rice?)) & ((self: Network, class: "RegionCloaker") -> (RegionCloaker?)) & ((self: Network, class: "Pulverizer") -> (Pulverizer?)) & ((self: Network, class: "SmoothReinforcedGlass") -> (SmoothReinforcedGlass?)) & ((self: Network, class: "EnergyShield") -> (EnergyShield?)) & ((self: Network, class: "PlasmaCannon") -> (PlasmaCannon?)) & ((self: Network, class: "TouchSensor") -> (TouchSensor?)) & ((self: Network, class: "PDWX44") -> (PDWX44?)) & ((self: Network, class: "StorageSensor") -> (StorageSensor?)) & ((self: Network, class: "Beaker") -> (Beaker?)) & ((self: Network, class: "Lantern") -> (Lantern?)) & ((self: Network, class: "Katana") -> (Katana?)) & ((self: Network, class: "Aerogel") -> (Aerogel?)) & ((self: Network, class: "Microphone") -> (Microphone?)) & ((self: Network, class: "Gold") -> (Gold?)) & ((self: Network, class: "Hotdog") -> (Hotdog?)) & ((self: Network, class: "Heater") -> (Heater?)) & ((self: Network, class: "VintagePlasmaPistol") -> (VintagePlasmaPistol?)) & ((self: Network, class: "Gun") -> (Gun?)) & ((self: Network, class: "Blade") -> (Blade?)) & ((self: Network, class: "Sorter") -> (Sorter?)) & ((self: Network, class: "Goo") -> (Goo?)) & ((self: Network, class: "Framewire") -> (Framewire?)) & ((self: Network, class: "Hydroponic") -> (Hydroponic?)) & ((self: Network, class: "FloatDevice") -> (FloatDevice?)) & ((self: Network, class: "GravityGenerator") -> (GravityGenerator?)) & ((self: Network, class: "Flashlight") -> (Flashlight?)) & ((self: Network, class: "Gyro") -> (Gyro?)) & ((self: Network, class: "WindTurbine") -> (WindTurbine?)) & ((self: Network, class: "EnergySword") -> (EnergySword?)) & ((self: Network, class: "Cylinder") -> (Cylinder?)) & ((self: Network, class: "Sign") -> (Sign?)) & ((self: Network, class: "Radiator") -> (Radiator?)) & ((self: Network, class: "ConveyorBelt") -> (ConveyorBelt?)) & ((self: Network, class: "Plastic") -> (Plastic?)) & ((self: Network, class: "Egg") -> (Egg?)) & ((self: Network, class: "Shotgun") -> (Shotgun?)) & ((self: Network, class: "Titanium") -> (Titanium?)) & ((self: Network, class: "Hologram") -> (Hologram?)) & ((self: Network, class: "Button") -> (Button?)) & ((self: Network, class: "FourthOfJuly") -> (FourthOfJuly?)) & ((self: Network, class: "Relay") -> (Relay?)) & ((self: Network, class: "Transporter") -> (Transporter?)) & ((self: Network, class: "Oil") -> (Oil?)) & ((self: Network, class: "DevBattery") -> (DevBattery?)) & ((self: Network, class: "Uranium") -> (Uranium?)) & ((self: Network, class: "Door") -> (Door?)) & ((self: Network, class: "Handle") -> (Handle?)) & ((self: Network, class: "SolarScoop") -> (SolarScoop?)) & ((self: Network, class: "Refinery") -> (Refinery?)) & ((self: Network, class: "Bin") -> (Bin?)) & ((self: Network, class: "Propeller") -> (Propeller?)) & ((self: Network, class: "CornerWedge") -> (CornerWedge?)) & ((self: Network, class: "Copper") -> (Copper?)) & ((self: Network, class: "Kiln") -> (Kiln?)) & ((self: Network, class: "Plasma") -> (Plasma?)) & ((self: Network, class: "SMG") -> (SMG?)) & ((self: Network, class: "Boiler") -> (Boiler?)) & ((self: Network, class: "Asphalt") -> (Asphalt?)) & ((self: Network, class: "HyperspaceRadar") -> (HyperspaceRadar?)) & ((self: Network, class: "NuclearWaste") -> (NuclearWaste?)) & ((self: Network, class: "Tank") -> (Tank?)) & ((self: Network, class: "Switch") -> (Switch?)) & ((self: Network, class: "Port") -> (Port?)) & ((self: Network, class: "Beacon") -> (Beacon?)) & ((self: Network, class: "Silicon") -> (Silicon?)) & ((self: Network, class: "Spotlight") -> (Spotlight?)),
	GetPartFromPort: ((self: Network, port: PortLike?, class: "Wood") -> (Wood?)) & ((self: Network, port: PortLike?, class: "DelayWire") -> (DelayWire?)) & ((self: Network, port: PortLike?, class: "Hatch") -> (Hatch?)) & ((self: Network, port: PortLike?, class: "Screen") -> (Screen?)) & ((self: Network, port: PortLike?, class: "Extractor") -> (Extractor?)) & ((self: Network, port: PortLike?, class: "DevGenerator") -> (DevGenerator?)) & ((self: Network, port: PortLike?, class: "BurstLaser") -> (BurstLaser?)) & ((self: Network, port: PortLike?, class: "Apparel") -> (Apparel?)) & ((self: Network, port: PortLike?, class: "Cooler") -> (Cooler?)) & ((self: Network, port: PortLike?, class: "CloningBay") -> (CloningBay?)) & ((self: Network, port: PortLike?, class: "Food") -> (Food?)) & ((self: Network, port: PortLike?, class: "Lead") -> (Lead?)) & ((self: Network, port: PortLike?, class: "BlastingCap") -> (BlastingCap?)) & ((self: Network, port: PortLike?, class: "HeatPipe") -> (HeatPipe?)) & ((self: Network, port: PortLike?, class: "Filter") -> (Filter?)) & ((self: Network, port: PortLike?, class: "Antenna") -> (Antenna?)) & ((self: Network, port: PortLike?, class: "Instrument") -> (Instrument?)) & ((self: Network, port: PortLike?, class: "ARController") -> (ARController?)) & ((self: Network, port: PortLike?, class: "Teleporter") -> (Teleporter?)) & ((self: Network, port: PortLike?, class: "Water") -> (Water?)) & ((self: Network, port: PortLike?, class: "Stick") -> (Stick?)) & ((self: Network, port: PortLike?, class: "Laser") -> (Laser?)) & ((self: Network, port: PortLike?, class: "DevHeatStorage") -> (DevHeatStorage?)) & ((self: Network, port: PortLike?, class: "WaterCooler") -> (WaterCooler?)) & ((self: Network, port: PortLike?, class: "Cone") -> (Cone?)) & ((self: Network, port: PortLike?, class: "ReinforcedGlass") -> (ReinforcedGlass?)) & ((self: Network, port: PortLike?, class: "BurnerGenerator") -> (BurnerGenerator?)) & ((self: Network, port: PortLike?, class: "TestStarMap") -> (TestStarMap?)) & ((self: Network, port: PortLike?, class: "DevGravityGenerator") -> (DevGravityGenerator?)) & ((self: Network, port: PortLike?, class: "Fence") -> (Fence?)) & ((self: Network, port: PortLike?, class: "Servo") -> (Servo?)) & ((self: Network, port: PortLike?, class: "DriveBox") -> (DriveBox?)) & ((self: Network, port: PortLike?, class: "FluidProjector") -> (FluidProjector?)) & ((self: Network, port: PortLike?, class: "ScubaMask") -> (ScubaMask?)) & ((self: Network, port: PortLike?, class: "Frog") -> (Frog?)) & ((self: Network, port: PortLike?, class: "MonsterMashPotion") -> (MonsterMashPotion?)) & ((self: Network, port: PortLike?, class: "TimeSensor") -> (TimeSensor?)) & ((self: Network, port: PortLike?, class: "ElectricFence") -> (ElectricFence?)) & ((self: Network, port: PortLike?, class: "ARGlasses") -> (ARGlasses?)) & ((self: Network, port: PortLike?, class: "Pump") -> (Pump?)) & ((self: Network, port: PortLike?, class: "Torch") -> (Torch?)) & ((self: Network, port: PortLike?, class: "Assembler") -> (Assembler?)) & ((self: Network, port: PortLike?, class: "Diode") -> (Diode?)) & ((self: Network, port: PortLike?, class: "Neon") -> (Neon?)) & ((self: Network, port: PortLike?, class: "Keyboard") -> (Keyboard?)) & ((self: Network, port: PortLike?, class: "Cement") -> (Cement?)) & ((self: Network, port: PortLike?, class: "StasisField") -> (StasisField?)) & ((self: Network, port: PortLike?, class: "EnergyGun") -> (EnergyGun?)) & ((self: Network, port: PortLike?, class: "LightBridge") -> (LightBridge?)) & ((self: Network, port: PortLike?, class: "Ball") -> (Ball?)) & ((self: Network, port: PortLike?, class: "ProximityButton") -> (ProximityButton?)) & ((self: Network, port: PortLike?, class: "Block") -> (Block?)) & ((self: Network, port: PortLike?, class: "WirelessButton") -> (WirelessButton?)) & ((self: Network, port: PortLike?, class: "DarkConverter") -> (DarkConverter?)) & ((self: Network, port: PortLike?, class: "Disk") -> (Disk?)) & ((self: Network, port: PortLike?, class: "LightTube") -> (LightTube?)) & ((self: Network, port: PortLike?, class: "LifeSensor") -> (LifeSensor?)) & ((self: Network, port: PortLike?, class: "SteamTurbine") -> (SteamTurbine?)) & ((self: Network, port: PortLike?, class: "PilotObject") -> (PilotObject?)) & ((self: Network, port: PortLike?, class: "Component") -> (Component?)) & ((self: Network, port: PortLike?, class: "Brick") -> (Brick?)) & ((self: Network, port: PortLike?, class: "Warhead") -> (Warhead?)) & ((self: Network, port: PortLike?, class: "AdBoard") -> (AdBoard?)) & ((self: Network, port: PortLike?, class: "Transformer") -> (Transformer?)) & ((self: Network, port: PortLike?, class: "Fireworks") -> (Fireworks?)) & ((self: Network, port: PortLike?, class: "Cleat") -> (Cleat?)) & ((self: Network, port: PortLike?, class: "Pistol") -> (Pistol?)) & ((self: Network, port: PortLike?, class: "FactionHub") -> (FactionHub?)) & ((self: Network, port: PortLike?, class: "TriggerRelay") -> (TriggerRelay?)) & ((self: Network, port: PortLike?, class: "PowerCell") -> (PowerCell?)) & ((self: Network, port: PortLike?, class: "Transistor") -> (Transistor?)) & ((self: Network, port: PortLike?, class: "Piston") -> (Piston?)) & ((self: Network, port: PortLike?, class: "Treads") -> (Treads?)) & ((self: Network, port: PortLike?, class: "Marble") -> (Marble?)) & ((self: Network, port: PortLike?, class: "CrudeWing") -> (CrudeWing?)) & ((self: Network, port: PortLike?, class: "Quartz") -> (Quartz?)) & ((self: Network, port: PortLike?, class: "SteamEngine") -> (SteamEngine?)) & ((self: Network, port: PortLike?, class: "Microcontroller") -> (Microcontroller?)) & ((self: Network, port: PortLike?, class: "Plutonium") -> (Plutonium?)) & ((self: Network, port: PortLike?, class: "HeatPump") -> (HeatPump?)) & ((self: Network, port: PortLike?, class: "Speaker") -> (Speaker?)) & ((self: Network, port: PortLike?, class: "Cloth") -> (Cloth?)) & ((self: Network, port: PortLike?, class: "Magnesium") -> (Magnesium?)) & ((self: Network, port: PortLike?, class: "SoundMuffler") -> (SoundMuffler?)) & ((self: Network, port: PortLike?, class: "Hull") -> (Hull?)) & ((self: Network, port: PortLike?, class: "VehicleSeat") -> (VehicleSeat?)) & ((self: Network, port: PortLike?, class: "RoundWedge") -> (RoundWedge?)) & ((self: Network, port: PortLike?, class: "Winch") -> (Winch?)) & ((self: Network, port: PortLike?, class: "Insulation") -> (Insulation?)) & ((self: Network, port: PortLike?, class: "RTG") -> (RTG?)) & ((self: Network, port: PortLike?, class: "Rotor") -> (Rotor?)) & ((self: Network, port: PortLike?, class: "Controller") -> (Controller?)) & ((self: Network, port: PortLike?, class: "Obelisk") -> (Obelisk?)) & ((self: Network, port: PortLike?, class: "Motor") -> (Motor?)) & ((self: Network, port: PortLike?, class: "Aluminum") -> (Aluminum?)) & ((self: Network, port: PortLike?, class: "Telescope") -> (Telescope?)) & ((self: Network, port: PortLike?, class: "BlackBox") -> (BlackBox?)) & ((self: Network, port: PortLike?, class: "Reactor") -> (Reactor?)) & ((self: Network, port: PortLike?, class: "Prosthetic") -> (Prosthetic?)) & ((self: Network, port: PortLike?, class: "RoundWedge2") -> (RoundWedge2?)) & ((self: Network, port: PortLike?, class: "Decoupler") -> (Decoupler?)) & ((self: Network, port: PortLike?, class: "Solenoid") -> (Solenoid?)) & ((self: Network, port: PortLike?, class: "DevSink") -> (DevSink?)) & ((self: Network, port: PortLike?, class: "HyperDrive") -> (HyperDrive?)) & ((self: Network, port: PortLike?, class: "Tile") -> (Tile?)) & ((self: Network, port: PortLike?, class: "TriggerSwitch") -> (TriggerSwitch?)) & ((self: Network, port: PortLike?, class: "SolarPanel") -> (SolarPanel?)) & ((self: Network, port: PortLike?, class: "Boombox") -> (Boombox?)) & ((self: Network, port: PortLike?, class: "RepairKit") -> (RepairKit?)) & ((self: Network, port: PortLike?, class: "Igniter") -> (Igniter?)) & ((self: Network, port: PortLike?, class: "FactionSpawn") -> (FactionSpawn?)) & ((self: Network, port: PortLike?, class: "DeleteSwitch") -> (DeleteSwitch?)) & ((self: Network, port: PortLike?, class: "Electromagnet") -> (Electromagnet?)) & ((self: Network, port: PortLike?, class: "SpawnPoint") -> (SpawnPoint?)) & ((self: Network, port: PortLike?, class: "Rocket") -> (Rocket?)) & ((self: Network, port: PortLike?, class: "Scrapper") -> (Scrapper?)) & ((self: Network, port: PortLike?, class: "TemperatureGate") -> (TemperatureGate?)) & ((self: Network, port: PortLike?, class: "RemoteControl") -> (RemoteControl?)) & ((self: Network, port: PortLike?, class: "Flamethrower") -> (Flamethrower?)) & ((self: Network, port: PortLike?, class: "Light") -> (Light?)) & ((self: Network, port: PortLike?, class: "Balloon") -> (Balloon?)) & ((self: Network, port: PortLike?, class: "VitalScanner") -> (VitalScanner?)) & ((self: Network, port: PortLike?, class: "BeamRifle") -> (BeamRifle?)) & ((self: Network, port: PortLike?, class: "DarkReactor") -> (DarkReactor?)) & ((self: Network, port: PortLike?, class: "Extinguisher") -> (Extinguisher?)) & ((self: Network, port: PortLike?, class: "Sail") -> (Sail?)) & ((self: Network, port: PortLike?, class: "Rail") -> (Rail?)) & ((self: Network, port: PortLike?, class: "RepairLaser") -> (RepairLaser?)) & ((self: Network, port: PortLike?, class: "Polysilicon") -> (Polysilicon?)) & ((self: Network, port: PortLike?, class: "Scanner") -> (Scanner?)) & ((self: Network, port: PortLike?, class: "CornerTetra") -> (CornerTetra?)) & ((self: Network, port: PortLike?, class: "Gear") -> (Gear?)) & ((self: Network, port: PortLike?, class: "Furnace") -> (Furnace?)) & ((self: Network, port: PortLike?, class: "Cannon") -> (Cannon?)) & ((self: Network, port: PortLike?, class: "DevSource") -> (DevSource?)) & ((self: Network, port: PortLike?, class: "CornerRoundWedge2") -> (CornerRoundWedge2?)) & ((self: Network, port: PortLike?, class: "Tetrahedron") -> (Tetrahedron?)) & ((self: Network, port: PortLike?, class: "Pipe") -> (Pipe?)) & ((self: Network, port: PortLike?, class: "EthernetCable") -> (EthernetCable?)) & ((self: Network, port: PortLike?, class: "AutomaticLaser") -> (AutomaticLaser?)) & ((self: Network, port: PortLike?, class: "Faucet") -> (Faucet?)) & ((self: Network, port: PortLike?, class: "NightVisionGoggles") -> (NightVisionGoggles?)) & ((self: Network, port: PortLike?, class: "Thruster") -> (Thruster?)) & ((self: Network, port: PortLike?, class: "Billboardium") -> (Billboardium?)) & ((self: Network, port: PortLike?, class: "CombustionTurbine") -> (CombustionTurbine?)) & ((self: Network, port: PortLike?, class: "TouchScreen") -> (TouchScreen?)) & ((self: Network, port: PortLike?, class: "GeigerCounter") -> (GeigerCounter?)) & ((self: Network, port: PortLike?, class: "HeatCell") -> (HeatCell?)) & ((self: Network, port: PortLike?, class: "Camera") -> (Camera?)) & ((self: Network, port: PortLike?, class: "Gasoline") -> (Gasoline?)) & ((self: Network, port: PortLike?, class: "SubspaceTripmine") -> (SubspaceTripmine?)) & ((self: Network, port: PortLike?, class: "Modem") -> (Modem?)) & ((self: Network, port: PortLike?, class: "TemperatureSensor") -> (TemperatureSensor?)) & ((self: Network, port: PortLike?, class: "StanSword") -> (StanSword?)) & ((self: Network, port: PortLike?, class: "Engine") -> (Engine?)) & ((self: Network, port: PortLike?, class: "ImpactPlate") -> (ImpactPlate?)) & ((self: Network, port: PortLike?, class: "Radar") -> (Radar?)) & ((self: Network, port: PortLike?, class: "FireWood") -> (FireWood?)) & ((self: Network, port: PortLike?, class: "AdminTool") -> (AdminTool?)) & ((self: Network, port: PortLike?, class: "RepairPlate") -> (RepairPlate?)) & ((self: Network, port: PortLike?, class: "Chute") -> (Chute?)) & ((self: Network, port: PortLike?, class: "MiningLaser") -> (MiningLaser?)) & ((self: Network, port: PortLike?, class: "Sulfur") -> (Sulfur?)) & ((self: Network, port: PortLike?, class: "Stone") -> (Stone?)) & ((self: Network, port: PortLike?, class: "TintedGlass") -> (TintedGlass?)) & ((self: Network, port: PortLike?, class: "Steam") -> (Steam?)) & ((self: Network, port: PortLike?, class: "IonRocket") -> (IonRocket?)) & ((self: Network, port: PortLike?, class: "Stanlium") -> (Stanlium?)) & ((self: Network, port: PortLike?, class: "Snow") -> (Snow?)) & ((self: Network, port: PortLike?, class: "BallastTank") -> (BallastTank?)) & ((self: Network, port: PortLike?, class: "Petroleum") -> (Petroleum?)) & ((self: Network, port: PortLike?, class: "Claymore") -> (Claymore?)) & ((self: Network, port: PortLike?, class: "Sand") -> (Sand?)) & ((self: Network, port: PortLike?, class: "Ruby") -> (Ruby?)) & ((self: Network, port: PortLike?, class: "CrossBow") -> (CrossBow?)) & ((self: Network, port: PortLike?, class: "Router") -> (Router?)) & ((self: Network, port: PortLike?, class: "Explosive") -> (Explosive?)) & ((self: Network, port: PortLike?, class: "Generator") -> (Generator?)) & ((self: Network, port: PortLike?, class: "ObjectDetector") -> (ObjectDetector?)) & ((self: Network, port: PortLike?, class: "Ice") -> (Ice?)) & ((self: Network, port: PortLike?, class: "StarMap") -> (StarMap?)) & ((self: Network, port: PortLike?, class: "Perfectium") -> (Perfectium?)) & ((self: Network, port: PortLike?, class: "TractorBeam") -> (TractorBeam?)) & ((self: Network, port: PortLike?, class: "Obamium") -> (Obamium?)) & ((self: Network, port: PortLike?, class: "Turbofan") -> (Turbofan?)) & ((self: Network, port: PortLike?, class: "NitrogenOxide") -> (NitrogenOxide?)) & ((self: Network, port: PortLike?, class: "Battery") -> (Battery?)) & ((self: Network, port: PortLike?, class: "Neutronium") -> (Neutronium?)) & ((self: Network, port: PortLike?, class: "Coupler") -> (Coupler?)) & ((self: Network, port: PortLike?, class: "Iron") -> (Iron?)) & ((self: Network, port: PortLike?, class: "Jade") -> (Jade?)) & ((self: Network, port: PortLike?, class: "DevTeleporter") -> (DevTeleporter?)) & ((self: Network, port: PortLike?, class: "Lava") -> (Lava?)) & ((self: Network, port: PortLike?, class: "Mandrillium") -> (Mandrillium?)) & ((self: Network, port: PortLike?, class: "Rubber") -> (Rubber?)) & ((self: Network, port: PortLike?, class: "Helium") -> (Helium?)) & ((self: Network, port: PortLike?, class: "Heatshield") -> (Heatshield?)) & ((self: Network, port: PortLike?, class: "HeatSink") -> (HeatSink?)) & ((self: Network, port: PortLike?, class: "Grass") -> (Grass?)) & ((self: Network, port: PortLike?, class: "PlutoniumCore") -> (PlutoniumCore?)) & ((self: Network, port: PortLike?, class: "Seat") -> (Seat?)) & ((self: Network, port: PortLike?, class: "ImpulseCannon") -> (ImpulseCannon?)) & ((self: Network, port: PortLike?, class: "Glass") -> (Glass?)) & ((self: Network, port: PortLike?, class: "Part100k") -> (Part100k?)) & ((self: Network, port: PortLike?, class: "Wedge") -> (Wedge?)) & ((self: Network, port: PortLike?, class: "Valve") -> (Valve?)) & ((self: Network, port: PortLike?, class: "ExoticMatter") -> (ExoticMatter?)) & ((self: Network, port: PortLike?, class: "CornerRoundWedge") -> (CornerRoundWedge?)) & ((self: Network, port: PortLike?, class: "Diamond") -> (Diamond?)) & ((self: Network, port: PortLike?, class: "Freezer") -> (Freezer?)) & ((self: Network, port: PortLike?, class: "DarkMatter") -> (DarkMatter?)) & ((self: Network, port: PortLike?, class: "Melter") -> (Melter?)) & ((self: Network, port: PortLike?, class: "Coal") -> (Coal?)) & ((self: Network, port: PortLike?, class: "ChemicalSynthiser") -> (ChemicalSynthiser?)) & ((self: Network, port: PortLike?, class: "Beryllium") -> (Beryllium?)) & ((self: Network, port: PortLike?, class: "MustardGas") -> (MustardGas?)) & ((self: Network, port: PortLike?, class: "AlienCore") -> (AlienCore?)) & ((self: Network, port: PortLike?, class: "Railgun") -> (Railgun?)) & ((self: Network, port: PortLike?, class: "Dispenser") -> (Dispenser?)) & ((self: Network, port: PortLike?, class: "RustedMetal") -> (RustedMetal?)) & ((self: Network, port: PortLike?, class: "ZapWire") -> (ZapWire?)) & ((self: Network, port: PortLike?, class: "Wing") -> (Wing?)) & ((self: Network, port: PortLike?, class: "Wire") -> (Wire?)) & ((self: Network, port: PortLike?, class: "Flint") -> (Flint?)) & ((self: Network, port: PortLike?, class: "Anchor") -> (Anchor?)) & ((self: Network, port: PortLike?, class: "Rifle") -> (Rifle?)) & ((self: Network, port: PortLike?, class: "HalfSphere") -> (HalfSphere?)) & ((self: Network, port: PortLike?, class: "Tire") -> (Tire?)) & ((self: Network, port: PortLike?, class: "Artillery") -> (Artillery?)) & ((self: Network, port: PortLike?, class: "StudAligner") -> (StudAligner?)) & ((self: Network, port: PortLike?, class: "Truss") -> (Truss?)) & ((self: Network, port: PortLike?, class: "TriggerWire") -> (TriggerWire?)) & ((self: Network, port: PortLike?, class: "HeatValve") -> (HeatValve?)) & ((self: Network, port: PortLike?, class: "Spheroid") -> (Spheroid?)) & ((self: Network, port: PortLike?, class: "AirSupply") -> (AirSupply?)) & ((self: Network, port: PortLike?, class: "Constructor") -> (Constructor?)) & ((self: Network, port: PortLike?, class: "tinnitus") -> (tinnitus?)) & ((self: Network, port: PortLike?, class: "Primer") -> (Primer?)) & ((self: Network, port: PortLike?, class: "SmoothGlass") -> (SmoothGlass?)) & ((self: Network, port: PortLike?, class: "EnergyBomb") -> (EnergyBomb?)) & ((self: Network, port: PortLike?, class: "Hydrogen") -> (Hydrogen?)) & ((self: Network, port: PortLike?, class: "Rice") -> (Rice?)) & ((self: Network, port: PortLike?, class: "RegionCloaker") -> (RegionCloaker?)) & ((self: Network, port: PortLike?, class: "Pulverizer") -> (Pulverizer?)) & ((self: Network, port: PortLike?, class: "SmoothReinforcedGlass") -> (SmoothReinforcedGlass?)) & ((self: Network, port: PortLike?, class: "EnergyShield") -> (EnergyShield?)) & ((self: Network, port: PortLike?, class: "PlasmaCannon") -> (PlasmaCannon?)) & ((self: Network, port: PortLike?, class: "TouchSensor") -> (TouchSensor?)) & ((self: Network, port: PortLike?, class: "PDWX44") -> (PDWX44?)) & ((self: Network, port: PortLike?, class: "StorageSensor") -> (StorageSensor?)) & ((self: Network, port: PortLike?, class: "Beaker") -> (Beaker?)) & ((self: Network, port: PortLike?, class: "Lantern") -> (Lantern?)) & ((self: Network, port: PortLike?, class: "Katana") -> (Katana?)) & ((self: Network, port: PortLike?, class: "Aerogel") -> (Aerogel?)) & ((self: Network, port: PortLike?, class: "Microphone") -> (Microphone?)) & ((self: Network, port: PortLike?, class: "Gold") -> (Gold?)) & ((self: Network, port: PortLike?, class: "Hotdog") -> (Hotdog?)) & ((self: Network, port: PortLike?, class: "Heater") -> (Heater?)) & ((self: Network, port: PortLike?, class: "VintagePlasmaPistol") -> (VintagePlasmaPistol?)) & ((self: Network, port: PortLike?, class: "Gun") -> (Gun?)) & ((self: Network, port: PortLike?, class: "Blade") -> (Blade?)) & ((self: Network, port: PortLike?, class: "Sorter") -> (Sorter?)) & ((self: Network, port: PortLike?, class: "Goo") -> (Goo?)) & ((self: Network, port: PortLike?, class: "Framewire") -> (Framewire?)) & ((self: Network, port: PortLike?, class: "Hydroponic") -> (Hydroponic?)) & ((self: Network, port: PortLike?, class: "FloatDevice") -> (FloatDevice?)) & ((self: Network, port: PortLike?, class: "GravityGenerator") -> (GravityGenerator?)) & ((self: Network, port: PortLike?, class: "Flashlight") -> (Flashlight?)) & ((self: Network, port: PortLike?, class: "Gyro") -> (Gyro?)) & ((self: Network, port: PortLike?, class: "WindTurbine") -> (WindTurbine?)) & ((self: Network, port: PortLike?, class: "EnergySword") -> (EnergySword?)) & ((self: Network, port: PortLike?, class: "Cylinder") -> (Cylinder?)) & ((self: Network, port: PortLike?, class: "Sign") -> (Sign?)) & ((self: Network, port: PortLike?, class: "Radiator") -> (Radiator?)) & ((self: Network, port: PortLike?, class: "ConveyorBelt") -> (ConveyorBelt?)) & ((self: Network, port: PortLike?, class: "Plastic") -> (Plastic?)) & ((self: Network, port: PortLike?, class: "Egg") -> (Egg?)) & ((self: Network, port: PortLike?, class: "Shotgun") -> (Shotgun?)) & ((self: Network, port: PortLike?, class: "Titanium") -> (Titanium?)) & ((self: Network, port: PortLike?, class: "Hologram") -> (Hologram?)) & ((self: Network, port: PortLike?, class: "Button") -> (Button?)) & ((self: Network, port: PortLike?, class: "FourthOfJuly") -> (FourthOfJuly?)) & ((self: Network, port: PortLike?, class: "Relay") -> (Relay?)) & ((self: Network, port: PortLike?, class: "Transporter") -> (Transporter?)) & ((self: Network, port: PortLike?, class: "Oil") -> (Oil?)) & ((self: Network, port: PortLike?, class: "DevBattery") -> (DevBattery?)) & ((self: Network, port: PortLike?, class: "Uranium") -> (Uranium?)) & ((self: Network, port: PortLike?, class: "Door") -> (Door?)) & ((self: Network, port: PortLike?, class: "Handle") -> (Handle?)) & ((self: Network, port: PortLike?, class: "SolarScoop") -> (SolarScoop?)) & ((self: Network, port: PortLike?, class: "Refinery") -> (Refinery?)) & ((self: Network, port: PortLike?, class: "Bin") -> (Bin?)) & ((self: Network, port: PortLike?, class: "Propeller") -> (Propeller?)) & ((self: Network, port: PortLike?, class: "CornerWedge") -> (CornerWedge?)) & ((self: Network, port: PortLike?, class: "Copper") -> (Copper?)) & ((self: Network, port: PortLike?, class: "Kiln") -> (Kiln?)) & ((self: Network, port: PortLike?, class: "Plasma") -> (Plasma?)) & ((self: Network, port: PortLike?, class: "SMG") -> (SMG?)) & ((self: Network, port: PortLike?, class: "Boiler") -> (Boiler?)) & ((self: Network, port: PortLike?, class: "Asphalt") -> (Asphalt?)) & ((self: Network, port: PortLike?, class: "HyperspaceRadar") -> (HyperspaceRadar?)) & ((self: Network, port: PortLike?, class: "NuclearWaste") -> (NuclearWaste?)) & ((self: Network, port: PortLike?, class: "Tank") -> (Tank?)) & ((self: Network, port: PortLike?, class: "Switch") -> (Switch?)) & ((self: Network, port: PortLike?, class: "Port") -> (Port?)) & ((self: Network, port: PortLike?, class: "Beacon") -> (Beacon?)) & ((self: Network, port: PortLike?, class: "Silicon") -> (Silicon?)) & ((self: Network, port: PortLike?, class: "Spotlight") -> (Spotlight?)),
	GetSubnet: (self: Network, port: PortLike?) -> (Network),
	GetParts: ((self: Network, class: "Wood") -> ({ Wood })) & ((self: Network, class: "DelayWire") -> ({ DelayWire })) & ((self: Network, class: "Hatch") -> ({ Hatch })) & ((self: Network, class: "Screen") -> ({ Screen })) & ((self: Network, class: "Extractor") -> ({ Extractor })) & ((self: Network, class: "DevGenerator") -> ({ DevGenerator })) & ((self: Network, class: "BurstLaser") -> ({ BurstLaser })) & ((self: Network, class: "Apparel") -> ({ Apparel })) & ((self: Network, class: "Cooler") -> ({ Cooler })) & ((self: Network, class: "CloningBay") -> ({ CloningBay })) & ((self: Network, class: "Food") -> ({ Food })) & ((self: Network, class: "Lead") -> ({ Lead })) & ((self: Network, class: "BlastingCap") -> ({ BlastingCap })) & ((self: Network, class: "HeatPipe") -> ({ HeatPipe })) & ((self: Network, class: "Filter") -> ({ Filter })) & ((self: Network, class: "Antenna") -> ({ Antenna })) & ((self: Network, class: "Instrument") -> ({ Instrument })) & ((self: Network, class: "ARController") -> ({ ARController })) & ((self: Network, class: "Teleporter") -> ({ Teleporter })) & ((self: Network, class: "Water") -> ({ Water })) & ((self: Network, class: "Stick") -> ({ Stick })) & ((self: Network, class: "Laser") -> ({ Laser })) & ((self: Network, class: "DevHeatStorage") -> ({ DevHeatStorage })) & ((self: Network, class: "WaterCooler") -> ({ WaterCooler })) & ((self: Network, class: "Cone") -> ({ Cone })) & ((self: Network, class: "ReinforcedGlass") -> ({ ReinforcedGlass })) & ((self: Network, class: "BurnerGenerator") -> ({ BurnerGenerator })) & ((self: Network, class: "TestStarMap") -> ({ TestStarMap })) & ((self: Network, class: "DevGravityGenerator") -> ({ DevGravityGenerator })) & ((self: Network, class: "Fence") -> ({ Fence })) & ((self: Network, class: "Servo") -> ({ Servo })) & ((self: Network, class: "DriveBox") -> ({ DriveBox })) & ((self: Network, class: "FluidProjector") -> ({ FluidProjector })) & ((self: Network, class: "ScubaMask") -> ({ ScubaMask })) & ((self: Network, class: "Frog") -> ({ Frog })) & ((self: Network, class: "MonsterMashPotion") -> ({ MonsterMashPotion })) & ((self: Network, class: "TimeSensor") -> ({ TimeSensor })) & ((self: Network, class: "ElectricFence") -> ({ ElectricFence })) & ((self: Network, class: "ARGlasses") -> ({ ARGlasses })) & ((self: Network, class: "Pump") -> ({ Pump })) & ((self: Network, class: "Torch") -> ({ Torch })) & ((self: Network, class: "Assembler") -> ({ Assembler })) & ((self: Network, class: "Diode") -> ({ Diode })) & ((self: Network, class: "Neon") -> ({ Neon })) & ((self: Network, class: "Keyboard") -> ({ Keyboard })) & ((self: Network, class: "Cement") -> ({ Cement })) & ((self: Network, class: "StasisField") -> ({ StasisField })) & ((self: Network, class: "EnergyGun") -> ({ EnergyGun })) & ((self: Network, class: "LightBridge") -> ({ LightBridge })) & ((self: Network, class: "Ball") -> ({ Ball })) & ((self: Network, class: "ProximityButton") -> ({ ProximityButton })) & ((self: Network, class: "Block") -> ({ Block })) & ((self: Network, class: "WirelessButton") -> ({ WirelessButton })) & ((self: Network, class: "DarkConverter") -> ({ DarkConverter })) & ((self: Network, class: "Disk") -> ({ Disk })) & ((self: Network, class: "LightTube") -> ({ LightTube })) & ((self: Network, class: "LifeSensor") -> ({ LifeSensor })) & ((self: Network, class: "SteamTurbine") -> ({ SteamTurbine })) & ((self: Network, class: "PilotObject") -> ({ PilotObject })) & ((self: Network, class: "Component") -> ({ Component })) & ((self: Network, class: "Brick") -> ({ Brick })) & ((self: Network, class: "Warhead") -> ({ Warhead })) & ((self: Network, class: "AdBoard") -> ({ AdBoard })) & ((self: Network, class: "Transformer") -> ({ Transformer })) & ((self: Network, class: "Fireworks") -> ({ Fireworks })) & ((self: Network, class: "Cleat") -> ({ Cleat })) & ((self: Network, class: "Pistol") -> ({ Pistol })) & ((self: Network, class: "FactionHub") -> ({ FactionHub })) & ((self: Network, class: "TriggerRelay") -> ({ TriggerRelay })) & ((self: Network, class: "PowerCell") -> ({ PowerCell })) & ((self: Network, class: "Transistor") -> ({ Transistor })) & ((self: Network, class: "Piston") -> ({ Piston })) & ((self: Network, class: "Treads") -> ({ Treads })) & ((self: Network, class: "Marble") -> ({ Marble })) & ((self: Network, class: "CrudeWing") -> ({ CrudeWing })) & ((self: Network, class: "Quartz") -> ({ Quartz })) & ((self: Network, class: "SteamEngine") -> ({ SteamEngine })) & ((self: Network, class: "Microcontroller") -> ({ Microcontroller })) & ((self: Network, class: "Plutonium") -> ({ Plutonium })) & ((self: Network, class: "HeatPump") -> ({ HeatPump })) & ((self: Network, class: "Speaker") -> ({ Speaker })) & ((self: Network, class: "Cloth") -> ({ Cloth })) & ((self: Network, class: "Magnesium") -> ({ Magnesium })) & ((self: Network, class: "SoundMuffler") -> ({ SoundMuffler })) & ((self: Network, class: "Hull") -> ({ Hull })) & ((self: Network, class: "VehicleSeat") -> ({ VehicleSeat })) & ((self: Network, class: "RoundWedge") -> ({ RoundWedge })) & ((self: Network, class: "Winch") -> ({ Winch })) & ((self: Network, class: "Insulation") -> ({ Insulation })) & ((self: Network, class: "RTG") -> ({ RTG })) & ((self: Network, class: "Rotor") -> ({ Rotor })) & ((self: Network, class: "Controller") -> ({ Controller })) & ((self: Network, class: "Obelisk") -> ({ Obelisk })) & ((self: Network, class: "Motor") -> ({ Motor })) & ((self: Network, class: "Aluminum") -> ({ Aluminum })) & ((self: Network, class: "Telescope") -> ({ Telescope })) & ((self: Network, class: "BlackBox") -> ({ BlackBox })) & ((self: Network, class: "Reactor") -> ({ Reactor })) & ((self: Network, class: "Prosthetic") -> ({ Prosthetic })) & ((self: Network, class: "RoundWedge2") -> ({ RoundWedge2 })) & ((self: Network, class: "Decoupler") -> ({ Decoupler })) & ((self: Network, class: "Solenoid") -> ({ Solenoid })) & ((self: Network, class: "DevSink") -> ({ DevSink })) & ((self: Network, class: "HyperDrive") -> ({ HyperDrive })) & ((self: Network, class: "Tile") -> ({ Tile })) & ((self: Network, class: "TriggerSwitch") -> ({ TriggerSwitch })) & ((self: Network, class: "SolarPanel") -> ({ SolarPanel })) & ((self: Network, class: "Boombox") -> ({ Boombox })) & ((self: Network, class: "RepairKit") -> ({ RepairKit })) & ((self: Network, class: "Igniter") -> ({ Igniter })) & ((self: Network, class: "FactionSpawn") -> ({ FactionSpawn })) & ((self: Network, class: "DeleteSwitch") -> ({ DeleteSwitch })) & ((self: Network, class: "Electromagnet") -> ({ Electromagnet })) & ((self: Network, class: "SpawnPoint") -> ({ SpawnPoint })) & ((self: Network, class: "Rocket") -> ({ Rocket })) & ((self: Network, class: "Scrapper") -> ({ Scrapper })) & ((self: Network, class: "TemperatureGate") -> ({ TemperatureGate })) & ((self: Network, class: "RemoteControl") -> ({ RemoteControl })) & ((self: Network, class: "Flamethrower") -> ({ Flamethrower })) & ((self: Network, class: "Light") -> ({ Light })) & ((self: Network, class: "Balloon") -> ({ Balloon })) & ((self: Network, class: "VitalScanner") -> ({ VitalScanner })) & ((self: Network, class: "BeamRifle") -> ({ BeamRifle })) & ((self: Network, class: "DarkReactor") -> ({ DarkReactor })) & ((self: Network, class: "Extinguisher") -> ({ Extinguisher })) & ((self: Network, class: "Sail") -> ({ Sail })) & ((self: Network, class: "Rail") -> ({ Rail })) & ((self: Network, class: "RepairLaser") -> ({ RepairLaser })) & ((self: Network, class: "Polysilicon") -> ({ Polysilicon })) & ((self: Network, class: "Scanner") -> ({ Scanner })) & ((self: Network, class: "CornerTetra") -> ({ CornerTetra })) & ((self: Network, class: "Gear") -> ({ Gear })) & ((self: Network, class: "Furnace") -> ({ Furnace })) & ((self: Network, class: "Cannon") -> ({ Cannon })) & ((self: Network, class: "DevSource") -> ({ DevSource })) & ((self: Network, class: "CornerRoundWedge2") -> ({ CornerRoundWedge2 })) & ((self: Network, class: "Tetrahedron") -> ({ Tetrahedron })) & ((self: Network, class: "Pipe") -> ({ Pipe })) & ((self: Network, class: "EthernetCable") -> ({ EthernetCable })) & ((self: Network, class: "AutomaticLaser") -> ({ AutomaticLaser })) & ((self: Network, class: "Faucet") -> ({ Faucet })) & ((self: Network, class: "NightVisionGoggles") -> ({ NightVisionGoggles })) & ((self: Network, class: "Thruster") -> ({ Thruster })) & ((self: Network, class: "Billboardium") -> ({ Billboardium })) & ((self: Network, class: "CombustionTurbine") -> ({ CombustionTurbine })) & ((self: Network, class: "TouchScreen") -> ({ TouchScreen })) & ((self: Network, class: "GeigerCounter") -> ({ GeigerCounter })) & ((self: Network, class: "HeatCell") -> ({ HeatCell })) & ((self: Network, class: "Camera") -> ({ Camera })) & ((self: Network, class: "Gasoline") -> ({ Gasoline })) & ((self: Network, class: "SubspaceTripmine") -> ({ SubspaceTripmine })) & ((self: Network, class: "Modem") -> ({ Modem })) & ((self: Network, class: "TemperatureSensor") -> ({ TemperatureSensor })) & ((self: Network, class: "StanSword") -> ({ StanSword })) & ((self: Network, class: "Engine") -> ({ Engine })) & ((self: Network, class: "ImpactPlate") -> ({ ImpactPlate })) & ((self: Network, class: "Radar") -> ({ Radar })) & ((self: Network, class: "FireWood") -> ({ FireWood })) & ((self: Network, class: "AdminTool") -> ({ AdminTool })) & ((self: Network, class: "RepairPlate") -> ({ RepairPlate })) & ((self: Network, class: "Chute") -> ({ Chute })) & ((self: Network, class: "MiningLaser") -> ({ MiningLaser })) & ((self: Network, class: "Sulfur") -> ({ Sulfur })) & ((self: Network, class: "Stone") -> ({ Stone })) & ((self: Network, class: "TintedGlass") -> ({ TintedGlass })) & ((self: Network, class: "Steam") -> ({ Steam })) & ((self: Network, class: "IonRocket") -> ({ IonRocket })) & ((self: Network, class: "Stanlium") -> ({ Stanlium })) & ((self: Network, class: "Snow") -> ({ Snow })) & ((self: Network, class: "BallastTank") -> ({ BallastTank })) & ((self: Network, class: "Petroleum") -> ({ Petroleum })) & ((self: Network, class: "Claymore") -> ({ Claymore })) & ((self: Network, class: "Sand") -> ({ Sand })) & ((self: Network, class: "Ruby") -> ({ Ruby })) & ((self: Network, class: "CrossBow") -> ({ CrossBow })) & ((self: Network, class: "Router") -> ({ Router })) & ((self: Network, class: "Explosive") -> ({ Explosive })) & ((self: Network, class: "Generator") -> ({ Generator })) & ((self: Network, class: "ObjectDetector") -> ({ ObjectDetector })) & ((self: Network, class: "Ice") -> ({ Ice })) & ((self: Network, class: "StarMap") -> ({ StarMap })) & ((self: Network, class: "Perfectium") -> ({ Perfectium })) & ((self: Network, class: "TractorBeam") -> ({ TractorBeam })) & ((self: Network, class: "Obamium") -> ({ Obamium })) & ((self: Network, class: "Turbofan") -> ({ Turbofan })) & ((self: Network, class: "NitrogenOxide") -> ({ NitrogenOxide })) & ((self: Network, class: "Battery") -> ({ Battery })) & ((self: Network, class: "Neutronium") -> ({ Neutronium })) & ((self: Network, class: "Coupler") -> ({ Coupler })) & ((self: Network, class: "Iron") -> ({ Iron })) & ((self: Network, class: "Jade") -> ({ Jade })) & ((self: Network, class: "DevTeleporter") -> ({ DevTeleporter })) & ((self: Network, class: "Lava") -> ({ Lava })) & ((self: Network, class: "Mandrillium") -> ({ Mandrillium })) & ((self: Network, class: "Rubber") -> ({ Rubber })) & ((self: Network, class: "Helium") -> ({ Helium })) & ((self: Network, class: "Heatshield") -> ({ Heatshield })) & ((self: Network, class: "HeatSink") -> ({ HeatSink })) & ((self: Network, class: "Grass") -> ({ Grass })) & ((self: Network, class: "PlutoniumCore") -> ({ PlutoniumCore })) & ((self: Network, class: "Seat") -> ({ Seat })) & ((self: Network, class: "ImpulseCannon") -> ({ ImpulseCannon })) & ((self: Network, class: "Glass") -> ({ Glass })) & ((self: Network, class: "Part100k") -> ({ Part100k })) & ((self: Network, class: "Wedge") -> ({ Wedge })) & ((self: Network, class: "Valve") -> ({ Valve })) & ((self: Network, class: "ExoticMatter") -> ({ ExoticMatter })) & ((self: Network, class: "CornerRoundWedge") -> ({ CornerRoundWedge })) & ((self: Network, class: "Diamond") -> ({ Diamond })) & ((self: Network, class: "Freezer") -> ({ Freezer })) & ((self: Network, class: "DarkMatter") -> ({ DarkMatter })) & ((self: Network, class: "Melter") -> ({ Melter })) & ((self: Network, class: "Coal") -> ({ Coal })) & ((self: Network, class: "ChemicalSynthiser") -> ({ ChemicalSynthiser })) & ((self: Network, class: "Beryllium") -> ({ Beryllium })) & ((self: Network, class: "MustardGas") -> ({ MustardGas })) & ((self: Network, class: "AlienCore") -> ({ AlienCore })) & ((self: Network, class: "Railgun") -> ({ Railgun })) & ((self: Network, class: "Dispenser") -> ({ Dispenser })) & ((self: Network, class: "RustedMetal") -> ({ RustedMetal })) & ((self: Network, class: "ZapWire") -> ({ ZapWire })) & ((self: Network, class: "Wing") -> ({ Wing })) & ((self: Network, class: "Wire") -> ({ Wire })) & ((self: Network, class: "Flint") -> ({ Flint })) & ((self: Network, class: "Anchor") -> ({ Anchor })) & ((self: Network, class: "Rifle") -> ({ Rifle })) & ((self: Network, class: "HalfSphere") -> ({ HalfSphere })) & ((self: Network, class: "Tire") -> ({ Tire })) & ((self: Network, class: "Artillery") -> ({ Artillery })) & ((self: Network, class: "StudAligner") -> ({ StudAligner })) & ((self: Network, class: "Truss") -> ({ Truss })) & ((self: Network, class: "TriggerWire") -> ({ TriggerWire })) & ((self: Network, class: "HeatValve") -> ({ HeatValve })) & ((self: Network, class: "Spheroid") -> ({ Spheroid })) & ((self: Network, class: "AirSupply") -> ({ AirSupply })) & ((self: Network, class: "Constructor") -> ({ Constructor })) & ((self: Network, class: "tinnitus") -> ({ tinnitus })) & ((self: Network, class: "Primer") -> ({ Primer })) & ((self: Network, class: "SmoothGlass") -> ({ SmoothGlass })) & ((self: Network, class: "EnergyBomb") -> ({ EnergyBomb })) & ((self: Network, class: "Hydrogen") -> ({ Hydrogen })) & ((self: Network, class: "Rice") -> ({ Rice })) & ((self: Network, class: "RegionCloaker") -> ({ RegionCloaker })) & ((self: Network, class: "Pulverizer") -> ({ Pulverizer })) & ((self: Network, class: "SmoothReinforcedGlass") -> ({ SmoothReinforcedGlass })) & ((self: Network, class: "EnergyShield") -> ({ EnergyShield })) & ((self: Network, class: "PlasmaCannon") -> ({ PlasmaCannon })) & ((self: Network, class: "TouchSensor") -> ({ TouchSensor })) & ((self: Network, class: "PDWX44") -> ({ PDWX44 })) & ((self: Network, class: "StorageSensor") -> ({ StorageSensor })) & ((self: Network, class: "Beaker") -> ({ Beaker })) & ((self: Network, class: "Lantern") -> ({ Lantern })) & ((self: Network, class: "Katana") -> ({ Katana })) & ((self: Network, class: "Aerogel") -> ({ Aerogel })) & ((self: Network, class: "Microphone") -> ({ Microphone })) & ((self: Network, class: "Gold") -> ({ Gold })) & ((self: Network, class: "Hotdog") -> ({ Hotdog })) & ((self: Network, class: "Heater") -> ({ Heater })) & ((self: Network, class: "VintagePlasmaPistol") -> ({ VintagePlasmaPistol })) & ((self: Network, class: "Gun") -> ({ Gun })) & ((self: Network, class: "Blade") -> ({ Blade })) & ((self: Network, class: "Sorter") -> ({ Sorter })) & ((self: Network, class: "Goo") -> ({ Goo })) & ((self: Network, class: "Framewire") -> ({ Framewire })) & ((self: Network, class: "Hydroponic") -> ({ Hydroponic })) & ((self: Network, class: "FloatDevice") -> ({ FloatDevice })) & ((self: Network, class: "GravityGenerator") -> ({ GravityGenerator })) & ((self: Network, class: "Flashlight") -> ({ Flashlight })) & ((self: Network, class: "Gyro") -> ({ Gyro })) & ((self: Network, class: "WindTurbine") -> ({ WindTurbine })) & ((self: Network, class: "EnergySword") -> ({ EnergySword })) & ((self: Network, class: "Cylinder") -> ({ Cylinder })) & ((self: Network, class: "Sign") -> ({ Sign })) & ((self: Network, class: "Radiator") -> ({ Radiator })) & ((self: Network, class: "ConveyorBelt") -> ({ ConveyorBelt })) & ((self: Network, class: "Plastic") -> ({ Plastic })) & ((self: Network, class: "Egg") -> ({ Egg })) & ((self: Network, class: "Shotgun") -> ({ Shotgun })) & ((self: Network, class: "Titanium") -> ({ Titanium })) & ((self: Network, class: "Hologram") -> ({ Hologram })) & ((self: Network, class: "Button") -> ({ Button })) & ((self: Network, class: "FourthOfJuly") -> ({ FourthOfJuly })) & ((self: Network, class: "Relay") -> ({ Relay })) & ((self: Network, class: "Transporter") -> ({ Transporter })) & ((self: Network, class: "Oil") -> ({ Oil })) & ((self: Network, class: "DevBattery") -> ({ DevBattery })) & ((self: Network, class: "Uranium") -> ({ Uranium })) & ((self: Network, class: "Door") -> ({ Door })) & ((self: Network, class: "Handle") -> ({ Handle })) & ((self: Network, class: "SolarScoop") -> ({ SolarScoop })) & ((self: Network, class: "Refinery") -> ({ Refinery })) & ((self: Network, class: "Bin") -> ({ Bin })) & ((self: Network, class: "Propeller") -> ({ Propeller })) & ((self: Network, class: "CornerWedge") -> ({ CornerWedge })) & ((self: Network, class: "Copper") -> ({ Copper })) & ((self: Network, class: "Kiln") -> ({ Kiln })) & ((self: Network, class: "Plasma") -> ({ Plasma })) & ((self: Network, class: "SMG") -> ({ SMG })) & ((self: Network, class: "Boiler") -> ({ Boiler })) & ((self: Network, class: "Asphalt") -> ({ Asphalt })) & ((self: Network, class: "HyperspaceRadar") -> ({ HyperspaceRadar })) & ((self: Network, class: "NuclearWaste") -> ({ NuclearWaste })) & ((self: Network, class: "Tank") -> ({ Tank })) & ((self: Network, class: "Switch") -> ({ Switch })) & ((self: Network, class: "Port") -> ({ Port })) & ((self: Network, class: "Beacon") -> ({ Beacon })) & ((self: Network, class: "Silicon") -> ({ Silicon })) & ((self: Network, class: "Spotlight") -> ({ Spotlight })),
	GetHistory: (self: Network) -> ({
		[PilotObject]: true,
	}),
	GetPorts: (self: Network, port: PortLike?) -> ({ Port }),
	__index: Network,
	new: (rootParts: { PilotObject }, parentNetwork: Network?) -> (Network),
}
export type Range = string
export type ARCamera = {
	FieldOfViewMode: Enum.FieldOfViewMode,
	FieldOfView: number,
	CFrame: CFrame,
	NearPlaneZ: number,
	ViewportSizeUI: Vector2,
	RenderCFrame: CFrame,
	CameraType: Enum.CameraType,
	Focus: CFrame,
	HeadScale: number,
	ViewportSize: Vector2,
	MaxAxisFieldOfView: number,
	DiagonalFieldOfView: number,
}
export type ARCursor = {
	WorldPosition: Vector3,
	Origin: Vector3,
	UserId: number,
	Target: PilotObject,
	Pressed: boolean,
	UnitRay: Ray,
	Camera: ARCamera,
	ScreenPosition: Vector2,
	UserCFrames: {
		RightHand: CFrame,
		LeftHand: CFrame,
		Head: CFrame,
	},
	Y: number,
	Player: string,
	VirtualWorldPosition: Vector3,
	Hit: CFrame,
	X: number,
	VirtualTarget: BasePart,
	MouseDelta: Vector2,
	UserInput: ARInput,
}
type ARInput = {
	Keyboard: { InputObject },
	VREnabled: boolean,
	Mouse: { InputObject },
	GamepadEnabled: boolean,
	LastInputType: Enum.UserInputType,
	Gamepad: {
		[Enum.UserInputType]: { InputObject },
	},
	KeyboardEnabled: boolean,
	TouchEnabled: boolean,
}
export type UserInputObject = {
	KeyName: string,
	UserInputState: Enum.UserInputState,
	UserInputType: Enum.UserInputType,
	KeyCode: Enum.KeyCode,
}
export type Cursor = {
	Y: number,
	Player: string,
	X: number,
	UserId: number,
	Pressed: boolean,
}
export type ObjectDetectorHitNothing = {
	Distance: number,
	Name: never,
	Position: never,
	Color: never,
	isTerrain: true,
	CFrame: never,
	Size: never,
}
export type ObjectDetectorHitTerrain = {
	Distance: number,
	Name: "Head" | "Torso" | "Left Arm" | "Right Arm" | "Left Leg" | "Right Leg" | "Collider" | string,
	Position: Vector3,
	Color: Color3,
	isTerrain: true,
	CFrame: CFrame,
	Size: Vector3,
}
export type ObjectDetectorHitData = ObjectDetectorHitNothing | ObjectDetectorHitTerrain | ObjectDetectorHitObject
export type ObjectDetectorHitObject = {
	Color: Color3,
	LockedBy: number?,
	Distance: number,
	Name: string,
	Position: Vector3,
	Size: Vector3,
	isTerrain: false,
	CFrame: CFrame,
	CreatedBy: number?,
}
export type ResourceString = string
export type PortLike = Port | number
export type ModemRequestResponse = {
	StatusMessage: string,
	Success: boolean,
	StatusCode: number,
	Headers: {
		[string]: any,
	},
	Body: any,
}
export type ModemRequest = {
	Method: ("GET" | "POST" | "PUSH" | "PATCH" | "DELETE")?,
	Compress: Enum.HttpCompression?,
	Url: string,
	Headers: {
		[string]: any,
	}?,
	Body: string?,
}
type MethodParameter = {
	Type: string?,
	Description: string?,
	Name: string?,
}
export type MethodData = {
	Name: string,
	Arguments: { MethodParameter },
	Description: string?,
	Results: { MethodParameter },
}
export type ConfigurableData = {
	Type: string,
	Description: string,
	Options: ({ string } | { number })?,
	DefaultValue: any?,
	Name: string,
}
export type FileSystem = {
	split: (pathname: string) -> ({ string }),
	copy: (self: FileSystem, pathnameFrom: string, pathnameTo: string?) -> (),
	mkdir: (self: FileSystem, pathname: string?) -> (),
	pwd: (self: FileSystem) -> (string),
	readdir: (self: FileSystem, pathname: string) -> ({ string }),
	__index: FileSystem,
	new: (source: (FileSystemRoot | FileSystemDirectory)?) -> (FileSystem),
	separator: string,
	resolve: (pathname: string) -> (string),
	parentdir: (pathname: string) -> (string),
	join: (...string) -> (string),
	filename: (pathname: string) -> (string),
	unlink: (self: FileSystem, pathname: string) -> (),
	readfile: (self: FileSystem, filepath: string) -> (string),
	exists: (pathname: string) -> (boolean),
	moveMerge: (self: FileSystem, pathnameFrom: string, pathnameTo: string?) -> (),
	mklink: (self: FileSystem, linkName: string, targetName: string?) -> (),
	writefile: (self: FileSystem, filepath: string, contents: string) -> (),
	rename: (self: FileSystem, pathnameFrom: string, pathnameTo: string?) -> (),
	chdir: (self: FileSystem, pathname: string) -> (string),
}
export type RawFileSystem = {
	SYSTEM_READONLY: FileAttributes,
	Directory: (contents: {
		[string]: FileSystemFileNode,
	}, attributes: FileAttributes, readonly: boolean?) -> (FileSystemDirectory),
	write: (self: RawFileSystem, root: FileSystemRoot, pathname: string, node: FileSystemFileNode, options: FileSystemOperationOptions?) -> (),
	Link: (pathname: string, attributes: FileAttributes) -> (FileSystemLink),
	Device: (device: any, attributes: FileAttributes) -> (FileSystemDevice),
	SYSTEM_NOACCESS: FileAttributes,
	read: (self: RawFileSystem, root: FileSystemRoot, pathname: string, options: FileSystemOperationOptions?) -> (FileSystemFileNode?),
	readlink: (self: RawFileSystem, root: FileSystemRoot, link: FileSystemLink, options: FileSystemOperationOptions?) -> (FileSystemFileNode?),
	File: (contents: string, attributes: FileAttributes) -> (FileSystemFile),
	Root: (root: FileSystemDirectory, attributes: FileAttributes) -> (FileSystemRoot),
}
type primitive = string | number | boolean | { primitive } | {
	[primitive]: primitive,
}
export type FileSystemDevice = {
	device: any,
	kind: "device",
	attributes: FileAttributes?,
}
export type FileSystemFile = {
	kind: "file",
	attributes: FileAttributes?,
	contents: string,
}
export type FileSystemFileNode = FileSystemDirectory | FileSystemFile | FileSystemLink | FileSystemRoot | FileSystemDevice
export type FileSystemLink = {
	target: string,
	kind: "link",
	attributes: FileAttributes?,
}
export type FileSystemRoot = {
	pwd: string,
	kind: "root",
	attributes: FileAttributes?,
	root: FileSystemDirectory,
}
export type FileSystemDirectory = {
	kind: "directory",
	attributes: FileAttributes?,
	contents: {
		[string]: FileSystemFileNode,
	},
}
export type FileAttributes = {
	permissions: ({
		userPermissions: {
			[number]: FilePermissions,
		}?,
		owner: "system" | number,
	} | FilePermissions)?,
	metadata: {
		[primitive]: primitive,
	}?,
}
export type FilePermissions = {
	write: boolean?,
	read: boolean?,
}
export type FileSystemOperationOptions = {
	ignorePermissions: boolean?,
	ignoreLinks: boolean?,
}
type TerrainData = {
	TreeSettings: {
		branch_size_percentage: {
			max: {
				height: number,
				width: number,
			},
			min: {
				height: number,
				width: number,
			},
		},
		branch_angles: { number },
		trunk_size: {
			max: {
				height: number,
				width: number,
			},
			min: {
				height: number,
				width: number,
			},
		},
		branch_offset: { number },
		amount_of_branches: { number },
		amount_of_splits: { number },
	},
	Slopes: { number },
	Life: boolean,
	Water: boolean,
	Temperature: { number },
	Height: { number },
	RockDensity: { number },
	MountainDensity: { number },
	Roughness: { number },
}
export type CompleteRegionInfo = OrbitRegionInfo | SpaceRegionInfo | BlackHoleRegionInfo | StarRegionInfo | PlanetRegionInfo
export type PlanetRegionInfo = {
	StarType: never,
	BlackHoleSize: never,
	StarSize: never,
	OrbitBody: never,
	PlanetData: {
		DayCycleIncrement: number,
		EnterLocation: RegionEntryLocation,
		PrimaryColor: RegionColor,
		Gravity: number,
		TerrainConfig: TerrainData,
		GenerationHeightScale: number,
		Temperature: number,
		Rings: RingData?,
		PlanetType: PlanetType,
		StartingTime: number,
		Atmosphere: boolean,
		Resources: { string },
		SecondaryColor: RegionColor,
		PlanetMaterial: "Grass" | "Sand" | "Snow" | "Rock1" | "Rock2",
	},
} & PlayableRegionInfo
export type StarRegionInfo = {
	StarType: StarType,
	BlackHoleSize: never,
	EnterLocation: RegionEntryLocation,
	StarSize: number,
	OrbitBody: never,
	PlanetData: never,
} & PlayableRegionInfo
type RegionColor = {
	B: number,
	G: number,
	R: number,
}
export type OrbitRegionInfo = {
	OrbitBody: BlackHoleRegionInfo | StarRegionInfo | PlanetRegionInfo,
	RegionType: "Orbit",
} & PlayableRegionInfo
export type PlayableRegionInfo = {
	Name: string,
	EnterLocation: RegionEntryLocation,
	Coordinate: Coordinates,
	StringCoordinate: string,
	RegionServer: string,
	RegionSeed: number,
}
type RegionEntryLocation = {
	Y: number,
	X: number,
	Z: number,
}
type RingData = {
	RingsAmount: number,
	RingsEnd: number,
	RingsType: "Ice" | "Stone",
	RingStart: number,
}
export type SpaceRegionInfo = {
	RegionType: "Space",
	OrbitBody: never,
} & PlayableRegionInfo
export type BlackHoleRegionInfo = {
	StarSize: never,
	BlackHoleSize: number,
	EnterLocation: RegionEntryLocation,
	StarType: never,
	OrbitBody: never,
	PlanetData: never,
} & PlayableRegionInfo
export type Coordinates = typeof(setmetatable(
	{} :: {
		InPlanet: boolean,
		SolarCoordinates: Vector2,
		UniverseCoordinates: Vector2,
	},
	{} :: CoordinatesMetatable
))
type CoordinatesMetatable = {
	type: "Coordinates",
	__eq: (self: Coordinates, Coordinates) -> (boolean),
	__mul: (self: Coordinates, Coordinates) -> (Coordinates),
	ToArray: (self: Coordinates) -> ({ number | boolean }),
	__add: (self: Coordinates, Coordinates) -> (Coordinates),
	__div: (self: Coordinates, Coordinates) -> (Coordinates),
	Clone: (self: Coordinates) -> (Coordinates),
	GetRandom: (self: Coordinates) -> (Random),
	GetSeed: (self: Coordinates) -> (number),
	CoordStringWithoutPlanet: (self: Coordinates) -> (string),
	__index: CoordinatesMetatable,
	__tostring: (self: Coordinates) -> (string),
}
export type SimplePlanetRegionInfo = {
	HasAtmosphere: boolean,
	Type: "Planet",
	Color: Color3,
	Gravity: number,
	SubType: PlanetType,
	BeaconCount: number,
	Temperature: number,
	Name: string,
	Resources: { string },
	TidallyLocked: boolean,
	HasRings: boolean,
}
export type SimpleSpaceRegionInfo = {
	BeaconCount: number,
	Type: "Planet",
	Name: string,
	HasRings: boolean,
	SubType: never,
	TidallyLocked: boolean,
}
export type SimpleStarRegionInfo = {
	BeaconCount: number,
	Type: "Star",
	Name: string,
	SubType: StarType,
	Size: number,
}
export type RegionInfo = SimpleSpaceRegionInfo | SimplePlanetRegionInfo | SimpleBlackHoleRegionInfo | SimpleStarRegionInfo
export type SimpleBlackHoleRegionInfo = {
	BeaconCount: number,
	Type: "BlackHole",
	Name: string,
	Size: number,
}
export type StarType = "Red" | "Orange" | "Yellow" | "Blue" | "Neutron"
export type CelestialBodyType = "Planet" | "BlackHole" | "Star"
export type PlanetType = "Desert" | "Terra" | "EarthLike" | "Ocean" | "Tundra" | "Forest" | "Exotic" | "Barren" | "Gas" | "RobotDepot" | "RobotFactory"
type EventConnectionMetatable<Name = string, Callback = (...unknown) -> ()> = {
	Unbind: (self: EventConnection<Name, Callback>) -> (),
	__index: EventConnectionMetatable<Name, Callback>,
	Disconnect: (self: EventConnection<Name, Callback>) -> (),
	__mode: "v",
}
export type Event<Name = string, Callback = (...unknown) -> (), Parameters... = ...unknown> = {
	_eventName: Name,
	Connect: (self: Event<Name, Callback, Parameters...>, callback: Callback) -> (EventConnection<Name, Callback>),
}
export type EventConnection<Name = string, Callback = (...unknown) -> ()> = typeof(setmetatable(
	{} :: {
		_eventName: Name,
		Callback: Callback,
	},
	{} :: EventConnectionMetatable<Name, Callback>
))
declare extern type PilotObject with
	read Durability: number
	read CFrame: CFrame
	function IsDestroyed(self): (boolean)
	read Mass: number
	function GetColor(self): (Color3)
	function Click(self): ()
	function GetNameOfOwnerAsync(self): (string?)
	function GetConfigurables(self): ({
		[string]: ConfigurableData,
	})
	read AssemblyMass: number
	read AssemblyLinearVelocity: Vector3
	read Health: number
	read ClassName: "PilotObject"
	read Orientation: Vector3
	read AssemblyCenterOfMass: Vector3
	read CreatedBy: number?
	function GetComponent(self, componentName: ComponentName): (Component?)
	read Heat: number
	Destroying: Event<"Destroying", () -> ()>
	read Color: Color3
	function CanUninstallComponent(self, componentName: ComponentName?): (boolean)
	function HasComponent(self): (boolean)
	read Material: Enum.Material
	read PartLocked: number?
	function GetEvents(self): ({ string })
	function CanInstallComponent(self, componentName: ComponentName?): (boolean)
	read Anchored: boolean
	function GetOwnerId(self): (number?)
	function GetShape(self): (string?)
	function Trigger(self): ()
	function GetDurability(self): (number)
	read GUID: string
	function GetMethods(self): ({
		[string]: MethodData,
	})
	function ListComponents(self): ({ string })
	read Temperature: number
	function HasPermission(self, permission: Permission): (boolean)
	read Position: Vector3
	function IsGrounded(self): (boolean)
	read Size: Vector3
	function GetSize(self): (Vector3)
	function GetTemperature(self): (number)
end
declare extern type Spotlight extends PilotObject with
	read ClassName: "Spotlight"
	function SetColor(self, color: Color3): ()
end
declare extern type Silicon extends PilotObject with
	read ClassName: "Silicon"
end
declare extern type Beacon extends PilotObject with
	ShowOnMap: boolean
	BeaconName: string
	Configured: Event<"Configured", (configurerId: number) -> ()>
	read ClassName: "Beacon"
	function Configure(self, configuration: BeaconConfiguration): ()
end
declare extern type Port extends PilotObject with
	PortID: number
	function Configure(self, configuration: PortConfiguration): ()
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	read ClassName: "Port"
end
declare extern type Switch extends PilotObject with
	function Configure(self, configuration: SwitchConfiguration): ()
	read ClassName: "Switch"
	SwitchValue: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
end
declare extern type Tank extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function Configure(self, configuration: TankConfiguration): ()
	Resource: string
	CanBeCraftedFrom: boolean
	function GetResourceAmount(self): (number)
	read ClassName: "Tank"
	function GetAmount(self): (number)
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	function GetResource(self): (string | "nil")
end
declare extern type NuclearWaste extends PilotObject with
	read ClassName: "NuclearWaste"
end
declare extern type HyperspaceRadar extends PilotObject with
	ViewCoordinates: Coordinates | string
	function Configure(self, configuration: HyperspaceRadarConfiguration): ()
	read ClassName: "HyperspaceRadar"
end
declare extern type Asphalt extends PilotObject with
	read ClassName: "Asphalt"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Boiler extends PilotObject with
	read ClassName: "Boiler"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type SMG extends PilotObject with
	read ClassName: "SMG"
end
declare extern type Plasma extends PilotObject with
	read ClassName: "Plasma"
end
declare extern type Kiln extends PilotObject with
	read ClassName: "Kiln"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Copper extends PilotObject with
	read ClassName: "Copper"
end
declare extern type CornerWedge extends PilotObject with
	read ClassName: "CornerWedge"
end
declare extern type Propeller extends PilotObject with
	read ClassName: "Propeller"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Bin extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function Configure(self, configuration: BinConfiguration): ()
	Resource: string
	CanBeCraftedFrom: boolean
	function GetResourceAmount(self): (number)
	read ClassName: "Bin"
	function GetAmount(self): (number)
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	function GetResource(self): (string | "nil")
end
declare extern type Refinery extends PilotObject with
	read ClassName: "Refinery"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type SolarScoop extends PilotObject with
	read ClassName: "SolarScoop"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Handle extends PilotObject with
	TriggerMode: HandleTriggerMode
	read ClassName: "Handle"
	function Configure(self, configuration: HandleConfiguration): ()
	Swing: HandleSwingMode
	ToolName: string
end
declare extern type Door extends PilotObject with
	ComponentsUpdated: Event<"ComponentsUpdated", (...any) -> ()>
	read ClassName: "Door"
end
declare extern type Uranium extends PilotObject with
	read ClassName: "Uranium"
end
declare extern type DevBattery extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function GetAmount(self): (number)
	read ClassName: "DevBattery"
	function GetResourceAmount(self): (number)
	function GetResource(self): (string | "nil")
end
declare extern type Oil extends PilotObject with
	read ClassName: "Oil"
end
declare extern type Transporter extends PilotObject with
	function Configure(self, configuration: TransporterConfiguration): ()
	read ClassName: "Transporter"
	TransporterID: string
end
declare extern type Relay extends PilotObject with
	LinkerID: number
	Mode: RelayMode
	function Configure(self, configuration: RelayConfiguration): ()
	read ClassName: "Relay"
end
declare extern type FourthOfJuly extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Damaged: Event<"Damaged", (damage: number, damageType: "Kinetic" | "Energy", damageSource: PilotObject?) -> ()>
	read ClassName: "FourthOfJuly"
end
declare extern type Button extends PilotObject with
	read ClassName: "Button"
end
declare extern type Hologram extends PilotObject with
	function Configure(self, configuration: HologramConfiguration): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
	UserId: number
	read ClassName: "Hologram"
end
declare extern type Titanium extends PilotObject with
	read ClassName: "Titanium"
end
declare extern type Shotgun extends PilotObject with
	read ClassName: "Shotgun"
end
declare extern type Egg extends PilotObject with
	read ClassName: "Egg"
end
declare extern type Plastic extends PilotObject with
	read ClassName: "Plastic"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type ConveyorBelt extends PilotObject with
	ConveyorBeltSpeed: number
	function Configure(self, configuration: ConveyorBeltConfiguration): ()
	read ClassName: "ConveyorBelt"
end
declare extern type Radiator extends PilotObject with
	read ClassName: "Radiator"
end
declare extern type Sign extends PilotObject with
	TextFont: string
	function Configure(self, configuration: SignConfiguration): ()
	read ClassName: "Sign"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	TextColor: Color3
	SignText: string
end
declare extern type Cylinder extends PilotObject with
	read ClassName: "Cylinder"
end
declare extern type EnergySword extends PilotObject with
	read ClassName: "EnergySword"
end
declare extern type WindTurbine extends PilotObject with
	read ClassName: "WindTurbine"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Gyro extends PilotObject with
	function Configure(self, configuration: GyroConfiguration): ()
	DisableWhenUnpowered: boolean
	MaxTorque: number
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	TriggerWhenSeeked: boolean
	read ClassName: "Gyro"
	function PointAlong(self, direction: Vector3, up: Vector3?): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function PointAt(self, position: Vector3, up: Vector3?): ()
	Seek: string
end
declare extern type Flashlight extends PilotObject with
	read ClassName: "Flashlight"
end
declare extern type GravityGenerator extends PilotObject with
	Gravity: number
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function Configure(self, configuration: GravityGeneratorConfiguration): ()
	read ClassName: "GravityGenerator"
end
declare extern type FloatDevice extends PilotObject with
	read ClassName: "FloatDevice"
end
declare extern type Hydroponic extends PilotObject with
	Grow: string
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function Configure(self, configuration: HydroponicConfiguration): ()
	read ClassName: "Hydroponic"
end
declare extern type Framewire extends PilotObject with
	read ClassName: "Framewire"
end
declare extern type Goo extends PilotObject with
	read ClassName: "Goo"
end
declare extern type Sorter extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function Sort(self, amount: number?): ()
	read ClassName: "Sorter"
	function Configure(self, configuration: SorterConfiguration): ()
	TriggerQuantity: number
	Rate: number
	Resource: string
end
declare extern type Blade extends PilotObject with
	ComponentsUpdated: Event<"ComponentsUpdated", (...any) -> ()>
	function Configure(self, configuration: BladeConfiguration): ()
	Shape: BladeShape
	read ClassName: "Blade"
end
declare extern type Gun extends PilotObject with
	read ClassName: "Gun"
end
declare extern type VintagePlasmaPistol extends PilotObject with
	read ClassName: "VintagePlasmaPistol"
end
declare extern type Heater extends PilotObject with
	read ClassName: "Heater"
end
declare extern type Hotdog extends PilotObject with
	read ClassName: "Hotdog"
end
declare extern type Gold extends PilotObject with
	read ClassName: "Gold"
end
declare extern type Microphone extends PilotObject with
	Chatted: Event<"Chatted", (player: number, message: string) -> ()>
	read ClassName: "Microphone"
end
declare extern type Aerogel extends PilotObject with
	read ClassName: "Aerogel"
end
declare extern type Katana extends PilotObject with
	read ClassName: "Katana"
end
declare extern type Lantern extends PilotObject with
	read ClassName: "Lantern"
end
declare extern type Beaker extends PilotObject with
	read ClassName: "Beaker"
end
declare extern type StorageSensor extends PilotObject with
	function Configure(self, configuration: StorageSensorConfiguration): ()
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	QuantityRange: Range
	read ClassName: "StorageSensor"
end
declare extern type PDWX44 extends PilotObject with
	read ClassName: "PDW-X44"
end
declare extern type TouchSensor extends PilotObject with
	Touched: Event<"Touched", (object: (number | PilotObject)?) -> ()>
	read ClassName: "TouchSensor"
end
declare extern type PlasmaCannon extends PilotObject with
	read ClassName: "PlasmaCannon"
end
declare extern type EnergyShield extends PilotObject with
	function Configure(self, configuration: EnergyShieldConfiguration): ()
	function GetShieldHealth(self): (number)
	ShieldStrength: number
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function CalculateCost(self, radius: number?): (number)
	read ClassName: "EnergyShield"
	ShieldRadius: number
	Configured: Event<"Configured", (configurerId: number) -> ()>
	RegenerationSpeed: number
	function SetColor(self, color: Color3): ()
end
declare extern type SmoothReinforcedGlass extends PilotObject with
	read ClassName: "SmoothReinforcedGlass"
end
declare extern type Pulverizer extends PilotObject with
	read ClassName: "Pulverizer"
end
declare extern type RegionCloaker extends PilotObject with
	read ClassName: "RegionCloaker"
end
declare extern type Rice extends PilotObject with
	read ClassName: "Rice"
end
declare extern type Hydrogen extends PilotObject with
	read ClassName: "Hydrogen"
end
declare extern type EnergyBomb extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Damaged: Event<"Damaged", (damage: number, damageType: "Kinetic" | "Energy", damageSource: PilotObject?) -> ()>
	read ClassName: "EnergyBomb"
end
declare extern type SmoothGlass extends PilotObject with
	read ClassName: "SmoothGlass"
end
declare extern type Primer extends PilotObject with
	read ClassName: "Primer"
end
declare extern type tinnitus extends PilotObject with
	read ClassName: "tinnitus"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Constructor extends PilotObject with
	function CalculateModelRecipe(self, modelCode: string): ({
		[string]: number,
	})
	function Configure(self, configuration: ConstructorConfiguration): ()
	ModelCode: string
	read ClassName: "Constructor"
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Configured: Event<"Configured", (configurerId: number) -> ()>
	Autolock: boolean
	RelativeToConstructor: boolean
end
declare extern type AirSupply extends PilotObject with
	read ClassName: "AirSupply"
end
declare extern type Spheroid extends PilotObject with
	read ClassName: "Spheroid"
end
declare extern type HeatValve extends PilotObject with
	function Configure(self, configuration: HeatValveConfiguration): ()
	read ClassName: "HeatValve"
	SwitchValue: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
end
declare extern type TriggerWire extends PilotObject with
	read ClassName: "TriggerWire"
end
declare extern type Truss extends PilotObject with
	read ClassName: "Truss"
end
declare extern type StudAligner extends PilotObject with
	read ClassName: "StudAligner"
end
declare extern type Artillery extends PilotObject with
	read ClassName: "Artillery"
end
declare extern type Tire extends PilotObject with
	read ClassName: "Tire"
end
declare extern type HalfSphere extends PilotObject with
	read ClassName: "HalfSphere"
end
declare extern type Rifle extends PilotObject with
	read ClassName: "Rifle"
end
declare extern type Anchor extends PilotObject with
	Anchored: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: AnchorConfiguration): ()
	read ClassName: "Anchor"
end
declare extern type Flint extends PilotObject with
	read ClassName: "Flint"
end
declare extern type Wire extends PilotObject with
	read ClassName: "Wire"
end
declare extern type Wing extends PilotObject with
	read ClassName: "Wing"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type ZapWire extends PilotObject with
	read ClassName: "ZapWire"
end
declare extern type RustedMetal extends PilotObject with
	read ClassName: "RustedMetal"
end
declare extern type Dispenser extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	Filter: string
	read ClassName: "Dispenser"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: DispenserConfiguration): ()
	function Dispense(self): ()
end
declare extern type Railgun extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	Damaged: Event<"Damaged", (damage: number, damageType: "Kinetic" | "Energy", damageSource: PilotObject?) -> ()>
	read ClassName: "Railgun"
end
declare extern type AlienCore extends PilotObject with
	read ClassName: "AlienCore"
end
declare extern type MustardGas extends PilotObject with
	read ClassName: "MustardGas"
end
declare extern type Beryllium extends PilotObject with
	read ClassName: "Beryllium"
end
declare extern type ChemicalSynthiser extends PilotObject with
	read ClassName: "ChemicalSynthiser"
end
declare extern type Coal extends PilotObject with
	read ClassName: "Coal"
end
declare extern type Melter extends PilotObject with
	read ClassName: "Melter"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type DarkMatter extends PilotObject with
	read ClassName: "DarkMatter"
end
declare extern type Freezer extends PilotObject with
	read ClassName: "Freezer"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Diamond extends PilotObject with
	read ClassName: "Diamond"
end
declare extern type CornerRoundWedge extends PilotObject with
	read ClassName: "CornerRoundWedge"
end
declare extern type ExoticMatter extends PilotObject with
	read ClassName: "ExoticMatter"
end
declare extern type Valve extends PilotObject with
	function Configure(self, configuration: ValveConfiguration): ()
	read ClassName: "Valve"
	SwitchValue: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
end
declare extern type Wedge extends PilotObject with
	read ClassName: "Wedge"
end
declare extern type Part100k extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	read ClassName: "100k"
end
declare extern type Glass extends PilotObject with
	read ClassName: "Glass"
end
declare extern type ImpulseCannon extends PilotObject with
	read ClassName: "ImpulseCannon"
end
declare extern type Seat extends PilotObject with
	OccupantChanged: Event<"OccupantChanged", (newOccupant: number?) -> ()>
	function EjectOccupant(self): ()
	function GetOccupant(self): (number?)
	read ClassName: "Seat"
end
declare extern type PlutoniumCore extends PilotObject with
	read ClassName: "PlutoniumCore"
end
declare extern type Grass extends PilotObject with
	read ClassName: "Grass"
end
declare extern type HeatSink extends PilotObject with
	read ClassName: "HeatSink"
end
declare extern type Heatshield extends PilotObject with
	read ClassName: "Heatshield"
end
declare extern type Helium extends PilotObject with
	read ClassName: "Helium"
end
declare extern type Rubber extends PilotObject with
	read ClassName: "Rubber"
end
declare extern type Mandrillium extends PilotObject with
	read ClassName: "Mandrillium"
end
declare extern type Lava extends PilotObject with
	read ClassName: "Lava"
end
declare extern type DevTeleporter extends PilotObject with
	TeleporterID: string
	function Configure(self, configuration: DevTeleporterConfiguration): ()
	read ClassName: "DevTeleporter"
end
declare extern type Jade extends PilotObject with
	read ClassName: "Jade"
end
declare extern type Iron extends PilotObject with
	read ClassName: "Iron"
end
declare extern type Coupler extends PilotObject with
	Couple: Event<"Couple", () -> ()>
	function Configure(self, configuration: CouplerConfiguration): ()
	function IsCoupled(self): (boolean)
	CouplerID: string
	read ClassName: "Coupler"
	AutoTrigger: boolean
	Decouple: Event<"Decouple", () -> ()>
	function GetAttachedCoupler(self): (Coupler?)
end
declare extern type Neutronium extends PilotObject with
	read ClassName: "Neutronium"
end
declare extern type Battery extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function GetAmount(self): (number)
	read ClassName: "Battery"
	function GetResourceAmount(self): (number)
	function GetResource(self): (string | "nil")
end
declare extern type NitrogenOxide extends PilotObject with
	read ClassName: "NitrogenOxide"
end
declare extern type Turbofan extends PilotObject with
	TurboFanSpeed: number
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function Configure(self, configuration: TurbofanConfiguration): ()
	read ClassName: "Turbofan"
end
declare extern type Obamium extends PilotObject with
	read ClassName: "Obamium"
end
declare extern type TractorBeam extends PilotObject with
	PowerPercent: number
	function Configure(self, configuration: TractorBeamConfiguration): ()
	read ClassName: "TractorBeam"
end
declare extern type Perfectium extends PilotObject with
	read ClassName: "Perfectium"
end
declare extern type StarMap extends PilotObject with
	function GetBodies(self): ({
		[string]: {
			PlanetType: PlanetType,
		},
	})
	function GetSystems(self): ({ string })
	read ClassName: "StarMap"
end
declare extern type Ice extends PilotObject with
	read ClassName: "Ice"
end
declare extern type ObjectDetector extends PilotObject with
	MaxDistance: number
	read ClassName: "ObjectDetector"
	TriggerAtDistance: Vector2
	function Configure(self, configuration: ObjectDetectorConfiguration): ()
	function GetLastHitPart(self): (PilotObject)
	function GetLastHitData(self): (ObjectDetectorHitData)
end
declare extern type Generator extends PilotObject with
	read ClassName: "Generator"
end
declare extern type Explosive extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Damaged: Event<"Damaged", (damage: number, damageType: "Kinetic" | "Energy", damageSource: PilotObject?) -> ()>
	read ClassName: "Explosive"
end
declare extern type Router extends PilotObject with
	RouterID: string
	function Configure(self, configuration: RouterConfiguration): ()
	read ClassName: "Router"
end
declare extern type CrossBow extends PilotObject with
	read ClassName: "CrossBow"
end
declare extern type Ruby extends PilotObject with
	read ClassName: "Ruby"
end
declare extern type Sand extends PilotObject with
	read ClassName: "Sand"
end
declare extern type Claymore extends PilotObject with
	read ClassName: "Claymore"
end
declare extern type Petroleum extends PilotObject with
	read ClassName: "Petroleum"
end
declare extern type BallastTank extends PilotObject with
	read ClassName: "BallastTank"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function Configure(self, configuration: BallastTankConfiguration): ()
	Buoyancy: number
end
declare extern type Snow extends PilotObject with
	read ClassName: "Snow"
end
declare extern type Stanlium extends PilotObject with
	read ClassName: "Stanlium"
end
declare extern type IonRocket extends PilotObject with
	read ClassName: "IonRocket"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: IonRocketConfiguration): ()
	Propulsion: number
end
declare extern type Steam extends PilotObject with
	read ClassName: "Steam"
end
declare extern type TintedGlass extends PilotObject with
	read ClassName: "TintedGlass"
end
declare extern type Stone extends PilotObject with
	read ClassName: "Stone"
end
declare extern type Sulfur extends PilotObject with
	read ClassName: "Sulfur"
end
declare extern type MiningLaser extends PilotObject with
	function Configure(self, configuration: MiningLaserConfiguration): ()
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	MaterialToExtract: string
	read ClassName: "MiningLaser"
end
declare extern type Chute extends PilotObject with
	read ClassName: "Chute"
end
declare extern type RepairPlate extends PilotObject with
	read ClassName: "RepairPlate"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type AdminTool extends PilotObject with
	read ClassName: "AdminTool"
end
declare extern type FireWood extends PilotObject with
	read ClassName: "FireWood"
end
declare extern type Radar extends PilotObject with
	read ClassName: "Radar"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type ImpactPlate extends PilotObject with
	read ClassName: "ImpactPlate"
end
declare extern type Engine extends PilotObject with
	function Configure(self, configuration: EngineConfiguration): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
	EngineSpeed: number
	read ClassName: "Engine"
end
declare extern type StanSword extends PilotObject with
	read ClassName: "StanSword"
end
declare extern type TemperatureSensor extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function GetTemp(self): (number)
	read ClassName: "TemperatureSensor"
	function Configure(self, configuration: TemperatureSensorConfiguration): ()
	TemperatureRange: Range
end
declare extern type Modem extends PilotObject with
	function GetAsync(self, url: string, nocache: boolean?, headers: {
		[string]: any,
	}?): (string)
	function Configure(self, configuration: ModemConfiguration): ()
	function PostAsync(self, url: string, data: string, contentType: Enum.HttpContentType?, compress: boolean?, headers: {
		[string]: any,
	}?): (string)
	function SendLocalMessage(self, data: any, id: string?): ()
	function PostRequest(self, domain: string, data: string): ()
	function UrlEncode(self, input: string): (string)
	function GetRequest(self, domain: string): (string)
	function RealPostRequest(self, domain: string, data: string, asyncBool: boolean, transformFunction: (...any) -> (), optionalHeaders: {
		[string]: any,
	}?): ({
		success: boolean,
		response: string,
	})
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	read ClassName: "Modem"
	NetworkID: string
	function SendMessage(self, data: JSON, id: string?): ()
	function RequestAsync(self, options: ModemRequest): (ModemRequestResponse)
	MessageSent: Event<"MessageSent", (data: any) -> ()>
end
declare extern type SubspaceTripmine extends PilotObject with
	read ClassName: "SubspaceTripmine"
end
declare extern type Gasoline extends PilotObject with
	read ClassName: "Gasoline"
end
declare extern type Camera extends PilotObject with
	VideoID: number
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: CameraConfiguration): ()
	read ClassName: "Camera"
end
declare extern type HeatCell extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function GetAmount(self): (number)
	read ClassName: "HeatCell"
	function GetResourceAmount(self): (number)
	function GetResource(self): (string | "nil")
end
declare extern type GeigerCounter extends PilotObject with
	read ClassName: "GeigerCounter"
end
declare extern type CombustionTurbine extends PilotObject with
	read ClassName: "CombustionTurbine"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Billboardium extends PilotObject with
	read ClassName: "Billboardium"
end
declare extern type Thruster extends PilotObject with
	read ClassName: "Thruster"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: ThrusterConfiguration): ()
	Propulsion: number
end
declare extern type NightVisionGoggles extends PilotObject with
	read ClassName: "NightVisionGoggles"
end
declare extern type Faucet extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	Filter: string
	read ClassName: "Faucet"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: FaucetConfiguration): ()
	function Dispense(self): ()
end
declare extern type AutomaticLaser extends PilotObject with
	read ClassName: "AutomaticLaser"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type EthernetCable extends PilotObject with
	read ClassName: "EthernetCable"
end
declare extern type Pipe extends PilotObject with
	read ClassName: "Pipe"
end
declare extern type Tetrahedron extends PilotObject with
	read ClassName: "Tetrahedron"
end
declare extern type CornerRoundWedge2 extends PilotObject with
	read ClassName: "CornerRoundWedge2"
end
declare extern type DevSource extends PilotObject with
	function Configure(self, configuration: DevSourceConfiguration): ()
	function GetAmountGenerated(self, ...: any): (...any)
	read ClassName: "DevSource"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	Resource: ResourceString
end
declare extern type Cannon extends PilotObject with
	read ClassName: "Cannon"
end
declare extern type Furnace extends PilotObject with
	read ClassName: "Furnace"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Gear extends PilotObject with
	read ClassName: "Gear"
end
declare extern type CornerTetra extends PilotObject with
	read ClassName: "CornerTetra"
end
declare extern type Scanner extends PilotObject with
	function CalculateCost(self, range: number?): (number)
	function Locate(self, part: PilotObject, scanners: { Scanner }): (Vector3)
	function GetPartsInRange(self, range: number?, className: string?): ({ PilotObject })
	read ClassName: "Scanner"
	function GetDistance(self, part: PilotObject): (number)
	Range: number
	function Configure(self, configuration: ScannerConfiguration): ()
end
declare extern type Polysilicon extends PilotObject with
	function Configure(self, configuration: PolysiliconConfiguration): ()
	read ClassName: "Polysilicon"
	PolysiliconMode: PolysiliconMode | number
	Configured: Event<"Configured", (configurerId: number) -> ()>
	Frequency: number
end
declare extern type RepairLaser extends PilotObject with
	read ClassName: "RepairLaser"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Rail extends PilotObject with
	function SetPosition(self, depth: number): ()
	function Configure(self, configuration: RailConfiguration): ()
	read ClassName: "Rail"
	Position1: number
	TweenTime: number
	Position2: number
end
declare extern type Sail extends PilotObject with
	read ClassName: "Sail"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Extinguisher extends PilotObject with
	read ClassName: "Extinguisher"
end
declare extern type DarkReactor extends PilotObject with
	read ClassName: "DarkReactor"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type BeamRifle extends PilotObject with
	read ClassName: "BeamRifle"
end
declare extern type VitalScanner extends PilotObject with
	read ClassName: "VitalScanner"
end
declare extern type Balloon extends PilotObject with
	read ClassName: "Balloon"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: BalloonConfiguration): ()
	Buoyancy: number
end
declare extern type Light extends PilotObject with
	Shadows: boolean
	function Configure(self, configuration: LightConfiguration): ()
	read ClassName: "Light"
	Brightness: number
	Configured: Event<"Configured", (configurerId: number) -> ()>
	LightRange: number
	function SetColor(self, color: Color3): ()
end
declare extern type Flamethrower extends PilotObject with
	read ClassName: "Flamethrower"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type RemoteControl extends PilotObject with
	read ClassName: "RemoteControl"
	function Configure(self, configuration: RemoteControlConfiguration): ()
	RemoteControlMode: RemoteControlMode
	RemoteControlRange: number
end
declare extern type TemperatureGate extends PilotObject with
	function Configure(self, configuration: TemperatureGateConfiguration): ()
	Inverted: boolean
	function GetState(self): ()
	read ClassName: "TemperatureGate"
	function GetTemp(self): (number)
	SwitchValue: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	TemperatureRange: Range
	function GetTemperature(self): (number)
end
declare extern type Scrapper extends PilotObject with
	read ClassName: "Scrapper"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Rocket extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	read ClassName: "Rocket"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: RocketConfiguration): ()
	Propulsion: number
end
declare extern type SpawnPoint extends PilotObject with
	read ClassName: "SpawnPoint"
end
declare extern type Electromagnet extends PilotObject with
	read ClassName: "Electromagnet"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type DeleteSwitch extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	read ClassName: "DeleteSwitch"
end
declare extern type FactionSpawn extends PilotObject with
	read ClassName: "FactionSpawn"
end
declare extern type Igniter extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	read ClassName: "Igniter"
end
declare extern type RepairKit extends PilotObject with
	read ClassName: "RepairKit"
end
declare extern type Boombox extends PilotObject with
	function Configure(self, configuration: BoomboxConfiguration): ()
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	Audio: number
	read ClassName: "Boombox"
end
declare extern type SolarPanel extends PilotObject with
	read ClassName: "SolarPanel"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type TriggerSwitch extends PilotObject with
	function Configure(self, configuration: TriggerSwitchConfiguration): ()
	read ClassName: "TriggerSwitch"
	SwitchValue: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
end
declare extern type Tile extends PilotObject with
	read ClassName: "Tile"
end
declare extern type HyperDrive extends PilotObject with
	function GetRequiredPower(self): ()
	Coordinates: Coordinates | string
	read ClassName: "HyperDrive"
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: HyperDriveConfiguration): ()
end
declare extern type DevSink extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function Configure(self, configuration: DevSinkConfiguration): ()
	function GetAmount(self): (number)
	function GetResourceAmount(self): (number)
	Resource: ResourceString
	read ClassName: "DevSink"
	function GetAmountConsumed(self, ...: any): (...any)
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	function GetResource(self): (string | "nil")
end
declare extern type Solenoid extends PilotObject with
	function Configure(self, configuration: SolenoidConfiguration): ()
	PowerRange: Range
	Inverted: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	read ClassName: "Solenoid"
end
declare extern type Decoupler extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	read ClassName: "Decoupler"
end
declare extern type RoundWedge2 extends PilotObject with
	read ClassName: "RoundWedge2"
end
declare extern type Prosthetic extends PilotObject with
	Limb: PlayerLimb
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: ProstheticConfiguration): ()
	read ClassName: "Prosthetic"
end
declare extern type Reactor extends PilotObject with
	read ClassName: "Reactor"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function GetFuel(self): ({ number })
	function GetTemp(self): (number)
	function GetEfficiency(self): (number)
	function Configure(self, configuration: ReactorConfiguration): ()
	TriggerWhenEmpty: boolean
	Alarm: boolean
end
declare extern type BlackBox extends PilotObject with
	function GetLogs(self): ({ RegionLog })
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	read ClassName: "BlackBox"
end
declare extern type Telescope extends PilotObject with
	function WhenRegionLoads(self, callback: (regionInfo: CompleteRegionInfo) -> ()): ()
	function Configure(self, configuration: TelescopeConfiguration): ()
	function GetCurrentCoordinate(self): (Coordinates)
	read ClassName: "Telescope"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function GetCoordinate(self): (RegionInfo)
	ViewCoordinates: Coordinates | string
end
declare extern type Aluminum extends PilotObject with
	read ClassName: "Aluminum"
end
declare extern type Motor extends PilotObject with
	function Configure(self, configuration: MotorConfiguration): ()
	Power: number
	Ratio: number
	read ClassName: "Motor"
	Configured: Event<"Configured", (configurerId: number) -> ()>
end
declare extern type Obelisk extends PilotObject with
	read ClassName: "Obelisk"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Controller extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	read ClassName: "Controller"
end
declare extern type Rotor extends PilotObject with
	read ClassName: "Rotor"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type RTG extends PilotObject with
	read ClassName: "RTG"
end
declare extern type Insulation extends PilotObject with
	read ClassName: "Insulation"
end
declare extern type Winch extends PilotObject with
	function Configure(self, configuration: WinchConfiguration): ()
	read ClassName: "Winch"
	function AdjustLength(self, adjustment: number): ()
	DeltaLength: number
	function SetLength(self, length: number): ()
	MaxLength: number
	MinLength: number
end
declare extern type RoundWedge extends PilotObject with
	read ClassName: "RoundWedge"
end
declare extern type VehicleSeat extends PilotObject with
	Enabled: boolean
	function Configure(self, configuration: VehicleSeatConfiguration): ()
	OccupantChanged: Event<"OccupantChanged", (newOccupant: number?) -> ()>
	function GetOccupant(self): (number?)
	read ClassName: "VehicleSeat"
	function EjectOccupant(self): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
	Mode: VehicleSeatMode
	Speed: number
end
declare extern type Hull extends PilotObject with
	ComponentsUpdated: Event<"ComponentsUpdated", (...any) -> ()>
	read ClassName: "Hull"
end
declare extern type SoundMuffler extends PilotObject with
	read ClassName: "SoundMuffler"
end
declare extern type Magnesium extends PilotObject with
	read ClassName: "Magnesium"
end
declare extern type Cloth extends PilotObject with
	read ClassName: "Cloth"
end
declare extern type Speaker extends PilotObject with
	Pitch: number
	function ClearSounds(self): ()
	Audio: string
	function LoadSound(self, soundId: string): (Sound?)
	function Chat(self, message: string): ()
	read ClassName: "Speaker"
	Volume: number
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: SpeakerConfiguration): ()
	function PlaySound(self, soundId: string?): ()
end
declare extern type HeatPump extends PilotObject with
	function Configure(self, configuration: HeatPumpConfiguration): ()
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	TransferRate: number
	read ClassName: "HeatPump"
end
declare extern type Plutonium extends PilotObject with
	read ClassName: "Plutonium"
end
declare extern type Microcontroller extends PilotObject with
	function Receive(self): (Microcontroller, ...any)
	function Configure(self, configuration: MicrocontrollerConfiguration): ()
	Code: string
	StartOnSpawn: boolean
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function Shutdown(self): ()
	read ClassName: "Microcontroller"
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	function Send(self, ...: any): ()
end
declare extern type SteamEngine extends PilotObject with
	function Configure(self, configuration: SteamEngineConfiguration): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
	EngineSpeed: number
	read ClassName: "SteamEngine"
end
declare extern type Quartz extends PilotObject with
	read ClassName: "Quartz"
end
declare extern type CrudeWing extends PilotObject with
	read ClassName: "CrudeWing"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Marble extends PilotObject with
	read ClassName: "Marble"
end
declare extern type Treads extends PilotObject with
	read ClassName: "Treads"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Piston extends PilotObject with
	function SetPosition(self): ()
	function Configure(self, configuration: PistonConfiguration): ()
	read ClassName: "Piston"
	Speed: number
	Configured: Event<"Configured", (configurerId: number) -> ()>
	Position1: number
	Position2: number
end
declare extern type Transistor extends PilotObject with
	function Configure(self, configuration: TransistorConfiguration): ()
	Inverted: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	read ClassName: "Transistor"
end
declare extern type PowerCell extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function GetAmount(self): (number)
	read ClassName: "PowerCell"
	function GetResourceAmount(self): (number)
	function GetResource(self): (string | "nil")
end
declare extern type TriggerRelay extends PilotObject with
	function Configure(self, configuration: TriggerRelayConfiguration): ()
	read ClassName: "TriggerRelay"
	SwitchValue: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
end
declare extern type FactionHub extends PilotObject with
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	read ClassName: "FactionHub"
end
declare extern type Pistol extends PilotObject with
	read ClassName: "Pistol"
end
declare extern type Cleat extends PilotObject with
	read ClassName: "Cleat"
end
declare extern type Fireworks extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Damaged: Event<"Damaged", (damage: number, damageType: "Kinetic" | "Energy", damageSource: PilotObject?) -> ()>
	read ClassName: "Fireworks"
end
declare extern type Transformer extends PilotObject with
	function Configure(self, configuration: TransformerConfiguration): ()
	read ClassName: "Transformer"
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	LoopTime: number
end
declare extern type AdBoard extends PilotObject with
	read ClassName: "AdBoard"
end
declare extern type Warhead extends PilotObject with
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	Damaged: Event<"Damaged", (damage: number, damageType: "Kinetic" | "Energy", damageSource: PilotObject?) -> ()>
	read ClassName: "Warhead"
end
declare extern type Brick extends PilotObject with
	read ClassName: "Brick"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Component extends PilotObject with
	ComponentsUpdated: Event<"ComponentsUpdated", (...any) -> ()>
	read ClassName: "Component"
end
declare extern type SteamTurbine extends PilotObject with
	function GetEfficiency(self): (number)
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function GetProductionRate(self): (number)
	read ClassName: "SteamTurbine"
end
declare extern type LifeSensor extends PilotObject with
	read ClassName: "LifeSensor"
	function GetReading(self): ({
		[string]: Vector3,
	})
	function ListPlayers(self): ({ number })
	function GetPlayers(self): ({
		[number]: CFrame,
	})
end
declare extern type LightTube extends PilotObject with
	read ClassName: "LightTube"
	function SetColor(self, color: Color3): ()
end
declare extern type Disk extends PilotObject with
	function Clear(self): ()
	function Write(self, key: any, value: any): ()
	function Compress(self): ()
	function ReadAll(self): ({
		[any]: any,
	})
	function WriteAll(self, content: {
		[any]: any,
	}): ()
	function Read(self, key: any): (any)
	function Decompress(self): ()
	read ClassName: "Disk"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function ReadEntireDisk(self): ({
		[any]: any,
	})
	function ClearDisk(self): ()
end
declare extern type DarkConverter extends PilotObject with
	read ClassName: "DarkConverter"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type WirelessButton extends PilotObject with
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	read ClassName: "WirelessButton"
end
declare extern type Block extends PilotObject with
	read ClassName: "Block"
end
declare extern type ProximityButton extends PilotObject with
	PromptTriggered: Event<"PromptTriggered", () -> ()>
	GamepadKeyCode: string
	HoldDuration: number
	MaxActivationDistance: number
	ObjectText: string
	PromptButtonHoldBegan: Event<"PromptButtonHoldBegan", () -> ()>
	PromptButtonHoldEnded: Event<"PromptButtonHoldEnded", () -> ()>
	function Configure(self, configuration: ProximityButtonConfiguration): ()
	read ClassName: "ProximityButton"
	KeyboardKeyCode: string
	Configured: Event<"Configured", (configurerId: number) -> ()>
	PromptTriggerEnded: Event<"PromptTriggerEnded", () -> ()>
	RequiresLineOfSight: boolean
end
declare extern type Ball extends PilotObject with
	read ClassName: "Ball"
end
declare extern type LightBridge extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	read ClassName: "LightBridge"
	BeamColor: Color3
	function Configure(self, configuration: LightBridgeConfiguration): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
end
declare extern type EnergyGun extends PilotObject with
	read ClassName: "EnergyGun"
end
declare extern type StasisField extends PilotObject with
	read ClassName: "StasisField"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Cement extends PilotObject with
	read ClassName: "Cement"
end
declare extern type Keyboard extends PilotObject with
	TextInputted: Event<"TextInputted", (text: string, player: string) -> ()>
	read ClassName: "Keyboard"
	UserInput: Event<"UserInput", (inputObject: UserInputObject, userId: number) -> ()>
	function SimulateUserInput(self): ()
	function SimulateKeyPress(self, key: string?, player: string): ()
	function SimulateTextInput(self, input: string?, player: string): ()
	KeyPressed: Event<"KeyPressed", (key: Enum.KeyCode, keyName: string, userId: number) -> ()>
end
declare extern type Neon extends PilotObject with
	read ClassName: "Neon"
end
declare extern type Diode extends PilotObject with
	read ClassName: "Diode"
end
declare extern type Assembler extends PilotObject with
	function CalculateCraftingRecipe(self, items: { string } | {
		[string]: number,
	}): ({
		Power: number,
		[string]: number,
	})
	function Configure(self, configuration: AssemblerConfiguration): ()
	function CraftItems(self, items: { string } | {
		[string]: number,
	}): ()
	function Craft(self, itemName: string): (boolean)
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function GetRecipe(self, itemName: string): ({
		[string]: number,
	})
	Configured: Event<"Configured", (configurerId: number) -> ()>
	read ClassName: "Assembler"
	Assemble: string
end
declare extern type Torch extends PilotObject with
	read ClassName: "Torch"
end
declare extern type Pump extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	read ClassName: "Pump"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: PumpConfiguration): ()
	LiquidToPump: string
end
declare extern type ElectricFence extends PilotObject with
	read ClassName: "ElectricFence"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type TimeSensor extends PilotObject with
	function Configure(self, configuration: TimeSensorConfiguration): ()
	Time: string
	read ClassName: "TimeSensor"
end
declare extern type MonsterMashPotion extends PilotObject with
	read ClassName: "MonsterMashPotion"
end
declare extern type Frog extends PilotObject with
	read ClassName: "Frog"
end
declare extern type ScubaMask extends PilotObject with
	read ClassName: "ScubaMask"
end
declare extern type FluidProjector extends PilotObject with
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	read ClassName: "FluidProjector"
	Fluid: string
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: FluidProjectorConfiguration): ()
	Size: Vector3
end
declare extern type DriveBox extends PilotObject with
	function Configure(self, configuration: DriveBoxConfiguration): ()
	read ClassName: "DriveBox"
	Reversal: boolean
	Ratio: number
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Servo extends PilotObject with
	function SetAngle(self, angle: number): ()
	function Configure(self, configuration: ServoConfiguration): ()
	ServoSpeed: number
	read ClassName: "Servo"
	Responsiveness: number
	Angle: number
	Configured: Event<"Configured", (configurerId: number) -> ()>
	AngleStep: number
end
declare extern type Fence extends PilotObject with
	read ClassName: "Fence"
end
declare extern type DevGravityGenerator extends PilotObject with
	Enabled: boolean
	Radius: number
	read ClassName: "DevGravityGenerator"
	Gravity: number
	function Configure(self, configuration: DevGravityGeneratorConfiguration): ()
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type TestStarMap extends PilotObject with
	read ClassName: "TestStarMap"
end
declare extern type BurnerGenerator extends PilotObject with
	read ClassName: "BurnerGenerator"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type ReinforcedGlass extends PilotObject with
	read ClassName: "ReinforcedGlass"
end
declare extern type Cone extends PilotObject with
	read ClassName: "Cone"
end
declare extern type WaterCooler extends PilotObject with
	read ClassName: "WaterCooler"
end
declare extern type DevHeatStorage extends PilotObject with
	ContainerChanged: Event<"ContainerChanged", (resourceType: "Power" | "Solid" | "Fluid", resourceAmount: number) -> ()>
	function GetAmount(self): (number)
	read ClassName: "DevHeatStorage"
	function GetResourceAmount(self): (number)
	function GetResource(self): (string | "nil")
end
declare extern type Laser extends PilotObject with
	read ClassName: "Laser"
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	function Configure(self, configuration: LaserConfiguration): ()
	DamageOnlyPlayers: boolean
end
declare extern type Stick extends PilotObject with
	read ClassName: "Stick"
end
declare extern type Water extends PilotObject with
	read ClassName: "Water"
end
declare extern type Teleporter extends PilotObject with
	TeleporterID: number
	ForceLocalTeleport: boolean
	Triggered: Event<"Triggered", (otherPart: PilotObject) -> ()>
	read ClassName: "Teleporter"
	function Configure(self, configuration: TeleporterConfiguration): ()
	Coordinates: Coordinates | string
end
declare extern type ARController extends PilotObject with
	function ClearElements(self, context: CanvasContext): ()
	function CreateElement3D(self, shape: "Ball" | "Block" | "Cylinder" | "Wedge" | "CornerWedge", properties: {
		[string]: any,
	}): (Part)
	function CreateElement(self, className: string, properties: {
		[string]: any,
	}, context: CanvasContext): (Instance)
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	CursorReleased: Event<"CursorReleased", (cursor: ARCursor) -> ()>
	function Configure(self, configuration: ARControllerConfiguration): ()
	CursorMoved: Event<"CursorMoved", (cursor: ARCursor) -> ()>
	function GetCursor(self): (ARCursor)
	KeyPressed: Event<"KeyPressed", (key: Enum.KeyCode, keyName: string, userId: number) -> ()>
	GetCanvas: ((self: ARController, context: "2D"?) -> (Folder)) & ((self: ARController, context: "3D") -> (WorldModel))
	Transparency: number
	function GetCursors(self): ({
		[number]: ARCursor,
	})
	function ClearElements3D(self): ()
	read ClassName: "ARController"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	UserInput: Event<"UserInput", (inputObject: UserInputObject, userId: number) -> ()>
	CursorPressed: Event<"CursorPressed", (cursor: ARCursor) -> ()>
end
declare extern type ARGlasses extends ARController with
	read ClassName: "ARGlasses"
end
declare extern type Instrument extends PilotObject with
	Type: InstrumentType
	read ClassName: "Instrument"
	function GetReading(self, type: (InstrumentType | number)?): (number | Vector3 | string)
	function Configure(self, configuration: InstrumentConfiguration): ()
	Loop: Event<"Loop", (tickInterval: number) -> ()>
end
declare extern type Antenna extends PilotObject with
	AntennaID: string
	function Configure(self, configuration: AntennaConfiguration): ()
	read ClassName: "Antenna"
end
declare extern type Filter extends PilotObject with
	read ClassName: "Filter"
	function Configure(self, configuration: FilterConfiguration): ()
	Invert: boolean
	Filter: string
end
declare extern type HeatPipe extends PilotObject with
	read ClassName: "HeatPipe"
end
declare extern type BlastingCap extends PilotObject with
	read ClassName: "BlastingCap"
end
declare extern type Lead extends PilotObject with
	read ClassName: "Lead"
end
declare extern type Food extends PilotObject with
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	read ClassName: "Food"
end
declare extern type CloningBay extends PilotObject with
	function Configure(self, configuration: CloningBayConfiguration): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
	Name: string
	read ClassName: "CloningBay"
end
declare extern type Cooler extends PilotObject with
	read ClassName: "Cooler"
end
declare extern type Apparel extends PilotObject with
	Transparency: number
	read ClassName: "Apparel"
	Limb: PlayerLimb
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
	function Configure(self, configuration: ApparelConfiguration): ()
end
declare extern type BurstLaser extends PilotObject with
	read ClassName: "BurstLaser"
end
declare extern type DevGenerator extends PilotObject with
	read ClassName: "DevGenerator"
end
declare extern type Extractor extends PilotObject with
	function Configure(self, configuration: ExtractorConfiguration): ()
	Loop: Event<"Loop", (tickInterval: number) -> ()>
	MaterialToExtract: string
	read ClassName: "Extractor"
end
declare extern type Screen extends PilotObject with
	function GetCanvas(self): (Frame)
	VideoID: number
	function CreateElement(self, className: string, properties: {
		[string]: any,
	}): (Instance)
	read ClassName: "Screen"
	function ClearElements(self): ()
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function Configure(self, configuration: ScreenConfiguration): ()
	function GetDimensions(self): (Vector2)
end
declare extern type TouchScreen extends Screen with
	CursorReleased: Event<"CursorReleased", (Cursor: Cursor) -> ()>
	CursorMoved: Event<"CursorMoved", (Cursor: Cursor) -> ()>
	function GetCursor(self, username: string): (Cursor)
	read ClassName: "TouchScreen"
	Configured: Event<"Configured", (configurerId: number) -> ()>
	function GetCursors(self): ({
		[string]: Cursor,
	})
	CursorPressed: Event<"CursorPressed", (Cursor: Cursor) -> ()>
end
declare extern type Hatch extends PilotObject with
	function Configure(self, configuration: HatchConfiguration): ()
	read ClassName: "Hatch"
	SwitchValue: boolean
	Configured: Event<"Configured", (configurerId: number) -> ()>
	OnClick: Event<"OnClick", (clickerId: number) -> ()>
end
declare extern type DelayWire extends PilotObject with
	function Configure(self, configuration: DelayWireConfiguration): ()
	read ClassName: "DelayWire"
	DelayTime: number
end
declare extern type Wood extends PilotObject with
	read ClassName: "Wood"
end
export type HyperspaceRadarConfiguration = {
	[string]: any,
	ViewCoordinates: (Coordinates | string)?,
}
export type HeatPumpConfiguration = {
	[string]: any,
	TransferRate: number?,
}
export type TriggerRelayConfiguration = {
	[string]: any,
	SwitchValue: boolean?,
}
export type LightConfiguration = {
	Shadows: boolean?,
	Brightness: number?,
	LightRange: number?,
	[string]: any,
}
export type PortConfiguration = {
	PortID: number?,
	[string]: any,
}
export type SolenoidConfiguration = {
	[string]: any,
	PowerRange: Range?,
	Inverted: boolean?,
}
export type BallastTankConfiguration = {
	[string]: any,
	Buoyancy: number?,
}
export type ValveConfiguration = {
	SwitchValue: boolean?,
	[string]: any,
}
export type TemperatureGateConfiguration = {
	SwitchValue: boolean?,
	TemperatureRange: Range?,
	[string]: any,
	Inverted: boolean?,
}
export type HandleConfiguration = {
	ToolName: string?,
	Swing: HandleSwingMode?,
	TriggerMode: HandleTriggerMode?,
	[string]: any,
}
export type ExtractorConfiguration = {
	[string]: any,
	MaterialToExtract: string?,
}
export type MotorConfiguration = {
	[string]: any,
	Ratio: number?,
	Power: number?,
}
export type TriggerSwitchConfiguration = {
	[string]: any,
	SwitchValue: boolean?,
}
export type DevTeleporterConfiguration = {
	TeleporterID: string?,
	[string]: any,
}
export type PumpConfiguration = {
	[string]: any,
	LiquidToPump: string?,
}
export type PolysiliconConfiguration = {
	PolysiliconMode: (PolysiliconMode | number)?,
	Frequency: number?,
	[string]: any,
}
export type ObjectDetectorConfiguration = {
	TriggerAtDistance: Vector2?,
	MaxDistance: number?,
	[string]: any,
}
export type DevSourceConfiguration = {
	Resource: ResourceString?,
	[string]: any,
}
export type DevSinkConfiguration = {
	[string]: any,
	Resource: ResourceString?,
}
export type CameraConfiguration = {
	VideoID: number?,
	[string]: any,
}
export type BalloonConfiguration = {
	[string]: any,
	Buoyancy: number?,
}
export type ThrusterConfiguration = {
	[string]: any,
	Propulsion: number?,
}
export type TransformerConfiguration = {
	[string]: any,
	LoopTime: number?,
}
export type BinConfiguration = {
	[string]: any,
	Resource: string?,
	CanBeCraftedFrom: boolean?,
}
export type LightBridgeConfiguration = {
	[string]: any,
	BeamColor: Color3?,
}
export type FaucetConfiguration = {
	Filter: string?,
	[string]: any,
}
export type TransistorConfiguration = {
	Inverted: boolean?,
	[string]: any,
}
export type CloningBayConfiguration = {
	[string]: any,
	Name: string?,
}
export type TeleporterConfiguration = {
	[string]: any,
	Coordinates: (Coordinates | string)?,
	TeleporterID: number?,
	ForceLocalTeleport: boolean?,
}
export type TelescopeConfiguration = {
	ViewCoordinates: (Coordinates | string)?,
	[string]: any,
}
export type ProstheticConfiguration = {
	[string]: any,
	Limb: PlayerLimb?,
}
export type HydroponicConfiguration = {
	Grow: string?,
	[string]: any,
}
export type ServoConfiguration = {
	[string]: any,
	ServoSpeed: number?,
	Responsiveness: number?,
	Angle: number?,
	AngleStep: number?,
}
export type TurbofanConfiguration = {
	[string]: any,
	TurboFanSpeed: number?,
}
export type VehicleSeatConfiguration = {
	Speed: number?,
	Mode: VehicleSeatMode?,
	[string]: any,
	Enabled: boolean?,
}
export type ConveyorBeltConfiguration = {
	ConveyorBeltSpeed: number?,
	[string]: any,
}
export type EnergyShieldConfiguration = {
	ShieldRadius: number?,
	ShieldStrength: number?,
	RegenerationSpeed: number?,
	[string]: any,
}
export type CouplerConfiguration = {
	[string]: any,
	CouplerID: string?,
	AutoTrigger: boolean?,
}
export type InstrumentConfiguration = {
	Type: InstrumentType?,
	[string]: any,
}
export type FilterConfiguration = {
	[string]: any,
	Invert: boolean?,
	Filter: string?,
}
export type ModemConfiguration = {
	NetworkID: string?,
	[string]: any,
}
export type HyperDriveConfiguration = {
	[string]: any,
	Coordinates: (Coordinates | string)?,
}
export type FluidProjectorConfiguration = {
	Fluid: string?,
	[string]: any,
	Size: Vector3?,
}
export type TemperatureSensorConfiguration = {
	TemperatureRange: Range?,
	[string]: any,
}
export type TankConfiguration = {
	[string]: any,
	Resource: string?,
	CanBeCraftedFrom: boolean?,
}
export type ConstructorConfiguration = {
	RelativeToConstructor: boolean?,
	ModelCode: string?,
	Autolock: boolean?,
	[string]: any,
}
export type WinchConfiguration = {
	DeltaLength: number?,
	[string]: any,
	MinLength: number?,
	MaxLength: number?,
}
export type TransporterConfiguration = {
	TransporterID: string?,
	[string]: any,
}
export type GravityGeneratorConfiguration = {
	[string]: any,
	Gravity: number?,
}
export type RelayConfiguration = {
	LinkerID: number?,
	[string]: any,
	Mode: RelayMode?,
}
export type HologramConfiguration = {
	UserId: number?,
	[string]: any,
}
export type SignConfiguration = {
	TextFont: string?,
	SignText: string?,
	[string]: any,
	TextColor: Color3?,
}
export type PistonConfiguration = {
	Position1: number?,
	[string]: any,
	Speed: number?,
	Position2: number?,
}
export type ReactorConfiguration = {
	Alarm: boolean?,
	[string]: any,
	TriggerWhenEmpty: boolean?,
}
export type ProximityButtonConfiguration = {
	[string]: any,
	GamepadKeyCode: string?,
	HoldDuration: number?,
	ObjectText: string?,
	KeyboardKeyCode: string?,
	RequiresLineOfSight: boolean?,
	MaxActivationDistance: number?,
}
export type GyroConfiguration = {
	DisableWhenUnpowered: boolean?,
	TriggerWhenSeeked: boolean?,
	Seek: string?,
	[string]: any,
	MaxTorque: number?,
}
export type AssemblerConfiguration = {
	Assemble: string?,
	[string]: any,
}
export type RemoteControlConfiguration = {
	[string]: any,
	RemoteControlRange: number?,
	RemoteControlMode: RemoteControlMode?,
}
export type SteamEngineConfiguration = {
	EngineSpeed: number?,
	[string]: any,
}
export type StorageSensorConfiguration = {
	QuantityRange: Range?,
	[string]: any,
}
export type HeatValveConfiguration = {
	SwitchValue: boolean?,
	[string]: any,
}
export type EngineConfiguration = {
	[string]: any,
	EngineSpeed: number?,
}
export type HatchConfiguration = {
	[string]: any,
	SwitchValue: boolean?,
}
export type DispenserConfiguration = {
	[string]: any,
	Filter: string?,
}
export type TractorBeamConfiguration = {
	PowerPercent: number?,
	[string]: any,
}
export type RouterConfiguration = {
	RouterID: string?,
	[string]: any,
}
export type ARControllerConfiguration = {
	[string]: any,
	Transparency: number?,
}
export type IonRocketConfiguration = {
	[string]: any,
	Propulsion: number?,
}
export type MicrocontrollerConfiguration = {
	StartOnSpawn: boolean?,
	[string]: any,
	Code: string?,
}
export type ScreenConfiguration = {
	[string]: any,
	VideoID: number?,
}
export type AnchorConfiguration = {
	Anchored: boolean?,
	[string]: any,
}
export type SwitchConfiguration = {
	[string]: any,
	SwitchValue: boolean?,
}
export type DevGravityGeneratorConfiguration = {
	Enabled: boolean?,
	Gravity: number?,
	Radius: number?,
	[string]: any,
}
export type BoomboxConfiguration = {
	[string]: any,
	Audio: number?,
}
export type DelayWireConfiguration = {
	DelayTime: number?,
	[string]: any,
}
export type DriveBoxConfiguration = {
	Reversal: boolean?,
	[string]: any,
	Ratio: number?,
}
export type BladeConfiguration = {
	[string]: any,
	Shape: BladeShape?,
}
export type BeaconConfiguration = {
	ShowOnMap: boolean?,
	[string]: any,
	BeaconName: string?,
}
export type AntennaConfiguration = {
	AntennaID: string?,
	[string]: any,
}
export type TimeSensorConfiguration = {
	[string]: any,
	Time: string?,
}
export type SpeakerConfiguration = {
	Pitch: number?,
	[string]: any,
	Audio: string?,
	Volume: number?,
}
export type RocketConfiguration = {
	Propulsion: number?,
	[string]: any,
}
export type ApparelConfiguration = {
	Limb: PlayerLimb?,
	Transparency: number?,
	[string]: any,
}
export type MiningLaserConfiguration = {
	[string]: any,
	MaterialToExtract: string?,
}
export type SorterConfiguration = {
	Rate: number?,
	Resource: string?,
	TriggerQuantity: number?,
	[string]: any,
}
export type RailConfiguration = {
	Position1: number?,
	Position2: number?,
	[string]: any,
	TweenTime: number?,
}
export type ScannerConfiguration = {
	[string]: any,
	Range: number?,
}
export type LaserConfiguration = {
	DamageOnlyPlayers: boolean?,
	[string]: any,
}
