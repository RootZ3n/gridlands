# P10: structural environmental resolution

**Built 2026-10-02 under the operator-approved proposal and decisions 1–5. READY FOR OPERATOR REVIEW: not merged,
not tagged.**
- Decision record: [ADR-0038](../../ADR/0038-structural-environmental-resolution.md). It amends
  [ADR-0030](../../ADR/0030-structural-salvage-and-deterministic-collapse.md); ADR-0033 and ADR-0037 carry notes.
- This is system work, not content. The fixture is the ordinary P6 carport (`structure.modern.carport`, structure data
  unchanged), placed in the P9 dev proof room. P9's warden gains the `Neutralize.Pinned` susceptibility, as data.

## The rule
1. **SUPPORT FAILED** decides the fall: trajectory, rest, impact time and volume, severity, and one collapse event.
   It never decides who is affected.
2. **IN FLIGHT** the target is free to move.
3. **IMPACT** decides from the world at that instant. For each active creature model touching the volume, exactly one
   of:
   - **pinned** (`Neutralize.Pinned`), if the impact's severity ≥ `collapse.pinMinSeverity` (2.0, provisional) and the
     creature is susceptible: no damage, no death, no kill, health kept;
   - the **ordinary environmental damage** path, which can DEFEAT it with the usual drops, credit and events.

   Already defeated or neutralized creatures are skipped.

## Real-game proof (`Tools/p9-dungeon-proof.sh structural …`, results in [`proof/`](proof/))
The route is played in the real game: real ticking, navigation and streaming, with Zenny's own movement input.
1. Through the west gap and the north corridor into the arena (detected 5 times; the script cannot hide, as in P9).
2. Zenny takes the carport's **first post**. It stands: the redundancy rule holds and nothing is in flight.
3. He takes Pehlichi under the hanging deck, orders him to **Stay**, walks back to the posts, and orders a
   **Distract**.
4. The warden comes to investigate. Once it stands under the decks, Zenny takes the **last post** (6 salvage hits
   through the ordinary pipeline).
5. **Support fails**, and both decks fall (about 1 s). The warden keeps moving: its distance to the east deck's centre
   drifts from 186 to about 200 cm during the fall.

### `structural` ([json](proof/dungeon-structural.json))
| Measure | Value |
|---|---|
| Warden outcome | **NEUTRALIZED, `Neutralize.Pinned`** (by the west deck, which it was under at impact) |
| Damage to the warden | **0**; health 240 of 240 |
| `Event.Creature.Defeated` / kill credit | **0 / none** |
| `Event.Creature.Neutralized` / `Event.Encounter.Resolved` | 1 / 1 (the reward, +5 residue, once) |
| Same impact on the two non-susceptible gremlins under it | **ordinary damage, 59 each** (60 hp: they survive with 1) |
| Impacts | 2 (one per deck): severity 2.45 / 2.46, damage 59 / 59.2 |
| Warden actor | present, inert |
| Restart ([json](proof/dungeon-structural-restart.json)) | still Pinned, inert, full health; **zero events**; residue unchanged |

### `structural-control` ([json](proof/dungeon-structural-control.json)): the non-susceptible control
The same route, except that Zenny first wounds the arena patrol in an ordinary fight (it has 52 of 60 hp left).
The same impact then:
- **pins the warden** (0 damage);
- **DEFEATS the wounded patrol through its health**: one `Event.Creature.Defeated`, and its 2 drops go to Zenny, who
  removed the support (residue 7 = the 5 reward + 2 drops);
- **wounds the full-health gremlin** to 1 hp.

The restart replays nothing. P10 did not replace structural damage with a boss-specific neutralization.

### `structural-midfall`: quit half a second into the fall
1. **`structural-midfall`** ([json](proof/dungeon-structural-midfall.json)): the run quits mid-fall, and the autosave
   holds **2 collapses in flight**. No impact has happened yet; the debris is decided but not landed.
2. **`structural-midfall-resume`** ([json](proof/dungeon-structural-midfall-resume.json)): a restart from that save.
   - The fall resumes and lands **once**: `Event.Structure.Collapsed` 0, `Event.Structure.Impact` 2.
   - The warden is **Pinned**, and the two gremlins are wounded to 1 hp.
   - Neutralized 1, Resolved 1, residue 5.

   This is the same outcome as the uninterrupted run.
3. **`structural-midfall-restart`**: zero events, nothing replays.

### `structural-unload`: leave the lots mid-fall
1. 0.4 s into the fall Zenny leaves for the origin, and the lots stream out with the fall frozen in their record.
2. Five seconds later he comes back.
3. The fall resumes where it stopped and lands once: the warden **Pinned**, gremlins wounded, the same outcome.
4. The restart replays nothing.

