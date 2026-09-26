# P6 evidence: structural salvage, natural gathering, gravity collapse, authoritative noise

- Engine: Unreal 5.8.3, Linux, Vulkan.
- Hardware: RX 6800, i7-11700K. Development evidence, not a minimum specification.
- Decisions:
  - [ADR-0030](../../ADR/0030-structural-salvage-and-deterministic-collapse.md): a data part graph
    and deterministic collapse;
  - [ADR-0031](../../ADR/0031-authoritative-world-noise.md): one world-noise model;
  - [ADR-0014](../../ADR/0014-threat-comes-from-place-not-activity.md): the noise amendment.
- Reproduce:
  - `Tools/p6-acceptance.sh` for the real game;
  - `Tools/test.sh` for automation;
  - `Tools/perf-crossing.sh -t` for the P5 budgets.

## What was built
- **`structure.*`: authored structures as data part graphs** in the shared structural language
  (parts are `buildpiece.*`; world-only pieces are `buildable: false`).
  - Proof content: a lean-to carport (two posts, two decks) and a pine tree (a stump and a trunk).
  - Each is placed once near the start and once 16 m from the origin/lots boundary.
- **`GLCollapseRules` (Core, pure).** It decides:
  - what loses support (a cascade);
  - the drop or topple motion, from data, and the topple direction policy, from data;
  - the rest transform, the impact time and the oriented impact volume;
  - the damage (`tuning.world.physical`, provisional).
- **`UGLStructureSubsystem`.** It:
  - commits the outcome at the moment of decision;
  - damages at impact through `UGLHealthComponent`;
  - drives the presentation pose from the same plan;
  - turns debris into salvage;
  - streams with its cell and saves facts per part (save v2, optional field).
- **`UGLNoiseSubsystem`.** Terraforming, salvage hits, chopping, building, demolition, breakage,
  collapse and Pehlichi's lure all emit.
  - Creatures hear by `min(noise radius, hearing radius)`, investigate, and search the last known
    position after losing sight.

## Real-game acceptance (`real-game/`, final binary)

| Proof | Run | Result |
|---|---|---|
| A. Zenny removes the wrong structural support | `collapse` | The south post comes away and nothing falls (redundancy). The north post, the last support, comes away. |
| B. A supported section becomes unsupported | `collapse` | "decided at once": both decks become debris, 2 collapses active |
| C. Physical collapse occurs | `collapse`, `collapse-1.jpg` (mid-fall), `collapse-2.jpg` (after) | The decks fall; at impact the actors are at their planned rest |
| D. It can damage or kill Zenny | `collapse` (health 100 → 40), `kill` (50 → 0) | The west deck hits Zenny for the planned 60 (10 + 20 × 2.5 m) |
| E. World state valid afterwards | `collapse` | Posts removed; decks are debris at rest, solid and salvageable; 2 break and 2 collapse noises |
| F. Save/restart preserves it | `collapse-restart` | After relaunch: posts removed, both decks debris at the same rest, Zenny health 40, no impacts |
| G. The same collapse with a creature in the impact area | `creature` | Collapse decided; a (dev proof) gremlin is under the east deck |
| H. The same model damages or kills it | `creature` | The east deck hit it (60): defeated; its drops went to Zenny, who brought it down |
| I. Terraform out of sight, inside hearing | `noise` | A dig 9.5 m behind creature 1 is heard by 1 |
| J. It investigates | `noise` | Investigate; it walked 7.9 m toward the dig |
| K. Outside hearing, no reaction | `noise` | A dig 20 m from creature 2 is heard by 0; it stays Idle at home |
| L. Cover after detection | `noise` | Creature 3 chases Zenny; Zenny raises a mound between them |
| M. Sight lost is not knowledge lost | `noise` | Search, last known = Zenny's position, 7.8 s of memory; it walks there, then gives up |
| N. Noise as a distraction | `noise` | Control: passing in front of creature 5 is spotted (Chase). After a dig behind creature 4, the same walk passes unseen. |
| O. Structural change near a boundary | `edge` | The carport, 16 m from the boundary, collapses; the edge pine is chopped |
| P. Cross away and back during and after | `edge` | Crossing mid-fall (2 active collapses) drops them; the origin streams out; back: debris at rest |
| Q. No duplicates, restored supports, replays, double damage or lost salvage | `edge` | After 4 crossings: posts removed, the west deck at rest, the east deck salvaged, exactly 1 carport actor, 0 active collapses |
| R. Save in the neighbouring cell, restart, return | `edge-restart` | The same facts, including the felled edge pine at its rest |
| S. Cut and harvest a tree through the governed pipeline | `tree`, `tree-1/2.jpg` | 3 chop noises; the trunk topples east (away from Zenny) and becomes a log; salvaging it gives planks 1 → 7 |
| T. Tree persistence through unload and restart | `tree-restart` | Stump removed, log gathered, no actors |

