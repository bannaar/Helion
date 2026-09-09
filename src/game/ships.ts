import type { ShipId } from "./types";

export type ShipDef = {
  id: ShipId;
  name: string;
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

export const SHIPS: Record<ShipId, ShipDef> = {
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
    turnRate: 1.75,
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
    turnRate: 1.45,
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
    turnRate: 1.2,
  },
};

export const SHIP_ORDER: ShipId[] = ["sidewinder", "cobra", "asp"];

export function cargoCapacity(shipId: ShipId, upgraded: boolean): number {
  return SHIPS[shipId].cargo + (upgraded ? 6 : 0);
}

export function cargoUsed(cargo: Partial<Record<string, number>>): number {
  let n = 0;
  for (const v of Object.values(cargo)) n += v ?? 0;
  return n;
}
