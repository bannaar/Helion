import { useMemo } from "react";
import { combatRank, getSystem } from "@/game/galaxy";
import { cargoCapacity, cargoUsed, hasModule, shipStats } from "@/game/ships";
import { useGameStore } from "@/game/store";
import type { Contact } from "@/game/types";

function Corner({ className }: { className: string }) {
  return <span className={`pointer-events-none absolute size-3 border-accent/70 ${className}`} />;
}

function Bar({ value, max, tone }: { value: number; max: number; tone: "ok" | "danger" | "fg" }) {
  const pct = Math.max(0, Math.min(100, (value / Math.max(1, max)) * 100));
  const color =
    tone === "ok" ? "bg-ok" : tone === "danger" ? "bg-danger" : "bg-fg/80";
  return (
    <div className="h-1.5 w-full overflow-hidden rounded-xs bg-border">
      <div className={`h-full ${color}`} style={{ width: `${pct}%` }} />
    </div>
  );
}

function Scanner({ contacts }: { contacts: Contact[] }) {
  const dots = useMemo(() => {
    const scale = 420;
    return contacts.map((c) => {
      const x = 50 + (c.localX / scale) * 46;
      const y = 50 - (c.localZ / scale) * 46;
      return { ...c, x: Math.max(4, Math.min(96, x)), y: Math.max(6, Math.min(94, y)) };
    });
  }, [contacts]);

  return (
    <div className="relative h-[108px] w-[148px] overflow-hidden rounded-md border border-border bg-bg/80">
      <div className="absolute inset-1 rounded-sm border border-accent/20" />
      <div className="absolute left-1/2 top-1/2 h-8 w-8 -translate-x-1/2 -translate-y-1/2 rounded-full border border-accent/15" />
      <div className="absolute left-[8%] right-[8%] top-1/2 h-px bg-accent/15" />
      <div className="absolute bottom-[10%] left-1/2 top-[10%] w-px bg-accent/15" />
      {dots.map((d) => (
        <span
          key={d.id}
          className={
            d.kind === "pirate"
              ? "absolute size-1.5 -translate-x-1/2 -translate-y-1/2 bg-danger"
              : d.kind === "police"
                ? "absolute size-1.5 -translate-x-1/2 -translate-y-1/2 bg-fg"
                : d.kind === "station"
                  ? "absolute size-2 -translate-x-1/2 -translate-y-1/2 rotate-45 border border-accent"
                  : d.kind === "canister"
                    ? "absolute size-1.5 -translate-x-1/2 -translate-y-1/2 bg-warn"
                    : "absolute size-1 -translate-x-1/2 -translate-y-1/2 bg-muted"
          }
          style={{ left: `${d.x}%`, top: `${d.y}%` }}
        />
      ))}
      <span className="absolute bottom-1/2 left-1/2 size-1.5 -translate-x-1/2 translate-y-1/2 bg-accent" />
    </div>
  );
}