**Proof creatures** are spawned by a dev-only placement API (`SpawnProofCreature`, non-shipping). Only
placements create gameplay creatures (ADR-0014). The one authored creature lives in the drain, where
no structure can yet stand (see the debt below).

## Automation
- **102 tests, all green** (`30-full-gate.index.json`), 39 requirements met. The 7 warnings are all one environment log line from the desktop session ("Querying IsUsingWayland before SDL is initialized"), not from Gridlands.
- **New tests:**
  - `Gridlands.Core.Structure` (3): who falls and the cascade; drop determinism and truthful impact;
    topple and its direction policy.
  - `Gridlands.Core.Combat.HearingAndMemoryAreSeparateFromSight`.
  - `Gridlands.Game.Structure` (5): wrong support hurts and can kill; collapse damages creatures
    through the same model; persistence through streaming, saves and restart at the boundary; tree
    felling; every authored structure stands.
  - `Gridlands.Game.Noise` (2): heard only within hearing; cover after detection leaves a last
    known position.

## Planted defects (`P1`…`P8`: diff plus report)

| Defect | Caught by |
|---|---|
| P1 support loss never re-derived | all 4 collapse tests |
| P2 the impact never reaches the health system | wrong support, creature |
| P3 loading re-runs the collapse (debris restored as intact) | persistence |
| P4 debris is not a saved fact | persistence, wrong support |
| P5 terraforming is silent | heard only within hearing |
| P6 losing sight erases knowledge | core hearing/memory, cover after detection |
| P7 every creature hears every noise | core hearing/memory, heard only within hearing |
| P8 a stale unload: collapses keep running after their cell is gone | persistence |

**The test suite had a crash of its own.** At first, P5 crashed the test run instead of failing it:
the noise test read the last noise of an empty list. The test now fails cleanly, and P5 is caught.

## Performance
**Collapse cost (real game, game thread):**
- deciding a collapse (support plus plan): **0.06–0.16 ms**;
- resolving an impact (volume, health, noise): **0.01–0.13 ms**.

**P5 budgets with P6 in place** (`perf/`, `Tools/perf/budgets.json`): all 6 runs within budget.

| Run | Worst frame | p99 | Note |
|---|---|---|---|
| Straight | 23.6 ms | 4.5 ms | |
| Sprint | 18.5 ms | | |
| Reversal | 37.0 ms | 8.2 ms | Budget 40; P5 measured 34.1. This is within the run-to-run spread seen in P5 (28.8–34.1). |
| Teleport | 25.3 ms | | |

- **Emergency chunks:** 0.
- **Navigation:** tiles peak at 301, localized (initial navigation 0.52 s).

## Known debt
- **Structures stand only on the terrain heightfield.** Interiors and dungeons need a ground source
  for authored geometry (ADR-0030, Future decisions).
- **Save and load mid-fall.** A save made during a fall keeps the outcome, but the impact damage of
  that fall is not re-applied on load. That is consistent with "never replay", and exploitable only
  by saving mid-fall.
- **No load or impact modelling between parts.** Falling debris does not break other parts;
  cascades come from support alone.
- **All numbers are provisional data:** radii, damage, yields, start delay and memory.
- **Debris yields its part's full salvage.** This is the approved provisional default, and the
  economy is untuned.
- **Blockout presentation.** A creak/warning telegraph uses `startDelaySeconds`, but nothing is
  drawn yet; cosmetic physics hangs on `OnPresentationImpact`.

## Fresh clone
**PASS:** a fresh clone of `1dab7aa` built and passed **102/102** tests (39 requirements) and the data
validation (162 entities), from tracked inputs plus the pinned engine (`00-*`).
