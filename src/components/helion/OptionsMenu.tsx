import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { isMuted, setMuted, unlockAudio } from "@/game/audio";
import { isTTSEnabled, initializeTTS, setTTSEnabled, speakTTS, stopTTS } from "@/game/tts";
import { useGameStore } from "@/game/store";

type Props = {
  fullscreen: boolean;
  onToggleFullscreen: () => void;
};

export function OptionsMenu({ fullscreen, onToggleFullscreen }: Props) {
  const [open, setOpen] = useState(false);
  const [muted, setMutedUi] = useState(isMuted());
  const [ttsEnabled, setTtsEnabledUi] = useState(isTTSEnabled());
  const dampeners = useGameStore((state) => state.dampeners);

  useEffect(() => {
    if (!open) return;
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") setOpen(false);
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [open]);

  function toggleSound() {
    unlockAudio();
    const next = !muted;
    setMuted(next);
    setMutedUi(next);
  }

  function toggleVoice() {
    initializeTTS();
    const next = !ttsEnabled;
    setTTSEnabled(next);
    setTtsEnabledUi(next);
    if (next) speakTTS("Voice comms online.");
    else stopTTS();
  }

  function toggleDampeners() {
    useGameStore.getState().setFlight({ dampeners: !dampeners });
    useGameStore.getState().setFlash(`INERTIA DAMPENERS ${dampeners ? "OFF" : "ON"}`);
  }

  return (
    <>
      <button
        type="button"
        aria-label={open ? "Close options" : "Open options"}
        aria-expanded={open}
        className="absolute right-3 top-[max(10.75rem,calc(env(safe-area-inset-top)+10.75rem))] z-40 h-9 rounded-sm border border-border bg-surface/80 px-3 font-mono text-[10px] tracking-widest text-muted hover:border-accent/60 hover:text-accent"
        onClick={() => setOpen((value) => !value)}
      >
        OPTIONS
      </button>

      {open ? (
        <div className="absolute inset-0 z-50 flex items-start justify-center bg-bg/75 p-4 pt-[max(5rem,calc(env(safe-area-inset-top)+5rem))] backdrop-blur-sm">
          <section
            role="dialog"
            aria-modal="true"
            aria-labelledby="options-title"
            className="w-full max-w-md rounded-xl border border-border bg-surface p-5 shadow-2xl"
          >
            <div className="flex items-start justify-between gap-4">
              <div>
                <p className="font-mono text-[10px] tracking-[0.28em] text-accent">SHIP SYSTEMS</p>
                <h2 id="options-title" className="mt-1 font-display text-3xl tracking-[0.08em]">
                  Options
                </h2>
              </div>
              <button
                type="button"
                aria-label="Close options"
                className="font-mono text-xs tracking-widest text-muted hover:text-fg"
                onClick={() => setOpen(false)}
              >
                ESC
              </button>
            </div>

            <div className="mt-6 grid gap-2">
              <SettingRow
                label="Synthwave audio"
                detail={muted ? "Muted" : "Music, engines, station ambience"}
                value={!muted}
                onClick={toggleSound}
              />
              <SettingRow
                label="Voice comms"
                detail={ttsEnabled ? "Browser speech enabled" : "Silent comms"}
                value={ttsEnabled}
                onClick={toggleVoice}
              />
              <SettingRow
                label="Inertia dampeners"
                detail={dampeners ? "Flight assists active" : "Manual drift enabled"}
                value={dampeners}
                onClick={toggleDampeners}
              />
              <button
                type="button"
                className="flex min-h-14 items-center justify-between rounded-md border border-border px-4 text-left hover:border-accent/60"
                onClick={() => {
                  onToggleFullscreen();
                  setOpen(false);
                }}
              >
                <span>
                  <span className="block text-sm">Display mode</span>
                  <span className="mt-1 block font-mono text-[10px] tracking-wider text-muted">
                    {fullscreen ? "Fullscreen active" : "Windowed"}
                  </span>
                </span>
                <span className="font-mono text-[10px] tracking-widest text-accent">
                  {fullscreen ? "EXIT" : "ENTER"}
                </span>
              </button>
            </div>

            <div className="mt-6 border-t border-border pt-4">
              <p className="font-mono text-[10px] leading-relaxed tracking-wider text-muted">
                W/S throttle · A/D yaw · R/F pitch · Q/E roll · V dampeners · M chart · P pause
              </p>
              <Button className="mt-4 w-full" variant="ghost" onClick={() => setOpen(false)}>
                Close
              </Button>
            </div>
          </section>
        </div>
      ) : null}
    </>
  );
}

function SettingRow({
  label,
  detail,
  value,
  onClick,
}: {
  label: string;
  detail: string;
  value: boolean;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={value}
      className="flex min-h-14 items-center justify-between rounded-md border border-border px-4 text-left hover:border-accent/60"
      onClick={onClick}
    >
      <span>
        <span className="block text-sm">{label}</span>
        <span className="mt-1 block font-mono text-[10px] tracking-wider text-muted">{detail}</span>
      </span>
      <span className={`font-mono text-[10px] tracking-widest ${value ? "text-accent" : "text-muted"}`}>
        {value ? "ON" : "OFF"}
      </span>
    </button>
  );
}
