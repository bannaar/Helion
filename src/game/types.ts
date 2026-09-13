export type Economy =
  | "agri"
  | "industrial"
  | "extraction"
  | "refinery"
  | "hightech"
  | "tourism"
  | "military"
  | "colony";

export type Government =
  | "anarchy"
  | "feudal"
  | "dictatorship"
  | "corporate"
  | "democracy"
  | "confederacy";

export type CommodityId =
  | "food"
  | "textiles"
  | "minerals"
  | "alloys"
  | "machinery"
  | "computers"
  | "luxuries"
  | "liquor"
  | "furs"
  | "gold"
  | "medicine"
  | "hydrogen";

export type ShipId = "sidewinder" | "cobra" | "asp";
export type ModuleId =
  | "pulse_laser"
  | "beam_laser"
  | "multicannon"
  | "missile_rack"
  | "mine_launcher"
  | "plasma_bomb"
  | "fragmentation_bomb"
  | "mining_laser"
  | "prospector_laser"
  | "excavator_laser"
  | "ore_limpet"
  | "repair_limpet"
  | "scanner_limpet"
  | "discovery_scanner"
  | "surface_analyzer"
  | "docking_computer"
  | "ecm_suite"
  | "eccm_suite"
  | "hacking_suite"
  | "shield_booster"
  | "shield_array"
  | "armor_plating"
  | "cooling_springs"
  | "stealth_mesh"
  | "energy_grid"
  | "chaff_launcher"
  | "fuel_scoop"
  | "cargo_rack"
  | "ore_processor"
  | "refinery_unit"
  | "combat_drone"
  | "mining_drone"
  | "scanner_drone"
  | "detailed_scanner"
  | "salvage_beam";

export type ShipClass = "scout" | "multipurpose" | "explorer";
export type ShipRole = "courier" | "combat" | "exploration";
export type HardpointSize = "small" | "medium";
export type HardpointMount = "fixed" | "gimbal";

export type GameMode = "title" | "space" | "station" | "map" | "dead";

export type MarketRow = {
  commodity: CommodityId;
  name: string;
  price: number;
  stock: number;
  base: number;
};

export type Dispatch = {
  tick: number;
  systemId: string | null;
  headline: string;
};

export type TrafficEvent = {
  systemId: string;
  kind: string;
  detail: string;
};

export type ContactKind = "station" | "planet" | "star" | "pirate" | "police" | "canister";

export type Contact = {
  id: string;
  kind: ContactKind;
  localX: number;
  localZ: number;
  dist: number;
};

export type StarSystem = {
  id: string;
  name: string;
  x: number;
  y: number;
  economy: Economy;
  government: Government;
  tech: number;
  population: number;
  starColor: number;
  planetColor: number;
  pirateThreat: number;
};

export type CargoHold = Partial<Record<CommodityId, number>>;

export type MissionType = "courier" | "mining" | "exploration" | "salvage";

export type Mission = {
  id: string;
  type: MissionType;
  originId: string;
  destinationId: string;
  cargo: CommodityId;
  quantity: number;
  reward: number;
  acceptedAt: number;
  requirement: number;
  progressAtAccept: number;
};

export type CommanderSave = {
  version: number;
  name: string;
  credits: number;
  systemId: string;
  fuel: number;
  cargo: CargoHold;
  shipId: ShipId;
  hull: number;
  shields: number;
  kills: number;
  wanted: boolean;
  docked: boolean;
  cargoUpgrade: boolean;
  loadout: Record<string, ModuleId>;
  exploredSystems: Record<string, boolean>;
  salvageRecovered: number;
  explorationData: number;
  activeMission: Mission | null;
  completedMissions: number;
};
