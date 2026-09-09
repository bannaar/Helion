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
};
