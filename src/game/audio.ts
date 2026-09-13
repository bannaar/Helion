/** Procedural Web Audio SFX — unlock on the first gesture. */

let ctx: AudioContext | null = null;
let master: GainNode | null = null;
let sfx: GainNode | null = null;
let muted = false;
let music: GainNode | null = null;
let musicTimer: number | null = null;
let scene: "title" | "space" | "station" = "title";
let engineOsc: OscillatorNode | null = null;
let engineGain: GainNode | null = null;

function ac(): AudioContext | null {
  if (typeof window === "undefined") return null;
  if (!ctx) {
    const Ctor = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    ctx = new Ctor({ latencyHint: "interactive" });
    master = ctx.createGain();
    sfx = ctx.createGain();
    music = ctx.createGain();
    sfx.gain.value = 0.28;
    master.gain.value = muted ? 0 : 0.8;
    sfx.connect(master);
    music.gain.value = 0.08;
    music.connect(master);
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

function musicNote(freq: number, duration: number, offset = 0) {
  const c = ac();
  if (!c || !music || c.state !== "running") return;
  const osc = c.createOscillator();
  const gain = c.createGain();
  const start = c.currentTime + offset;
  osc.type = "sawtooth";
  osc.frequency.setValueAtTime(freq, start);
  gain.gain.setValueAtTime(0.0001, start);
  gain.gain.exponentialRampToValueAtTime(0.11, start + 0.04);
  gain.gain.exponentialRampToValueAtTime(0.0001, start + duration);
  osc.connect(gain);
  gain.connect(music);
  osc.start(start);
  osc.stop(start + duration + 0.03);
}

function scheduleSynthwaveBar() {
  if (scene !== "space" || !ctx) return;
  const bass = [55, 55, 65.41, 73.42, 55, 55, 82.41, 73.42];
  bass.forEach((note, index) => musicNote(note, 0.24, index * 0.28));
  [220, 277.18, 329.63].forEach((note, index) => musicNote(note, 1.8, index * 0.56));
}

export function setAudioScene(next: "title" | "space" | "station"): void {
  scene = next;
  const c = ac();
  if (!c) return;
  if (musicTimer !== null) {
    window.clearInterval(musicTimer);
    musicTimer = null;
  }
  if (next === "space") {
    scheduleSynthwaveBar();
    musicTimer = window.setInterval(scheduleSynthwaveBar, 2200);
  } else if (next === "station") {
    musicNote(110, 1.8);
    musicNote(164.81, 1.8, 0.5);
    musicNote(220, 1.8, 1);
    musicTimer = window.setInterval(() => {
      if (scene === "station") {
        musicNote(110, 1.8);
        musicNote(164.81, 1.8, 0.5);
        musicNote(220, 1.8, 1);
      }
    }, 2600);
  }
}

export function setEngineLevel(level: number): void {
  const c = ac();
  if (!c || !sfx) return;
  const amount = Math.max(0, Math.min(1, level));
  if (!engineOsc) {
    engineOsc = c.createOscillator();
    engineGain = c.createGain();
    engineOsc.type = "sawtooth";
    engineOsc.frequency.value = 48;
    engineGain.gain.value = 0.0001;
    engineOsc.connect(engineGain);
    engineGain.connect(sfx);
    engineOsc.start();
  }
  engineOsc.frequency.setTargetAtTime(42 + amount * 44, c.currentTime, 0.06);
  engineGain?.gain.setTargetAtTime(amount > 0.01 ? amount * 0.055 : 0.0001, c.currentTime, 0.08);
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
  mining: () => beep(260, 0.18, "sine", 0.12, 180),
  scan: () => beep(720, 0.24, "triangle", 0.1, 320),
  salvage: () => beep(390, 0.2, "sine", 0.12, -120),
  hit: () => beep(180, 0.14, "sawtooth", 0.18, -80),
  hull: () => beep(90, 0.22, "square", 0.22, -40),
  dock: () => beep(420, 0.28, "triangle", 0.16, 180),
  jump: () => beep(140, 0.7, "sawtooth", 0.2, 520),
  scoop: () => beep(640, 0.12, "sine", 0.14, 200),
  ui: () => beep(520, 0.05, "square", 0.08, 0),
  warn: () => beep(240, 0.18, "square", 0.14, 0),
  kill: () => beep(310, 0.35, "triangle", 0.16, -220),
  bomb: () => beep(120, 0.5, "sawtooth", 0.24, 260),
  chaff: () => beep(1400, 0.16, "square", 0.08, -800),
  station: () => beep(92, 0.5, "sine", 0.12, 22),
};

if (typeof window !== "undefined") {
  window.addEventListener("pointerdown", unlockAudio, { once: true });
  window.addEventListener("keydown", unlockAudio, { once: true });
  document.addEventListener("visibilitychange", () => {
    if (!document.hidden) unlockAudio();
  });
}
