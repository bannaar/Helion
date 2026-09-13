import { useEffect, useRef, useState } from "react";
import { Button } from "@/components/ui/button";
import { unlockAudio, setAudioScene, setMuted, isMuted } from "@/game/audio";
import { initializeTTS, isTTSEnabled, setTTSEnabled, speakTTS, stopTTS } from "@/game/tts";
import {
  defaultCommanderProfile,
  loadCommanderProfile,
  saveCommanderProfile,
  type CommanderProfile,
} from "@/game/commander-profile";
import type { EngineHandle } from "@/game/engineApi";
import { HOME_SYSTEM_ID } from "@/game/galaxy";
import { defaultSave, loadSave, clearSave } from "@/game/save";
import { shipStats } from "@/game/ships";
import { useGameStore } from "@/game/store";
import { GalaxyChart } from "./GalaxyChart";
import { Hud } from "./Hud";
import { StationDock } from "./StationDock";
import { TouchControls } from "./TouchControls";

export function HelionApp() {
  const rootRef = useRef<HTMLElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const engineRef = useRef<EngineHandle | null>(null);
  const [ready, setReady] = useState(false);
  const [name, setName] = useState("JAMESON");
  const [profile, setProfile] = useState<CommanderProfile>(() => defaultCommanderProfile("JAMESON"));
  const [profileSync, setProfileSync] = useState<"idle" | "syncing" | "synced" | "offline">("idle");
  const [hasSave, setHasSave] = useState(false);
  const [muted, setMutedUi] = useState(false);
  const [ttsEnabled, setTtsEnabledUi] = useState(true);
  const [fullscreen, setFullscreen] = useState(false);
  const mode = useGameStore((s) => s.mode);
  const paused = useGameStore((s) => s.paused);
  const save = useGameStore((s) => s.save);

  useEffect(() => {
    setAudioScene(mode === "space" ? "space" : mode === "station" ? "station" : "title");
  }, [mode]);

  useEffect(() => {
    initializeTTS();
    setTtsEnabledUi(isTTSEnabled());
    return () => stopTTS();
  }, []);

  const flash = useGameStore((s) => s.flash);
  useEffect(() => {
    if (!flash || mode === "title") return;
    speakTTS(flash.text);
  }, [flash, mode]);

  useEffect(() => {
    const onFullscreenChange = () => {
      setFullscreen(document.fullscreenElement === rootRef.current);
    };
    document.addEventListener("fullscreenchange", onFullscreenChange);
    return () => document.removeEventListener("fullscreenchange", onFullscreenChange);
  }, []);

  useEffect(() => {
    let cancelled = false;
    void loadCommanderProfile({})
      .then((nextProfile) => {
        if (cancelled) return;
        setProfile(nextProfile);
        setName(nextProfile.commanderName);
      })
      .catch(() => {
        if (!cancelled) {
          setProfile(defaultCommanderProfile(name));
        }
      });
    return () => {
      cancelled = true;
    };
  }, []);

  async function persistProfile(nextName = name, snapshot = save) {
    setProfileSync("syncing");
    const payload: CommanderProfile = {
      ...profile,
      commanderName: nextName,
      credits: snapshot.credits,
      shipId: snapshot.shipId,
      inventory: snapshot.cargo,
      reputation: {
        kills: snapshot.kills,
        standing: profile.standing,
        wanted: snapshot.wanted ? 1 : 0,
        salvaged: snapshot.salvageRecovered,
        explored: snapshot.explorationData,
        missions: snapshot.completedMissions,
      },
      saveState: {
        systemId: snapshot.systemId,
        docked: snapshot.docked,
        cargoUpgrade: snapshot.cargoUpgrade,
        loadout: snapshot.loadout,
        hull: snapshot.hull,
        shields: snapshot.shields,
      },
    };
    setProfile(payload);
    try {
      const saved = await saveCommanderProfile({ data: payload });
      setProfile(saved);
      setName(saved.commanderName);
      setProfileSync("synced");
    } catch {
      // Local saves remain playable when the server is unavailable.
      setProfileSync("offline");
    }
  }

  useEffect(() => {
    if (mode === "title" || mode === "dead") return;
    const timer = window.setTimeout(() => {
      void persistProfile(name, save);
    }, 1200);
    return () => window.clearTimeout(timer);
    // The save object is the synchronization trigger; persistProfile reads the
    // current commander identity and sends a debounced snapshot.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [save, mode]);

  useEffect(() => {
    setHasSave(!!loadSave());
    let engine: EngineHandle | null = null;
    let cancelled = false;
    const canvas = canvasRef.current;
    if (!canvas) return;
    void import("@/game/engine").then(({ HelionEngine }) => {
      if (cancelled) return;
      engine = new HelionEngine(canvas);
      engineRef.current = engine;
      engine.start();
      setReady(true);
      const qa = new URLSearchParams(window.location.search).has("qa");
      if (qa) startNew("JAMESON", engine);
    });
    const persist = () => useGameStore.getState().persist();
    document.addEventListener("visibilitychange", () => {
      if (document.hidden) persist();
    });
    window.addEventListener("pagehide", persist);
    return () => {
      cancelled = true;
      persist();
      engine?.dispose();
      engineRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  function applySaveToFlight(next: ReturnType<typeof defaultSave>) {
    const def = shipStats(next.shipId, next.cargoUpgrade, next.loadout);
    useGameStore.getState().replaceSave(next);
    useGameStore.getState().setFlight({
      hull: next.hull,
      shields: next.shields,
      maxHull: def.hull,
      maxShields: def.shields,
    });
  }

  function startNew(cmdr: string, eng = engineRef.current) {
    unlockAudio();
    initializeTTS();
    const next = defaultSave(cmdr);
    applySaveToFlight(next);
    void persistProfile(cmdr, next);
    eng?.enterSystem(HOME_SYSTEM_ID, "spawn");
    useGameStore.getState().setMode("space");
    useGameStore.getState().setFlash("STATION BEARING MARKED  —  FLY IN, H TO DOCK");
  }

  function continueSave() {
    unlockAudio();
    initializeTTS();
    const next = loadSave() ?? defaultSave(name);
    applySaveToFlight(next);
    void persistProfile(name, next);
    const eng = engineRef.current;
    eng?.enterSystem(next.systemId, next.docked ? "undock" : "spawn");
    if (next.docked) {
      useGameStore.getState().setMode("station");
      eng?.setMode("station");
    } else {
      useGameStore.getState().setMode("space");
    }
  }

  function rebuy() {
    unlockAudio();
    initializeTTS();
    const fee = Math.max(400, Math.round(save.credits * 0.12));
    if (save.credits < fee) {
      clearSave();
      useGameStore.getState().replaceSave(defaultSave("JAMESON"), false);
      useGameStore.getState().setMode("title");
      engineRef.current?.setMode("title");
      setHasSave(false);
      return;
    }
    const def = shipStats(save.shipId, save.cargoUpgrade, save.loadout);
    const next = {
      ...save,
      credits: save.credits - fee,
      hull: def.hull,
      shields: def.shields,
      docked: true,
      cargo: {},
    };
    applySaveToFlight(next);
    engineRef.current?.enterSystem(next.systemId, "undock");
    engineRef.current?.setMode("station");
    useGameStore.getState().setMode("station");
  }

  async function toggleFullscreen() {
    const root = rootRef.current;
    if (!root) return;
    try {
      if (document.fullscreenElement) {
        await document.exitFullscreen();
      } else {
        await root.requestFullscreen();
      }
    } catch {
      useGameStore.getState().setFlash("FULLSCREEN BLOCKED BY BROWSER");
    }
  }

  return (
    <main
      ref={rootRef}
      className="relative h-[100dvh] w-full overflow-hidden bg-bg text-fg"
      style={{ touchAction: "none" }}
    >
      <canvas ref={canvasRef} className="absolute inset-0 h-full w-full" />

      {mode === "title" ? (
        <div className="absolute inset-0 z-20 flex flex-col justify-end bg-gradient-to-t from-bg via-bg/70 to-transparent px-5 pb-[max(2rem,env(safe-area-inset-bottom))] pt-16 sm:justify-center sm:px-16">
          <p className="font-mono text-xs tracking-[0.42em] text-accent">PERSISTENT UNIVERSE</p>
          <h1 className="mt-2 font-display text-6xl font-semibold tracking-[0.22em] sm:text-8xl">HELION</h1>
          <p className="mt-4 max-w-md text-sm leading-relaxed text-muted sm:text-base">
            Wireframe space. Shared markets. Buy low in the agri lanes, sell high in the industrial core, and try not
            to become a footnote on GalNet.
          </p>
          <label className="mt-8 block max-w-sm font-mono text-xs tracking-[0.2em] text-muted">
            Commander
            <input
              value={name}
              onChange={(e) => setName(e.target.value.toUpperCase())}
              maxLength={16}
              className="mt-2 h-12 w-full rounded-md border border-border bg-surface px-3 font-mono text-sm tracking-[0.18em] text-fg outline-none focus:ring-2 focus:ring-accent/60"
              aria-label="Commander name"
            />
          </label>
          <div className="mt-4 grid max-w-sm grid-cols-2 gap-2">
            <label className="font-mono text-[10px] tracking-[0.22em] text-muted">
              Allegiance
              <select
                value={profile.allegiance}
                onChange={(e) =>
                  setProfile((prev) => ({
                    ...prev,
                    allegiance: e.target.value as CommanderProfile["allegiance"],
                  }))
                }
                className="mt-2 h-10 w-full rounded-md border border-border bg-surface px-2 font-mono text-[10px] tracking-[0.16em] text-fg outline-none focus:ring-2 focus:ring-accent/60"
              >
                <option value="independent">Independent</option>
                <option value="empire">Empire</option>
                <option value="federation">Federation</option>
                <option value="pirate">Pirate</option>
                <option value="union">Union</option>
              </select>
            </label>
            <label className="font-mono text-[10px] tracking-[0.22em] text-muted">
              Faction
              <input
                value={profile.faction}
                onChange={(e) => setProfile((prev) => ({ ...prev, faction: e.target.value }))}
                maxLength={20}
                className="mt-2 h-10 w-full rounded-md border border-border bg-surface px-2 font-mono text-[10px] tracking-[0.14em] text-fg outline-none focus:ring-2 focus:ring-accent/60"
              />
            </label>
          </div>
          <div className="mt-5 flex max-w-sm flex-col gap-2 sm:flex-row">
            <Button className="flex-1" disabled={!ready} onClick={() => startNew(name)}>
              Start
            </Button>
            <Button
              className="flex-1"
              variant="ghost"
              disabled={!ready}
              onClick={() => void persistProfile(name, useGameStore.getState().save)}
            >
              Save profile
            </Button>
            {hasSave ? (
              <Button className="flex-1" variant="ghost" disabled={!ready} onClick={continueSave}>
                Continue
              </Button>
            ) : null}
          </div>
          <p className="mt-3 max-w-sm font-mono text-[10px] tracking-[0.16em] text-muted" aria-live="polite">
            PROFILE {profileSync === "syncing" ? "SYNCING..." : profileSync === "synced" ? "SYNCED" : profileSync === "offline" ? "LOCAL SAVE ONLY" : "READY"}
          </p>
          <p className="mt-6 max-w-lg font-mono text-[11px] leading-relaxed text-muted">
            W/S throttle · A/D yaw left/right · R/F pitch · Q/E roll · Space fire · K mine · L scan · X salvage · H dock · M chart · J jump
          </p>
        </div>
      ) : null}

      {mode === "space" ? <Hud /> : null}
      {mode === "station" ? <StationDock engine={engineRef.current} /> : null}
      {mode === "map" ? <GalaxyChart engine={engineRef.current} /> : null}

      {mode === "dead" ? (
        <div className="absolute inset-0 z-30 flex items-center justify-center bg-bg/80 p-6">
          <div className="w-full max-w-md rounded-xl border border-border bg-surface p-6">
            <p className="font-mono text-xs tracking-[0.3em] text-danger">SHIP LOST</p>
            <h2 className="mt-2 font-display text-3xl font-semibold">Rebuy board</h2>
            <p className="mt-3 text-sm text-muted">
              Insurance will restore your hull for {Math.max(400, Math.round(save.credits * 0.12)).toLocaleString()} CR.
              Cargo is gone.
            </p>
            <div className="mt-6 flex gap-2">
              <Button className="flex-1" onClick={rebuy}>
                Rebuy
              </Button>
              <Button
                className="flex-1"
                variant="ghost"
                onClick={() => {
                  clearSave();
                  useGameStore.getState().setMode("title");
                  engineRef.current?.setMode("title");
                }}
              >
                Resign
              </Button>
            </div>
          </div>
        </div>
      ) : null}

      {paused && mode === "space" ? (
        <div className="absolute inset-0 z-30 flex items-center justify-center bg-bg/70 p-6">
          <div className="w-full max-w-sm rounded-xl border border-border bg-surface p-6">
            <h2 className="font-display text-2xl">Paused</h2>
            <div className="mt-5 flex flex-col gap-2">
              <Button onClick={() => useGameStore.getState().setPaused(false)}>Resume</Button>
              <Button variant="ghost" onClick={() => useGameStore.getState().setMode("map")}>
                Chart
              </Button>
              <Button
                variant="ghost"
                onClick={() => {
                  useGameStore.getState().persist();
                  useGameStore.getState().setMode("title");
                  engineRef.current?.setMode("title");
                  setHasSave(true);
                }}
              >
                Exit to title
              </Button>
            </div>
          </div>
        </div>
      ) : null}

      <TouchControls input={engineRef.current?.input ?? null} engine={engineRef.current} />

      {mode === "title" ? (
        <button
          className="absolute right-3 top-[max(0.75rem,env(safe-area-inset-top))] z-40 h-9 rounded-sm border border-border bg-surface/80 px-3 font-mono text-[10px] tracking-widest text-muted"
          onClick={() => {
            const next = !isMuted();
            setMuted(next);
            setMutedUi(next);
          }}
        >
          {muted ? "SOUND OFF" : "SOUND ON"}
        </button>
      ) : null}
      <button
        type="button"
        aria-label={ttsEnabled ? "Disable voice comms" : "Enable voice comms"}
        className={`absolute right-3 top-[max(6.25rem,calc(env(safe-area-inset-top)+6.25rem))] z-40 h-9 rounded-sm border px-3 font-mono text-[10px] tracking-widest ${
          ttsEnabled ? "border-accent/60 bg-accent/10 text-accent" : "border-border bg-surface/80 text-muted"
        }`}
        onClick={() => {
          initializeTTS();
          const next = !ttsEnabled;
          setTTSEnabled(next);
          setTtsEnabledUi(next);
          if (next) speakTTS("Voice comms online.");
        }}
      >
        {ttsEnabled ? "VOICE ON" : "VOICE OFF"}
      </button>
      <button
        type="button"
        aria-label={fullscreen ? "Exit fullscreen" : "Enter fullscreen"}
        className="absolute right-3 top-[max(3.5rem,calc(env(safe-area-inset-top)+3.5rem))] z-40 h-9 rounded-sm border border-border bg-surface/80 px-3 font-mono text-[10px] tracking-widest text-muted"
        onClick={() => void toggleFullscreen()}
      >
        {fullscreen ? "EXIT FULLSCREEN" : "FULLSCREEN"}
      </button>
    </main>
  );
}
