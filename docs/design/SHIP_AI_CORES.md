# Ship AI Cores — future design direction

Status: approved high-level direction, documented 2026-10-22. **Not implemented.**
This document introduces no code, protocol, equipment, automation, personality,
memory, LLM or voice system. Batch 15 is not started; Batch 14 must finish and
the user must explicitly authorize future implementation. Setting authority is
[DraftComp8](../lore/HELION_World_Bible_draftcomp8.txt); exact engineering and
balance require later design and acceptance.

An AI Core is physical ship equipment with persistent identity. Categories may
include civilian assistance, industry, exploration, tactical, military/restricted,
experimental, xenotech/Rift-derived and unique advanced intelligences. Possible
personality, memory, trust, pilot familiarity, experience and proficiency belong
to that persistent equipment identity. These possibilities do not lock schemas,
numeric limits, engineering classes or exact Sovereign AI counts.

The native authoritative server remains the only owner of gameplay outcomes.
Deterministic native C++ automation must work without LLMs. Automation produces
typed intents that existing authoritative gameplay systems validate against ship
condition, equipment, pilot permissions, environment, range, cooldowns, resources
and other applicable rules. A suggestion or accepted transport message does not
grant success. No Core or cognitive provider becomes an independent simulation.

Core capability and pilot authorization are separate: possessing an advanced
Core must not grant a permit, Production administrator role, TEST privilege or
access to another commander's information. Damage/degradation may reduce
capability, availability or confidence. It must never weaken network integrity,
TLS verification, framing bounds, replay protection or authorization. Manual
gameplay remains available when a Core or provider fails.

Optional asynchronous cognitive providers may assist within bounded permitted
information and request policies. Mandatory no-provider / Null-provider behavior
keeps deterministic gameplay functional with no internet, cloud AI or local LLM.
“Null-provider” means no cognitive provider; it has no connection to The Null
or Zero Space. Provider latency/failure must not block the authoritative loop.
No external AI vendor or API is architectural authority. Exact provider APIs,
deadlines, queues, memory retention and consent rules remain future design work.

Optional companion interfaces may display permitted Core state or request allowed
actions. No companion hardware, microphone, voice recognition or TTS is required.
Permission-filtered read models must omit hidden server information before any
provider/device boundary; hiding received data in a UI is insufficient. Do not
send account secrets, bearer tokens, hidden world truth or unauthorized personal
history to a provider. Detailed data/minimization policies need explicit future
review; this document does not approve external data transmission.

Future integration should extend the existing HELION TLS/negotiated binary
protocol and server dispatch. Preserve explicit legacy compatibility as required;
do not create a parallel AI network/backend. Local native server/client play
remains possible without internet; remote Production access still needs networking.

Exact mass, power, heat, processing rates, autonomy percentages, damage formulas,
memory schemas, vendor dependencies and exotic powers are deliberately unresolved.
Existing limited ship/module/combat systems are validated foundations, not evidence
that AI Cores or the future outfitting expansion exist today.
