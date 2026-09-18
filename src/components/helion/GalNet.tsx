import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { readGalNetFn, sendGalNetFn } from "@/game/galnet.functions";

const CHAT_KEY = "helion.galnet.chat.v1";
const DEFAULT_BULLETINS = [
  "GALNET // HELION: Independent pilots report increased pirate activity along the Zaon trade lane.",
  "GALNET // MARKETS: Industrial demand for palladium and superconductors is climbing in the outer systems.",
  "GALNET // SECURITY: Federation patrols are recruiting escorts for a deep-space survey beyond the human bubble.",
];

type ChatMessage = { id: string; channel: string; commander: string; text: string; at: number };
type Channel = "LOCAL" | "FACTION" | "GALACTIC";

function loadChat(): ChatMessage[] {
  try {
    const raw = localStorage.getItem(CHAT_KEY);
    return raw ? (JSON.parse(raw) as ChatMessage[]).slice(-40) : [];
  } catch {
    return [];
  }
}

export function GalNet({ news, commander }: { news: string[]; commander: string }) {
  const [open, setOpen] = useState(false);
  const [channel, setChannel] = useState<Channel>("LOCAL");
  const [text, setText] = useState("");
  const [chat, setChat] = useState<ChatMessage[]>([]);

  useEffect(() => {
    setChat(loadChat());
    void readGalNetFn({ data: { channel } }).then((rows) => setChat(rows as ChatMessage[])).catch(() => {
      // Unauthenticated/local deployments continue with the client relay.
    });
  }, [channel]);

  function send() {
    const message = text.trim();
    if (!message) return;
    const next = [
      ...chat,
      { id: crypto.randomUUID(), channel, commander: commander || "JAMESON", text: message, at: Date.now() },
    ].slice(-40);
    setChat(next);
    setText("");
    try {
      localStorage.setItem(CHAT_KEY, JSON.stringify(next));
    } catch {
      // Chat remains available in memory if browser storage is unavailable.
    }
    void sendGalNetFn({ data: { channel, commander: commander || "JAMESON", text: message } }).catch(() => {
      // The local relay remains usable when the account server is unavailable.
    });
  }

  return (
    <>
      <button
        type="button"
        aria-label={open ? "Close GalNet" : "Open GalNet"}
        className="absolute bottom-4 left-1/2 z-40 -translate-x-1/2 rounded-sm border border-border bg-surface/90 px-3 py-2 font-mono text-[10px] tracking-widest text-muted hover:border-accent/60 hover:text-accent sm:bottom-6"
        onClick={() => setOpen((value) => !value)}
      >
        GALNET
      </button>
      {open ? (
        <div className="absolute inset-0 z-50 flex items-start justify-center bg-bg/80 p-4 pt-[max(4rem,env(safe-area-inset-top))] backdrop-blur-sm">
          <section role="dialog" aria-modal="true" aria-labelledby="galnet-title" className="flex max-h-[min(42rem,90dvh)] w-full max-w-3xl flex-col rounded-xl border border-border bg-surface p-5 shadow-2xl">
            <div className="flex items-start justify-between">
              <div>
                <p className="font-mono text-[10px] tracking-[0.3em] text-accent">INTERSPACE COMMUNICATIONS NETWORK</p>
                <h2 id="galnet-title" className="mt-1 font-display text-3xl tracking-[0.08em]">GalNet</h2>
              </div>
              <button type="button" className="font-mono text-xs tracking-widest text-muted hover:text-fg" onClick={() => setOpen(false)}>
                ESC
              </button>
            </div>
            <div className="mt-5 grid min-h-0 gap-4 md:grid-cols-[0.9fr_1.1fr]">
              <section className="min-h-0 rounded-md border border-border bg-surface-2 p-3">
                <p className="font-mono text-[10px] tracking-[0.2em] text-accent">NEWS BULLETINS</p>
                <div className="mt-3 max-h-64 space-y-3 overflow-auto">
                  {(news.length ? news : DEFAULT_BULLETINS).map((line, index) => (
                    <p key={`${line}-${index}`} className="border-b border-border pb-2 text-sm leading-relaxed text-muted">
                      {line}
                    </p>
                  ))}
                </div>
              </section>
              <section className="flex min-h-0 flex-col rounded-md border border-accent/30 bg-surface-2 p-3">
                <div className="flex items-center justify-between gap-2">
                  <p className="font-mono text-[10px] tracking-[0.2em] text-accent">COMMANDER COMMS</p>
                  <select value={channel} onChange={(event) => setChannel(event.target.value as Channel)} className="h-8 rounded border border-border bg-surface px-2 font-mono text-[10px] text-fg">
                    <option>LOCAL</option>
                    <option>FACTION</option>
                    <option>GALACTIC</option>
                  </select>
                </div>
                <div className="mt-3 min-h-32 flex-1 space-y-2 overflow-auto rounded border border-border bg-bg/60 p-2">
                  {chat.filter((message) => message.channel === channel).map((message) => (
                    <p key={message.id} className="text-xs leading-relaxed">
                      <span className="font-mono text-accent">{message.commander}:</span> <span className="text-muted">{message.text}</span>
                    </p>
                  ))}
                  {!chat.some((message) => message.channel === channel) ? <p className="text-xs text-muted">Channel quiet. Your transmission will be stored locally for now.</p> : null}
                </div>
                <div className="mt-2 flex gap-2">
                  <input value={text} onChange={(event) => setText(event.target.value)} onKeyDown={(event) => { if (event.key === "Enter") send(); }} maxLength={240} placeholder={`Transmit on ${channel.toLowerCase()} channel...`} className="h-10 min-w-0 flex-1 rounded border border-border bg-surface px-3 text-sm text-fg outline-none focus:ring-2 focus:ring-accent/60" />
                  <Button size="sm" onClick={send}>Send</Button>
                </div>
              </section>
            </div>
            <p className="mt-4 font-mono text-[10px] leading-relaxed tracking-wider text-muted">
              GalNet bulletin sync is active. Multiplayer relay will use the dedicated server channel when deployed; local transmissions are retained safely in this client until then.
            </p>
          </section>
        </div>
      ) : null}
    </>
  );
}
