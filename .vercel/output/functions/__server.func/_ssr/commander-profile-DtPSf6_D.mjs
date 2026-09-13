import { Ft as number, It as object, Ot as _enum, Rt as record, kt as any, zt as string } from "../_libs/@better-auth/core+[...].mjs";
import { r as createServerFn } from "./ssr.mjs";
import { r as getSql } from "./db-CZNJwj7z.mjs";
import { t as createServerRpc } from "./createServerRpc-CcvdN_gc.mjs";
import { t as authMiddleware } from "./middleware-CIWj3S0M.mjs";
//#region node_modules/.nitro/vite/services/ssr/assets/commander-profile-DtPSf6_D.js
var commanderProfileSchema = object({
	commanderName: string().trim().min(2).max(16).default("JAMESON"),
	allegiance: _enum([
		"independent",
		"empire",
		"federation",
		"pirate",
		"union"
	]).default("independent"),
	faction: string().trim().min(2).max(32).default("free-traders"),
	standing: number().int().min(-1e3).max(1e4).default(0),
	experience: number().int().min(0).max(1e6).default(0),
	credits: number().int().min(0).default(1500),
	shipId: string().trim().min(2).max(32).default("sidewinder"),
	inventory: record(string(), number().int().nonnegative()).default({}),
	reputation: record(string(), number().int()).default({}),
	notes: string().max(250).default(""),
	saveState: record(string(), any()).default({})
});
function defaultCommanderProfile(name = "JAMESON") {
	return {
		commanderName: name.trim().slice(0, 16).toUpperCase() || "JAMESON",
		allegiance: "independent",
		faction: "free-traders",
		standing: 0,
		experience: 0,
		credits: 1500,
		shipId: "sidewinder",
		inventory: {},
		reputation: {},
		notes: "",
		saveState: {}
	};
}
function parseJsonField(value) {
	if (!value) return {};
	if (typeof value === "string") try {
		return JSON.parse(value);
	} catch {
		return {};
	}
	return value;
}
var loadCommanderProfile_createServerFn_handler = createServerRpc({
	id: "4f559e749a2879a5035d393bcb79821ea7ce89b474d42d6ba75b4581b4afc9b1",
	name: "loadCommanderProfile",
	filename: "src/game/commander-profile.ts"
}, (opts) => loadCommanderProfile.__executeServer(opts));
var loadCommanderProfile = createServerFn({ method: "GET" }).middleware([authMiddleware]).handler(loadCommanderProfile_createServerFn_handler, async ({ context }) => {
	const row = (await (await getSql()).query(`
        select *
        from commander_profiles
        where user_id = $1
        limit 1
      `, [context.userId]))[0];
	if (!row) return defaultCommanderProfile();
	return commanderProfileSchema.parse({
		commanderName: row.commander_name || "JAMESON",
		allegiance: row.allegiance || "independent",
		faction: row.faction || "free-traders",
		standing: Number(row.standing ?? 0),
		experience: Number(row.experience ?? 0),
		credits: Number(row.credits ?? 1500),
		shipId: row.ship_id || "sidewinder",
		inventory: parseJsonField(row.inventory),
		reputation: parseJsonField(row.reputation),
		notes: row.notes ?? "",
		saveState: parseJsonField(row.save_state)
	});
});
var saveCommanderProfile_createServerFn_handler = createServerRpc({
	id: "d1e14feff2ba7a3f7ad1ce277b3acfb9b00a201206c37d5a018fdd3dadf144e8",
	name: "saveCommanderProfile",
	filename: "src/game/commander-profile.ts"
}, (opts) => saveCommanderProfile.__executeServer(opts));
var saveCommanderProfile = createServerFn({ method: "POST" }).middleware([authMiddleware]).validator(commanderProfileSchema).handler(saveCommanderProfile_createServerFn_handler, async ({ data, context }) => {
	const sql = await getSql();
	const cleaned = commanderProfileSchema.parse({
		...defaultCommanderProfile(),
		...data,
		commanderName: (data.commanderName ?? "JAMESON").trim().slice(0, 16).toUpperCase() || "JAMESON",
		allegiance: data.allegiance ?? "independent",
		faction: (data.faction ?? "free-traders").trim().slice(0, 32) || "free-traders",
		inventory: data.inventory ?? {},
		reputation: data.reputation ?? {},
		notes: data.notes ?? "",
		saveState: data.saveState ?? {}
	});
	await sql.query(`
        insert into commander_profiles (
          user_id,
          commander_name,
          allegiance,
          faction,
          standing,
          experience,
          credits,
          ship_id,
          inventory,
          reputation,
          notes,
          save_state,
          updated_at
        ) values (
          $1, $2, $3, $4, $5, $6, $7, $8, $9::jsonb, $10::jsonb, $11, $12::jsonb, now()
        )
        on conflict (user_id) do update set
          commander_name = excluded.commander_name,
          allegiance = excluded.allegiance,
          faction = excluded.faction,
          standing = excluded.standing,
          experience = excluded.experience,
          credits = excluded.credits,
          ship_id = excluded.ship_id,
          inventory = excluded.inventory,
          reputation = excluded.reputation,
          notes = excluded.notes,
          save_state = excluded.save_state,
          updated_at = now()
      `, [
		context.userId,
		cleaned.commanderName,
		cleaned.allegiance,
		cleaned.faction,
		cleaned.standing,
		cleaned.experience,
		cleaned.credits,
		cleaned.shipId,
		JSON.stringify(cleaned.inventory),
		JSON.stringify(cleaned.reputation),
		cleaned.notes,
		JSON.stringify(cleaned.saveState)
	]);
	return cleaned;
});
//#endregion
export { loadCommanderProfile_createServerFn_handler, saveCommanderProfile_createServerFn_handler };
