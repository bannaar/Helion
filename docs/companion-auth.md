# HELION companion credential foundation

Status: native-server authentication foundation for companion clients such as HELION Commander.

Companion hardware is optional. HELION Commander, HELION Terminal, HELION Flight
Deck and future mobile/cockpit/cyberdeck interfaces are product direction, not
implemented clients or independent game backends. They must consume permission-
filtered authoritative data; a UI's ability to conceal information never justifies
receiving hidden truth. No companion token grants Production administrator
authority. See [DraftComp8](lore/HELION_World_Bible_draftcomp8.txt) and the
[future AI Core design](design/SHIP_AI_CORES.md) for direction, not additional scope.

## Purpose

The native game login uses a username and password on each TLS connection. Companion clients must not persist the player's game password merely to reconnect.

The server therefore supports a separate bearer credential with these properties:

- issued only from a normal password-authenticated player session;
- random 256-bit bearer secret;
- only a SHA-256 verifier is persisted;
- plaintext token is returned once at issuance;
- expires after 30 days;
- revocable by its public token ID;
- maximum eight active tokens per account;
- fixed initial scope: `profile.read`;
- a companion-authenticated connection cannot create accounts, perform password login, issue/revoke tokens, chat, trade, fly, mine, repair, upgrade, or run other gameplay mutations.

The token belongs to the server state that issued it. HELION environments must continue using separate state/config/credentials and must not copy active companion credentials between PRIVATE TEST and PRODUCTION.

## Transport and authorization

Companion commands use the same parser and authoritative dispatcher as game
commands. They work as bounded legacy text or M4 binary `textRequest` payloads
(with correlated `textResponse` replies). Negotiate `PROTOCOL BINARY 1` before
any authentication when using binary transport; authentication closes negotiation.
No companion-specific transport, automatic downgrade, or gameplay authority is
introduced. Binary request IDs, framing limits and ordered terminal errors apply
unchanged, including replay rejection for token issuance and revocation.

Only `PROFILE`, the public diagnostic `STATE`, and `QUIT` are available to a
companion session. Typed binary input is subject to the same scope check as text
input. Companion sessions do not subscribe to player chat broadcasts, including
when a player connection switches to companion authentication. Every profile read
rechecks the live token under the server state lock: durable revocation or expiry
also removes read access from existing sessions. A response authorized before a
concurrent revocation may already be in flight.

## Manual development pairing

Log in normally through the native client:

```text
LOGIN pilot player-password
```

Issue a token:

```text
COMPANION ISSUE
```

The server returns:

```text
OK COMPANION ISSUED id=<public-id> token=<bearer-token> expires=<unix-seconds> scope=profile.read
```

The bearer token is a secret. Copy it only into the intended companion client. It cannot be recovered from the server database later.

List active token metadata:

```text
COMPANION LIST
COMPANION TOKEN id=<public-id> expires=<unix-seconds> scope=profile.read
COMPANION END
```

Revoke from a normal password-authenticated player session:

```text
COMPANION REVOKE <public-id>
OK COMPANION REVOKED id=<public-id>
```

Authenticate a new TLS connection without the game password:

```text
COMPANION AUTH <bearer-token>
OK COMPANION AUTH user=<username> display=<display-name> scope=profile.read
```

That connection may request:

```text
PROFILE
STATE
QUIT
```

`STATE` remains a public/pre-auth diagnostic in the current protocol. The companion token itself grants only profile-read authority.

Other commands return:

```text
ERR scope-denied
```

## Persistence

Companion records use `T` records in the native persistence file:

```text
T <id> <user> $sha256$<verifier> <expires> profile.read
```

Fields are tab-delimited on disk. The plaintext bearer token is never written to persistence.
M3 economic telemetry retains its existing two-column `T <telemetry>` shape;
companion rows have exactly six columns. The loader validates both shapes
separately, preserving old main companion records and Batch 14 telemetry,
owned ships, galaxy knowledge, environment tags and snapshot terminators.

Expired token records are rejected and are omitted from subsequent snapshots.

## Security boundary

This is a development pairing foundation, not the final launcher/QR pairing user experience.

Do not:

- store a game password in HELION Commander for automatic reconnect;
- log or persist plaintext bearer tokens on the server;
- grant gameplay mutation authority to `profile.read`;
- disable TLS certificate verification;
- copy active token records between server environments.

A later pairing UX may wrap issuance in the launcher or a one-time QR/device-code flow without changing the server-side principle: password authentication authorizes issuance, while the companion stores only a scoped revocable bearer credential.
