import { getSql } from "@/lib/db";
import {
  COMMODITY_IDS,
  getGalaxy,
  getSystem,
  marketTemplate,
} from "./galaxy";
import type { CommodityId, MarketRow } from "./types";

const TICK_MS = 90_000;
const MAX_CATCHUP = 8;

type MarketDb = { system_id: string; commodity: string; stock: number; price: number };
type ClockRow = { tick: number; last_tick_at: string };

async function ensureClock() {
  const sql = await getSql();
  await sql`insert into galaxy_clock (id, tick) values (1, 0) on conflict (id) do nothing`;
}

export async function ensureMarkets(systemId: string): Promise<MarketRow[]> {
  const sys = getSystem(systemId);
  if (!sys) throw new Error("Unknown system");
  const sql = await getSql();
  const existing = await sql<MarketDb>`select system_id, commodity, stock, price from markets where system_id = ${systemId}`;
  if (existing.length >= COMMODITY_IDS.length) {
    return existing.map((r) => {
      const meta = marketTemplate(sys).find((m) => m.commodity === r.commodity);
      return {
        commodity: r.commodity as CommodityId,
        name: meta?.name ?? r.commodity,
        price: r.price,
        stock: r.stock,
        base: meta?.base ?? 10,
      };
    });
  }
  const rows = marketTemplate(sys);
  for (const row of rows) {
    await sql`
      insert into markets (system_id, commodity, stock, price)
      values (${systemId}, ${row.commodity}, ${row.stock}, ${row.price})
      on conflict (system_id, commodity) do nothing
    `;
  }
  return rows;
}

function equilibrium(systemId: string, commodity: CommodityId): { price: number; stock: number } {
  const sys = getSystem(systemId)!;
  const row = marketTemplate(sys).find((m) => m.commodity === commodity)!;
  return { price: row.price, stock: row.stock };
}

export async function tickUniverse(): Promise<number> {
  await ensureClock();
  const sql = await getSql();
  const [clock] = await sql<ClockRow>`select tick, last_tick_at::text as last_tick_at from galaxy_clock where id = 1`;
  if (!clock) return 0;
  const last = Date.parse(clock.last_tick_at);
  const elapsed = Number.isFinite(last) ? Date.now() - last : TICK_MS;
  const steps = Math.min(MAX_CATCHUP, Math.max(0, Math.floor(elapsed / TICK_MS)));
  if (steps <= 0) return clock.tick;

  const markets = await sql<MarketDb>`select system_id, commodity, stock, price from markets`;
  let tick = clock.tick;
  for (let s = 0; s < steps; s += 1) {
    tick += 1;
    for (const row of markets) {
      if (!COMMODITY_IDS.includes(row.commodity as CommodityId)) continue;
      const eq = equilibrium(row.system_id, row.commodity as CommodityId);
      const drift = Math.sign(eq.price - row.price) * (tick % 3 === 0 ? 1 : 0);
      const restock = Math.sign(eq.stock - row.stock) * Math.max(1, Math.round(eq.stock * 0.03));
      row.price = Math.max(2, Math.min(400, row.price + drift));
      row.stock = Math.max(0, Math.min(400, row.stock + restock));
    }
  }
  for (const row of markets) {
    await sql`update markets set stock = ${row.stock}, price = ${row.price} where system_id = ${row.system_id} and commodity = ${row.commodity}`;
  }

  const galaxy = getGalaxy();
  const sys = galaxy[tick % galaxy.length]!;
  const headlines = [
    `Food convoys rerouted through ${sys.name}.`,
    `Pirate activity reported near ${sys.name}.`,
    `${sys.name} station restocked industrial goods.`,
    `Luxury demand rising in ${sys.name}.`,
    `Refinery output steady across the ${sys.name} cluster.`,
  ];
  const headline = headlines[tick % headlines.length]!;
  await sql`insert into dispatches (tick, system_id, headline) values (${tick}, ${sys.id}, ${headline})`;
  await sql`delete from dispatches where id not in (select id from dispatches order by id desc limit 24)`;
  await sql`delete from traffic where id not in (select id from traffic order by id desc limit 40)`;
  await sql`update galaxy_clock set tick = ${tick}, last_tick_at = now() where id = 1`;
  return tick;
}

export async function readMarket(systemId: string): Promise<{ tick: number; market: MarketRow[] }> {
  const tick = await tickUniverse();
  const market = await ensureMarkets(systemId);
  return { tick, market };
}

export async function applyTrade(input: {
  systemId: string;
  commodity: CommodityId;
  side: "buy" | "sell";
  qty: number;
}): Promise<{ unitPrice: number; stock: number; price: number; tick: number }> {
  const qty = Math.max(1, Math.min(20, Math.floor(input.qty)));
  if (!COMMODITY_IDS.includes(input.commodity)) throw new Error("Unknown cargo");
  if (!getSystem(input.systemId)) throw new Error("Unknown system");
  await tickUniverse();
  await ensureMarkets(input.systemId);
  const sql = await getSql();
  const [row] = await sql<MarketDb>`
    select system_id, commodity, stock, price from markets
    where system_id = ${input.systemId} and commodity = ${input.commodity}
  `;
  if (!row) throw new Error("No market");

  let stock = row.stock;
  let price = row.price;
  let unitPrice = price;

  if (input.side === "buy") {
    if (stock <= 0) throw new Error("Sold out");
    const take = Math.min(qty, stock);
    stock -= take;
    price = Math.max(2, price + Math.max(1, Math.round(take * 0.35)));
    unitPrice = row.price;
    await sql`update markets set stock = ${stock}, price = ${price} where system_id = ${input.systemId} and commodity = ${input.commodity}`;
    await sql`
      insert into traffic (system_id, kind, detail)
      values (${input.systemId}, 'trade', ${`A trader lifted ${take}t ${input.commodity}.`})
    `;
    const [clock] = await sql<{ tick: number }>`select tick from galaxy_clock where id = 1`;
    return { unitPrice, stock, price, tick: clock?.tick ?? 0 };
  }

  stock = Math.min(400, stock + qty);
  price = Math.max(2, price - Math.max(1, Math.round(qty * 0.3)));
  await sql`update markets set stock = ${stock}, price = ${price} where system_id = ${input.systemId} and commodity = ${input.commodity}`;
  await sql`
    insert into traffic (system_id, kind, detail)
    values (${input.systemId}, 'trade', ${`A trader offloaded ${qty}t ${input.commodity}.`})
  `;
  const [clock] = await sql<{ tick: number }>`select tick from galaxy_clock where id = 1`;
  return { unitPrice, stock, price, tick: clock?.tick ?? 0 };
}

export async function readBoard(systemId: string) {
  const tick = await tickUniverse();
  await ensureMarkets(systemId);
  const sql = await getSql();
  const news = await sql<{ headline: string; tick: number }>`
    select headline, tick from dispatches order by id desc limit 8
  `;
  const traffic = await sql<{ system_id: string; kind: string; detail: string }>`
    select system_id, kind, detail from traffic order by id desc limit 10
  `;
  return {
    tick,
    news: news.map((n) => n.headline),
    traffic: traffic.map((t) => ({ systemId: t.system_id, kind: t.kind, detail: t.detail })),
  };
}

export async function insertTraffic(systemId: string, kind: string, detail: string) {
  if (!getSystem(systemId)) return;
  const sql = await getSql();
  await sql`insert into traffic (system_id, kind, detail) values (${systemId}, ${kind}, ${detail})`;
}
