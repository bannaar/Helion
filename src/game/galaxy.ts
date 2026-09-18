import { mulberry32, pick, randInt, randRange, xmur3 } from "./rng";
import type {
  CommodityId,
  Economy,
  FactionId,
  Government,
  MarketRow,
  StarSystem,
} from "./types";

export const GALAXY_SEED = "HELION-1984";
export const HOME_SYSTEM_ID = "helion";
export const SYSTEM_COUNT = 64;

export const ECONOMIES: readonly Economy[] = [
  "agri",
  "industrial",
  "extraction",
  "refinery",
  "hightech",
  "tourism",
  "military",
  "colony",
];

export const GOVERNMENTS: readonly Government[] = [
  "anarchy",
  "feudal",
  "dictatorship",
  "corporate",
  "democracy",
  "confederacy",
];

export const COMMODITIES: readonly {
  id: CommodityId;
  name: string;
  base: number;
}[] = [
  { id: "food", name: "Food", base: 6 },
  { id: "textiles", name: "Textiles", base: 11 },
  { id: "minerals", name: "Minerals", base: 14 },
  { id: "alloys", name: "Alloys", base: 26 },
  { id: "machinery", name: "Machinery", base: 34 },
  { id: "computers", name: "Computers", base: 48 },
  { id: "luxuries", name: "Luxuries", base: 68 },
  { id: "liquor", name: "Liquor", base: 20 },
  { id: "furs", name: "Furs", base: 38 },
  { id: "gold", name: "Gold", base: 86 },
  { id: "medicine", name: "Medicine", base: 42 },
  { id: "hydrogen", name: "H-fuel", base: 5 },
];

export const COMMODITY_IDS = COMMODITIES.map((c) => c.id);

const PRICE_MOD: Record<Economy, Partial<Record<CommodityId, number>>> = {
  agri: {
    food: 0.42,
    textiles: 0.62,
    liquor: 0.58,
    furs: 0.7,
    machinery: 1.55,
    computers: 1.65,
    alloys: 1.35,
    medicine: 1.2,
  },
  industrial: {
    machinery: 0.52,
    computers: 0.68,
    alloys: 0.72,
    textiles: 0.8,
    food: 1.48,
    minerals: 1.25,
    luxuries: 1.15,
    hydrogen: 1.1,
  },
  extraction: {
    minerals: 0.48,
    gold: 0.7,
    hydrogen: 0.6,
    alloys: 0.85,
    food: 1.3,
    machinery: 1.4,
    computers: 1.55,
    medicine: 1.25,
  },
  refinery: {
    alloys: 0.5,
    hydrogen: 0.72,
    minerals: 0.85,
    machinery: 0.9,
    gold: 0.95,
    food: 1.35,
    luxuries: 1.2,
  },
  hightech: {
    computers: 0.5,
    machinery: 0.72,
    medicine: 0.68,
    luxuries: 0.85,
    food: 1.6,
    minerals: 1.3,
    furs: 1.2,
    gold: 1.05,
  },
  tourism: {
    luxuries: 0.62,
    liquor: 0.7,
    furs: 0.75,
    food: 1.15,
    medicine: 1.1,
    machinery: 1.4,
    minerals: 1.35,
  },
  military: {
    machinery: 0.7,
    computers: 0.78,
    alloys: 0.8,
    medicine: 0.85,
    food: 1.25,
    luxuries: 1.45,
    gold: 1.1,
  },
  colony: {
    food: 0.85,
    hydrogen: 0.8,
    textiles: 0.9,
    machinery: 1.3,
    computers: 1.4,
    medicine: 1.15,
    luxuries: 1.5,
  },
};

