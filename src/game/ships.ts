import type { HardpointMount, HardpointSize, ModuleId, ShipClass, ShipId, ShipRole } from "./types";

export type HardpointDef = {
  id: string;
  size: HardpointSize;
  mount: HardpointMount;
};

export type ShipDef = {
  id: ShipId;
  name: string;
  class: ShipClass;
  roles: ShipRole[];
  hardpoints: HardpointDef[];
  utilitySlots: number;
  internalSlots: number;
  cargo: number;
  maxSpeed: number;
  shields: number;
  hull: number;
  jump: number;
  tank: number;
  laser: number;
  price: number;
  turnRate: number;
};

export type ModuleSlot = "hardpoint" | "utility" | "internal";
export type ModuleDef = {
  id: ModuleId;
  name: string;
  slot: ModuleSlot;
  size: HardpointSize | "any";
  price: number;
  description: string;
  laser?: number;
  shields?: number;
  jump?: number;
  cargo?: number;
  scan?: number;
  armor?: number;
  cooling?: number;
  stealth?: number;
  power?: number;
  chaff?: number;
  processor?: number;
  refinery?: number;
  docking?: boolean;
  scoop?: boolean;
  capability?: "ecm" | "eccm" | "hacking";
  drone?: "combat" | "mining" | "scanner";
};

