import { createServerFn } from "@tanstack/react-start";
import { z } from "zod";
import { COMMODITY_IDS } from "./galaxy";
import type { CommodityId } from "./types";

const commodity = z.string().refine(
  (v): v is CommodityId => (COMMODITY_IDS as readonly string[]).includes(v),
  { message: "Unknown cargo" },
);

export const getMarketFn = createServerFn({ method: "GET" })
  .validator(z.object({ systemId: z.string().min(1).max(40) }))
  .handler(async ({ data }) => {
    const { readMarket } = await import("./universe.server");
    return readMarket(data.systemId);
  });

export const getBoardFn = createServerFn({ method: "GET" })
  .validator(z.object({ systemId: z.string().min(1).max(40) }))
  .handler(async ({ data }) => {
    const { readBoard } = await import("./universe.server");
    return readBoard(data.systemId);
  });

export const executeTradeFn = createServerFn({ method: "POST" })
  .validator(
    z.object({
      systemId: z.string().min(1).max(40),
      commodity,
      side: z.enum(["buy", "sell"]),
      qty: z.number().int().min(1).max(20),
    }),
  )
  .handler(async ({ data }) => {
    const { applyTrade } = await import("./universe.server");
    return applyTrade({
      systemId: data.systemId,
      commodity: data.commodity as (typeof COMMODITY_IDS)[number],
      side: data.side,
      qty: data.qty,
    });
  });

export const logTrafficFn = createServerFn({ method: "POST" })
  .validator(
    z.object({
      systemId: z.string().min(1).max(40),
      kind: z.string().min(1).max(20),
      detail: z.string().min(1).max(160),
    }),
  )
  .handler(async ({ data }) => {
    const { insertTraffic } = await import("./universe.server");
    await insertTraffic(data.systemId, data.kind, data.detail);
    return { ok: true as const };
  });
