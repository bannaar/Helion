export type Actions = {
  yaw: number;
  pitch: number;
  roll: number;
  thrust: number;
  fire: boolean;
  dock: boolean;
  map: boolean;
  jump: boolean;
  target: boolean;
  pause: boolean;
};

const empty = (): Actions => ({
  yaw: 0,
  pitch: 0,
  roll: 0,
  thrust: 0,
  fire: false,
  dock: false,
  map: false,
  jump: false,
  target: false,
  pause: false,
});

function radialDeadzone(x: number, y: number, dz = 0.18): { x: number; y: number } {
  const m = Math.hypot(x, y);
  if (m < dz) return { x: 0, y: 0 };
  const scale = (m - dz) / (1 - dz) / m;
  return { x: x * scale, y: y * scale };
}

const GAME_KEYS = new Set([
  "KeyW",
  "KeyA",
  "KeyS",
  "KeyD",
  "KeyQ",
  "KeyE",
  "KeyR",
  "KeyF",
  "KeyH",
  "KeyJ",
  "KeyM",
  "Space",
  "Tab",
  "Escape",
  "ArrowUp",
  "ArrowDown",
  "ArrowLeft",
  "ArrowRight",
  "ShiftLeft",
  "ShiftRight",
]);

export class Input {
  keys = new Set<string>();
  injected: string[] | null = null;
  touchYaw = 0;
  touchPitch = 0;
  touchThrust = 0;
  touchFire = false;
  touchDock = false;
  prev: Actions = empty();
  current: Actions = empty();
  edges = { dock: false, map: false, jump: false, target: false, pause: false, fire: false };
  private unbind: Array<() => void> = [];
  enabled = true;

  attach(target: HTMLElement | Window = window) {
    const onDown = (e: KeyboardEvent) => {
      if (!this.enabled) return;
      this.keys.add(e.code);
      if (GAME_KEYS.has(e.code)) e.preventDefault();
    };
    const onUp = (e: KeyboardEvent) => this.keys.delete(e.code);
    const clear = () => this.keys.clear();
    window.addEventListener("keydown", onDown);
    window.addEventListener("keyup", onUp);
    window.addEventListener("blur", clear);
    document.addEventListener("visibilitychange", () => {
      if (document.hidden) clear();
    });
    this.unbind.push(() => {
      window.removeEventListener("keydown", onDown);
      window.removeEventListener("keyup", onUp);
      window.removeEventListener("blur", clear);
    });
    void target;
  }

  detach() {
    for (const fn of this.unbind) fn();
    this.unbind = [];
    this.keys.clear();
  }

  setKeys(codes: string[]) {
    this.injected = codes;
  }

  held(code: string): boolean {
    if (this.injected) return this.injected.includes(code);
    return this.keys.has(code);
  }

  poll(): Actions {
    const a = empty();
    if (this.held("KeyA") || this.held("ArrowLeft")) a.yaw += 1;
    if (this.held("KeyD") || this.held("ArrowRight")) a.yaw -= 1;
    if (this.held("ArrowUp") || this.held("KeyR")) a.pitch += 1;
    if (this.held("ArrowDown") || this.held("KeyF")) a.pitch -= 1;
    if (this.held("KeyQ")) a.roll += 1;
    if (this.held("KeyE")) a.roll -= 1;
    if (this.held("KeyW")) a.thrust += 1;
    if (this.held("KeyS")) a.thrust -= 1;
    a.fire = this.held("Space") || this.touchFire;
    a.dock = this.held("KeyH") || this.touchDock;
    a.map = this.held("KeyM");
    a.jump = this.held("KeyJ");
    a.target = this.held("Tab");
    a.pause = this.held("Escape");

    a.yaw += this.touchYaw;
    a.pitch += this.touchPitch;
    a.thrust += this.touchThrust;

    const pads = typeof navigator !== "undefined" ? navigator.getGamepads?.() : null;
    if (pads) {
      for (const pad of pads) {
        if (!pad || pad.mapping !== "standard") continue;
        const stick = radialDeadzone(pad.axes[0] ?? 0, pad.axes[1] ?? 0);
        a.yaw += -stick.x;
        a.pitch += -stick.y;
        const rstick = radialDeadzone(pad.axes[2] ?? 0, pad.axes[3] ?? 0);
        a.roll += -rstick.x;
        const rt = pad.buttons[7]?.value ?? 0;
        const lt = pad.buttons[6]?.value ?? 0;
        a.thrust += rt - lt;
        if (pad.buttons[0]?.pressed) a.fire = true;
        if (pad.buttons[3]?.pressed) a.dock = true;
        if (pad.buttons[9]?.pressed) a.map = true;
        if (pad.buttons[2]?.pressed) a.jump = true;
        if (pad.buttons[1]?.pressed) a.pause = true;
        if (pad.buttons[4]?.pressed) a.target = true;
      }
    }

    a.yaw = Math.max(-1, Math.min(1, a.yaw));
    a.pitch = Math.max(-1, Math.min(1, a.pitch));
    a.roll = Math.max(-1, Math.min(1, a.roll));
    a.thrust = Math.max(-1, Math.min(1, a.thrust));

    this.edges.dock = a.dock && !this.prev.dock;
    this.edges.map = a.map && !this.prev.map;
    this.edges.jump = a.jump && !this.prev.jump;
    this.edges.target = a.target && !this.prev.target;
    this.edges.pause = a.pause && !this.prev.pause;
    this.edges.fire = a.fire && !this.prev.fire;
    this.prev = { ...a };
    this.current = a;
    this.touchDock = false;
    return a;
  }
}

declare global {
  interface Window {
    __controlsTest?: {
      getYaw: () => number;
      getSpeed: () => number;
      setSteer?: (v: number) => void;
      setKeys?: (codes: string[]) => void;
    };
  }
}