export function Hud() {
  const speed = useGameStore((s) => s.speed);
  const throttle = useGameStore((s) => s.throttle);
  const dampeners = useGameStore((s) => s.dampeners);
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
  const def = shipStats(save.shipId, save.cargoUpgrade, save.loadout);
  const used = cargoUsed(save.cargo);
  const cap = cargoCapacity(save.shipId, save.cargoUpgrade, save.loadout);
  const dest = jumpLocked ? getSystem(jumpLocked) : null;

  return (
    <div className="pointer-events-none absolute inset-0 font-mono text-[11px] tracking-wide text-fg">
      <Corner className="left-4 top-4 border-l border-t" />
      <Corner className="right-4 top-4 border-r border-t" />
      <Corner className="bottom-4 left-4 border-b border-l" />
      <Corner className="bottom-4 right-4 border-b border-r" />

      <div className="absolute left-4 top-4 max-w-[58%] sm:left-6 sm:top-6">
        <p className="font-display text-lg font-semibold tracking-[0.28em] text-accent sm:text-xl">
          {sys?.name.toUpperCase() ?? "VOID"}
        </p>
        <p className="mt-1 text-muted">
          {save.shipId.toUpperCase()} · {combatRank(save.kills)}
          {save.wanted ? <span className="ml-2 text-danger">WANTED</span> : <span className="ml-2">CLEAN</span>}
        </p>
      </div>

      <div className="absolute right-4 top-4 text-right sm:right-6 sm:top-6">
        <p className="text-accent tabular-nums">{save.credits.toLocaleString()} CR</p>
        <p className="text-muted">
          FUEL {save.fuel.toFixed(1)}/{def.tank} · HOLD {used}/{cap}
        </p>
        <p className="mt-1 max-w-56 text-[9px] tracking-[0.12em] text-muted">
          {hasModule(save.loadout, "ecm_suite") ? "ECM " : ""}
          {hasModule(save.loadout, "eccm_suite") ? "ECCM " : ""}
          {hasModule(save.loadout, "hacking_suite") ? "HACK " : ""}
          {hasModule(save.loadout, "missile_rack") ? "MISSILES " : ""}
          {hasModule(save.loadout, "mine_launcher") ? "MINES" : ""}
        </p>
      </div>

      <div className="absolute left-1/2 top-[18%] w-[min(90%,28rem)] -translate-x-1/2 text-center">
        {flash && Date.now() - flash.at < 2200 ? (
          <p className="text-sm tracking-[0.2em] text-accent">{flash.text}</p>
        ) : alert ? (
          <p className="text-sm tracking-[0.18em] text-warn">{alert}</p>
        ) : null}
      </div>

      <div className="absolute bottom-4 left-4 w-[min(46vw,16rem)] space-y-2 sm:bottom-6 sm:left-6">
        <div className="flex justify-between text-muted">
          <span>SPD {speed.toFixed(0)}</span>
          <span>{dampeners ? "DAMP" : "DRIFT"} · THR {(throttle * 100).toFixed(0)}%</span>
        </div>
        <Bar value={speed} max={def.maxSpeed} tone="fg" />
        <div className="flex justify-between text-muted">
          <span>SHLD</span>
          <span className="tabular-nums">
            {Math.max(0, shields).toFixed(0)}/{maxShields}
          </span>
        </div>
        <Bar value={shields} max={maxShields} tone="ok" />
        <div className="flex justify-between text-muted">
          <span>HULL</span>
          <span className="tabular-nums">
            {Math.max(0, hull).toFixed(0)}/{maxHull}
          </span>
        </div>
        <Bar value={hull} max={maxHull} tone={hull < maxHull * 0.35 ? "danger" : "fg"} />
        <div className="flex justify-between text-muted">
          <span>LASER</span>
        </div>
        <Bar value={laserHeat} max={1} tone="danger" />
      </div>

      <div className="absolute bottom-4 right-4 flex flex-col items-end gap-2 sm:bottom-6 sm:right-6">
        <div className="rounded-md border border-border bg-bg/70 px-3 py-2 text-right">
          <p className="text-muted">TARGET</p>
          <p className="text-fg">{targetName ?? "—"}</p>
          <p className="tabular-nums text-accent">{targetDist ? `${targetDist.toFixed(0)} m` : ""}</p>
          {dest ? (
            <p className="mt-1 text-muted">
              LOCK {dest.name}
              {jumpCharge > 0 ? `  ${(jumpCharge * 100).toFixed(0)}%` : ""}
            </p>
          ) : null}
          {massLocked ? <p className="text-warn">MASS LOCK</p> : null}
          {canDock ? (
            <p className="text-accent">
              {hasModule(save.loadout, "docking_computer") ? "DOCKING COMPUTER" : "STATION IN RANGE"}
            </p>
          ) : null}
        </div>
        <div className="hidden sm:block">
          <Scanner contacts={contacts} />
        </div>
      </div>

      <p className="absolute bottom-3 left-1/2 hidden -translate-x-1/2 text-[10px] text-muted md:block">
        W/S throttle · V dampeners · A/D yaw · R/F pitch · Q/E roll · SPACE fire · B bomb · C chaff · K mine · L scan · X salvage · H dock · M chart · J jump
      </p>
    </div>
  );
}
