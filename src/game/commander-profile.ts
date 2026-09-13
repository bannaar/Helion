import { createServerFn } from "@tanstack/react-start";
import { z } from "zod";
import { authMiddleware } from "@/lib/auth/middleware";
import { getSql } from "@/lib/db";

export const allegianceValues = [
  "independent",
  "empire",
  "federation",
  "pirate",
  "union",
] as const;

export const commanderProfileSchema = z.object({
  commanderName: z.string().trim().min(2).max(16).default("JAMESON"),
  allegiance: z.enum(allegianceValues).default("independent"),
  faction: z.string().trim().min(2).max(32).default("free-traders"),
  standing: z.number().int().min(-1000).max(10000).default(0),
  experience: z.number().int().min(0).max(1000000).default(0),
  credits: z.number().int().min(0).default(1500),
  shipId: z.string().trim().min(2).max(32).default("sidewinder"),
  inventory: z.record(z.string(), z.number().int().nonnegative()).default({}),
  reputation: z.record(z.string(), z.number().int()).default({}),
  notes: z.string().max(250).default(""),
  saveState: z.record(z.string(), z.any()).default({}),
});

export type CommanderProfile = z.infer<typeof commanderProfileSchema>;

type CommanderProfileRow = {
  user_id: string;
  commander_name: string;
  allegiance: string;
  faction: string;
  standing: number | string;
  experience: number | string;
  credits: number | string;
  ship_id: string;
  inventory: Record<string, number> | string | null;
  reputation: Record<string, number> | string | null;
  notes: string | null;
  save_state: Record<string, unknown> | string | null;
};

export function defaultCommanderProfile(name = "JAMESON"): CommanderProfile {
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
    saveState: {},
  };
}

function parseJsonField<T>(value: T | string | null): T {
  if (!value) return {} as T;
  if (typeof value === "string") {
    try {
      return JSON.parse(value) as T;
    } catch {
      return {} as T;
    }
  }
  return value as T;
}

export const loadCommanderProfile = createServerFn({ method: "GET" })
  .middleware([authMiddleware])
  .handler(async ({ context }) => {
    const sql = await getSql();
    const rows = await sql.query<CommanderProfileRow>(
      `
        select *
        from commander_profiles
        where user_id = $1
        limit 1
      `,
      [context.userId],
    );

    const row = rows[0];
    if (!row) return defaultCommanderProfile();

    return commanderProfileSchema.parse({
      commanderName: row.commander_name || "JAMESON",
      allegiance: row.allegiance || "independent",
      faction: row.faction || "free-traders",
      standing: Number(row.standing ?? 0),
      experience: Number(row.experience ?? 0),
      credits: Number(row.credits ?? 1500),
      shipId: row.ship_id || "sidewinder",
      inventory: parseJsonField<Record<string, number>>(row.inventory),
      reputation: parseJsonField<Record<string, number>>(row.reputation),
      notes: row.notes ?? "",
      saveState: parseJsonField<Record<string, unknown>>(row.save_state),
    });
  });

export const saveCommanderProfile = createServerFn({ method: "POST" })
  .middleware([authMiddleware])
  .validator(commanderProfileSchema)
  .handler(async ({ data, context }) => {
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
      saveState: data.saveState ?? {},
    });

    await sql.query(
      `
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
      `,
      [
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
        JSON.stringify(cleaned.saveState),
      ],
    );

    return cleaned;
  });
