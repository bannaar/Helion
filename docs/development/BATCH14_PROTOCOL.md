# Batch 14 M4 native transport contract

TLS 1.2+ remains mandatory. Binary framing runs above the authenticated TLS byte
stream; it neither authenticates players nor changes gameplay authority. Both
transports dispatch to the existing authoritative server handlers. Persistence
formats and the approved M3 galaxy are unchanged.

Current currency prose uses GSC. Literal `credits` fields below are unchanged
wire identifiers, not a competing currency. Canon authority is
[DraftComp8](../lore/HELION_World_Bible_draftcomp8.txt).

## Negotiation and compatibility

Every connection starts with bounded legacy text, `WELCOME Helion/2` and an
information line advertising `binary-version=1`. Before authentication, send
`PROTOCOL BINARY 1\n` alone and wait for `OK PROTOCOL BINARY 1\n`. Bytes after
that acknowledgement use binary framing. Do not pipeline gameplay with the
upgrade: additional complete or partial text in the same decoded batch causes
negotiation failure and connection closure. There is no magic-byte autodetection.

An unsupported requested version returns `ERR CLIENT_UPDATE_REQUIRED
binary-version=1` without switching transport. Authentication closes the
negotiation window; an established binary session cannot renegotiate/downgrade.
The graphical client requests binary by default and fails closed on negotiation
errors, incompatible welcomes/acknowledgements, or unsolicited acknowledgements.
It preserves binary bytes coalesced with the acknowledgement in one TLS read.

`helion_client --legacy-protocol` deliberately selects bounded text for a legacy
server. There is no automatic downgrade after failure. `--terminal` remains the
explicit text compatibility/diagnostic interface and cannot manually switch its
decoder to binary. The same TLS, account validation, environment isolation and
gameplay authorization apply to text and binary requests.

## Binary version 1

All multibyte integers are unsigned, fixed-width, big-endian. The header is
exactly 14 bytes; payloads are at most 4096 bytes (4110 bytes per frame).

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 2 | ASCII `HB` |
| 2 | 1 | Version, exactly 1 |
| 3 | 1 | Message type |
| 4 | 2 | Reserved flags, exactly zero |
| 6 | 4 | Payload length |
| 10 | 4 | Request/correlation ID |

| Type | Direction | Payload |
| --- | --- | --- |
| 1 | Client → server | Existing bounded text command, no newline |
| 2 | Server → client | Existing bounded text response, no newline |
| 3 | Client → server | Three bytes: thrust (0/1), turn (0/1/255 for 0/+1/−1), brake (0/1) |
| 4 | Server → client | 57-byte authoritative flight snapshot |

Snapshot offsets: nine IEEE-754 binary32 values at 0–35, in order
`x,y,vx,vy,yaw,cooldown,fuel,maxFuel,weaponCooldown`; `docked:u8` at 36;
`cargo:u16` at 37; `credits:u32` at 39; `experience:u32` at 43;
`food:u16` at 47; `parts:u16` at 49; `station:u8` at 51;
`hull:u16` at 52; `maxHull:u16` at 54; `destroyed:u8` at 56.
Booleans must be 0/1. Floats must be finite and pass the existing supported
world/ship ranges; `credits`/`experience` must fit the authoritative signed-int
range. Invalid snapshots leave destination state unchanged. Encoding rounds
double values toward zero when necessary to avoid moving legal boundary values
outside their validation range. No native struct layout is sent on the wire.

Clients send strictly increasing, nonzero IDs for the lifetime of a connection.
Gaps are allowed; duplicate, decreasing, zero or wrapped IDs terminate it.
All replies to a request share its ID; several reply frames with the same ID
are expected, not duplicates. Broadcasts use ID zero on **every** recipient.
The client rejects future or decreasing nonzero response IDs. IDs restart on
a fresh connection: this is connection-scoped replay rejection, **not** durable
cross-reconnection transaction deduplication or an exactly-once guarantee.

Malformed headers, oversized lengths, unsupported flags/types/versions, invalid
directions and malformed typed/text payloads terminate binary sessions. Valid
requests before a bad frame are committed/replied to first; the bad frame and
all following frames cannot mutate gameplay. Ordinary command-grammar errors
remain recoverable replies. Incomplete data is buffered up to one bounded frame;
EOF never dispatches an incomplete command. The server retains its complete-line
deadline and rate limits; the client deadline advances on complete responses,
not arbitrary partial bytes. A failed decoder cannot resume parsing trailing data.

## Measurement and scope

`helion_protocol_benchmark` preserves the original 100,000-iteration benchmark
and now validates semantics, exact typed sizes, representative aggregate byte
savings and drained decoder buffers. Timing is reported, not a machine-specific
pass/fail threshold. Flight codec timing compares equal fields, including combat
cooldown; aggregate interval accounting uses the actual routine legacy responses
and includes the still-textual ACTIVE SHIP envelope in both transports. TLS/IP
overhead and other gameplay events are excluded; this is not a network capture.
Allocation counts are explicitly uninstrumented, not claimed to be zero.

INPUT is larger in binary (17 vs 13 bytes); the 71-byte flight snapshot is the
measured benefit. Low-frequency commands, contacts, markets, ship/outfit metadata
and diagnostics retain their bounded text payloads inside binary envelopes.
Additional typed schemas need volume measurements and a new explicit version if
their existing wire meaning changes. Per-environment approved build policies,
durable idempotency keys, compression/deltas and client engine restructuring are
not implemented by M4. These are the historical M4 scope limits; M5 and M6 are
now complete, with current status in [BATCH14_EXECUTION.md](BATCH14_EXECUTION.md).

## M6 TEST administration extension

`TESTADMIN TEST id verb target field value` uses the existing bounded text request
parser over explicit legacy TLS or the negotiated binary v1 text-request envelope.
It adds no frame type, transport, fallback or client-authoritative metadata.
The server loads environment and roles from trusted startup/access policy and
audits/denies unauthorized attempts, including companion-authenticated requests.
Administrative mutation IDs persist beyond connection-local binary request IDs;
retry uses the same admin ID. See [private TEST administration](../private-test-admin.md).
Existing typed input, replay/order checks, framing bounds and negotiation remain
unchanged. M5 neutral INPUT/liveness behavior is unchanged.

## M7 service drain

Service lifecycle is server-owned operational state. No operations/admin network
protocol is added. Once draining, an existing connection's next complete request
receives `ERR service-draining` through its existing bounded transport, correlated
to that request in binary mode, then closes. New sockets are refused before TLS
without plaintext output. A transaction already executing may finish under the
existing authoritative lock. The bounded drain deadline then interrupts remaining
connections, including partial handshakes. Local readiness/maintenance hooks and
Production backup verification are documented in
[Production operations](../production-operations.md). M4 framing, typed input,
normal replay/order rules and explicit legacy compatibility remain unchanged.
