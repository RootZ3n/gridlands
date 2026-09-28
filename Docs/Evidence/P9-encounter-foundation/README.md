# P9: dual-route encounter foundation (dev proof room)

**Operator-approved 2026-09-27**, with decisions M1 (ADR-0029 hybrid) and M2 (lures obey masking).
This is foundation work, not content.
- Decision records: [ADR-0037](../../ADR/0037-dual-route-encounter-foundation.md), and
  [ADR-0029 as amended](../../ADR/0029-localized-navigation.md#amendment-p9-navigation-exists-where-active-gameplay-requires-it).
- Design intent: [DUNGEONS-AND-LEGENDARIES §1.2–1.7](../../DUNGEONS-AND-LEGENDARIES.md).

## The real-game proof (`Tools/p9-dungeon-proof.sh`, results in [`proof/`](proof/))
The dev proof room (`-GLDungeonProof`) is played in the real game, with real ticking, navigation and
streaming.
- Zenny is driven through his own movement input, so his footsteps are real movement noise.
- Pehlichi is commanded the way the player commands him.
- The creatures run their own behaviour.
- A script cannot dodge like a player. When Zenny is about to die (a respawn would carry him out of the
  room), his health is restored, and each restore is counted as `Dev.ZennyHealed`. The damage he took is
  reported in full.

### Direct route ([`dungeon-direct.json`](proof/dungeon-direct.json))
**The route:** enter, fight the 4 patrols, fight the warden. The warden is **DEFEATED** through the health
system:

| Measure | Value |
|---|---|
| Damage to the warden | 240 (its full health) |
| `Event.Creature.Defeated` | 5 |
| `Event.Encounter.Resolved` | 1 |
| Encounter reward | +5 residue (13 in all, with the patrols' drops) |
| Warden actor after defeat | none (the P8 rule for Defeated) |
| Damage to Zenny | 252 (2 dev heals) |
| Navigation active tiles, peak | 217 |
| Stale tiles | 0 |

**After restart** ([`dungeon-direct-restart.json`](proof/dungeon-direct-restart.json)): still Defeated, no
actor, **zero events** (nothing replays), and the residue unchanged at 13.

### Environmental route ([`dungeon-environmental.json`](proof/dungeon-environmental.json), [log](proof/environmental.log.txt))
**The route:**
1. Enter, wait for the hall patrol to walk away, and go into the north corridor.
2. Wait at the grate for the fan's on-window (mask 0.8 at the nearest guard), then cross the sheet-steel
   grate.
3. Wait for the arena patrol to walk away, and go to the cage zone's south side.
4. The warden comes and **attacks**. Zenny commands Pehlichi to Operate; Pehlichi reaches the control behind
   the east wall.

**After 8.1 s of Pehlichi's work** the cage switches, and the warden, inside the zone at that instant and
susceptible, is **NEUTRALIZED** (`Neutralize.Contained`):

| Measure | Value |
|---|---|
| Damage to the warden (from Zenny, Pehlichi and the cage) | **0**; health 240 of 240 |
| `Event.Creature.Defeated` | **0** (no death event) |
| Kill credit | **none** |
| `Event.Creature.Neutralized` | 1 |
| `Event.Encounter.Resolved` | 1 (neutralized) |
| Encounter reward | +5 residue (the same as a defeat) |
| Warden actor | present and inert, inside the dropped cage |

**It is not a bypass.** The route took 47 s, and Zenny took **432 damage** (5 dev heals):
- the warden hit him throughout Pehlichi's work;
- the patrols spotted him 3 times.

Detection changed the state (spotted, searching, lost) and did not end the run. A player would hide instead
of absorbing the hits.

**An earlier run shows the rule is systemic.** Zenny stood at the zone's north edge, so the warden attacked
from outside it. The cage dropped and **nothing was neutralized**: the outcome followed where the warden
actually was at the decision, not the button.

**After restart** ([`dungeon-environmental-restart.json`](proof/dungeon-environmental-restart.json)): still
Neutralized (Contained), the actor present and inert at full health, the cage dropped (lift 0), **zero
events** (no Operate, switch, neutralization, reward, noise or completion replays), and the residue
unchanged at 5.

### Navigation ([`navscale.log.txt`](proof/navscale.log.txt), [`navscale-inside.log.txt`](proof/navscale-inside.log.txt))
**Zenny 60 m west of the room** (outside its relevance margin). The region is active only through the demand
of active creatures inside it:

| Step | Active tiles | Region | Extra creatures with their own navigation |
|---|---|---|---|
| The room's 5 creatures | **188** | active | — |
| + 1 active creature | **188** | active, covers it | 0 |
| + 16 active creatures | **188** | active, covers all 16 | 0 |
| The same 16 with regions **withheld** (dev contrast) | 223 | none | 16 (and 4 of the room's creatures) |

**Zenny inside the room:** 162 tiles in every step. The region is active and covers all 16; Zenny's own 64 m
circle covers the room too.

**So 1 → 16 creatures in one region adds 0 tiles**, against the unchanged 600 budget.

- The automation-world measurements from the proposal
  ([`automation-nav-scaling-proposal.txt`](automation-nav-scaling-proposal.txt)) are supporting evidence only.
- **Two early runs gave misleading numbers.** Zenny fell (the first teleport arrived before the lots' ground
  existed) or was killed by patrols, respawned at the origin, and took the room out of memory. The runs now
  hold Zenny still until the ground exists, and navscale ignores his damage.

## Real-game performance and memory (unchanged P5/P8 budgets)
`Tools/perf/quiet-run.sh`, all four suites back to back: local, dense, town and towndense.
**All four ended QUIETDONE PASS, and no budget changed.** Results are in [`perf/`](perf/).

**Worst frame / p99 frame / streaming game-thread worst (ms):**

| Mode (budget) | local | dense | town | towndense |
|---|---|---|---|---|
| straight (30 / 7 / 12) | 23.3 / 4.60 / 5.05 | 23.7 / 4.75 / 4.50 | 23.7 / 5.13 / 4.75 | 24.3 / 5.35 / 4.78 |
| sprint (30 / 11 / 12) | 14.9 / 4.87 / 4.36 | 15.1 / 4.97 / 4.68 | 15.1 / 5.40 / 4.96 | 15.1 / 5.55 / 4.21 |
| reversal (40 / 11 / 12) | 30.7 / 4.70 / 4.75 | 35.8 / 4.81 / 6.49 | 36.7 / 5.16 / 7.90 | 36.1 / 5.34 / 10.37 |
| teleport (40 / 8 / 12) | 23.6 / 4.60 / 4.72 | 22.9 / 4.77 / 4.90 | 26.2 / 5.11 / 3.98 | 23.6 / 5.27 / 4.24 |
| resume (25 worst) | 18.7 | 14.3 | 17.3 | 14.1 |

- **Emergency chunks: 0 in all 20 crossing runs.**
- **Memory:**
  - peak 3.78 GB (budget 5.2);
  - round-trip growth 60 / 90 / 57 / 63 MB (budget 400);
  - round-trip peak ≤ 3.67 GB.
- **Terrain navigation full build:** 0.44 s (budget 1.5).
- **Navigation active tiles, peak (budget 600), P8 → P9:**
  - local and dense: 301 → **227**;
  - **town and towndense: 557 → 271–273.** The town's idle creatures no longer pay for navigation, and the
    sweep leaves no stale tiles.
  - resume: 162.

## Tests
- **Full gate** (`Saved/gate.sh`): **143/143 tests, 46 requirements met.** The one warning is the engine's own
  connectivity ping timing out, which is environmental.
- **New tests:**
  - `Gridlands.Core.Combat` +3: patrol loops, the neutralized rules and the navigation rule, masked hearing.
  - `Gridlands.Game.Encounter` 11:
    - the proof room stands, and its models come first;
    - wounded stays wounded;
    - chase and search survive streaming and restart;
    - an unpresented creature hears;
    - masking is general (steps, lures, listener versus source, duty cycle, a switched-off fan);
    - the direct route;
    - the environmental route;
    - the decision moment (outside the box, not susceptible, retroactive);
    - contained after streaming and restart, with nothing replayed;
    - navigation scales with the region;
    - idle creatures add no navigation.
  - `Gridlands.Game.Navigation`: now asserts that no built tile lies outside the active set.
- **Data:** `Tools/selftest.sh` PASS; `Tools/data.sh validate`: 232 entities, with the new rules PRF-1,
  PLC-4, MEC-1, MEC-2 and NAV-1.

## Planted defects
### The P9 suite
`Tools/planted-defects/p9_encounter.py`: **22/22 caught on the first run.** Each defect is built and tested on
its own; there were no dead runs and no build failures. See [`planted-defects/summary.txt`](planted-defects/summary.txt).

**The operator's list:**
- N1: containment while the target is outside the volume;
- N2: retroactive neutralization after the decision;
- N3: reload replays the neutralization event;
- N4: reload grants the encounter reward again;
- N5: a neutralized actor resumes hostile behaviour when remade;
- N6: a wounded creature restores at full health;
- N7: save/reload in Alerted or Search returns to Calm;
- N8: Pehlichi's lure bypasses masking.

**The proposal's list:**
- N9: health not written through;
- N10: streaming ignores the time away;
- N11: position not restored;
- N12: an unpresented model cannot hear;
- N13: neutralized recorded as defeated;
- N14: neutralization through a fake damage event (ADR-0017);
- N15: susceptibility ignored;
- N16: mechanism state not saved;
- N17: mask judged at the source;
- N18: duty cycle ignored;
- N19: an idle creature keeps its invoker;
- N20: creatures in a region keep their invokers;
- N21: the sweep disabled;
- N22: a mechanism restore replays its effects.

### A test gap closed before the run
Mask-at-the-source (N17) would have survived the first masking test, where source and listener were both
under the fan. A listener-versus-source case was added before the suite ran.

### The earlier suites (regression)
P9 changed code the P8 suite anchors on (`bDefeated` became the outcome), so its runner was updated and
**re-run in full**, with the terrain suite too: see [`regression-planted/`](regression-planted/).
**Result: P8 19/19, terrain 27/27, all caught.**

## Fresh clone
`Tools/verify-fresh-clone.sh` of **ef9b34e** passed. It used tracked inputs and the pinned engine, and ran
**143/143 tests, 46 requirements**. Only README lines were committed after it.

## Defects found and fixed during P9
1. **Stale navigation tiles, the real root cause of ADR-0029's accepted "column".**
   - A Recast build task finishing after its tile was removed puts the tile back, outside every invoker,
     and nothing removed it.
   - This P9 run measured 16–17 such tiles after a 300 m walk.
   - Fixed by a sweep (removes 15–19 per run). A test now asserts none remain.
   - Real-game effect: local nav peak 301 → 227.
2. **Creature health was never saved.** A wounded creature came back at full health after streaming or
   reload. It is now part of the model (N6, N9).
3. **Noise reached only creature actors,** so a creature waiting for presentation could not hear. It is now
   delivered to models (N12).
4. **The test harness skipped every creature.** It relied on actor ticks, and a bare test world registers
   none. Tests now step creatures directly.
5. **Real-game proof harness defects** (the harness only; no gameplay code):
   - Zenny teleported before the ground existed, fell, died and respawned outside the room.
   - Zenny walked into walls on the direct route.
   - The zone's north edge left the attacking warden outside the box. The rule then correctly neutralized
     nobody.
   - All fixed and re-run.

## Remaining debt
- **Operate progress is not persisted.** An interrupted or reloaded operation must be commanded again (by
  design; recorded).
- **Pehlichi reaches controls by his existing straight-line positioning.** It ignores walls, which is the
  "vent" in the proof room. Real vent traversal is future work.
- **The cage presentation is collision-free** (presentation only). Containment is the authoritative outcome,
  not a physics box.
- **The real-game environmental script cannot hide from patrols the way a player would.** Detection is
  proven to change state (spotted, searching, lost), but the script tanks the hits (Dev.ZennyHealed 5). A
  hiding behaviour for scripted proofs would make the stealth half of the route measurable end to end.
- **Proof-room scale is small (5 creatures, 1 region).** A multi-region dungeon, and regions crossing cell
  boundaries, are not exercised yet.
- **The environmental route's P10 half is still open:** structural collapse as neutralization, and the
  mid-fall debt.
