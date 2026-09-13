import { Ft as number, It as object, Ot as _enum, zt as string } from "../_libs/@better-auth/core+[...].mjs";
import { r as createServerFn } from "./ssr.mjs";
import { t as createServerRpc } from "./createServerRpc-CcvdN_gc.mjs";
import { t as COMMODITY_IDS } from "./galaxy-CtkES_h3.mjs";
//#region node_modules/.nitro/vite/services/ssr/assets/universe.functions-DyTypaGR.js
var commodity = string().refine((v) => COMMODITY_IDS.includes(v), { message: "Unknown cargo" });
var getMarketFn_createServerFn_handler = createServerRpc({
	id: "288f4ca1033eaca325be254228a6d6591c1bcfda0d3429e068f1e2631ef6ddac",
	name: "getMarketFn",
	filename: "src/game/universe.functions.ts"
}, (opts) => getMarketFn.__executeServer(opts));
var getMarketFn = createServerFn({ method: "GET" }).validator(object({ systemId: string().min(1).max(40) })).handler(getMarketFn_createServerFn_handler, async ({ data }) => {
	const { readMarket } = await import("./universe.server-qp3OmZwa.mjs");
	return readMarket(data.systemId);
});
var getBoardFn_createServerFn_handler = createServerRpc({
	id: "4d3f8d23e6aba35868770752f96cfb6f5bd489ad54e370832a44bedbfebbba4a",
	name: "getBoardFn",
	filename: "src/game/universe.functions.ts"
}, (opts) => getBoardFn.__executeServer(opts));
var getBoardFn = createServerFn({ method: "GET" }).validator(object({ systemId: string().min(1).max(40) })).handler(getBoardFn_createServerFn_handler, async ({ data }) => {
	const { readBoard } = await import("./universe.server-qp3OmZwa.mjs");
	return readBoard(data.systemId);
});
var executeTradeFn_createServerFn_handler = createServerRpc({
	id: "af6f6a26d0a73f6c38a47b4d752eb4bae4abc8ae9ee97e60450f821a6ca17f4f",
	name: "executeTradeFn",
	filename: "src/game/universe.functions.ts"
}, (opts) => executeTradeFn.__executeServer(opts));
var executeTradeFn = createServerFn({ method: "POST" }).validator(object({
	systemId: string().min(1).max(40),
	commodity,
	side: _enum(["buy", "sell"]),
	qty: number().int().min(1).max(20)
})).handler(executeTradeFn_createServerFn_handler, async ({ data }) => {
	const { applyTrade } = await import("./universe.server-qp3OmZwa.mjs");
	return applyTrade({
		systemId: data.systemId,
		commodity: data.commodity,
		side: data.side,
		qty: data.qty
	});
});
var logTrafficFn_createServerFn_handler = createServerRpc({
	id: "ccde32018b33552d98b0d8485000e88c094562617493fb1c052c01eff27aa3a2",
	name: "logTrafficFn",
	filename: "src/game/universe.functions.ts"
}, (opts) => logTrafficFn.__executeServer(opts));
var logTrafficFn = createServerFn({ method: "POST" }).validator(object({
	systemId: string().min(1).max(40),
	kind: string().min(1).max(20),
	detail: string().min(1).max(160)
})).handler(logTrafficFn_createServerFn_handler, async ({ data }) => {
	const { insertTraffic } = await import("./universe.server-qp3OmZwa.mjs");
	await insertTraffic(data.systemId, data.kind, data.detail);
	return { ok: true };
});
//#endregion
export { executeTradeFn_createServerFn_handler, getBoardFn_createServerFn_handler, getMarketFn_createServerFn_handler, logTrafficFn_createServerFn_handler };