export const MODULES: Record<ModuleId, ModuleDef> = {
  pulse_laser: {
    id: "pulse_laser",
    name: "Pulse Laser",
    slot: "hardpoint",
    size: "small",
    price: 450,
    description: "Reliable close-range weapon.",
    laser: 4,
  },
  beam_laser: {
    id: "beam_laser",
    name: "Beam Laser",
    slot: "hardpoint",
    size: "medium",
    price: 1650,
    description: "High-output energy weapon with stronger sustained fire.",
    laser: 9,
  },
  multicannon: {
    id: "multicannon",
    name: "Multi-Cannon",
    slot: "hardpoint",
    size: "medium",
    price: 1450,
    description: "Kinetic weapon package for reliable hull damage.",
    laser: 7,
  },
  missile_rack: {
    id: "missile_rack",
    name: "Missile Rack",
    slot: "hardpoint",
    size: "medium",
    price: 2200,
    description: "Lock-on ordnance for heavy burst damage.",
    laser: 13,
  },
  mine_launcher: {
    id: "mine_launcher",
    name: "Mine Launcher",
    slot: "hardpoint",
    size: "small",
    price: 950,
    description: "Deploys proximity mines for area denial.",
    laser: 8,
  },
  mining_laser: {
    id: "mining_laser",
    name: "Mining Laser",
    slot: "hardpoint",
    size: "small",
    price: 700,
    description: "Industrial beam tuned for asteroid work.",
    laser: 2,
  },
  prospector_laser: {
    id: "prospector_laser",
    name: "Prospector Laser",
    slot: "hardpoint",
    size: "small",
    price: 820,
    description: "Focused pulse laser for dense vein mapping and fast ore validation.",
    laser: 3,
  },
  excavator_laser: {
    id: "excavator_laser",
    name: "Excavator Laser",
    slot: "hardpoint",
    size: "medium",
    price: 1550,
    description: "Heavy-duty mining beam for larger asteroid clusters and deep strip operations.",
    laser: 6,
  },
  plasma_bomb: {
    id: "plasma_bomb",
    name: "Plasma Bomb",
    slot: "hardpoint",
    size: "medium",
    price: 2100,
    description: "High-yield ordnance for broad blast damage against shielded targets.",
    laser: 16,
  },
  fragmentation_bomb: {
    id: "fragmentation_bomb",
    name: "Fragmentation Bomb",
    slot: "hardpoint",
    size: "small",
    price: 1350,
    description: "Dense fragmentation payload for close-range anti-hull burst damage.",
    laser: 10,
  },
  ore_limpet: {
    id: "ore_limpet",
    name: "Ore Limpet",
    slot: "utility",
    size: "any",
    price: 420,
    description: "Autonomous collector for local ore retrieval and haul assistance.",
    cargo: 2,
  },
  repair_limpet: {
    id: "repair_limpet",
    name: "Repair Limpet",
    slot: "utility",
    size: "any",
    price: 610,
    description: "Deploys micro-repair drones to preserve hull integrity in the field.",
    armor: 4,
  },
  scanner_limpet: {
    id: "scanner_limpet",
    name: "Scanner Limpet",
    slot: "utility",
    size: "any",
    price: 550,
    description: "Probe drone that expands scanning range and local survey detail.",
    scan: 2,
  },
  discovery_scanner: {
    id: "discovery_scanner",
    name: "Discovery Scanner",
    slot: "internal",
    size: "any",
    price: 500,
    description: "Reveals system bodies and starts exploration scans.",
    scan: 1,
  },
  shield_booster: {
    id: "shield_booster",
    name: "Shield Booster",
    slot: "utility",
    size: "any",
    price: 900,
    description: "Projects additional shield capacity.",
    shields: 14,
  },
  shield_array: {
    id: "shield_array",
    name: "Shield Array",
    slot: "internal",
    size: "any",
    price: 1480,
    description: "Reinforced regeneration field giving stronger shield reserves.",
    shields: 22,
  },
  armor_plating: {
    id: "armor_plating",
    name: "Armor Plating",
    slot: "internal",
    size: "any",
    price: 1120,
    description: "Adds layered hull reinforcement for more survivability.",
    armor: 16,
  },
  cooling_springs: {
    id: "cooling_springs",
    name: "Cooling Springs",
    slot: "internal",
    size: "any",
    price: 980,
    description: "Improves thermal management and reduced heat buildup from repeated fire.",
    cooling: 8,
  },
  stealth_mesh: {
    id: "stealth_mesh",
    name: "Stealth Mesh",
    slot: "internal",
    size: "any",
    price: 1350,
    description: "Diffuse sensor signature for cleaner covert operations.",
    stealth: 12,
  },
  energy_grid: {
    id: "energy_grid",
    name: "Energy Grid",
    slot: "utility",
    size: "any",
    price: 1260,
    description: "Boosted power routing for stronger module output and better energy economy.",
    power: 10,
  },
  chaff_launcher: {
    id: "chaff_launcher",
    name: "Chaff Launcher",
    slot: "utility",
    size: "any",
    price: 760,
    description: "Deploys decoys to break missile locks and distract hostile scanners.",
    chaff: 1,
  },
  fuel_scoop: {
    id: "fuel_scoop",
    name: "Fuel Scoop",
    slot: "utility",
    size: "any",
    price: 700,
    description: "Improves long-range jump efficiency.",
    jump: 1.5,
  },
  cargo_rack: {
    id: "cargo_rack",
    name: "Cargo Rack",
    slot: "internal",
    size: "any",
    price: 600,
    description: "Adds four tonnes of protected capacity.",
    cargo: 4,
  },
  detailed_scanner: {
    id: "detailed_scanner",
    name: "Detailed Scanner",
    slot: "internal",
    size: "any",
    price: 800,
    description: "Maps bodies and reveals exploration data.",
    jump: 0.5,
    scan: 2,
  },
  surface_analyzer: {
    id: "surface_analyzer",
    name: "Surface Analyzer",
    slot: "internal",
    size: "any",
    price: 1200,
    description: "Detailed planetary analysis increases data payouts.",
    scan: 3,
  },
  docking_computer: {
    id: "docking_computer",
    name: "Docking Computer",
    slot: "utility",
    size: "any",
    price: 1000,
    description: "Extends safe docking approach range.",
    docking: true,
  },
  ecm_suite: {
    id: "ecm_suite",
    name: "ECM Suite",
    slot: "utility",
    size: "any",
    price: 1100,
    description: "Breaks hostile missile locks and disrupts guidance.",
    capability: "ecm",
  },
  eccm_suite: {
    id: "eccm_suite",
    name: "ECCM Suite",
    slot: "utility",
    size: "any",
    price: 1350,
    description: "Hardens sensors against electronic countermeasures.",
    capability: "eccm",
  },
  hacking_suite: {
    id: "hacking_suite",
    name: "Hacking Suite",
    slot: "utility",
    size: "any",
    price: 1600,
    description: "Interfaces with locked beacons, wrecks, and illicit cargo systems.",
    capability: "hacking",
  },
  ore_processor: {
    id: "ore_processor",
    name: "Ore Processor",
    slot: "internal",
    size: "any",
    price: 1180,
    description: "Refines raw ore into better salvage value during industrial runs.",
    processor: 3,
  },
  refinery_unit: {
    id: "refinery_unit",
    name: "Refinery Unit",
    slot: "internal",
    size: "any",
    price: 1460,
    description: "Adds dedicated refinement throughput for economical material processing.",
    refinery: 4,
  },
  combat_drone: {
    id: "combat_drone",
    name: "Combat Drone",
    slot: "utility",
    size: "any",
    price: 1680,
    description: "Autonomous support drone that adds attack pressure to hostile contacts.",
    laser: 5,
    drone: "combat",
  },
  mining_drone: {
    id: "mining_drone",
    name: "Mining Drone",
    slot: "utility",
    size: "any",
    price: 1460,
    description: "Drone swarms increase mining yield and harvesting efficiency.",
    laser: 2,
    drone: "mining",
  },
  scanner_drone: {
    id: "scanner_drone",
    name: "Scanner Drone",
    slot: "utility",
    size: "any",
    price: 1320,
    description: "Autonomous probe drone for deeper survey sweeps and anomaly detection.",
    scan: 3,
    drone: "scanner",
  },
  salvage_beam: {
    id: "salvage_beam",
    name: "Salvage Beam",
    slot: "utility",
    size: "any",
    price: 950,
    description: "Tractors useful material from wrecked craft.",
    cargo: 2,
  },
};

