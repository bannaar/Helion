import { o as __toESM } from "../_runtime.mjs";
import { a as getGalaxy, c as jumpFuelCost, i as economyLabel, n as HOME_SYSTEM_ID, o as getSystem, r as combatRank, s as governmentLabel, t as COMMODITY_IDS, u as systemDistance } from "./galaxy-CtkES_h3.mjs";
import { R as require_react, v as require_jsx_runtime } from "../_libs/@tanstack/react-router+[...].mjs";
import { n as TSS_SERVER_FUNCTION, r as getServerFnById, t as createServerFn } from "./ssr.mjs";
import { t as create } from "../_libs/zustand.mjs";
import { a as string, i as object, r as number, t as _enum } from "../_libs/zod.mjs";
import { n as clsx, t as cva } from "../_libs/class-variance-authority+clsx.mjs";
import { t as twMerge } from "../_libs/tailwind-merge.mjs";
//#region node_modules/.nitro/vite/services/ssr/assets/routes-C8RGMmzr.js
var import_react = /* @__PURE__ */ __toESM(require_react());
var import_jsx_runtime = require_jsx_runtime();
var __defProp = Object.defineProperty;
var __exportAll = (all, no_symbols) => {
	let target = {};
	for (var name in all) __defProp(target, name, {
		get: all[name],
		enumerable: true
	});
	if (!no_symbols) __defProp(target, Symbol.toStringTag, { value: "Module" });
	return target;
};
/** Procedural Web Audio SFX — unlock on the first gesture. */
var ctx = null;
var master = null;
var sfx = null;
var muted = false;
function ac() {
	if (typeof window === "undefined") return null;
	if (!ctx) {
		ctx = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: "interactive" });
		master = ctx.createGain();
		sfx = ctx.createGain();
		sfx.gain.value = .28;
		master.gain.value = muted ? 0 : .8;
		sfx.connect(master);
		master.connect(ctx.destination);
	}
	return ctx;
}
function unlockAudio() {
	const c = ac();
	if (!c) return;
	if (c.state === "suspended") c.resume();
}
function setMuted(v) {
	muted = v;
	if (master && ctx) master.gain.setTargetAtTime(v ? 0 : .8, ctx.currentTime, .03);
}
function isMuted() {
	return muted;
}
function beep(freq, dur, type, gain = .2, slide = 0) {
	const c = ac();
	if (!c || !sfx || c.state !== "running") return;
	const osc = c.createOscillator();
	const g = c.createGain();
	osc.type = type;
	osc.frequency.setValueAtTime(freq, c.currentTime);
	if (slide) osc.frequency.exponentialRampToValueAtTime(Math.max(40, freq + slide), c.currentTime + dur);
	g.gain.setValueAtTime(1e-4, c.currentTime);
	g.gain.exponentialRampToValueAtTime(gain, c.currentTime + .012);
	g.gain.exponentialRampToValueAtTime(1e-4, c.currentTime + dur);
	osc.connect(g);
	g.connect(sfx);
	osc.start();
	osc.stop(c.currentTime + dur + .02);
	osc.onended = () => {
		osc.disconnect();
		g.disconnect();
	};
}
var sfxPlay = {
	laser: () => beep(920, .07, "square", .12, -400),
	hit: () => beep(180, .14, "sawtooth", .18, -80),
	hull: () => beep(90, .22, "square", .22, -40),
	dock: () => beep(420, .28, "triangle", .16, 180),
	jump: () => beep(140, .7, "sawtooth", .2, 520),
	scoop: () => beep(640, .12, "sine", .14, 200),
	ui: () => beep(520, .05, "square", .08, 0),
	warn: () => beep(240, .18, "square", .14, 0),
	kill: () => beep(310, .35, "triangle", .16, -220)
};
if (typeof window !== "undefined") {
	window.addEventListener("pointerdown", unlockAudio, { once: true });
	window.addEventListener("keydown", unlockAudio, { once: true });
	document.addEventListener("visibilitychange", () => {
		if (!document.hidden) unlockAudio();
	});
}
var SHIPS = {
	sidewinder: {
		id: "sidewinder",
		name: "Sidewinder",
		cargo: 8,
		maxSpeed: 78,
		shields: 42,
		hull: 50,
		jump: 7,
		tank: 8,
		laser: 9,
		price: 0,
		turnRate: 1.75
	},
	cobra: {
		id: "cobra",
		name: "Cobra Mk III",
		cargo: 20,
		maxSpeed: 94,
		shields: 72,
		hull: 82,
		jump: 8.5,
		tank: 14,
		laser: 15,
		price: 8200,
		turnRate: 1.45
	},
	asp: {
		id: "asp",
		name: "Asp Explorer",
		cargo: 16,
		maxSpeed: 108,
		shields: 88,
		hull: 74,
		jump: 12.5,
		tank: 18,
		laser: 13,
		price: 17600,
		turnRate: 1.2
	}
};
var SHIP_ORDER = [
	"sidewinder",
	"cobra",
	"asp"
];
function cargoCapacity(shipId, upgraded) {
	return SHIPS[shipId].cargo + (upgraded ? 6 : 0);
}
function cargoUsed(cargo) {
	let n = 0;
	for (const v of Object.values(cargo)) n += v ?? 0;
	return n;
}
var KEY = "helion.commander.v1";
var BACKUP = "helion.commander.bak";
function defaultSave(name = "JAMESON") {
	const ship = SHIPS.sidewinder;
	return {
		version: 1,
		name: name.trim().slice(0, 16).toUpperCase() || "JAMESON",
		credits: 1500,
		systemId: HOME_SYSTEM_ID,
		fuel: 7,
		cargo: { food: 2 },
		shipId: "sidewinder",
		hull: ship.hull,
		shields: ship.shields,
		kills: 0,
		wanted: false,
		docked: false,
		cargoUpgrade: false
	};
}
function migrate(raw) {
	return {
		...defaultSave(raw.name),
		...raw,
		version: 1
	};
}
function loadSave() {
	try {
		const raw = localStorage.getItem(KEY);
		if (!raw) return null;
		const parsed = JSON.parse(raw);
		if (!parsed || typeof parsed !== "object") return null;
		return migrate(parsed);
	} catch {
		try {
			const bak = localStorage.getItem(BACKUP);
			if (!bak) return null;
			return migrate(JSON.parse(bak));
		} catch {
			return null;
		}
	}
}
function writeSave(save) {
	try {
		const prev = localStorage.getItem(KEY);
		if (prev) localStorage.setItem(BACKUP, prev);
		localStorage.setItem(KEY, JSON.stringify({
			...save,
			version: 1
		}));
	} catch {}
}
function clearSave() {
	try {
		localStorage.removeItem(KEY);
	} catch {}
}
function bootSave() {
	if (typeof window === "undefined") return defaultSave();
	return loadSave() ?? defaultSave();
}
var useGameStore = create((set, get) => {
	const save = bootSave();
	const def = SHIPS[save.shipId];
	return {
		mode: "title",
		paused: false,
		save,
		speed: 0,
		throttle: 0,
		yaw: 0,
		pitch: 0,
		shields: save.shields,
		hull: save.hull,
		maxShields: def.shields,
		maxHull: def.hull,
		targetName: null,
		targetDist: 0,
		contacts: [],
		canDock: false,
		massLocked: false,
		jumpLocked: null,
		jumpCharge: 0,
		alert: "",
		flash: null,
		market: null,
		news: [],
		tick: 0,
		laserHeat: 0,
		setMode: (mode) => set({
			mode,
			paused: false
		}),
		setPaused: (paused) => set({ paused }),
		patchSave: (partial, persist = true) => {
			const next = {
				...get().save,
				...partial
			};
			set({ save: next });
			if (persist) writeSave(next);
		},
		replaceSave: (next, persist = true) => {
			const defn = SHIPS[next.shipId];
			set({
				save: next,
				shields: next.shields,
				hull: next.hull,
				maxShields: defn.shields,
				maxHull: defn.hull
			});
			if (persist) writeSave(next);
		},
		persist: () => writeSave(get().save),
		setFlight: (p) => set(p),
		setJump: (id) => set({
			jumpLocked: id,
			jumpCharge: 0
		}),
		setJumpCharge: (v) => set({ jumpCharge: v }),
		setAlert: (alert) => set({ alert }),
		setFlash: (text) => set({ flash: {
			text,
			at: Date.now()
		} }),
		setMarket: (market, tick) => set(tick !== void 0 ? {
			market,
			tick
		} : { market }),
		setNews: (news) => set({ news })
	};
});
var createSsrRpc = (functionId) => {
	const url = "/_serverFn/" + functionId;
	const serverFnMeta = { id: functionId };
	const fn = async (...args) => {
		return (await getServerFnById(functionId, { origin: "server" }))(...args);
	};
	return Object.assign(fn, {
		url,
		serverFnMeta,
		[TSS_SERVER_FUNCTION]: true
	});
};
var commodity = string().refine((v) => COMMODITY_IDS.includes(v), { message: "Unknown cargo" });
var getMarketFn = createServerFn({ method: "GET" }).validator(object({ systemId: string().min(1).max(40) })).handler(createSsrRpc("288f4ca1033eaca325be254228a6d6591c1bcfda0d3429e068f1e2631ef6ddac"));
var getBoardFn = createServerFn({ method: "GET" }).validator(object({ systemId: string().min(1).max(40) })).handler(createSsrRpc("4d3f8d23e6aba35868770752f96cfb6f5bd489ad54e370832a44bedbfebbba4a"));
var executeTradeFn = createServerFn({ method: "POST" }).validator(object({
	systemId: string().min(1).max(40),
	commodity,
	side: _enum(["buy", "sell"]),
	qty: number().int().min(1).max(20)
})).handler(createSsrRpc("af6f6a26d0a73f6c38a47b4d752eb4bae4abc8ae9ee97e60450f821a6ca17f4f"));
var logTrafficFn = createServerFn({ method: "POST" }).validator(object({
	systemId: string().min(1).max(40),
	kind: string().min(1).max(20),
	detail: string().min(1).max(160)
})).handler(createSsrRpc("ccde32018b33552d98b0d8485000e88c094562617493fb1c052c01eff27aa3a2"));
function cn(...inputs) {
	return twMerge(clsx(inputs));
}
var buttonVariants = cva("inline-flex items-center justify-center gap-2 font-medium tracking-wide uppercase transition-opacity duration-150 ease-out disabled:pointer-events-none disabled:opacity-40 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent/70", {
	variants: {
		variant: {
			primary: "bg-accent text-accent-fg hover:opacity-90 active:scale-[0.98]",
			ghost: "bg-transparent text-fg border border-border hover:bg-surface-2",
			danger: "bg-danger text-fg hover:opacity-90",
			quiet: "bg-surface-2 text-fg border border-border hover:border-accent/40"
		},
		size: {
			md: "h-11 px-5 text-sm rounded-md",
			sm: "h-9 px-3 text-xs rounded-sm",
			lg: "h-12 px-6 text-base rounded-lg",
			icon: "size-11 rounded-md"
		}
	},
	defaultVariants: {
		variant: "primary",
		size: "md"
	}
});
function Button({ className, variant, size, ...props }) {
	return /* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
		className: cn(buttonVariants({
			variant,
			size
		}), className),
		...props
	});
}
function GalaxyChart({ engine }) {
	const save = useGameStore((s) => s.save);
	const jumpLocked = useGameStore((s) => s.jumpLocked);
	const setMode = useGameStore((s) => s.setMode);
	const systems = (0, import_react.useMemo)(() => getGalaxy(), []);
	const here = getSystem(save.systemId);
	const selected = getSystem(jumpLocked ?? save.systemId);
	const range = SHIPS[save.shipId].jump;
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
		className: "absolute inset-0 z-20 flex flex-col bg-bg/92 p-4 pt-[max(1rem,env(safe-area-inset-top))] sm:p-8",
		children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
			className: "mx-auto flex w-full max-w-5xl items-end justify-between gap-4",
			children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", { children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
				className: "font-mono text-xs tracking-[0.3em] text-muted",
				children: "GALACTIC CHART"
			}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("h2", {
				className: "font-display text-3xl font-semibold tracking-[0.12em]",
				children: "LANES"
			})] }), /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
				variant: "ghost",
				onClick: () => setMode("space"),
				children: "Close"
			})]
		}), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
			className: "mx-auto mt-4 grid min-h-0 w-full max-w-5xl flex-1 grid-rows-[1fr_auto] gap-4 lg:grid-cols-[1fr_18rem] lg:grid-rows-1",
			children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
				className: "relative min-h-[280px] overflow-hidden rounded-xl border border-border bg-surface",
				children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("svg", {
					viewBox: "0 0 96 48",
					className: "h-full w-full",
					role: "img",
					"aria-label": "Galaxy map",
					children: [
						here ? systems.filter((s) => systemDistance(here, s) <= range && s.id !== here.id).map((s) => /* @__PURE__ */ (0, import_jsx_runtime.jsx)("line", {
							x1: here.x,
							y1: here.y,
							x2: s.x,
							y2: s.y,
							stroke: "currentColor",
							className: "text-accent/25",
							strokeWidth: "0.18"
						}, `l-${s.id}`)) : null,
						here ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("circle", {
							cx: here.x,
							cy: here.y,
							r: range,
							fill: "none",
							stroke: "currentColor",
							className: "text-accent/30",
							strokeWidth: "0.22",
							strokeDasharray: "0.8 0.6"
						}) : null,
						systems.map((s) => {
							const d = here ? systemDistance(here, s) : 99;
							const inRange = !!here && d <= range + .05;
							const isHere = s.id === save.systemId;
							const isSel = s.id === selected?.id;
							return /* @__PURE__ */ (0, import_jsx_runtime.jsx)("g", {
								className: "cursor-pointer",
								onClick: () => engine?.lockJump(s.id),
								children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)("circle", {
									cx: s.x,
									cy: s.y,
									r: isHere ? 1.15 : isSel ? .95 : .7,
									fill: "currentColor",
									className: isHere ? "text-accent" : inRange ? "text-fg" : "text-muted/50"
								})
							}, s.id);
						})
					]
				})
			}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("aside", {
				className: "rounded-xl border border-border bg-surface p-4",
				children: selected ? /* @__PURE__ */ (0, import_jsx_runtime.jsxs)(import_jsx_runtime.Fragment, { children: [
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
						className: "font-mono text-xs tracking-[0.25em] text-muted",
						children: "SYSTEM"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h3", {
						className: "mt-1 font-display text-2xl font-semibold",
						children: selected.name
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("dl", {
						className: "mt-3 space-y-1 font-mono text-xs text-muted",
						children: [
							/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
								className: "flex justify-between gap-3",
								children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("dt", { children: "Economy" }), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("dd", {
									className: "text-fg",
									children: economyLabel(selected.economy)
								})]
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
								className: "flex justify-between gap-3",
								children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("dt", { children: "Government" }), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("dd", {
									className: "text-fg",
									children: governmentLabel(selected.government)
								})]
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
								className: "flex justify-between gap-3",
								children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("dt", { children: "Tech" }), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("dd", {
									className: "text-fg",
									children: selected.tech
								})]
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
								className: "flex justify-between gap-3",
								children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("dt", { children: "Range" }), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("dd", {
									className: "text-fg",
									children: here ? `${systemDistance(here, selected).toFixed(1)} ly` : "—"
								})]
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
								className: "flex justify-between gap-3",
								children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("dt", { children: "Fuel" }), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("dd", {
									className: "text-fg",
									children: [jumpFuelCost(save.systemId, selected.id).toFixed(1), " t"]
								})]
							})
						]
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
						className: "mt-4 w-full",
						disabled: selected.id === save.systemId,
						onClick: () => {
							engine?.lockJump(selected.id);
							setMode("space");
							useGameStore.getState().setFlash(`DESTINATION LOCKED  —  ${selected.name.toUpperCase()}`);
						},
						children: "Lock destination"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
						className: "mt-2 font-mono text-[11px] text-muted",
						children: "Then press J in flight to charge the drive."
					})
				] }) : /* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
					className: "text-muted",
					children: "Select a star."
				})
			})]
		})]
	});
}
function Corner({ className }) {
	return /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", { className: `pointer-events-none absolute size-3 border-accent/70 ${className}` });
}
function Bar({ value, max, tone }) {
	const pct = Math.max(0, Math.min(100, value / Math.max(1, max) * 100));
	return /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
		className: "h-1.5 w-full overflow-hidden rounded-xs bg-border",
		children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
			className: `h-full ${tone === "ok" ? "bg-ok" : tone === "danger" ? "bg-danger" : "bg-fg/80"}`,
			style: { width: `${pct}%` }
		})
	});
}
function Scanner({ contacts }) {
	const dots = (0, import_react.useMemo)(() => {
		const scale = 420;
		return contacts.map((c) => {
			const x = 50 + c.localX / scale * 46;
			const y = 50 - c.localZ / scale * 46;
			return {
				...c,
				x: Math.max(4, Math.min(96, x)),
				y: Math.max(6, Math.min(94, y))
			};
		});
	}, [contacts]);
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
		className: "relative h-[108px] w-[148px] overflow-hidden rounded-md border border-border bg-bg/80",
		children: [
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", { className: "absolute inset-1 rounded-sm border border-accent/20" }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", { className: "absolute left-1/2 top-1/2 h-8 w-8 -translate-x-1/2 -translate-y-1/2 rounded-full border border-accent/15" }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", { className: "absolute left-[8%] right-[8%] top-1/2 h-px bg-accent/15" }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", { className: "absolute bottom-[10%] left-1/2 top-[10%] w-px bg-accent/15" }),
			dots.map((d) => /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
				className: d.kind === "pirate" ? "absolute size-1.5 -translate-x-1/2 -translate-y-1/2 bg-danger" : d.kind === "police" ? "absolute size-1.5 -translate-x-1/2 -translate-y-1/2 bg-fg" : d.kind === "station" ? "absolute size-2 -translate-x-1/2 -translate-y-1/2 rotate-45 border border-accent" : d.kind === "canister" ? "absolute size-1.5 -translate-x-1/2 -translate-y-1/2 bg-warn" : "absolute size-1 -translate-x-1/2 -translate-y-1/2 bg-muted",
				style: {
					left: `${d.x}%`,
					top: `${d.y}%`
				}
			}, d.id)),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", { className: "absolute bottom-1/2 left-1/2 size-1.5 -translate-x-1/2 translate-y-1/2 bg-accent" })
		]
	});
}
function Hud() {
	const speed = useGameStore((s) => s.speed);
	const throttle = useGameStore((s) => s.throttle);
	const shields = useGameStore((s) => s.shields);
	const hull = useGameStore((s) => s.hull);
	const maxShields = useGameStore((s) => s.maxShields);
	const maxHull = useGameStore((s) => s.maxHull);
	const save = useGameStore((s) => s.save);
	const targetName = useGameStore((s) => s.targetName);
	const targetDist = useGameStore((s) => s.targetDist);
	const contacts = useGameStore((s) => s.contacts);
	const alert = useGameStore((s) => s.alert);
	const flash = useGameStore((s) => s.flash);
	const canDock = useGameStore((s) => s.canDock);
	const massLocked = useGameStore((s) => s.massLocked);
	const jumpLocked = useGameStore((s) => s.jumpLocked);
	const jumpCharge = useGameStore((s) => s.jumpCharge);
	const laserHeat = useGameStore((s) => s.laserHeat);
	const sys = getSystem(save.systemId);
	const def = SHIPS[save.shipId];
	const used = cargoUsed(save.cargo);
	const cap = cargoCapacity(save.shipId, save.cargoUpgrade);
	const dest = jumpLocked ? getSystem(jumpLocked) : null;
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
		className: "pointer-events-none absolute inset-0 font-mono text-[11px] tracking-wide text-fg",
		children: [
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Corner, { className: "left-4 top-4 border-l border-t" }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Corner, { className: "right-4 top-4 border-r border-t" }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Corner, { className: "bottom-4 left-4 border-b border-l" }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Corner, { className: "bottom-4 right-4 border-b border-r" }),
			/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
				className: "absolute left-4 top-4 max-w-[58%] sm:left-6 sm:top-6",
				children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
					className: "font-display text-lg font-semibold tracking-[0.28em] text-accent sm:text-xl",
					children: sys?.name.toUpperCase() ?? "VOID"
				}), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
					className: "mt-1 text-muted",
					children: [
						save.shipId.toUpperCase(),
						" · ",
						combatRank(save.kills),
						save.wanted ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
							className: "ml-2 text-danger",
							children: "WANTED"
						}) : /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", {
							className: "ml-2",
							children: "CLEAN"
						})
					]
				})]
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
				className: "absolute right-4 top-4 text-right sm:right-6 sm:top-6",
				children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
					className: "text-accent tabular-nums",
					children: [save.credits.toLocaleString(), " CR"]
				}), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
					className: "text-muted",
					children: [
						"FUEL ",
						save.fuel.toFixed(1),
						"/",
						def.tank,
						" · HOLD ",
						used,
						"/",
						cap
					]
				})]
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
				className: "absolute left-1/2 top-[18%] w-[min(90%,28rem)] -translate-x-1/2 text-center",
				children: flash && Date.now() - flash.at < 2200 ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
					className: "text-sm tracking-[0.2em] text-accent",
					children: flash.text
				}) : alert ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
					className: "text-sm tracking-[0.18em] text-warn",
					children: alert
				}) : null
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
				className: "absolute bottom-4 left-4 w-[min(46vw,16rem)] space-y-2 sm:bottom-6 sm:left-6",
				children: [
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "flex justify-between text-muted",
						children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("span", { children: ["SPD ", speed.toFixed(0)] }), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("span", { children: [
							"THR ",
							(throttle * 100).toFixed(0),
							"%"
						] })]
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Bar, {
						value: speed,
						max: def.maxSpeed,
						tone: "fg"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "flex justify-between text-muted",
						children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", { children: "SHLD" }), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("span", {
							className: "tabular-nums",
							children: [
								Math.max(0, shields).toFixed(0),
								"/",
								maxShields
							]
						})]
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Bar, {
						value: shields,
						max: maxShields,
						tone: "ok"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "flex justify-between text-muted",
						children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", { children: "HULL" }), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("span", {
							className: "tabular-nums",
							children: [
								Math.max(0, hull).toFixed(0),
								"/",
								maxHull
							]
						})]
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Bar, {
						value: hull,
						max: maxHull,
						tone: hull < maxHull * .35 ? "danger" : "fg"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
						className: "flex justify-between text-muted",
						children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)("span", { children: "LASER" })
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Bar, {
						value: laserHeat,
						max: 1,
						tone: "danger"
					})
				]
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
				className: "absolute bottom-4 right-4 flex flex-col items-end gap-2 sm:bottom-6 sm:right-6",
				children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
					className: "rounded-md border border-border bg-bg/70 px-3 py-2 text-right",
					children: [
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
							className: "text-muted",
							children: "TARGET"
						}),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
							className: "text-fg",
							children: targetName ?? "—"
						}),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
							className: "tabular-nums text-accent",
							children: targetDist ? `${targetDist.toFixed(0)} m` : ""
						}),
						dest ? /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
							className: "mt-1 text-muted",
							children: [
								"LOCK ",
								dest.name,
								jumpCharge > 0 ? `  ${(jumpCharge * 100).toFixed(0)}%` : ""
							]
						}) : null,
						massLocked ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
							className: "text-warn",
							children: "MASS LOCK"
						}) : null,
						canDock ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
							className: "text-accent",
							children: "DOCKING COMPUTER"
						}) : null
					]
				}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
					className: "hidden sm:block",
					children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Scanner, { contacts })
				})]
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
				className: "absolute bottom-3 left-1/2 hidden -translate-x-1/2 text-[10px] text-muted md:block",
				children: "W/S throttle · A/D yaw · R/F pitch · Q/E roll · SPACE fire · H dock · M chart · J jump"
			})
		]
	});
}
function StationDock({ engine }) {
	const save = useGameStore((s) => s.save);
	const market = useGameStore((s) => s.market);
	const news = useGameStore((s) => s.news);
	const tick = useGameStore((s) => s.tick);
	const setMode = useGameStore((s) => s.setMode);
	const [tab, setTab] = (0, import_react.useState)("market");
	const [busy, setBusy] = (0, import_react.useState)(false);
	const [err, setErr] = (0, import_react.useState)("");
	const sys = getSystem(save.systemId);
	const def = SHIPS[save.shipId];
	const used = cargoUsed(save.cargo);
	const cap = cargoCapacity(save.shipId, save.cargoUpgrade);
	(0, import_react.useEffect)(() => {
		let live = true;
		Promise.all([getMarketFn({ data: { systemId: save.systemId } }), getBoardFn({ data: { systemId: save.systemId } })]).then(([m, b]) => {
			if (!live) return;
			useGameStore.getState().setMarket(m.market, m.tick);
			useGameStore.getState().setNews(b.news);
		}).catch(() => {
			if (live) setErr("Station net is dark. Local prices unavailable.");
		});
		return () => {
			live = false;
		};
	}, [save.systemId]);
	async function trade(row, side) {
		setBusy(true);
		setErr("");
		try {
			if (side === "buy") {
				if (save.credits < row.price) throw new Error("Insufficient credits");
				if (used >= cap) throw new Error("Hold is full");
			} else if ((save.cargo[row.commodity] ?? 0) < 1) throw new Error("None in hold");
			const res = await executeTradeFn({ data: {
				systemId: save.systemId,
				commodity: row.commodity,
				side,
				qty: 1
			} });
			const st = useGameStore.getState();
			const cargo = { ...st.save.cargo };
			if (side === "buy") {
				cargo[row.commodity] = (cargo[row.commodity] ?? 0) + 1;
				st.patchSave({
					credits: st.save.credits - res.unitPrice,
					cargo
				});
			} else {
				const n = (cargo[row.commodity] ?? 0) - 1;
				if (n <= 0) delete cargo[row.commodity];
				else cargo[row.commodity] = n;
				st.patchSave({
					credits: st.save.credits + res.unitPrice,
					cargo
				});
			}
			const next = (st.market ?? []).map((m) => m.commodity === row.commodity ? {
				...m,
				price: res.price,
				stock: res.stock
			} : m);
			st.setMarket(next, res.tick);
			sfxPlay.ui();
		} catch (e) {
			setErr(e instanceof Error ? e.message : "Trade refused");
			sfxPlay.warn();
		} finally {
			setBusy(false);
		}
	}
	function repair() {
		const missing = Math.max(0, def.hull - save.hull) + Math.max(0, def.shields - save.shields);
		const cost = Math.ceil(missing * 2.5);
		if (cost <= 0) return;
		if (save.credits < cost) {
			setErr("Cannot afford repairs");
			return;
		}
		useGameStore.getState().patchSave({
			credits: save.credits - cost,
			hull: def.hull,
			shields: def.shields
		});
		useGameStore.getState().setFlight({
			hull: def.hull,
			shields: def.shields
		});
		sfxPlay.dock();
	}
	function refuel() {
		const need = Math.max(0, def.tank - save.fuel);
		const cost = Math.ceil(need) * (market?.find((m) => m.commodity === "hydrogen")?.price ?? 8);
		if (need < .05) return;
		if (save.credits < cost) {
			setErr("Cannot afford fuel");
			return;
		}
		useGameStore.getState().patchSave({
			credits: save.credits - cost,
			fuel: def.tank
		});
		sfxPlay.ui();
	}
	function buyShip(id) {
		const next = SHIPS[id];
		if (id === save.shipId) return;
		const tradeIn = Math.round(def.price * .55);
		const cost = Math.max(0, next.price - tradeIn);
		if (save.credits < cost) {
			setErr("Insufficient credits");
			return;
		}
		const nextCap = cargoCapacity(id, save.cargoUpgrade);
		if (used > nextCap) {
			setErr("Dump cargo before transferring hull");
			return;
		}
		useGameStore.getState().patchSave({
			shipId: id,
			credits: save.credits - cost,
			hull: next.hull,
			shields: next.shields
		});
		useGameStore.getState().setFlight({
			hull: next.hull,
			shields: next.shields,
			maxHull: next.hull,
			maxShields: next.shields
		});
		sfxPlay.dock();
	}
	function buyCargoUpgrade() {
		if (save.cargoUpgrade) return;
		if (save.credits < 2400) {
			setErr("Need 2,400 CR");
			return;
		}
		useGameStore.getState().patchSave({
			credits: save.credits - 2400,
			cargoUpgrade: true
		});
		sfxPlay.ui();
	}
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
		className: "absolute inset-0 z-20 flex flex-col bg-bg/90 pt-[max(0.75rem,env(safe-area-inset-top))]",
		children: [
			/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("header", {
				className: "mx-auto flex w-full max-w-5xl items-end justify-between gap-3 px-4 py-3",
				children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", { children: [
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
						className: "font-mono text-xs tracking-[0.3em] text-muted",
						children: ["CORIOLIS · TICK ", tick]
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h2", {
						className: "font-display text-3xl font-semibold tracking-[0.08em]",
						children: sys?.name ?? "Station"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
						className: "mt-1 font-mono text-xs text-muted",
						children: sys ? `${economyLabel(sys.economy)} · ${governmentLabel(sys.government)} · Tech ${sys.tech}` : ""
					})
				] }), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
					className: "text-right font-mono text-xs",
					children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
						className: "text-lg tabular-nums text-accent",
						children: [save.credits.toLocaleString(), " CR"]
					}), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
						className: "text-muted",
						children: [
							"Hold ",
							used,
							"/",
							cap,
							" · Fuel ",
							save.fuel.toFixed(1)
						]
					})]
				})]
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("nav", {
				className: "mx-auto flex w-full max-w-5xl gap-2 px-4",
				children: [
					"market",
					"yard",
					"board"
				].map((t) => /* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
					onClick: () => setTab(t),
					className: `h-10 rounded-md px-4 font-mono text-xs uppercase tracking-[0.18em] ${tab === t ? "bg-accent text-accent-fg" : "border border-border bg-surface text-muted"}`,
					children: t
				}, t))
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
				className: "mx-auto mt-3 min-h-0 w-full max-w-5xl flex-1 overflow-auto px-4 pb-28",
				children: [
					err ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
						className: "mb-3 font-mono text-xs text-danger",
						children: err
					}) : null,
					tab === "market" ? /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "overflow-hidden rounded-xl border border-border bg-surface",
						children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("table", {
							className: "w-full text-left font-mono text-xs",
							children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("thead", {
								className: "bg-surface-2 text-muted",
								children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("tr", { children: [
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", {
										className: "px-3 py-2 font-medium",
										children: "Commodity"
									}),
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", {
										className: "px-3 py-2 font-medium",
										children: "CR/t"
									}),
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", {
										className: "hidden px-3 py-2 font-medium sm:table-cell",
										children: "Stock"
									}),
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", {
										className: "px-3 py-2 font-medium",
										children: "Hold"
									}),
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)("th", { className: "px-3 py-2 font-medium" })
								] })
							}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)("tbody", { children: (market ?? []).map((row) => {
								const hot = row.price > row.base * 1.2;
								const cheap = row.price < row.base * .8;
								return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("tr", {
									className: "border-t border-border",
									children: [
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", {
											className: "px-3 py-2 text-fg",
											children: row.name
										}),
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", {
											className: `px-3 py-2 tabular-nums ${hot ? "text-danger" : cheap ? "text-accent" : ""}`,
											children: row.price
										}),
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", {
											className: "hidden px-3 py-2 tabular-nums text-muted sm:table-cell",
											children: row.stock
										}),
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", {
											className: "px-3 py-2 tabular-nums",
											children: save.cargo[row.commodity] ?? 0
										}),
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)("td", {
											className: "px-3 py-2 text-right",
											children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
												className: "flex justify-end gap-1",
												children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
													size: "sm",
													variant: "quiet",
													disabled: busy,
													onClick: () => void trade(row, "buy"),
													children: "Buy"
												}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
													size: "sm",
													variant: "ghost",
													disabled: busy,
													onClick: () => void trade(row, "sell"),
													children: "Sell"
												})]
											})
										})
									]
								}, row.commodity);
							}) })]
						}), !market ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
							className: "px-3 py-6 text-center text-muted",
							children: "Awaiting market feed…"
						}) : null]
					}) : null,
					tab === "yard" ? /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "grid gap-3 sm:grid-cols-2",
						children: [/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("section", {
							className: "rounded-xl border border-border bg-surface p-4",
							children: [
								/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h3", {
									className: "font-display text-lg",
									children: "Services"
								}),
								/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
									className: "mt-2 font-mono text-xs text-muted",
									children: "Repair hull/shields · Refuel from station hydrogen"
								}),
								/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
									className: "mt-4 flex flex-wrap gap-2",
									children: [
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
											variant: "quiet",
											onClick: repair,
											children: "Repair"
										}),
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
											variant: "quiet",
											onClick: refuel,
											children: "Refuel"
										}),
										/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
											variant: "quiet",
											disabled: save.cargoUpgrade,
											onClick: buyCargoUpgrade,
											children: save.cargoUpgrade ? "Hold expanded" : "Expand hold 2,400 CR"
										})
									]
								})
							]
						}), SHIP_ORDER.map((id) => {
							const s = SHIPS[id];
							const owned = id === save.shipId;
							const tradeIn = Math.round(def.price * .55);
							const cost = Math.max(0, s.price - tradeIn);
							return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("section", {
								className: "rounded-xl border border-border bg-surface p-4",
								children: [
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h3", {
										className: "font-display text-lg",
										children: s.name
									}),
									/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
										className: "mt-1 font-mono text-xs text-muted",
										children: [
											"Hold ",
											s.cargo,
											"t · Jump ",
											s.jump,
											" ly · Speed ",
											s.maxSpeed
										]
									}),
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
										className: "mt-3 font-mono text-sm text-accent",
										children: owned ? "Current hull" : cost === 0 ? "Transfer" : `${cost.toLocaleString()} CR`
									}),
									/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
										className: "mt-3",
										variant: owned ? "ghost" : "primary",
										disabled: owned,
										onClick: () => buyShip(id),
										children: owned ? "Fitted" : "Transfer"
									})
								]
							}, id);
						})]
					}) : null,
					tab === "board" ? /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "rounded-xl border border-border bg-surface p-4",
						children: [
							/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h3", {
								className: "font-display text-lg",
								children: "GalNet"
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
								className: "mt-1 font-mono text-xs text-muted",
								children: "Shared persistent board. Prices move when anyone trades."
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsx)("ul", {
								className: "mt-4 space-y-2 font-mono text-sm",
								children: news.length ? news.map((n, i) => /* @__PURE__ */ (0, import_jsx_runtime.jsx)("li", {
									className: "border-l border-accent/40 pl-3 text-fg",
									children: n
								}, i)) : /* @__PURE__ */ (0, import_jsx_runtime.jsx)("li", {
									className: "text-muted",
									children: "No dispatches yet."
								})
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
								className: "mt-6 font-mono text-xs text-muted",
								children: "First run: buy cheap Food here if Helion is agricultural, jump to Zaon, sell, return with Machinery."
							})
						]
					}) : null
				]
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
				className: "absolute inset-x-0 bottom-0 border-t border-border bg-surface/95 p-3 pb-[max(0.75rem,env(safe-area-inset-bottom))]",
				children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
					className: "mx-auto flex max-w-5xl gap-2",
					children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
						className: "flex-1",
						onClick: () => {
							unlockAudio();
							engine?.launch();
						},
						children: "Launch"
					}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
						variant: "ghost",
						onClick: () => setMode("map"),
						children: "Chart"
					})]
				})
			})
		]
	});
}
function TouchControls({ input, engine }) {
	const mode = useGameStore((s) => s.mode);
	const canDock = useGameStore((s) => s.canDock);
	const setMode = useGameStore((s) => s.setMode);
	const stick = (0, import_react.useRef)(null);
	const pid = (0, import_react.useRef)(null);
	if (mode !== "space" || !input) return null;
	const onDown = (e) => {
		if (pid.current !== null) return;
		pid.current = e.pointerId;
		e.currentTarget.setPointerCapture(e.pointerId);
		move(e);
	};
	const move = (e) => {
		if (!stick.current || !input) return;
		if (pid.current !== null && e.pointerId !== pid.current) return;
		const r = stick.current.getBoundingClientRect();
		const x = (e.clientX - r.left) / r.width * 2 - 1;
		const y = (e.clientY - r.top) / r.height * 2 - 1;
		input.touchYaw = Math.max(-1, Math.min(1, -x));
		input.touchPitch = Math.max(-1, Math.min(1, -y));
	};
	const end = (e) => {
		if (pid.current !== e.pointerId) return;
		pid.current = null;
		if (input) {
			input.touchYaw = 0;
			input.touchPitch = 0;
		}
	};
	const hold = (key, v) => ({
		onPointerDown: (e) => {
			e.preventDefault();
			if (!input) return;
			if (key === "touchThrust") input.touchThrust = v;
			else input.touchFire = Boolean(v);
		},
		onPointerUp: () => {
			if (!input) return;
			if (key === "touchThrust") input.touchThrust = 0;
			else input.touchFire = false;
		},
		onPointerCancel: () => {
			if (!input) return;
			if (key === "touchThrust") input.touchThrust = 0;
			else input.touchFire = false;
		}
	});
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
		className: "pointer-events-none absolute inset-x-0 bottom-0 z-10 flex items-end justify-between p-3 pb-[max(0.75rem,env(safe-area-inset-bottom))] md:hidden",
		children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
			ref: stick,
			className: "pointer-events-auto size-[132px] rounded-full border border-border bg-surface/70",
			onPointerDown: onDown,
			onPointerMove: move,
			onPointerUp: end,
			onPointerCancel: end,
			style: { touchAction: "none" },
			children: /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
				className: "flex h-full items-center justify-center font-mono text-[10px] tracking-widest text-muted",
				children: "STEER"
			})
		}), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
			className: "pointer-events-auto grid grid-cols-2 gap-2",
			children: [
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
					className: "h-12 min-w-14 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg",
					...hold("touchThrust", 1),
					children: "THR+"
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
					className: "h-12 min-w-14 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg",
					...hold("touchThrust", -1),
					children: "THR-"
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
					className: "h-12 min-w-14 rounded-md bg-accent font-mono text-xs text-accent-fg",
					...hold("touchFire", true),
					children: "FIRE"
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
					className: "h-12 min-w-14 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg",
					onPointerDown: () => {
						if (canDock) input.touchDock = true;
					},
					children: "DOCK"
				}),
				/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
					className: "col-span-2 h-11 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg",
					onClick: () => {
						setMode("map");
						engine?.lockJump(useGameStore.getState().jumpLocked);
					},
					children: "CHART"
				})
			]
		})]
	});
}
function HelionApp() {
	const canvasRef = (0, import_react.useRef)(null);
	const engineRef = (0, import_react.useRef)(null);
	const [ready, setReady] = (0, import_react.useState)(false);
	const [name, setName] = (0, import_react.useState)("JAMESON");
	const [hasSave, setHasSave] = (0, import_react.useState)(false);
	const [muted, setMutedUi] = (0, import_react.useState)(false);
	const mode = useGameStore((s) => s.mode);
	const paused = useGameStore((s) => s.paused);
	const save = useGameStore((s) => s.save);
	(0, import_react.useEffect)(() => {
		setHasSave(!!loadSave());
		let engine = null;
		let cancelled = false;
		const canvas = canvasRef.current;
		if (!canvas) return;
		import("./engine-XNc2KR_A.mjs").then(({ HelionEngine }) => {
			if (cancelled) return;
			engine = new HelionEngine(canvas);
			engineRef.current = engine;
			engine.start();
			setReady(true);
			if (new URLSearchParams(window.location.search).has("qa")) startNew("JAMESON", engine);
		});
		const persist = () => useGameStore.getState().persist();
		document.addEventListener("visibilitychange", () => {
			if (document.hidden) persist();
		});
		window.addEventListener("pagehide", persist);
		return () => {
			cancelled = true;
			persist();
			engine?.dispose();
			engineRef.current = null;
		};
	}, []);
	function applySaveToFlight(next) {
		const def = SHIPS[next.shipId];
		useGameStore.getState().replaceSave(next);
		useGameStore.getState().setFlight({
			hull: next.hull,
			shields: next.shields,
			maxHull: def.hull,
			maxShields: def.shields
		});
	}
	function startNew(cmdr, eng = engineRef.current) {
		unlockAudio();
		applySaveToFlight(defaultSave(cmdr));
		eng?.enterSystem(HOME_SYSTEM_ID, "spawn");
		useGameStore.getState().setMode("space");
		useGameStore.getState().setFlash("STATION BEARING MARKED  —  FLY IN, H TO DOCK");
	}
	function continueSave() {
		unlockAudio();
		const next = loadSave() ?? defaultSave(name);
		applySaveToFlight(next);
		const eng = engineRef.current;
		eng?.enterSystem(next.systemId, next.docked ? "undock" : "spawn");
		if (next.docked) {
			useGameStore.getState().setMode("station");
			eng?.setMode("station");
		} else useGameStore.getState().setMode("space");
	}
	function rebuy() {
		unlockAudio();
		const fee = Math.max(400, Math.round(save.credits * .12));
		if (save.credits < fee) {
			clearSave();
			useGameStore.getState().replaceSave(defaultSave("JAMESON"), false);
			useGameStore.getState().setMode("title");
			engineRef.current?.setMode("title");
			setHasSave(false);
			return;
		}
		const def = SHIPS[save.shipId];
		const next = {
			...save,
			credits: save.credits - fee,
			hull: def.hull,
			shields: def.shields,
			docked: true,
			cargo: {}
		};
		applySaveToFlight(next);
		engineRef.current?.enterSystem(next.systemId, "undock");
		engineRef.current?.setMode("station");
		useGameStore.getState().setMode("station");
	}
	return /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("main", {
		className: "relative h-[100dvh] w-full overflow-hidden bg-bg text-fg",
		style: { touchAction: "none" },
		children: [
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("canvas", {
				ref: canvasRef,
				className: "absolute inset-0 h-full w-full"
			}),
			mode === "title" ? /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
				className: "absolute inset-0 z-20 flex flex-col justify-end bg-gradient-to-t from-bg via-bg/70 to-transparent px-5 pb-[max(2rem,env(safe-area-inset-bottom))] pt-16 sm:justify-center sm:px-16",
				children: [
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
						className: "font-mono text-xs tracking-[0.42em] text-accent",
						children: "PERSISTENT UNIVERSE"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h1", {
						className: "mt-2 font-display text-6xl font-semibold tracking-[0.22em] sm:text-8xl",
						children: "HELION"
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
						className: "mt-4 max-w-md text-sm leading-relaxed text-muted sm:text-base",
						children: "Wireframe space. Shared markets. Buy low in the agri lanes, sell high in the industrial core, and try not to become a footnote on GalNet."
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("label", {
						className: "mt-8 block max-w-sm font-mono text-xs tracking-[0.2em] text-muted",
						children: ["Commander", /* @__PURE__ */ (0, import_jsx_runtime.jsx)("input", {
							value: name,
							onChange: (e) => setName(e.target.value.toUpperCase()),
							maxLength: 16,
							className: "mt-2 h-12 w-full rounded-md border border-border bg-surface px-3 font-mono text-sm tracking-[0.18em] text-fg outline-none focus:ring-2 focus:ring-accent/60",
							"aria-label": "Commander name"
						})]
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "mt-5 flex max-w-sm flex-col gap-2 sm:flex-row",
						children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
							className: "flex-1",
							disabled: !ready,
							onClick: () => startNew(name),
							children: "Start"
						}), hasSave ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
							className: "flex-1",
							variant: "ghost",
							disabled: !ready,
							onClick: continueSave,
							children: "Continue"
						}) : null]
					}),
					/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
						className: "mt-6 max-w-lg font-mono text-[11px] leading-relaxed text-muted",
						children: "W/S throttle · A/D yaw left/right · R/F pitch · Q/E roll · Space fire · H dock · M chart · J jump"
					})
				]
			}) : null,
			mode === "space" ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Hud, {}) : null,
			mode === "station" ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)(StationDock, { engine: engineRef.current }) : null,
			mode === "map" ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)(GalaxyChart, { engine: engineRef.current }) : null,
			mode === "dead" ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
				className: "absolute inset-0 z-30 flex items-center justify-center bg-bg/80 p-6",
				children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
					className: "w-full max-w-md rounded-xl border border-border bg-surface p-6",
					children: [
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("p", {
							className: "font-mono text-xs tracking-[0.3em] text-danger",
							children: "SHIP LOST"
						}),
						/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h2", {
							className: "mt-2 font-display text-3xl font-semibold",
							children: "Rebuy board"
						}),
						/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("p", {
							className: "mt-3 text-sm text-muted",
							children: [
								"Insurance will restore your hull for ",
								Math.max(400, Math.round(save.credits * .12)).toLocaleString(),
								" CR. Cargo is gone."
							]
						}),
						/* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
							className: "mt-6 flex gap-2",
							children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
								className: "flex-1",
								onClick: rebuy,
								children: "Rebuy"
							}), /* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
								className: "flex-1",
								variant: "ghost",
								onClick: () => {
									clearSave();
									useGameStore.getState().setMode("title");
									engineRef.current?.setMode("title");
								},
								children: "Resign"
							})]
						})
					]
				})
			}) : null,
			paused && mode === "space" ? /* @__PURE__ */ (0, import_jsx_runtime.jsx)("div", {
				className: "absolute inset-0 z-30 flex items-center justify-center bg-bg/70 p-6",
				children: /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
					className: "w-full max-w-sm rounded-xl border border-border bg-surface p-6",
					children: [/* @__PURE__ */ (0, import_jsx_runtime.jsx)("h2", {
						className: "font-display text-2xl",
						children: "Paused"
					}), /* @__PURE__ */ (0, import_jsx_runtime.jsxs)("div", {
						className: "mt-5 flex flex-col gap-2",
						children: [
							/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
								onClick: () => useGameStore.getState().setPaused(false),
								children: "Resume"
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
								variant: "ghost",
								onClick: () => useGameStore.getState().setMode("map"),
								children: "Chart"
							}),
							/* @__PURE__ */ (0, import_jsx_runtime.jsx)(Button, {
								variant: "ghost",
								onClick: () => {
									useGameStore.getState().persist();
									useGameStore.getState().setMode("title");
									engineRef.current?.setMode("title");
									setHasSave(true);
								},
								children: "Exit to title"
							})
						]
					})]
				})
			}) : null,
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)(TouchControls, {
				input: engineRef.current?.input ?? null,
				engine: engineRef.current
			}),
			/* @__PURE__ */ (0, import_jsx_runtime.jsx)("button", {
				className: "absolute right-3 top-[max(0.75rem,env(safe-area-inset-top))] z-40 hidden h-9 rounded-sm border border-border bg-surface/80 px-3 font-mono text-[10px] tracking-widest text-muted sm:block",
				onClick: () => {
					const next = !isMuted();
					setMuted(next);
					setMutedUi(next);
				},
				children: muted ? "SOUND OFF" : "SOUND ON"
			})
		]
	});
}
var routes_exports = /* @__PURE__ */ __exportAll({ component: () => Home });
function Home() {
	return /* @__PURE__ */ (0, import_jsx_runtime.jsx)(HelionApp, {});
}
//#endregion
export { cargoCapacity as a, SHIPS as i, logTrafficFn as n, cargoUsed as o, useGameStore as r, sfxPlay as s, routes_exports as t };
