/** Procedural Web Audio SFX — unlock on the first gesture. */

let ctx: AudioContext | null = null;
let master: GainNode | null = null;
let sfx: GainNode | null = null;
let muted = false;

function ac(): AudioContext | null {
  if (typeof window === "undefined") return null;
  if (!ctx) {
    const Ctor = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    ctx = new Ctor({ latencyHint: "interactive" });
    master = ctx.createGain();
    sfx = ctx.createGain();
    sfx.gain.value = 0.28;
    master.gain.value = muted ? 0 : 0.8;
    sfx.connect(master);
    master.connect(ctx.destination);
  }
  return ctx;
}

export function unlockAudio(): void {
  const c = ac();
  if (!c) return;
  if (c.state === "suspended") void c.resume();
}

export function setMuted(v: boolean): void {
  muted = v;
  if (master && ctx) master.gain.setTargetAtTime(v ? 0 : 0.8, ctx.currentTime, 0.03);
}

export function isMuted(): boolean {
  return muted;
}

function beep(freq: number, dur: number, type: OscillatorType, gain = 0.2, slide = 0) {
  const c = ac();
  if (!c || !sfx || c.state !== "running") return;
  const osc = c.createOscillator();
  const g = c.createGain();
  osc.type = type;
  osc.frequency.setValueAtTime(freq, c.currentTime);
  if (slide) osc.frequency.exponentialRampToValueAtTime(Math.max(40, freq + slide), c.currentTime + dur);
  g.gain.setValueAtTime(0.0001, c.currentTime);
  g.gain.exponentialRampToValueAtTime(gain, c.currentTime + 0.012);
  g.gain.exponentialRampToValueAtTime(0.0001, c.currentTime + dur);
  osc.connect(g);
  g.connect(sfx);
  osc.start();
  osc.stop(c.currentTime + dur + 0.02);
  osc.onended = () => {
    osc.disconnect();
    g.disconnect();
  };
}

export const sfxPlay = {
  laser: () => beep(920, 0.07, "square", 0.12, -400),
  hit: () => beep(180, 0.14, "sawtooth", 0.18, -80),
  hull: () => beep(90, 0.22, "square", 0.22, -40),
  dock: () => beep(420, 0.28, "triangle", 0.16, 180),
  jump: () => beep(140, 0.7, "sawtooth", 0.2, 520),
  scoop: () => beep(640, 0.12, "sine", 0.14, 200),
  ui: () => beep(520, 0.05, "square", 0.08, 0),
  warn: () => beep(240, 0.18, "square", 0.14, 0),
  kill: () => beep(310, 0.35, "triangle", 0.16, -220),
};

if (typeof window !== "undefined") {
  window.addEventListener("pointerdown", unlockAudio, { once: true });
  window.addEventListener("keydown", unlockAudio, { once: true });
  document.addEventListener("visibilitychange", () => {
    if (!document.hidden) unlockAudio();
  });
}