## P9 regression (results in [`p9-regression/`](p9-regression/))
The same scripts, re-run unchanged:

| Route | Result | Same as P9 evidence |
|---|---|---|
| direct | warden **Defeated** through 240 damage; 5 defeats; residue 13; Zenny took 252 (2 dev heals) | yes, identical |
| environmental | warden **Neutralized / Contained**, 0 damage; residue 5; Zenny took 432 (5 dev heals); 47 s | yes, identical |
| restarts | zero events; outcomes kept | yes |
| navscale | region 190 (inside 164) active tiles; **1 → 16 extra active creatures add 0 tiles**; without the region 225 | the carport adds 2 tiles to the room's baseline (188 → 190, 162 → 164) |

## Automation (`Tools/test.sh`; P10 suites `Gridlands.Game.Structural` (10) and `Gridlands.Core.Structure` (+3))
**Full gate: 156/156 tests, 47 requirements met.** The only warnings are the engine's environmental connectivity ping.

| The operator's case | How it is proven |
|---|---|
| 1. save before support removal | `SaveRestartAtAnyMoment…` break at −1 (also for the control) |
| 2. save right after support failure | break at 0.000 s |
| 3. save mid-fall | break at 0.5 s (also 0.3 / 0.7 s for escape and late entry) |
| 4. save ~1 ms before impact | break at impact − 0.001 s, with the step landing exactly there (and the baseline stepping identically) |
| 5. save after impact | break after the impact |
| 6. unload mid-fall | `UnloadingMidFall…`: the lots stream out at 0 / 0.3 / 0.5 / 0.7 s / impact − 1 ms; the fall waits frozen in the record (elapsed unchanged after 10 s away) and resumes at the same pose, not solid |
| 7. presentation after reload | the resumed part is presented at `Motion(elapsed)`, non-solid; every millisecond's pose is bit-identical to the plan (`APendingCollapseRebuildsBitIdentically…`) |
| 8. escapes before impact | `TheImpactDecides…`: under at failure, out at 0.5 s → not affected (and under every break) |
| 9. enters after failure | out at failure, in at 0.6 s → pinned, held where it was at impact |
| 10. outside at impact | not affected, encounter still open |
| 11. Neutralized survives restart | `APinnedCreatureStaysPinned…`: Pinned, inert, in place, zero events after restart |
| 12. no duplicates | every broken timeline must equal its straight run on outcome, how, health, held position, target damage, Zenny health, residue, collapse events, impacts, neutralizations, deaths, resolutions and impact noises |

**Also proven:**
- **The control** (`ANonSusceptibleTarget…`): a full-health gremlin takes exactly the plan's damage and survives; a
  wounded one is defeated, with one death event and its drops to Zenny; with both under one impact the warden is
  pinned, the gremlin damaged, and each gets exactly one outcome over all impacts.
- **No re-plan** (`ARestoredFallIsTheDecidedFall…`): the fall resumes in a world with other gravity, delay, damage and
  impact margin, and is still the decided fall (impact time, damage, volume, rest).
- **The hold** (`AFallWaitsUntilTheCellsCreaturesCanMove`): while restored creatures wait for presentation, the fall
  does not age.
- **Model impacts** (`ImpactsQueryCreatureModels…`): an unpresented creature is found by the shared capsule (exactly at
  its head; 1 cm above it, not), and damaged through the one health path (presented, then defeated, with drops
  credited).
- **Finality:** a second carport collapsing on the pinned warden and a defeated gremlin does not even consider them.

## Planted defects ([`planted-defects/`](planted-defects/), `Tools/planted-defects/p10_structural.py`)
**28/28 caught by an assertion.** The suite is stricter than earlier ones:
- a run that dies, does not build, or fails only through engine errors is not counted;
- verdicts are re-derived from the saved reports with `--summarize`.

| Group | Defects |
|---|---|
| timing and authority | S1 victims decided at failure · S2 the target's position at failure used (escapee hit) · S3 late entrant ignored · S4 the volume where the part was |
| who and how | S5 actorless creature immune · S6 non-susceptible pinned · S7 threshold ignored · S8 damaged AND pinned · S9 defeated/neutralized considered · S10 neutralized damaged later (P10 guards and P9 immunity removed) · S11 one impact hits twice · S12 a second, different capsule |
| persistence | S13 not captured · S14 a file save drops it · S15 resumed fall solid · S16 elapsed reset · S17 the dormant fall keeps time · S18 resumes before restored creatures can move · S19 re-planned on restore · S20 impacted still saved as pending (double impact at the boundary) · S21 credit lost after reload |
| replay | S22 collapse event · S23 impact noise · S24 pinned reward, on restore |
| reconstruction | S25 topple re-integration drift · S26 rest through a rotator · S27 severity ignores the material |
| cross-cell rule | S28 STR-4 lint disabled (a tooling test) |

