# Helion world-bible integration

The canonical setting and gameplay source is
[`world-bible-v0.2.txt`](world-bible-v0.2.txt). Version 0.2 is an approved
design source, not a claim that every described system is already implemented.

## Canonical rule

Lore should generate gameplay. New narrative material should identify the
player action, simulation state, reward or consequence, and the runtime that
owns that state. Browser and native implementations may arrive at different
times, but they should use the same names and rules.

## Canonical vocabulary

| Domain | Canonical entities |
| --- | --- |
| Human powers | Helion Commonwealth, Free Systems Compact, Solar Directorate, Meridian League, Outer Reach |
| Corporations | Orion Extraction Group, Aster Dynamics, Titan Forge, NovaGen, Horizon Systems, Helix Interstellar |
| Criminal groups | Vanta Syndicate, Red Wake |
| Security | System Security, faction law enforcement, INSA, Gatewatch |
| Military | Commonwealth Navy, Directorate Fleet, Compact Mutual Defense Fleet, Meridian Security Fleet |
| Alien civilizations | Khepri, Vael |
| Alien-space systems | Helion Gates, Rifts, Unknown Space, the Veil |

Use these names in UI copy, protocol-visible labels, test fixtures, mission
data, GalNet stories, reputation keys, and save migrations. Avoid parallel
placeholder factions once a canonical equivalent exists.

## Existing implementation alignment

| Existing Helion feature | World-bible role | Integration status |
| --- | --- | --- |
| Kepler starting sector | Quiet frontier system where the opening career begins | Implemented foundation |
| First Ore mission and mining loop | Orion Extraction Group introduction | Native contract issued by Orion under Kepler jurisdiction |
| Station markets and hauling | Meridian-style trade and dynamic economy foundation | Implemented foundation |
| Raiders and combat rewards | Vanta/Red Wake criminal pressure and bounty progression | Native Red Wake encounter, rewards, and standing changes |
| Police and hostile contacts | System security, wanted state, interdiction, and bounty response | Browser foundation; native expansion pending |
| GalNet news and chat | Simulation events, opportunities, political change, and alien escalation | Native deterministic transition events |
| Reputation | Standing with powers, local authorities, corporations, and criminal groups | Native bounded multi-organization foundation |
| Alien contacts | Khepri disposition and Vael archaeological progression | Browser foundation; staged native work |

## Implementation order

### Phase 1 — Kepler career identity

- Present First Ore as an Orion Extraction Group contract.
- Give Kepler Authority a local reputation track distinct from major powers.
- Attach canonical issuer, jurisdiction, legality, and reputation effects to
  every mission.
- Use GalNet to report mining output, pirate activity, market shortages, and
  local security changes produced by real simulation state.

### Phase 2 — Security, crime, and corporations

- Add legal, restricted, and prohibited cargo categories by jurisdiction.
- Add fines, bounties, scans, interdictions, and escalating wanted responses.
- Introduce Vanta Syndicate and Red Wake missions as alternative career paths.
- Associate equipment families and progression unlocks with corporations.

### Phase 3 — Regional economy and conflict

- Drive prices from war, famine, construction, piracy, and supply events.
- Add faction and corporation contracts with consequences for competing
  standings.
- Expand military careers, permits, restricted systems, and naval equipment.

### Phase 4 — Rifts and first contact

- Introduce unstable rifts as exploration hazards before routine alien combat.
- Implement Khepri disposition as observable behavior, not a simple red/blue
  faction flag.
- Make research, restraint, salvage, diplomacy, and aggression viable player
  choices with persistent consequences.

### Phase 5 — Unknown Space and the Vael

- Add expeditions, archaeological sites, translation, Veil interactions, and
  controlled recovery of Vael technology.
- Gate late-game alien technology behind knowledge, materials, reputation,
  risk, and political consequences.

## Data-contract guidance

Future shared data should use stable lowercase identifiers separate from
display text. Suggested namespaces:

```text
faction.commonwealth
faction.compact
faction.directorate
faction.meridian
authority.kepler
corp.orion
corp.aster
corp.titan
corp.novagen
corp.horizon
corp.helix
criminal.vanta
criminal.red_wake
alien.khepri
alien.vael
```

Reputation should support multiple simultaneous layers: major power, local
authority, corporation, criminal organization, military branch, and alien
disposition. Save formats and network protocols should transmit identifiers
and numeric state; clients own presentation text.

## Contribution checklist

When implementing an item from the Bible:

1. Cite the relevant Bible section in the issue or pull request.
2. Declare the authoritative runtime and persistence owner.
3. Define player action, validation, state transition, reward, and failure.
4. Add deterministic tests for rules that affect economy, combat, reputation,
   crime, or progression.
5. Update project-state documentation without marking later Bible phases as
   already implemented.
