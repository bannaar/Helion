import { spawn } from "node:child_process";

const mode = process.argv[2] ?? "dev";
const commands = {
  dev: ["npm", ["run", "dev"]],
  preview: ["npm", ["run", "preview"]],
};

const command = commands[mode];
if (!command) {
  console.error(`Unknown client mode "${mode}". Use "dev" or "preview".`);
  process.exit(1);
}

console.log(`[helion-client] starting ${mode} client at http://127.0.0.1:${mode === "dev" ? "8080" : "8081"}`);

const child = spawn(command[0], command[1], {
  stdio: "inherit",
  env: { ...process.env, BROWSER: "none" },
});

const shutdown = (signal) => {
  if (!child.killed) child.kill(signal);
};

process.on("SIGINT", () => shutdown("SIGINT"));
process.on("SIGTERM", () => shutdown("SIGTERM"));

child.on("exit", (code, signal) => {
  if (signal) {
    process.kill(process.pid, signal);
    return;
  }
  process.exit(code ?? 1);
});
