import { a as getGalaxy, l as marketTemplate, o as getSystem, t as COMMODITY_IDS } from "./galaxy-CtkES_h3.mjs";
//#region node_modules/.nitro/vite/services/ssr/assets/universe.server-2oPZAY48.js
var _0002_galaxy_default = "-- Persistent galaxy: shared markets, news, and anonymous traffic.\n-- Unowned rows (no user_id). No personal data.\n\ncreate table if not exists galaxy_clock (\n  id integer primary key check (id = 1),\n  tick integer not null default 0,\n  last_tick_at timestamptz not null default now()\n);\n\ninsert into galaxy_clock (id, tick)\n  values (1, 0)\n  on conflict (id) do nothing;\n\ncreate table if not exists markets (\n  system_id text not null,\n  commodity text not null,\n  stock integer not null,\n  price integer not null,\n  primary key (system_id, commodity)\n);\n\ncreate table if not exists dispatches (\n  id serial primary key,\n  tick integer not null,\n  system_id text,\n  headline text not null,\n  created_at timestamptz not null default now()\n);\n\ncreate table if not exists traffic (\n  id serial primary key,\n  system_id text not null,\n  kind text not null,\n  detail text not null,\n  created_at timestamptz not null default now()\n);\n\ncreate index if not exists traffic_created_idx on traffic (created_at desc);\ncreate index if not exists dispatches_created_idx on dispatches (created_at desc);\n";
/**
* Migration bookkeeping shared by the two appliers — `scripts/migrate.mjs`
* (deploy, `readdir`) and `src/lib/db.ts` (PGLite preview, `import.meta.glob`).
*
* Applied files are keyed by BASENAME, so the same file applies once no matter
* which directory it is globbed from. That is what makes the auth schema safe to
* copy from `migrations/auth/` into `migrations/` when an app turns sign-in on:
* a database that already has `0001_auth.sql` will not re-run it.
*
* Neither applier descends into subdirectories, so `migrations/auth/*.sql` is
* out of scope for both until it is copied up.
*/
/**
* The `_migrations` key for a migration path (or bare filename).
* @param {string} path
* @returns {string}
*/
function migrationName(path) {
	return path.split("/").pop() ?? path;
}
/**
* @param {string} path
* @returns {boolean}
*/
function isMigrationFile(path) {
	return path.endsWith(".sql");
}
/**
* Migrations in `paths` that are not yet in `applied`, in apply order.
* Non-`.sql` entries (a `readdir` also yields `migrations/auth/`) are dropped.
* @param {Iterable<string>} paths
* @param {Iterable<string>} applied
* @returns {Array<{ name: string, path: string }>}
*/
function pendingMigrations(paths, applied) {
	const done = new Set(applied);
	return [...paths].filter(isMigrationFile).map((path) => ({
		name: migrationName(path),
		path
	})).sort((a, b) => a.name.localeCompare(b.name)).filter(({ name }) => !done.has(name));
}
var rawDatabaseUrl = typeof process !== "undefined" ? process.env.DATABASE_URL : void 0;
var databaseUrl = rawDatabaseUrl && rawDatabaseUrl.trim() ? rawDatabaseUrl : void 0;
/**
* Active backend: real **Neon** when `DATABASE_URL` is set (deployed / configured
* sandbox), otherwise a local embedded **PGLite** (Postgres compiled to WASM) so
* the app has a working database even with nothing configured — the live preview
* included. Swap in Neon later by just setting `DATABASE_URL`; no code changes.
*/
var dbSource = databaseUrl ? "neon" : "pglite";
/**
* Init state lives on globalThis as promises: dev HMR creates new instances of
* this module, and two instances racing module-level state would open a second
* pool or run two concurrent PGLite migration passes (whose duplicate
* `_migrations` insert rejects — and would get memoized, poisoning every later
* `getSql()`). A failed init clears its slot so the next call retries.
*/
var globalRef = globalThis;
/**
* Result-type parity: Postgres sends every value as text plus a type OID — the
* JS value is the DRIVER's parsing choice, and pg and PGLite disagree (pg:
* int8 -> string, date -> local-midnight Date; PGLite: int8 -> BigInt, which
* JSON.stringify rejects, date -> UTC Date). Normalize both so preview and
* production return identical, JSON-safe shapes:
*   int8/bigint (incl. count(*)) -> number (past 2^53 loses precision — cast
*                                   `::text` if you ever need huge integers)
*   date                         -> 'YYYY-MM-DD' string
*   interval                     -> Postgres interval text
* numeric already comes back as a string on both (arbitrary precision).
*/
var OID_INT8 = 20;
var OID_DATE = 1082;
var OID_INTERVAL = 1186;
var identity = (v) => v;
/** Wrap a query runner in the tagged-template + `.query()` `Sql` surface. */
function toSql(run) {
	const sql = (async (strings, ...values) => {
		let text = strings[0];
		for (let i = 0; i < values.length; i += 1) text += `$${i + 1}${strings[i + 1]}`;
		return run(text, values);
	});
	sql.query = (text, params = []) => run(text, params);
	return sql;
}
function createNeonSql() {
	globalRef.__pgSqlPromise__ ??= (async () => {
		const { Pool, types } = await import("../_libs/pg.mjs").then((n) => n.t);
		types.setTypeParser(OID_INT8, Number);
		types.setTypeParser(OID_DATE, identity);
		types.setTypeParser(OID_INTERVAL, identity);
		const pool = new Pool({ connectionString: databaseUrl });
		return toSql(async (text, params) => {
			return (await pool.query(text, params)).rows;
		});
	})().catch((err) => {
		globalRef.__pgSqlPromise__ = void 0;
		throw err;
	});
	return globalRef.__pgSqlPromise__;
}
async function createPgliteSql() {
	globalRef.__pgliteInstance__ ??= (async () => {
		const { PGlite } = await import("../_libs/electric-sql__pglite.mjs").then((n) => n.t);
		const pg = new PGlite({ parsers: {
			[OID_INT8]: Number,
			[OID_DATE]: identity,
			[OID_INTERVAL]: identity
		} });
		await pg.waitReady;
		await pg.exec("create table if not exists _migrations (name text primary key, applied_at timestamptz not null default now())");
		return pg;
	})().catch((err) => {
		globalRef.__pgliteInstance__ = void 0;
		throw err;
	});
	const pg = await globalRef.__pgliteInstance__;
	const migrate = async () => {
		const migrations = /* #__PURE__ */ Object.assign({ "/migrations/0002_galaxy.sql": _0002_galaxy_default });
		const done = (await pg.query("select name from _migrations")).rows.map((r) => r.name);
		for (const { name, path } of pendingMigrations(Object.keys(migrations), done)) await pg.transaction(async (tx) => {
			await tx.exec(migrations[path]);
			await tx.query("insert into _migrations (name) values ($1)", [name]);
		});
	};
	const pass = (globalRef.__pgliteMigrateChain__ ?? Promise.resolve()).catch(() => void 0).then(migrate);
	globalRef.__pgliteMigrateChain__ = pass;
	await pass;
	return toSql(async (text, params) => {
		return (await pg.query(text, params)).rows;
	});
}
var sqlPromise = null;
async function createSql() {
	if (typeof window !== "undefined") throw new Error("@/lib/db is server-only — call getSql() from a createServerFn handler or a server route loader, never from client code.");
	return dbSource === "neon" ? createNeonSql() : createPgliteSql();
}
/**
* Get the shared, **server-only** SQL client. Neon when `DATABASE_URL` is set,
* otherwise the local PGLite fallback. Memoized — safe to call per request.
*
* Schema comes from `migrations/*.sql`, auto-applied before the first query on
* both backends — define tables there, never inline in server functions.
*/
function getSql() {
	sqlPromise ??= createSql().catch((err) => {
		sqlPromise = null;
		throw err;
	});
	return sqlPromise;
}
/**
* Finish DB bootstrap before the server handles traffic.
*
* - **PGLite** (preview / no `DATABASE_URL`): open the in-memory DB and apply
*   `migrations/*.sql`. Idempotent — concurrent callers share one promise.
* - **Neon**: no-op (pool is created lazily on first query).
*
* Vite `configureServer` awaits this at dev startup; production imports of this
* module kick it off immediately (see bottom of file).
*/
function ensureDbReady() {
	if (dbSource !== "pglite") return Promise.resolve();
	return getSql().then(() => void 0);
}
var globalBoot = globalThis;
if (typeof window === "undefined" && dbSource === "pglite") globalBoot.__pgBootstrapPromise__ ??= ensureDbReady().catch((err) => {
	globalBoot.__pgBootstrapPromise__ = void 0;
	console.error("[db] PGLite bootstrap failed:", err);
	throw err;
});
var TICK_MS = 9e4;
var MAX_CATCHUP = 8;
async function ensureClock() {
	await (await getSql())`insert into galaxy_clock (id, tick) values (1, 0) on conflict (id) do nothing`;
}
async function ensureMarkets(systemId) {
	const sys = getSystem(systemId);
	if (!sys) throw new Error("Unknown system");
	const sql = await getSql();
	const existing = await sql`select system_id, commodity, stock, price from markets where system_id = ${systemId}`;
	if (existing.length >= COMMODITY_IDS.length) return existing.map((r) => {
		const meta = marketTemplate(sys).find((m) => m.commodity === r.commodity);
		return {
			commodity: r.commodity,
			name: meta?.name ?? r.commodity,
			price: r.price,
			stock: r.stock,
			base: meta?.base ?? 10
		};
	});
	const rows = marketTemplate(sys);
	for (const row of rows) await sql`
      insert into markets (system_id, commodity, stock, price)
      values (${systemId}, ${row.commodity}, ${row.stock}, ${row.price})
      on conflict (system_id, commodity) do nothing
    `;
	return rows;
}
function equilibrium(systemId, commodity) {
	const sys = getSystem(systemId);
	const row = marketTemplate(sys).find((m) => m.commodity === commodity);
	return {
		price: row.price,
		stock: row.stock
	};
}
async function tickUniverse() {
	await ensureClock();
	const sql = await getSql();
	const [clock] = await sql`select tick, last_tick_at::text as last_tick_at from galaxy_clock where id = 1`;
	if (!clock) return 0;
	const last = Date.parse(clock.last_tick_at);
	const elapsed = Number.isFinite(last) ? Date.now() - last : TICK_MS;
	const steps = Math.min(MAX_CATCHUP, Math.max(0, Math.floor(elapsed / TICK_MS)));
	if (steps <= 0) return clock.tick;
	const markets = await sql`select system_id, commodity, stock, price from markets`;
	let tick = clock.tick;
	for (let s = 0; s < steps; s += 1) {
		tick += 1;
		for (const row of markets) {
			if (!COMMODITY_IDS.includes(row.commodity)) continue;
			const eq = equilibrium(row.system_id, row.commodity);
			const drift = Math.sign(eq.price - row.price) * (tick % 3 === 0 ? 1 : 0);
			const restock = Math.sign(eq.stock - row.stock) * Math.max(1, Math.round(eq.stock * .03));
			row.price = Math.max(2, Math.min(400, row.price + drift));
			row.stock = Math.max(0, Math.min(400, row.stock + restock));
		}
	}
	for (const row of markets) await sql`update markets set stock = ${row.stock}, price = ${row.price} where system_id = ${row.system_id} and commodity = ${row.commodity}`;
	const galaxy = getGalaxy();
	const sys = galaxy[tick % galaxy.length];
	const headlines = [
		`Food convoys rerouted through ${sys.name}.`,
		`Pirate activity reported near ${sys.name}.`,
		`${sys.name} station restocked industrial goods.`,
		`Luxury demand rising in ${sys.name}.`,
		`Refinery output steady across the ${sys.name} cluster.`
	];
	const headline = headlines[tick % headlines.length];
	await sql`insert into dispatches (tick, system_id, headline) values (${tick}, ${sys.id}, ${headline})`;
	await sql`delete from dispatches where id not in (select id from dispatches order by id desc limit 24)`;
	await sql`delete from traffic where id not in (select id from traffic order by id desc limit 40)`;
	await sql`update galaxy_clock set tick = ${tick}, last_tick_at = now() where id = 1`;
	return tick;
}
async function readMarket(systemId) {
	return {
		tick: await tickUniverse(),
		market: await ensureMarkets(systemId)
	};
}
async function applyTrade(input) {
	const qty = Math.max(1, Math.min(20, Math.floor(input.qty)));
	if (!COMMODITY_IDS.includes(input.commodity)) throw new Error("Unknown cargo");
	if (!getSystem(input.systemId)) throw new Error("Unknown system");
	await tickUniverse();
	await ensureMarkets(input.systemId);
	const sql = await getSql();
	const [row] = await sql`
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
		price = Math.max(2, price + Math.max(1, Math.round(take * .35)));
		unitPrice = row.price;
		await sql`update markets set stock = ${stock}, price = ${price} where system_id = ${input.systemId} and commodity = ${input.commodity}`;
		await sql`
      insert into traffic (system_id, kind, detail)
      values (${input.systemId}, 'trade', ${`A trader lifted ${take}t ${input.commodity}.`})
    `;
		const [clock] = await sql`select tick from galaxy_clock where id = 1`;
		return {
			unitPrice,
			stock,
			price,
			tick: clock?.tick ?? 0
		};
	}
	stock = Math.min(400, stock + qty);
	price = Math.max(2, price - Math.max(1, Math.round(qty * .3)));
	await sql`update markets set stock = ${stock}, price = ${price} where system_id = ${input.systemId} and commodity = ${input.commodity}`;
	await sql`
    insert into traffic (system_id, kind, detail)
    values (${input.systemId}, 'trade', ${`A trader offloaded ${qty}t ${input.commodity}.`})
  `;
	const [clock] = await sql`select tick from galaxy_clock where id = 1`;
	return {
		unitPrice,
		stock,
		price,
		tick: clock?.tick ?? 0
	};
}
async function readBoard(systemId) {
	const tick = await tickUniverse();
	await ensureMarkets(systemId);
	const sql = await getSql();
	const news = await sql`
    select headline, tick from dispatches order by id desc limit 8
  `;
	const traffic = await sql`
    select system_id, kind, detail from traffic order by id desc limit 10
  `;
	return {
		tick,
		news: news.map((n) => n.headline),
		traffic: traffic.map((t) => ({
			systemId: t.system_id,
			kind: t.kind,
			detail: t.detail
		}))
	};
}
async function insertTraffic(systemId, kind, detail) {
	if (!getSystem(systemId)) return;
	await (await getSql())`insert into traffic (system_id, kind, detail) values (${systemId}, ${kind}, ${detail})`;
}
//#endregion
export { applyTrade, insertTraffic, readBoard, readMarket };
