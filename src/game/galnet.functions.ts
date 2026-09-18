import { createServerFn } from "@tanstack/react-start";
import { z } from "zod";
import { authMiddleware } from "@/lib/auth/middleware";
import { getSql } from "@/lib/db";

const channel = z.enum(["LOCAL", "FACTION", "GALACTIC"]);
const message = z.object({ channel, text: z.string().trim().min(1).max(240), commander: z.string().trim().min(2).max(16) });

export const readGalNetFn = createServerFn({ method: "GET" })
  .middleware([authMiddleware])
  .validator(z.object({ channel }))
  .handler(async ({ data }) => {
    const sql = await getSql();
    return sql<{ id: string; channel: string; commander: string; text: string; at: number }>`
      select id::text, channel, commander, text, extract(epoch from created_at) * 1000 as at
      from galnet_messages where channel = ${data.channel}
      order by created_at desc limit 40
    `;
  });

export const sendGalNetFn = createServerFn({ method: "POST" })
  .middleware([authMiddleware])
  .validator(message)
  .handler(async ({ data }) => {
    const sql = await getSql();
    await sql`insert into galnet_messages (channel, commander, text) values (${data.channel}, ${data.commander.toUpperCase()}, ${data.text})`;
    return { ok: true as const };
  });
