import { useMemo } from "react";
import { Button } from "@/components/ui/button";
import { economyLabel, getGalaxy, getSystem, governmentLabel, jumpFuelCost, systemDistance } from "@/game/galaxy";
import { SHIPS } from "@/game/ships";
import { useGameStore } from "@/game/store";
import type { EngineHandle } from "@/game/engineApi";

export function GalaxyChart({ engine }: { engine: EngineHandle | null }) {
  const save = useGameStore((s) => s.save);
  const jumpLocked = useGameStore((s) => s.jumpLocked);
  const setMode = useGameStore((s) => s.setMode);
  const systems = useMemo(() => getGalaxy(), []);
  const here = getSystem(save.systemId);
  const selected = getSystem(jumpLocked ?? save.systemId);
  const range = SHIPS[save.shipId].jump;

  return (
    <div className="absolute inset-0 z-20 flex flex-col bg-bg/92 p-4 pt-[max(1rem,env(safe-area-inset-top))] sm:p-8">
      <div className="mx-auto flex w-full max-w-5xl items-end justify-between gap-4">
        <div>
          <p className="font-mono text-xs tracking-[0.3em] text-muted">GALACTIC CHART</p>
          <h2 className="font-display text-3xl font-semibold tracking-[0.12em]">LANES</h2>
        </div>
        <Button variant="ghost" onClick={() => setMode("space")}>
          Close
        </Button>
      </div>

      <div className="mx-auto mt-4 grid min-h-0 w-full max-w-5xl flex-1 grid-rows-[1fr_auto] gap-4 lg:grid-cols-[1fr_18rem] lg:grid-rows-1">
        <div className="relative min-h-[280px] overflow-hidden rounded-xl border border-border bg-surface">
          <svg viewBox="0 0 96 48" className="h-full w-full" role="img" aria-label="Galaxy map">
            {here
              ? systems
                  .filter((s) => systemDistance(here, s) <= range && s.id !== here.id)
                  .map((s) => (
                    <line
                      key={`l-${s.id}`}
                      x1={here.x}
                      y1={here.y}
                      x2={s.x}
                      y2={s.y}
                      stroke="currentColor"
                      className="text-accent/25"
                      strokeWidth="0.18"
                    />
                  ))
              : null}
            {here ? (
              <circle
                cx={here.x}
                cy={here.y}
                r={range}
                fill="none"
                stroke="currentColor"
                className="text-accent/30"
                strokeWidth="0.22"
                strokeDasharray="0.8 0.6"
              />
            ) : null}
            {systems.map((s) => {
              const d = here ? systemDistance(here, s) : 99;
              const inRange = !!here && d <= range + 0.05;
              const isHere = s.id === save.systemId;
              const isSel = s.id === selected?.id;
              return (
                <g key={s.id} className="cursor-pointer" onClick={() => engine?.lockJump(s.id)}>
                  <circle
                    cx={s.x}
                    cy={s.y}
                    r={isHere ? 1.15 : isSel ? 0.95 : 0.7}
                    fill="currentColor"
                    className={
                      isHere
                        ? "text-accent"
                        : inRange
                          ? "text-fg"
                          : "text-muted/50"
                    }
                  />
                </g>
              );
            })}
          </svg>
        </div>

        <aside className="rounded-xl border border-border bg-surface p-4">
          {selected ? (
            <>
              <p className="font-mono text-xs tracking-[0.25em] text-muted">SYSTEM</p>
              <h3 className="mt-1 font-display text-2xl font-semibold">{selected.name}</h3>
              <dl className="mt-3 space-y-1 font-mono text-xs text-muted">
                <div className="flex justify-between gap-3">
                  <dt>Economy</dt>
                  <dd className="text-fg">{economyLabel(selected.economy)}</dd>
                </div>
                <div className="flex justify-between gap-3">
                  <dt>Government</dt>
                  <dd className="text-fg">{governmentLabel(selected.government)}</dd>
                </div>
                <div className="flex justify-between gap-3">
                  <dt>Tech</dt>
                  <dd className="text-fg">{selected.tech}</dd>
                </div>
                <div className="flex justify-between gap-3">
                  <dt>Range</dt>
                  <dd className="text-fg">
                    {here ? `${systemDistance(here, selected).toFixed(1)} ly` : "—"}
                  </dd>
                </div>
                <div className="flex justify-between gap-3">
                  <dt>Fuel</dt>
                  <dd className="text-fg">{jumpFuelCost(save.systemId, selected.id).toFixed(1)} t</dd>
                </div>
              </dl>
              <Button
                className="mt-4 w-full"
                disabled={selected.id === save.systemId}
                onClick={() => {
                  engine?.lockJump(selected.id);
                  setMode("space");
                  useGameStore.getState().setFlash(`DESTINATION LOCKED  —  ${selected.name.toUpperCase()}`);
                }}
              >
                Lock destination
              </Button>
              <p className="mt-2 font-mono text-[11px] text-muted">Then press J in flight to charge the drive.</p>
            </>
          ) : (
            <p className="text-muted">Select a star.</p>
          )}
        </aside>
      </div>
    </div>
  );
}
