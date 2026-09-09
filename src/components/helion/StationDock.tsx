import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { economyLabel, getSystem, governmentLabel } from "@/game/galaxy";
import { cargoCapacity, cargoUsed, SHIPS, SHIP_ORDER } from "@/game/ships";
import { useGameStore } from "@/game/store";
import { executeTradeFn, getBoardFn, getMarketFn } from "@/game/universe.functions";
import { sfxPlay, unlockAudio } from "@/game/audio";
import type { CommodityId, MarketRow } from "@/game/types";
import type { EngineHandle } from "@/game/engineApi";

type Tab = "market" | "yard" | "board";

export function StationDock({ engine }: { engine: EngineHandle | null }) {
  const save = useGameStore((s) => s.save);
  const market = useGameStore((s) => s.market);
  const news = useGameStore((s) => s.news);
  const tick = useGameStore((s) => s.tick);
  const setMode = useGameStore((s) => s.setMode);
  const [tab, setTab] = useState<Tab>("market");
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState("");
  const sys = getSystem(save.systemId);
  const def = SHIPS[save.shipId];
  const used = cargoUsed(save.cargo);
  const cap = cargoCapacity(save.shipId, save.cargoUpgrade);

  useEffect(() => {
    let live = true;
    void Promise.all([
      getMarketFn({ data: { systemId: save.systemId } }),
      getBoardFn({ data: { systemId: save.systemId } }),
    ])
      .then(([m, b]) => {
        if (!live) return;
        useGameStore.getState().setMarket(m.market, m.tick);
        useGameStore.getState().setNews(b.news);
      })
      .catch(() => {
        if (live) setErr("Station net is dark. Local prices unavailable.");
      });
    return () => {
      live = false;
    };
  }, [save.systemId]);

  async function trade(row: MarketRow, side: "buy" | "sell") {
    setBusy(true);
    setErr("");
    try {
      if (side === "buy") {
        if (save.credits < row.price) throw new Error("Insufficient credits");
        if (used >= cap) throw new Error("Hold is full");
      } else if ((save.cargo[row.commodity] ?? 0) < 1) {
        throw new Error("None in hold");
      }
      const res = await executeTradeFn({
        data: { systemId: save.systemId, commodity: row.commodity, side, qty: 1 },
      });
      const st = useGameStore.getState();
      const cargo = { ...st.save.cargo };
      if (side === "buy") {
        cargo[row.commodity] = (cargo[row.commodity] ?? 0) + 1;
        st.patchSave({ credits: st.save.credits - res.unitPrice, cargo });
      } else {
        const n = (cargo[row.commodity] ?? 0) - 1;
        if (n <= 0) delete cargo[row.commodity];
        else cargo[row.commodity] = n;
        st.patchSave({ credits: st.save.credits + res.unitPrice, cargo });
      }
      const next = (st.market ?? []).map((m) =>
        m.commodity === row.commodity ? { ...m, price: res.price, stock: res.stock } : m,
      );
      st.setMarket(next, res.tick);
      sfxPlay.ui();
    } catch (e) {
      setErr(e instanceof Error ? e.message : "Trade refused");
      sfxPlay.warn();
    } finally {
      setBusy(false);
    }
  }

  function repair() {
    const missing = Math.max(0, def.hull - save.hull) + Math.max(0, def.shields - save.shields);
    const cost = Math.ceil(missing * 2.5);
    if (cost <= 0) return;
    if (save.credits < cost) {
      setErr("Cannot afford repairs");
      return;
    }
    useGameStore.getState().patchSave({
      credits: save.credits - cost,
      hull: def.hull,
      shields: def.shields,
    });
    useGameStore.getState().setFlight({ hull: def.hull, shields: def.shields });
    sfxPlay.dock();
  }

  function refuel() {
    const need = Math.max(0, def.tank - save.fuel);
    const units = Math.ceil(need);
    const hPrice = market?.find((m) => m.commodity === "hydrogen")?.price ?? 8;
    const cost = units * hPrice;
    if (need < 0.05) return;
    if (save.credits < cost) {
      setErr("Cannot afford fuel");
      return;
    }
    useGameStore.getState().patchSave({ credits: save.credits - cost, fuel: def.tank });
    sfxPlay.ui();
  }

  function buyShip(id: (typeof SHIP_ORDER)[number]) {
    const next = SHIPS[id];
    if (id === save.shipId) return;
    const tradeIn = Math.round(def.price * 0.55);
    const cost = Math.max(0, next.price - tradeIn);
    if (save.credits < cost) {
      setErr("Insufficient credits");
      return;
    }
    const nextCap = cargoCapacity(id, save.cargoUpgrade);
    if (used > nextCap) {
      setErr("Dump cargo before transferring hull");
      return;
    }
    useGameStore.getState().patchSave({
      shipId: id,
      credits: save.credits - cost,
      hull: next.hull,
      shields: next.shields,
    });
    useGameStore.getState().setFlight({
      hull: next.hull,
      shields: next.shields,
      maxHull: next.hull,
      maxShields: next.shields,
    });
    sfxPlay.dock();
  }

  function buyCargoUpgrade() {
    if (save.cargoUpgrade) return;
    if (save.credits < 2400) {
      setErr("Need 2,400 CR");
      return;
    }
    useGameStore.getState().patchSave({ credits: save.credits - 2400, cargoUpgrade: true });
    sfxPlay.ui();
  }

  return (
    <div className="absolute inset-0 z-20 flex flex-col bg-bg/90 pt-[max(0.75rem,env(safe-area-inset-top))]">
      <header className="mx-auto flex w-full max-w-5xl items-end justify-between gap-3 px-4 py-3">
        <div>
          <p className="font-mono text-xs tracking-[0.3em] text-muted">CORIOLIS · TICK {tick}</p>
          <h2 className="font-display text-3xl font-semibold tracking-[0.08em]">{sys?.name ?? "Station"}</h2>
          <p className="mt-1 font-mono text-xs text-muted">
            {sys ? `${economyLabel(sys.economy)} · ${governmentLabel(sys.government)} · Tech ${sys.tech}` : ""}
          </p>
        </div>
        <div className="text-right font-mono text-xs">
          <p className="text-lg tabular-nums text-accent">{save.credits.toLocaleString()} CR</p>
          <p className="text-muted">
            Hold {used}/{cap} · Fuel {save.fuel.toFixed(1)}
          </p>
        </div>
      </header>

      <nav className="mx-auto flex w-full max-w-5xl gap-2 px-4">
        {(["market", "yard", "board"] as const).map((t) => (
          <button
            key={t}
            onClick={() => setTab(t)}
            className={`h-10 rounded-md px-4 font-mono text-xs uppercase tracking-[0.18em] ${
              tab === t ? "bg-accent text-accent-fg" : "border border-border bg-surface text-muted"
            }`}
          >
            {t}
          </button>
        ))}
      </nav>

      <div className="mx-auto mt-3 min-h-0 w-full max-w-5xl flex-1 overflow-auto px-4 pb-28">
        {err ? <p className="mb-3 font-mono text-xs text-danger">{err}</p> : null}

        {tab === "market" ? (
          <div className="overflow-hidden rounded-xl border border-border bg-surface">
            <table className="w-full text-left font-mono text-xs">
              <thead className="bg-surface-2 text-muted">
                <tr>
                  <th className="px-3 py-2 font-medium">Commodity</th>
                  <th className="px-3 py-2 font-medium">CR/t</th>
                  <th className="hidden px-3 py-2 font-medium sm:table-cell">Stock</th>
                  <th className="px-3 py-2 font-medium">Hold</th>
                  <th className="px-3 py-2 font-medium" />
                </tr>
              </thead>
              <tbody>
                {(market ?? []).map((row) => {
                  const hot = row.price > row.base * 1.2;
                  const cheap = row.price < row.base * 0.8;
                  return (
                    <tr key={row.commodity} className="border-t border-border">
                      <td className="px-3 py-2 text-fg">{row.name}</td>
                      <td className={`px-3 py-2 tabular-nums ${hot ? "text-danger" : cheap ? "text-accent" : ""}`}>
                        {row.price}
                      </td>
                      <td className="hidden px-3 py-2 tabular-nums text-muted sm:table-cell">{row.stock}</td>
                      <td className="px-3 py-2 tabular-nums">{save.cargo[row.commodity as CommodityId] ?? 0}</td>
                      <td className="px-3 py-2 text-right">
                        <div className="flex justify-end gap-1">
                          <Button size="sm" variant="quiet" disabled={busy} onClick={() => void trade(row, "buy")}>
                            Buy
                          </Button>
                          <Button size="sm" variant="ghost" disabled={busy} onClick={() => void trade(row, "sell")}>
                            Sell
                          </Button>
                        </div>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
            {!market ? <p className="px-3 py-6 text-center text-muted">Awaiting market feed…</p> : null}
          </div>
        ) : null}

        {tab === "yard" ? (
          <div className="grid gap-3 sm:grid-cols-2">
            <section className="rounded-xl border border-border bg-surface p-4">
              <h3 className="font-display text-lg">Services</h3>
              <p className="mt-2 font-mono text-xs text-muted">
                Repair hull/shields · Refuel from station hydrogen
              </p>
              <div className="mt-4 flex flex-wrap gap-2">
                <Button variant="quiet" onClick={repair}>
                  Repair
                </Button>
                <Button variant="quiet" onClick={refuel}>
                  Refuel
                </Button>
                <Button variant="quiet" disabled={save.cargoUpgrade} onClick={buyCargoUpgrade}>
                  {save.cargoUpgrade ? "Hold expanded" : "Expand hold 2,400 CR"}
                </Button>
              </div>
            </section>
            {SHIP_ORDER.map((id) => {
              const s = SHIPS[id];
              const owned = id === save.shipId;
              const tradeIn = Math.round(def.price * 0.55);
              const cost = Math.max(0, s.price - tradeIn);
              return (
                <section key={id} className="rounded-xl border border-border bg-surface p-4">
                  <h3 className="font-display text-lg">{s.name}</h3>
                  <p className="mt-1 font-mono text-xs text-muted">
                    Hold {s.cargo}t · Jump {s.jump} ly · Speed {s.maxSpeed}
                  </p>
                  <p className="mt-3 font-mono text-sm text-accent">
                    {owned ? "Current hull" : cost === 0 ? "Transfer" : `${cost.toLocaleString()} CR`}
                  </p>
                  <Button className="mt-3" variant={owned ? "ghost" : "primary"} disabled={owned} onClick={() => buyShip(id)}>
                    {owned ? "Fitted" : "Transfer"}
                  </Button>
                </section>
              );
            })}
          </div>
        ) : null}

        {tab === "board" ? (
          <div className="rounded-xl border border-border bg-surface p-4">
            <h3 className="font-display text-lg">GalNet</h3>
            <p className="mt-1 font-mono text-xs text-muted">
              Shared persistent board. Prices move when anyone trades.
            </p>
            <ul className="mt-4 space-y-2 font-mono text-sm">
              {news.length ? news.map((n, i) => (
                <li key={i} className="border-l border-accent/40 pl-3 text-fg">
                  {n}
                </li>
              )) : <li className="text-muted">No dispatches yet.</li>}
            </ul>
            <p className="mt-6 font-mono text-xs text-muted">
              First run: buy cheap Food here if Helion is agricultural, jump to Zaon, sell, return with Machinery.
            </p>
          </div>
        ) : null}
      </div>

      <div className="absolute inset-x-0 bottom-0 border-t border-border bg-surface/95 p-3 pb-[max(0.75rem,env(safe-area-inset-bottom))]">
        <div className="mx-auto flex max-w-5xl gap-2">
          <Button
            className="flex-1"
            onClick={() => {
              unlockAudio();
              engine?.launch();
            }}
          >
            Launch
          </Button>
          <Button variant="ghost" onClick={() => setMode("map")}>
            Chart
          </Button>
        </div>
      </div>
    </div>
  );
}
