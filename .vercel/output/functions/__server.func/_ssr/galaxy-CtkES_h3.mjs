//#region node_modules/.nitro/vite/services/ssr/assets/galaxy-CtkES_h3.js
/** Seeded PRNG — mulberry32 + xmur3. Never use Math.random() for the galaxy. */
function xmur3(str) {
	let h = 1779033703 ^ str.length;
	for (let i = 0; i < str.length; i += 1) {
		h = Math.imul(h ^ str.charCodeAt(i), 3432918353);
		h = h << 13 | h >>> 19;
	}
	h = Math.imul(h ^ h >>> 16, 2246822507);
	h = Math.imul(h ^ h >>> 13, 3266489909);
	return (h ^= h >>> 16) >>> 0;
}
function mulberry32(seed) {
	let a = seed >>> 0;
	return () => {
		a |= 0;
		a = a + 1831565813 | 0;
		let t = Math.imul(a ^ a >>> 15, 1 | a);
		t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
		return ((t ^ t >>> 14) >>> 0) / 4294967296;
	};
}
function randRange(rng, a, b) {
	return a + rng() * (b - a);
}
function randInt(rng, a, b) {
	return Math.floor(randRange(rng, a, b + 1));
}
function pick(rng, arr) {
	return arr[Math.floor(rng() * arr.length)];
}
var GALAXY_SEED = "HELION-1984";
var HOME_SYSTEM_ID = "helion";
var ECONOMIES = [
	"agri",
	"industrial",
	"extraction",
	"refinery",
	"hightech",
	"tourism",
	"military",
	"colony"
];
var GOVERNMENTS = [
	"anarchy",
	"feudal",
	"dictatorship",
	"corporate",
	"democracy",
	"confederacy"
];
var COMMODITIES = [
	{
		id: "food",
		name: "Food",
		base: 6
	},
	{
		id: "textiles",
		name: "Textiles",
		base: 11
	},
	{
		id: "minerals",
		name: "Minerals",
		base: 14
	},
	{
		id: "alloys",
		name: "Alloys",
		base: 26
	},
	{
		id: "machinery",
		name: "Machinery",
		base: 34
	},
	{
		id: "computers",
		name: "Computers",
		base: 48
	},
	{
		id: "luxuries",
		name: "Luxuries",
		base: 68
	},
	{
		id: "liquor",
		name: "Liquor",
		base: 20
	},
	{
		id: "furs",
		name: "Furs",
		base: 38
	},
	{
		id: "gold",
		name: "Gold",
		base: 86
	},
	{
		id: "medicine",
		name: "Medicine",
		base: 42
	},
	{
		id: "hydrogen",
		name: "H-fuel",
		base: 5
	}
];
var COMMODITY_IDS = COMMODITIES.map((c) => c.id);
var PRICE_MOD = {
	agri: {
		food: .42,
		textiles: .62,
		liquor: .58,
		furs: .7,
		machinery: 1.55,
		computers: 1.65,
		alloys: 1.35,
		medicine: 1.2
	},
	industrial: {
		machinery: .52,
		computers: .68,
		alloys: .72,
		textiles: .8,
		food: 1.48,
		minerals: 1.25,
		luxuries: 1.15,
		hydrogen: 1.1
	},
	extraction: {
		minerals: .48,
		gold: .7,
		hydrogen: .6,
		alloys: .85,
		food: 1.3,
		machinery: 1.4,
		computers: 1.55,
		medicine: 1.25
	},
	refinery: {
		alloys: .5,
		hydrogen: .72,
		minerals: .85,
		machinery: .9,
		gold: .95,
		food: 1.35,
		luxuries: 1.2
	},
	hightech: {
		computers: .5,
		machinery: .72,
		medicine: .68,
		luxuries: .85,
		food: 1.6,
		minerals: 1.3,
		furs: 1.2,
		gold: 1.05
	},
	tourism: {
		luxuries: .62,
		liquor: .7,
		furs: .75,
		food: 1.15,
		medicine: 1.1,
		machinery: 1.4,
		minerals: 1.35
	},
	military: {
		machinery: .7,
		computers: .78,
		alloys: .8,
		medicine: .85,
		food: 1.25,
		luxuries: 1.45,
		gold: 1.1
	},
	colony: {
		food: .85,
		hydrogen: .8,
		textiles: .9,
		machinery: 1.3,
		computers: 1.4,
		medicine: 1.15,
		luxuries: 1.5
	}
};
var PREFIX = [
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
	"Ho"
];
var MID = [
	"ve",
	"on",
	"an",
	"er",
	"ix",
	"or",
	"al",
	"en",
	"us",
	"ar",
	"id",
	"el",
	"oo",
	"ai"
];
var SUFF = [
	"",
	"ce",
	"ti",
	"ra",
	"on",
	"is",
	"um",
	"or",
	"ea",
	"ix",
	"ine",
	"hold",
	"aar"
];
var STAR_COLORS = [
	15918788,
	13494015,
	16765096,
	16771232,
	15266047,
	16763060
];
var PLANET_COLORS = [
	6982298,
	8034926,
	9075290,
	5925514,
	10127978,
	5933706,
	8022666
];
var CLUSTER = [
	{
		name: "Helion",
		x: 48,
		y: 24,
		economy: "agri",
		government: "democracy",
		tech: 6
	},
	{
		name: "Zaon",
		x: 54,
		y: 27,
		economy: "industrial",
		government: "corporate",
		tech: 9
	},
	{
		name: "Disor",
		x: 43,
		y: 28,
		economy: "extraction",
		government: "feudal",
		tech: 4
	},
	{
		name: "Reorte",
		x: 50,
		y: 18,
		economy: "refinery",
		government: "dictatorship",
		tech: 7
	},
	{
		name: "Leesti",
		x: 61,
		y: 22,
		economy: "hightech",
		government: "corporate",
		tech: 12
	},
	{
		name: "Tiraor",
		x: 40,
		y: 20,
		economy: "agri",
		government: "anarchy",
		tech: 3
	},
	{
		name: "Uszaa",
		x: 58,
		y: 32,
		economy: "tourism",
		government: "confederacy",
		tech: 8
	},
	{
		name: "Orerra",
		x: 46,
		y: 34,
		economy: "military",
		government: "dictatorship",
		tech: 10
	}
];
function slug(name) {
	return name.toLowerCase().replace(/[^a-z0-9]+/g, "");
}
function pirateThreat(gov) {
	if (gov === "anarchy") return 3;
	if (gov === "feudal") return 2;
	if (gov === "dictatorship") return 1;
	return 0;
}
function makeSystem(s, rng) {
	const tech = s.tech ?? randInt(rng, 1, 12);
	return {
		id: slug(s.name),
		name: s.name,
		x: s.x,
		y: s.y,
		economy: s.economy,
		government: s.government,
		tech,
		population: Math.round(Math.pow(10, 3 + tech * .4 + rng() * 1.4)),
		starColor: pick(rng, STAR_COLORS),
		planetColor: pick(rng, PLANET_COLORS),
		pirateThreat: pirateThreat(s.government)
	};
}
function generateName(rng, used) {
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
var cached = null;
function getGalaxy() {
	if (cached) return cached;
	const rng = mulberry32(xmur3(GALAXY_SEED));
	const used = /* @__PURE__ */ new Set();
	const systems = [];
	for (const s of CLUSTER) {
		used.add(slug(s.name));
		systems.push(makeSystem(s, rng));
	}
	let guard = 0;
	while (systems.length < 64 && guard < 8e3) {
		guard += 1;
		const x = randRange(rng, 4, 92);
		const y = randRange(rng, 4, 44);
		if (systems.some((s) => Math.hypot(s.x - x, s.y - y) < 3.2)) continue;
		const name = generateName(rng, used);
		systems.push(makeSystem({
			name,
			x: Math.round(x * 10) / 10,
			y: Math.round(y * 10) / 10,
			economy: pick(rng, ECONOMIES),
			government: pick(rng, GOVERNMENTS),
			tech: randInt(rng, 1, 12)
		}, rng));
	}
	cached = systems;
	return systems;
}
function getSystem(id) {
	return getGalaxy().find((s) => s.id === id);
}
function systemDistance(a, b) {
	return Math.hypot(a.x - b.x, a.y - b.y);
}
function jumpFuelCost(fromId, toId) {
	const a = getSystem(fromId);
	const b = getSystem(toId);
	if (!a || !b) return 99;
	return Math.max(.4, Math.round(systemDistance(a, b) * 12) / 100);
}
function marketTemplate(system) {
	const rng = mulberry32(xmur3(`${GALAXY_SEED}:${system.id}:mkt`));
	return COMMODITIES.map((c) => {
		const mod = PRICE_MOD[system.economy][c.id] ?? 1;
		const noise = .88 + rng() * .24;
		const price = Math.max(2, Math.round(c.base * mod * noise));
		const stock = mod < .9 ? randInt(rng, 80, 220) : mod > 1.2 ? randInt(rng, 6, 40) : randInt(rng, 24, 90);
		return {
			commodity: c.id,
			name: c.name,
			price,
			stock,
			base: c.base
		};
	});
}
function economyLabel(e) {
	return {
		agri: "Agricultural",
		industrial: "Industrial",
		extraction: "Extraction",
		refinery: "Refinery",
		hightech: "High Tech",
		tourism: "Tourism",
		military: "Military",
		colony: "Colony"
	}[e];
}
function governmentLabel(g) {
	return {
		anarchy: "Anarchy",
		feudal: "Feudal",
		dictatorship: "Dictatorship",
		corporate: "Corporate",
		democracy: "Democracy",
		confederacy: "Confederacy"
	}[g];
}
var COMBAT_RANKS = [
	"Harmless",
	"Mostly Harmless",
	"Poor",
	"Average",
	"Above Average",
	"Competent",
	"Dangerous",
	"Deadly",
	"Elite"
];
function combatRank(kills) {
	const t = [
		0,
		1,
		3,
		7,
		14,
		26,
		48,
		80,
		140
	];
	let rank = "Harmless";
	for (let i = 0; i < t.length; i += 1) if (kills >= t[i]) rank = COMBAT_RANKS[i];
	return rank;
}
//#endregion
export { getGalaxy as a, jumpFuelCost as c, economyLabel as i, marketTemplate as l, HOME_SYSTEM_ID as n, getSystem as o, combatRank as r, governmentLabel as s, COMMODITY_IDS as t, systemDistance as u };
