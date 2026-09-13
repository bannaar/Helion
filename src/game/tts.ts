const TTS_STORAGE_KEY = "helion.tts.enabled";

let enabled = true;
let initialized = false;
let voices: SpeechSynthesisVoice[] = [];

function speech(): SpeechSynthesis | null {
  if (typeof window === "undefined" || !("speechSynthesis" in window)) return null;
  return window.speechSynthesis;
}

function refreshVoices(): void {
  const synth = speech();
  if (!synth) return;
  voices = synth.getVoices();
  initialized = true;
}

export function initializeTTS(): boolean {
  const synth = speech();
  if (!synth) return false;
  if (!initialized) {
    refreshVoices();
    synth.addEventListener("voiceschanged", refreshVoices);
    try {
      enabled = window.localStorage.getItem(TTS_STORAGE_KEY) !== "false";
    } catch {
      enabled = true;
    }
  }
  return true;
}

export function setTTSEnabled(value: boolean): void {
  enabled = value;
  try {
    window.localStorage.setItem(TTS_STORAGE_KEY, String(value));
  } catch {
    // Speech remains usable when storage is unavailable.
  }
  if (!value) speech()?.cancel();
}

export function isTTSEnabled(): boolean {
  return enabled;
}

export function stopTTS(): void {
  speech()?.cancel();
}

export function speakTTS(text: string): void {
  const synth = speech();
  if (!synth || !enabled || !text.trim()) return;
  initializeTTS();
  synth.cancel();
  const utterance = new SpeechSynthesisUtterance(text.replace(/\s+—\s+/g, ". "));
  utterance.lang = "en-US";
  utterance.rate = 0.92;
  utterance.pitch = 0.82;
  utterance.volume = 0.9;
  const voice =
    voices.find((candidate) => candidate.lang.toLowerCase() === "en-us") ??
    voices.find((candidate) => candidate.lang.toLowerCase().startsWith("en"));
  if (voice) utterance.voice = voice;
  synth.speak(utterance);
}