const PREFIX = [
  "La",
  "Za",
  "Di",
  "Re",
  "Or",
  "Ti",
  "Xe",
  "Qu",
  "Ae",
  "Ve",
  "Is",
  "Ka",
  "Mo",
  "Ri",
  "Sa",
  "Te",
  "Un",
  "Wi",
  "Yo",
  "Be",
  "Ce",
  "Fa",
  "Ge",
  "Ho",
];
const MID = ["ve", "on", "an", "er", "ix", "or", "al", "en", "us", "ar", "id", "el", "oo", "ai"];
const SUFF = ["", "ce", "ti", "ra", "on", "is", "um", "or", "ea", "ix", "ine", "hold", "aar"];

const STAR_COLORS = [0xf2e6c4, 0xcde6ff, 0xffd0a8, 0xffe8a0, 0xe8f0ff, 0xffc8b4];
const PLANET_COLORS = [0x6a8a9a, 0x7a9a6e, 0x8a7a5a, 0x5a6a8a, 0x9a8a6a, 0x5a8a8a, 0x7a6a8a];

type SeededSystem = {
  name: string;
  x: number;
  y: number;
  economy: Economy;
  government: Government;
  tech?: number;
};

const CLUSTER: SeededSystem[] = [
  { name: "Helion", x: 48, y: 24, economy: "agri", government: "democracy", tech: 6 },
  { name: "Zaon", x: 54, y: 27, economy: "industrial", government: "corporate", tech: 9 },
  { name: "Disor", x: 43, y: 28, economy: "extraction", government: "feudal", tech: 4 },
  { name: "Reorte", x: 50, y: 18, economy: "refinery", government: "dictatorship", tech: 7 },
  { name: "Leesti", x: 61, y: 22, economy: "hightech", government: "corporate", tech: 12 },
  { name: "Tiraor", x: 40, y: 20, economy: "agri", government: "anarchy", tech: 3 },
  { name: "Uszaa", x: 58, y: 32, economy: "tourism", government: "confederacy", tech: 8 },
  { name: "Orerra", x: 46, y: 34, economy: "military", government: "dictatorship", tech: 10 },
];

function slug(name: string): string {
  return name.toLowerCase().replace(/[^a-z0-9]+/g, "");
}

function pirateThreat(gov: Government): number {
  if (gov === "anarchy") return 3;
  if (gov === "feudal") return 2;
  if (gov === "dictatorship") return 1;
  return 0;
}

function makeSystem(s: SeededSystem, rng: () => number): StarSystem {
  const tech = s.tech ?? randInt(rng, 1, 12);
  return {
    id: slug(s.name),
    name: s.name,
    x: s.x,
    y: s.y,
    economy: s.economy,
    government: s.government,
    tech,
    population: Math.round(Math.pow(10, 3 + tech * 0.4 + rng() * 1.4)),
    starColor: pick(rng, STAR_COLORS),
    planetColor: pick(rng, PLANET_COLORS),
    pirateThreat: pirateThreat(s.government),
    alienThreat: Math.max(0, Math.floor(Math.hypot(s.x - 50, s.y - 25) / 10)),
  };
}

function generateName(rng: () => number, used: Set<string>): string {
  for (let i = 0; i < 40; i += 1) {
    const n = `${pick(rng, PREFIX)}${pick(rng, MID)}${pick(rng, SUFF)}`;
    const id = slug(n);
    if (!used.has(id) && n.length >= 4) {
      used.add(id);
      return n;
    }
  }
  const fallback = `Sector ${randInt(rng, 100, 999)}`;
  used.add(slug(fallback));
  return fallback;
}

let cached: StarSystem[] | null = null;

