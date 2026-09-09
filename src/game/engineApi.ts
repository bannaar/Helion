import type { Input } from "./input";
import type { GameMode } from "./types";

export type EngineHandle = {
  input: Input;
  start: () => void;
  stop: () => void;
  dispose: () => void;
  launch: () => void;
  enterSystem: (systemId: string, kind: "spawn" | "undock" | "jump") => void;
  lockJump: (id: string | null) => void;
  setMode: (mode: GameMode) => void;
  setPaused: (v: boolean) => void;
};