export const SHIPS: Record<ShipId, ShipDef> = {
  sidewinder: {
    id: "sidewinder",
    name: "Sidewinder",
    class: "scout",
    roles: ["courier", "combat"],
    hardpoints: [
      { id: "S1", size: "small", mount: "fixed" },
      { id: "S2", size: "small", mount: "fixed" },
    ],
    utilitySlots: 1,
    internalSlots: 3,
    cargo: 8,
    maxSpeed: 78,
    shields: 42,
    hull: 50,
    jump: 7,
    tank: 8,
    laser: 9,
    price: 0,
    turnRate: 1.75,
  },
  cobra: {
    id: "cobra",
    name: "Cobra Mk III",
    class: "multipurpose",
    roles: ["courier", "combat"],
    hardpoints: [
      { id: "S1", size: "small", mount: "fixed" },
      { id: "S2", size: "small", mount: "fixed" },
      { id: "M1", size: "medium", mount: "fixed" },
      { id: "M2", size: "medium", mount: "fixed" },
    ],
    utilitySlots: 2,
    internalSlots: 5,
    cargo: 20,
    maxSpeed: 94,
    shields: 72,
    hull: 82,
    jump: 8.5,
    tank: 14,
    laser: 15,
    price: 8200,
    turnRate: 1.45,
  },
  asp: {
    id: "asp",
    name: "Asp Explorer",
    class: "explorer",
    roles: ["exploration", "courier", "combat"],
    hardpoints: [
      { id: "M1", size: "medium", mount: "fixed" },
      { id: "M2", size: "medium", mount: "fixed" },
    ],
    utilitySlots: 2,
    internalSlots: 6,
    cargo: 16,
    maxSpeed: 108,
    shields: 88,
    hull: 74,
    jump: 12.5,
    tank: 18,
    laser: 13,
    price: 17600,
    turnRate: 1.2,
  },
};

export const SHIP_ORDER: ShipId[] = ["sidewinder", "cobra", "asp"];

export const SHIP_CLASS_LABELS: Record<ShipClass, string> = {
  scout: "Scout",
  multipurpose: "Multipurpose",
  explorer: "Explorer",
};

export const SHIP_ROLE_LABELS: Record<ShipRole, string> = {
  courier: "Courier",
  combat: "Combat",
  exploration: "Exploration",
};

export function cargoCapacity(
  shipId: ShipId,
  upgraded: boolean,
  loadout: Record<string, ModuleId> = {},
): number {
  return shipStats(shipId, upgraded, loadout).cargo;
}

export function shipStats(
  shipId: ShipId,
  cargoUpgrade: boolean,
  loadout: Record<string, ModuleId> = {},
): ShipDef {
  const base = SHIPS[shipId];
  const stats = { ...base };
  for (const moduleId of Object.values(loadout)) {
    const module = MODULES[moduleId];
    if (!module) continue;
    stats.laser += module.laser ?? 0;
    stats.shields += module.shields ?? 0;
    stats.hull += module.armor ?? 0;
    stats.laser += module.power ?? 0;
    stats.jump += module.jump ?? 0;
    stats.cargo += module.cargo ?? 0;
  }

  if (cargoUpgrade) stats.cargo += 6;
  return stats;
}

const HARDPOINT_WEAPONS: ModuleId[] = [
  "pulse_laser",
  "beam_laser",
  "multicannon",
  "missile_rack",
  "mine_launcher",
  "mining_laser",
  "prospector_laser",
  "excavator_laser",
  "plasma_bomb",
  "fragmentation_bomb",
];

export function fittedWeapon(loadout: Record<string, ModuleId>): ModuleId | null {
  for (const id of HARDPOINT_WEAPONS) {
    if (Object.values(loadout).includes(id)) return id;
  }
  return null;
}

export function moduleCount(loadout: Record<string, ModuleId>, moduleId: ModuleId): number {
  return Object.values(loadout).filter((id) => id === moduleId).length;
}

export function slotAccepts(slot: string, moduleId: ModuleId): boolean {
  const module = MODULES[moduleId];
  return (
    !!module &&
    ((slot.startsWith("S") || slot.startsWith("M")) ? module.slot === "hardpoint" : slot.startsWith("U") ? module.slot === "utility" : module.slot === "internal") &&
    (module.size === "any" || slot.startsWith(module.size[0]!.toUpperCase()))
  );
}

export function hasModule(loadout: Record<string, ModuleId>, moduleId: ModuleId): boolean {
  return Object.values(loadout).includes(moduleId);
}

export function moduleScanStrength(loadout: Record<string, ModuleId>): number {
  return Object.values(loadout).reduce((strength, moduleId) => strength + (MODULES[moduleId]?.scan ?? 0), 0);
}

export function cargoUsed(cargo: Partial<Record<string, number>>): number {
  let n = 0;
  for (const v of Object.values(cargo)) n += v ?? 0;
  return n;
}