export function getGalaxy(): StarSystem[] {
  if (cached) return cached;
  const rng = mulberry32(xmur3(GALAXY_SEED));
  const used = new Set<string>();
  const systems: StarSystem[] = [];

  for (const s of CLUSTER) {
    used.add(slug(s.name));
    systems.push(makeSystem(s, rng));
  }

  let guard = 0;
  while (systems.length < SYSTEM_COUNT && guard < 8000) {
    guard += 1;
    const x = randRange(rng, 4, 92);
    const y = randRange(rng, 4, 44);
    const tooClose = systems.some((s) => Math.hypot(s.x - x, s.y - y) < 3.2);
    if (tooClose) continue;
    const name = generateName(rng, used);
    systems.push(
      makeSystem(
        {
          name,
          x: Math.round(x * 10) / 10,
          y: Math.round(y * 10) / 10,
          economy: pick(rng, ECONOMIES),
          government: pick(rng, GOVERNMENTS),
          tech: randInt(rng, 1, 12),
        },
        rng,
      ),
    );
  }

  cached = systems;
  return systems;
}

export function getSystem(id: string): StarSystem | undefined {
  return getGalaxy().find((s) => s.id === id);
}

export function systemDistance(a: StarSystem, b: StarSystem): number {
  return Math.hypot(a.x - b.x, a.y - b.y);
}

export function jumpFuelCost(fromId: string, toId: string): number {
  const a = getSystem(fromId);
  const b = getSystem(toId);
  if (!a || !b) return 99;
  return Math.max(0.4, Math.round(systemDistance(a, b) * 12) / 100);
}


export function commodityMeta(id: CommodityId) {
  return COMMODITIES.find((c) => c.id === id)!;
}

export function marketTemplate(system: StarSystem): MarketRow[] {
  const rng = mulberry32(xmur3(`${GALAXY_SEED}:${system.id}:mkt`));
  return COMMODITIES.map((c) => {
    const mod = PRICE_MOD[system.economy][c.id] ?? 1;
    const noise = 0.88 + rng() * 0.24;
    const price = Math.max(2, Math.round(c.base * mod * noise));
    const producer = mod < 0.9;
    const stock = producer
      ? randInt(rng, 80, 220)
      : mod > 1.2
        ? randInt(rng, 6, 40)
        : randInt(rng, 24, 90);
    return { commodity: c.id, name: c.name, price, stock, base: c.base };
  });
}

export function economyLabel(e: Economy): string {
  const map: Record<Economy, string> = {
    agri: "Agricultural",
    industrial: "Industrial",
    extraction: "Extraction",
    refinery: "Refinery",
    hightech: "High Tech",
    tourism: "Tourism",
    military: "Military",
    colony: "Colony",
  };
  return map[e];
}

export function governmentLabel(g: Government): string {
  const map: Record<Government, string> = {
    anarchy: "Anarchy",
    feudal: "Feudal",
    dictatorship: "Dictatorship",
    corporate: "Corporate",
    democracy: "Democracy",
    confederacy: "Confederacy",
  };
  return map[g];
}

export function systemFaction(system: StarSystem): FactionId {
  if (system.government === "corporate" || system.government === "dictatorship") return "empire";
  if (system.government === "confederacy" || system.government === "democracy") return "federation";
  if (system.economy === "extraction" || system.economy === "refinery") return "union";
  return "free-traders";
}

export const FACTION_LABELS: Record<FactionId, string> = {
  federation: "Federation",
  empire: "Empire",
  union: "Mining Union",
  "free-traders": "Free Traders",
  pirates: "Pirate Clans",
};

export function factionPrice(base: number, standing: number): number {
  const clamped = Math.max(-100, Math.min(100, standing));
  return Math.max(1, Math.round(base * (1 - clamped * 0.0015)));
}

export const COMBAT_RANKS = [
  "Harmless",
  "Mostly Harmless",
  "Poor",
  "Average",
  "Above Average",
  "Competent",
  "Dangerous",
  "Deadly",
  "Elite",
] as const;

export function combatRank(kills: number): (typeof COMBAT_RANKS)[number] {
  const t = [0, 1, 3, 7, 14, 26, 48, 80, 140];
  let rank: (typeof COMBAT_RANKS)[number] = "Harmless";
  for (let i = 0; i < t.length; i += 1) {
    if (kills >= t[i]!) rank = COMBAT_RANKS[i]!;
  }
  return rank;
}