**Regression:** P9 **22/22**, P8 **19/19**, terrain **27/27** ([`regression-planted/`](regression-planted/)).

**Test gaps the suite exposed, closed before the final run:**
- **Crashes instead of catches.** Two tests dereferenced the fall without a null check, so a defect that ends the fall
  early (S1) crashed the run instead of failing an assertion. They now assert first.
- **The first classifier missed real catches.** It counted only errors located in test files, and UE locates its
  templated checks inside engine headers. It now reads the assertion text; engine log errors still never count.
- **S8 did not build** in its first form, and was rewritten.

## Fresh clones
- **P10 `d75cf37`:** PASS, 156/156, 47 requirements, from tracked inputs and the pinned engine.
- **The P9 merge `a69e106` on master:** PASS, 143/143, 46 requirements. This is the P9 closure verification after the
  merge.
- Only docs and evidence were committed after the P10 clone.

## Real-game performance and memory (unchanged budgets; [`perf/`](perf/))
`Tools/perf/quiet-run.sh`, all four suites: **local PASS, dense PASS, town FAIL, towndense FAIL**. Each failure is one
budget: the **reversal worst frame** (40 ms), at **40.2 ms** (town) and **41.6 ms** (towndense).

**A/B on the same machine** (fresh clones of master `a69e106` and of P10, alternated, three rounds each, quiet-gated;
[`perf/ab/`](perf/ab/)):

| Reversal (pairs whose load at the end was < 8) | P9 master (unchanged) | P10 |
|---|---|---|
| town worst frame (ms) | 38.5 / 36.1 / 38.9 | 36.9 / 36.8 |
| towndense worst frame (ms) | **42.4 / 42.2** | **41.9 / 41.5** / 39.3 |
| mean game thread (ms) | 1.82–1.84 (town), 1.88–1.89 (towndense) | 1.83 (town), 1.86–1.87 (towndense) |

**P10 adds no measurable cost.** The unchanged P9 build breaches the same budget on this machine today. P9's evidence
recorded 36.0 ms for towndense reversal. The budget was not changed, and nothing was optimized speculatively. **Operator
review item.**

**Everything else holds:**
- 0 emergency chunks in 20 crossing runs;
- streaming game-thread worst ≤ 10.2 ms (budget 12);
- p99 ≤ 5.83 ms;
- memory peak ≤ 3.80 GB (budget 5.2); round-trip growth 82 / 66 / 56 / 67 MB (budget 400);
- nav active tiles ≤ 273 (budget 600); nav full build 0.47 s (budget 1.5).

**P10's own costs (real game):**
- collapse decision 0.006–0.13 ms (P6: ≤ 0.16);
- impact resolution ≤ 0.06 ms with creature models;
- an in-flight record is ~30 fields, held only for the ~1 s of a fall;
- steady-state cost is nil (an empty loop).

## Defects found during P10
1. **P6 mid-fall debt (by design, now fixed).** An unload or save mid-fall dropped the impact still to come, and the
   impact only reached pawn actors, so an unpresented creature was immune.
2. **Identity-only attribution regressed live credit** for a generic pawn (P6 test). The live credited actor is now kept
   beside the identity.
3. **Latent include defect.** `GLLocalizedNavigationTests.cpp` used `UDynamicMeshComponent` without its header. It
   compiled only through its unity blob, and a new test file in the blob exposed it.
4. **Test-assumption errors, corrected (not engine faults):**
   - the carport lands on uneven terrain, so it falls 2.46 m and does 59.2 damage; a 60 hp gremlin survives;
   - a warden at the east deck's edge is under the west deck too, and that deck lands first.
5. **Planted-defect harness:** the crash-instead-of-catch and classifier issues above.

## Remaining debt
- **The reversal worst-frame budget is breached by the unchanged baseline** on today's machine (above).
- **Impact timing is frame-quantized:** the first structure step at or after the impact time, at most one frame
  (~10 cm at chase speed).
- **The hold is per cell** (any pending creature of the cell holds every fall there). It is coarse but conservative;
  pending creatures exist only for the frames after a cell's runtime layer arrives.
- **Mid-fall saves from builds before P10** carry no in-flight record. They load as before: debris at rest, impact lost.
- **The real-game script cannot hide** (as in P9). It is detected 5 times on the way in.
- **`pinMinSeverity` (2.0) and every collapse number are provisional.**
