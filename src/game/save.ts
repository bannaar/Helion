import { HOME_SYSTEM_ID } from "./galaxy";
import { cargoCapacity, SHIPS } from "./ships";
import type { CommanderSave } from "./types";

const KEY = "helion.commander.v1";
const BACKUP = "helion.commander.bak";
export const SAVE_VERSION = 1;

export function defaultSave(name = "JAMESON"): CommanderSave {
  const ship = SHIPS.sidewinder;
  return {
    version: SAVE_VERSION,
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
    cargoUpgrade: false,
  };
}

function migrate(raw: CommanderSave): CommanderSave {
  const base = defaultSave(raw.name);
  return { ...base, ...raw, version: SAVE_VERSION };
}

export function loadSave(): CommanderSave | null {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return null;
    const parsed = JSON.parse(raw) as CommanderSave;
    if (!parsed || typeof parsed !== "object") return null;
    return migrate(parsed);
  } catch {
    try {
      const bak = localStorage.getItem(BACKUP);
      if (!bak) return null;
      return migrate(JSON.parse(bak) as CommanderSave);
    } catch {
      return null;
    }
  }
}

export function writeSave(save: CommanderSave): void {
  try {
    const prev = localStorage.getItem(KEY);
    if (prev) localStorage.setItem(BACKUP, prev);
    localStorage.setItem(KEY, JSON.stringify({ ...save, version: SAVE_VERSION }));
  } catch {
    // private mode / quota — play in-memory
  }
}

export function clearSave(): void {
  try {
    localStorage.removeItem(KEY);
  } catch {
    /* ignore */
  }
}

export function holdFree(save: CommanderSave): number {
  const used = Object.values(save.cargo).reduce((a, b) => a + (b ?? 0), 0);
  return cargoCapacity(save.shipId, save.cargoUpgrade) - used;
}
