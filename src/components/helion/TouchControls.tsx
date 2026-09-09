import { useRef } from "react";
import type { EngineHandle } from "@/game/engineApi";
import type { Input } from "@/game/input";
import { useGameStore } from "@/game/store";

type Props = { input: Input | null; engine: EngineHandle | null };

export function TouchControls({ input, engine }: Props) {
  const mode = useGameStore((s) => s.mode);
  const canDock = useGameStore((s) => s.canDock);
  const setMode = useGameStore((s) => s.setMode);
  const stick = useRef<HTMLDivElement>(null);
  const pid = useRef<number | null>(null);

  if (mode !== "space" || !input) return null;

  const onDown = (e: React.PointerEvent<HTMLDivElement>) => {
    if (pid.current !== null) return;
    pid.current = e.pointerId;
    e.currentTarget.setPointerCapture(e.pointerId);
    move(e);
  };
  const move = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!stick.current || !input) return;
    if (pid.current !== null && e.pointerId !== pid.current) return;
    const r = stick.current.getBoundingClientRect();
    const x = (e.clientX - r.left) / r.width * 2 - 1;
    const y = (e.clientY - r.top) / r.height * 2 - 1;
    input.touchYaw = Math.max(-1, Math.min(1, -x));
    input.touchPitch = Math.max(-1, Math.min(1, -y));
  };
  const end = (e: React.PointerEvent<HTMLDivElement>) => {
    if (pid.current !== e.pointerId) return;
    pid.current = null;
    if (input) {
      input.touchYaw = 0;
      input.touchPitch = 0;
    }
  };

  const hold = (key: "touchThrust" | "touchFire", v: number | boolean) => ({
    onPointerDown: (e: React.PointerEvent) => {
      e.preventDefault();
      if (!input) return;
      if (key === "touchThrust") input.touchThrust = v as number;
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
    },
  });

  return (
    <div className="pointer-events-none absolute inset-x-0 bottom-0 z-10 flex items-end justify-between p-3 pb-[max(0.75rem,env(safe-area-inset-bottom))] md:hidden">
      <div
        ref={stick}
        className="pointer-events-auto size-[132px] rounded-full border border-border bg-surface/70"
        onPointerDown={onDown}
        onPointerMove={move}
        onPointerUp={end}
        onPointerCancel={end}
        style={{ touchAction: "none" }}
      >
        <div className="flex h-full items-center justify-center font-mono text-[10px] tracking-widest text-muted">
          STEER
        </div>
      </div>
      <div className="pointer-events-auto grid grid-cols-2 gap-2">
        <button
          className="h-12 min-w-14 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg"
          {...hold("touchThrust", 1)}
        >
          THR+
        </button>
        <button
          className="h-12 min-w-14 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg"
          {...hold("touchThrust", -1)}
        >
          THR-
        </button>
        <button
          className="h-12 min-w-14 rounded-md bg-accent font-mono text-xs text-accent-fg"
          {...hold("touchFire", true)}
        >
          FIRE
        </button>
        <button
          className="h-12 min-w-14 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg"
          onPointerDown={() => {
            if (canDock) input.touchDock = true;
          }}
        >
          DOCK
        </button>
        <button
          className="col-span-2 h-11 rounded-md border border-border bg-surface/80 font-mono text-xs text-fg"
          onClick={() => {
            setMode("map");
            engine?.lockJump(useGameStore.getState().jumpLocked);
          }}
        >
          CHART
        </button>
      </div>
    </div>
  );
}
