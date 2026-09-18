import { HOME_SYSTEM_ID } from "./galaxy";
import { cargoCapacity, SHIPS } from "./ships";
import type { CommanderSave } from "./types";

const KEY = "helion.commander.v1";
const BACKUP = "helion.commander.bak";
export const SAVE_VERSION = 2;

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
    loadout: { S1: "pulse_laser" },
    exploredSystems: {},
    salvageRecovered: 0,
    explorationData: 0,
    activeMission: null,
    completedMissions: 0,
    reputation: { federation: 0, empire: 0, union: 0, "free-traders": 0, pirates: -10 },
  };
}

function migrate(raw: CommanderSave): CommanderSave {
  const base = defaultSave(raw.name);
  const activeMission = raw.activeMission
    ? {
        ...raw.activeMission,
        requirement: raw.activeMission.requirement ?? raw.activeMission.quantity,
        progressAtAccept: raw.activeMission.progressAtAccept ?? 0,
      }
    : null;
  return {
    ...base,
    ...raw,
    activeMission,
    loadout: raw.loadout ?? base.loadout,
    exploredSystems: raw.exploredSystems ?? base.exploredSystems,
    salvageRecovered: raw.salvageRecovered ?? base.salvageRecovered,
    explorationData: raw.explorationData ?? base.explorationData,
    reputation: { ...base.reputation, ...(raw.reputation ?? {}) },
    version: SAVE_VERSION,
  };
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
  return cargoCapacity(save.shipId, save.cargoUpgrade, save.loadout) - used;
}
