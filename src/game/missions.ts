import { getGalaxy, getSystem, systemDistance } from "./galaxy";
import type { Mission, StarSystem } from "./types";

const CARGO_BY_ECONOMY: Record<StarSystem["economy"], Mission["cargo"]> = {
  agri: "food",
  industrial: "machinery",
  extraction: "minerals",
  refinery: "alloys",
  hightech: "computers",
  tourism: "luxuries",
  military: "medicine",
  colony: "textiles",
};

export function missionContracts(systemId: string): Mission[] {
  const origin = getSystem(systemId);
  if (!origin) return [];
  const destinations = getGalaxy()
    .filter((candidate) => candidate.id !== systemId && systemDistance(origin, candidate) <= 12)
    .sort((a, b) => systemDistance(origin, a) - systemDistance(origin, b))
    .slice(0, 3);

  const courierContracts: Mission[] = destinations.map((destination, index) => ({
    id: `courier:${origin.id}:${destination.id}`,
    type: "courier",
    originId: origin.id,
    destinationId: destination.id,
    cargo: CARGO_BY_ECONOMY[origin.economy],
    quantity: index === 0 ? 2 : 1,
    reward: Math.round(320 + systemDistance(origin, destination) * 95 + index * 180),
    acceptedAt: 0,
    requirement: index === 0 ? 2 : 1,
    progressAtAccept: 0,
  }));
  const miningTarget = origin.economy === "extraction" ? 4 : 2;
  const explorationTarget = destinations[0];
  const contracts: Mission[] = [
    ...courierContracts,
    {
      id: `mining:${origin.id}`,
      type: "mining",
      originId: origin.id,
      destinationId: origin.id,
      cargo: "minerals",
      quantity: miningTarget,
      reward: 540 + miningTarget * 110,
      acceptedAt: 0,
      requirement: miningTarget,
      progressAtAccept: 0,
    },
    ...(explorationTarget
      ? [{
          id: `exploration:${origin.id}:${explorationTarget.id}`,
          type: "exploration" as const,
          originId: origin.id,
          destinationId: explorationTarget.id,
          cargo: "computers" as const,
          quantity: 1,
          reward: Math.round(760 + systemDistance(origin, explorationTarget) * 125),
          acceptedAt: 0,
          requirement: 1,
          progressAtAccept: 0,
        }]
      : []),
    {
      id: `salvage:${origin.id}`,
      type: "salvage",
      originId: origin.id,
      destinationId: origin.id,
      cargo: "alloys",
      quantity: 2,
      reward: 680,
      acceptedAt: 0,
      requirement: 2,
      progressAtAccept: 0,
    },
  ];
  return contracts;
}

export function missionDestinationName(mission: Mission): string {
  return getSystem(mission.destinationId)?.name ?? mission.destinationId;
}

export function missionLabel(type: Mission["type"]): string {
  return type === "courier"
    ? "Courier"
    : type === "mining"
      ? "Mining"
      : type === "exploration"
        ? "Exploration"
        : "Salvage";
}
