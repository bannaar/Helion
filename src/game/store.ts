import { create } from "zustand";
import { combatRank, getSystem } from "./galaxy";
import { cargoCapacity, cargoUsed, SHIPS, shipStats } from "./ships";
import { defaultSave, loadSave, writeSave } from "./save";
import type { CommanderSave, Contact, GameMode, MarketRow } from "./types";

export type HudFlash = { text: string; at: number };

export type GameState = {
  mode: GameMode;
  paused: boolean;
  save: CommanderSave;
  speed: number;
  throttle: number;
  dampeners: boolean;
  yaw: number;
  pitch: number;
  shields: number;
  hull: number;
  maxShields: number;
  maxHull: number;
  targetName: string | null;
  targetDist: number;
  contacts: Contact[];
  canDock: boolean;
  massLocked: boolean;
  jumpLocked: string | null;
  jumpCharge: number;
  alert: string;
  flash: HudFlash | null;
  market: MarketRow[] | null;
  news: string[];
  tick: number;
  laserHeat: number;
  setMode: (mode: GameMode) => void;
  setPaused: (v: boolean) => void;
  patchSave: (partial: Partial<CommanderSave>, persist?: boolean) => void;
  replaceSave: (save: CommanderSave, persist?: boolean) => void;
  persist: () => void;
  setFlight: (
    p: Partial<
      Pick<
        GameState,
        | "speed"
        | "throttle"
        | "dampeners"
        | "yaw"
        | "pitch"
        | "shields"
        | "hull"
        | "maxShields"
        | "maxHull"
        | "targetName"
        | "targetDist"
        | "contacts"
        | "canDock"
        | "massLocked"
        | "laserHeat"
      >
    >,
  ) => void;
  setJump: (id: string | null) => void;
  setJumpCharge: (v: number) => void;
  setAlert: (text: string) => void;
  setFlash: (text: string) => void;
  setMarket: (rows: MarketRow[] | null, tick?: number) => void;
  setNews: (lines: string[]) => void;
};

function bootSave(): CommanderSave {
  if (typeof window === "undefined") return defaultSave();
  return loadSave() ?? defaultSave();
}

export const useGameStore = create<GameState>((set, get) => {
  const save = bootSave();
  const def = shipStats(save.shipId, save.cargoUpgrade, save.loadout);
  return {
    mode: "title",
    paused: false,
    save,
    speed: 0,
    throttle: 0,
    dampeners: true,
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
    setMode: (mode) => set({ mode, paused: false }),
    setPaused: (paused) => set({ paused }),
    patchSave: (partial, persist = true) => {
      const next = { ...get().save, ...partial };
      set({ save: next });
      if (persist) writeSave(next);
    },
    replaceSave: (next, persist = true) => {
      const defn = shipStats(next.shipId, next.cargoUpgrade, next.loadout);
      set({
        save: next,
        shields: next.shields,
        hull: next.hull,
        maxShields: defn.shields,
        maxHull: defn.hull,
      });
      if (persist) writeSave(next);
    },
    persist: () => writeSave(get().save),
    setFlight: (p) => set(p),
    setJump: (id) => set({ jumpLocked: id, jumpCharge: 0 }),
    setJumpCharge: (v) => set({ jumpCharge: v }),
    setAlert: (alert) => set({ alert }),
    setFlash: (text) => set({ flash: { text, at: Date.now() } }),
    setMarket: (market, tick) => set(tick !== undefined ? { market, tick } : { market }),
    setNews: (news) => set({ news }),
  };
});

export function currentSystemName(): string {
  const id = useGameStore.getState().save.systemId;
  return getSystem(id)?.name ?? id;
}

export function holdStats() {
  const s = useGameStore.getState().save;
  return {
    used: cargoUsed(s.cargo),
    cap: cargoCapacity(s.shipId, s.cargoUpgrade, s.loadout),
  };
}

export function rankLabel(): string {
  return combatRank(useGameStore.getState().save.kills);
}
