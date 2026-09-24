# Design reconciliation: 2026-09-24

The operator's current Gridlands design was reconciled with the M1 bootstrap
architecture. **This pass is docs, design and impact analysis only. No code
changed, and M2 has not started.**

Baseline: `master` at `c563b40` (M1 GREEN, tag `m1-green` -> `5b446c0`).

---

## A. Files changed

| File | Change |
|---|---|
| `Docs/DESIGN-BIBLE.md` | **new**, the canonical design entry point: premise, cast, loop, world, story delivery, progression, building, travel, development strategy |
| `Docs/GLOSSARY.md` | **new**: shared vocabulary; "zone" deprecated |
| `Docs/STORY-AND-DIALOGUE.md` | **new**: cast dynamic, delivery rules, triggers, voice guide, frequency setting, dialogue-director design, invariants D-1..5 |
| `Docs/SURVIVAL-AND-THREAT.md` | **new**: pressure philosophy, threat invariants T-1..4, combat/non-combat, creatures, skills, Pehlichi growth, economy E-1..2 |
| `Docs/BUILDING-SALVAGE-TERRAIN.md` | **new**: salvage, knowledge/discovery, mixed-era building, structural support, terraforming, terrain technology options |
| `Docs/WORLD-AND-PROGRESSION.md` | **renamed** from `ZONES-AND-PROGRESSION.md` and rewritten: three axes, cells, radial bands, static/interference, derived stability, eras, Glitch Storms, underground, NPCs, travel, NC restated per band |
| `Docs/DESIGN-PILLARS.md` | rewritten to the operator's 14 pillars, ADR-linked |
| `Docs/GLITCH-AND-PEHLICHI.md` | quota rows removed; riddles/puzzles, scan reliability under interference, repair effects, weak points; lifecycle table **unchanged** |
| `Docs/ARCHITECTURE.md` | one-paragraph summary, planned-systems table, tag namespaces, persistence scope, seams, invariant table, out-of-scope list |
| `Docs/VISUAL-DIRECTION.md` | target fidelity (stylized 3D, Valheim to ARK range), static, storms, era collisions |
| `Docs/MILESTONES.md` | reconciliation gate + revised roadmap M2..M11 + terrain spike S1 |
| `Docs/HANDOFF.md`, `CLAUDE.md`, `README.md` | reading order, invariants, premise |
| `Docs/ADR/0011`..`0016` | **new ADRs** (below) |
| `Docs/ADR/0004`, `0006`, `0009`, `0010` | **amendment sections appended**; original decisions left intact |
| `Docs/DESIGN-RECONCILIATION.md` | this report |

New ADRs, each of which constrains implementation and would be expensive to reverse:
- **0011** radial bands + soft interference; **no locks** (supersedes quota gating)
- **0012** band, cell and era are separate axes; era is not a tech tier
- **0013** stability, interference and NICE composure are derived from glitch state
- **0014** threat comes from place and choice, never routine activity
- **0015** data-driven contextual dialogue director
- **0016** world settings; yields scale by category; progression is never scaled

No ADR was written for Glitch Storms, NPCs, sewers, fast travel, skills or
creature roster. They are design content whose architecture follows from the
ADRs above, or they are decided later.

## B. New and changed design invariants

| Id | Invariant | Status |
|---|---|---|
| P-1 | Only Pehlichi completes a glitch repair; the Player authority has no transitions | unchanged (tested in M1) |
| P-2 | Access, guards, resources, **puzzles**, jamming are requirements, not states | extended (puzzles) |
| P-3 | Pehlichi does not deal damage; its creature abilities are detection, weak points, disruption, pacification | **new, an interpretation of "not a combat pet"; confirm (E6)** |
| NC-1 | No progression or completion requires a kill; progression is **stabilization** | **changed** (was "zone quota") |
| NC-2 | Every critical-path item, capability **or knowledge** has a non-combat source | extended (knowledge) |
| NC-3 | Per **band**, combat-free stabilization makes the next band traversable | **changed** (was per-zone quota) |
| NC-4..6 | Hunters avoidable (now including non-lethal disabling); bosses optional; not easy mode | refined |
| W-1 | Cell, band and era are independent; era is not a tech tier | **new** (ADR-0012) |
| W-2 | No invisible walls or repair-count locks; interference is the barrier | **new** (ADR-0011) |
| S-1 | Stability, interference and NICE composure are derived, never stored | **new** (ADR-0013) |
| T-1..4 | Routine activity never summons threat; every hostile has a spatial/authored source; stabilization suppresses locally; home supports low-attention play | **new** (ADR-0014) |
| D-1..5 | Dialogue authored; story-critical ignores the setting; silence gap and repetition caps; systems emit events; seeded deterministic selection | **new** (ADR-0015) |
| E-1..2 | Every yield passes through its category's setting; progression rewards never scale | **new** (ADR-0016) |
| G-1 | Grid lines define regions, not surfaces | unchanged |
| I-1 | No model/API/lab calls from game code, **including NICE and all dialogue** | extended |

## C. Architectural consequences for the M1 design

**Unchanged and still correct:**
- the module layering (Core pure / Game / Editor);
- JSON as source of truth;
- no GAS;
- snap-socket building;
- the tooling and the verification rules;
- the **glitch lifecycle table** and its tests.

The new design reinforced the lifecycle rather than challenging it: riddles,
guards and interference all fit as requirements or scan inputs.

**Grows:**
1. **GridlandsCore gains four pure rule sets**:
   - the stability/interference/composure model (ADR-0013);
   - dialogue selection (ADR-0015);
   - yield computation with world settings (ADR-0016);
   - later, structural-support propagation.

   This is exactly the M1 pattern: pure, headless-tested, and agent-maintainable.
2. **A gameplay event bus** becomes shared infrastructure (dialogue, later
   achievements/objectives). It must exist before dialogue and should be
   designed in M3/M4 so systems emit from day one.
3. **The M2 content schema is wider than the bootstrap assumed**:
   - materials separate from era/style;
   - support fields on pieces;
   - knowledge + `unlockedBy`;
   - acquisition `Source.*` tags;
   - `Yield.*` categories;
   - glitch stability weights and cell membership;
   - cell/band/era definitions;
   - dialogue exchanges.
4. **The persistence scope grows**: terrain deltas, knowledge, skills, puzzle
   states, dialogue history/story flags, explored-map state, world settings.
   Derived values are explicitly *not* saved.
5. **Terrain** is a new core subsystem with an undecided technology. It
   affects level authoring, navigation, streaming and saves.
6. **Interference is cross-cutting**: map/minimap, visibility, scan
   confidence and weak-point analysis all consume one derived tier.
7. **Creature model**: add non-lethal outcomes (disabled, pacified, fled),
   patrol routes, and weak points as a scan finding kind.

## D. Conflicts between the existing architecture and the current design

| # | Existing | Current design | Resolution |
|---|---|---|---|
| 1 | Zone quota: repair ~20 of ~35 to unlock the next boundary (first ZONES doc, ADR-0009 text, ADR-0010, ARCHITECTURE, GLITCH doc) | no locks; soft interference | superseded by ADR-0011; ADR-0009/0010 amended; docs rewritten |
| 2 | Linear zone chain ("Zone 1 -> later zones") | radial bands; sideways travel at similar difficulty | WORLD doc rewritten |
| 3 | `UGLZoneDefinition` conflated place, depth and content | three axes | ADR-0012; `UGLGridCellDefinition` + band + era |
| 4 | Unnamed "nefarious AI"; "hunters sent to find the player" | NICE is Game Master; hunters bound to place/band | cast documented; ADR-0014 constrains hunters |
| 5 | Visual: "low-poly, pixel-adjacent" | stylized 3D, Valheim to ARK range | VISUAL doc updated |
| 6 | Structural support "designed for, not built" | a core desired feature | ADR-0004 amended: schema fields from M2; built in M10 |
| 7 | Narrative out of scope | the banter system is core identity | ADR-0015; designed now, built in M9; cutscenes/voice still out |
| 8 | No terraforming in the architecture | core feature | new subsystem; spike + ADR-0017 before M3 (E7) |
| 9 | No economy settings | world-configurable yields by category | ADR-0016 |
| 10 | Capability component could serve player and Pehlichi | Zenny grows by use-based **skills**; Pehlichi by capabilities/knowledge | separate Skills system; capabilities stay Pehlichi-centric |
| 11 | Persistence covered glitches, inventory, pieces, capabilities | many more world facts | ARCHITECTURE section 6 expanded; character-vs-world split open (E3) |
| 12 | "Tiny test neighborhood" | build one Grid region deeply | home-region slice, multi-cell-ready |
| 13 | **ADR-0002 (JSON source of truth) vs where placements live** | a hand-authored world stores actor placements in binary `.umap` files that agents cannot edit | **open, E2**: placement manifests in JSON per cell, or accept umap placement |

## E. Decisions to make BEFORE M2

These shape the M2 content schema or are costly to change once content exists.

| # | Decision | Recommendation | Why before M2 |
|---|---|---|---|
| E1 | **Yield categories** (ADR-0016 pending): which scale with settings? | Scalable: `Yield.Common.Salvage`, `Yield.Common.Gather`, `Yield.Creature.Drop`. Never scaled: `Yield.Reward.Glitch`, `Yield.Reward.Unique`, `Yield.Knowledge`. **Operator: should `Yield.Rare` (rare materials) scale?** | every yield entry in the schema names a category |
| E2 | **Authored vs procedural world, and where placements live** | Hand-authored cells. Levels hold geometry and art; **gameplay placements (glitches, salvageables, requirements, spawn sources) live in JSON manifests per cell**, spawned by id at load, so agents can place content. Procedural is out. | decides the persistent-id scheme and whether M2 imports placements; the biggest AI-maintainability lever left |
| E3 | **Character-portable vs world-bound progression** (knowledge, skills, inventory, Pehlichi capabilities) | **World-bound** for everything. Pehlichi's growth is tied to repairing this world, and a single authored world makes portability pointless. Revisit only if multiple worlds are ever wanted. | save format and knowledge ids |
| E4 | **Content schema conventions**: id format, tag namespaces, schema versioning, one file per entity | ids `kind.domain.name` (`item.material.copper_wire`); namespaces `Era.* Band.* Material.* Source.* Yield.* Event.* Knowledge.*`; `schemaVersion` per file; one entity per file | the importer is built on them |
| E5 | **Era list as data** | the 10 known eras as `Era.*` tags; adding one is data only | build pieces and cells reference them |
| E6 | **Confirm P-3**: Pehlichi never deals damage | confirm | constrains Pehlichi capability data |
| E7 | **Terrain technology** (can run in parallel with M2; required **before M3**) | time-boxed spike S1 -> ADR-0017 | level authoring and saves depend on it |
| E8 | **Home slice brief** (before M3): size and era mix | ~250 x 250 m corner of the home cell; a 1950s suburb with one Roman fragment; one storm-drain entrance | M3 blockout needs it |

## F. Decisions that can safely wait

- exact cell size and count, band count, and cell-to-band map;
- era placement per cell and the rules for era mixing;
- interference tier thresholds and falloff (tuning data);
- the Glitch Storm catalogue and schedule;
- skill list and curves; the Pehlichi capability tree;
- creature roster, bosses and their rewards;
- NPC/trader economy and merchant-return rules;
- fast travel via Grid infrastructure (not approved);
- sewer/dungeon layouts;
- the streaming technology ADR (by the second cell);
- structural-support numbers;
- the non-lethal tool list;
- whether NICE's hunters may enter stabilized territory, and how far;
- the default commentary frequency setting;
- voice acting, cutscenes and localization;
- a naming review of "Neurolink" before any public release (close to a real
  company's name).

## G. Proposed roadmap, next ~30 days of Claude Code work

**Honest sizing:** M2 through M9 fit in about 30 working days. M10–M11 likely
spill into days 31–40. Each milestone ends green on `Tools/test.sh`, fresh
clone included.

| Days | Work |
|---|---|
| 1–6 | **M2** data pipeline v1 (schema per E1–E5, importer/validator, deterministic re-import, NC-2 source check, era-not-power lint) |
| 5–8 | **S1** terrain spike, overlapping M2's tail -> ADR-0017 -> operator decision |
| 7–9 | **M3** character, interaction, home-slice blockout (per E8), event-bus skeleton |
| 10–12 | **M4** salvage + inventory + world-settings yields |
| 13–15 | **M5** fabrication + knowledge unlocks |
| 16–19 | **M6** Pehlichi command/scan/repair + requirements (salvaged blocker, delivered item, simple riddle) |
| 20–22 | **M7** stability model + interference tiers + static edge v0 |
| 23–25 | **M8** persistence of all of the above |
| 26–30 | **M9** dialogue director v0 + frequency setting + ~40 exchanges |
| 31–40 | **M10** building v0 + terraform v0; **M11** automated first-playable loop + vertical-slice pass |

## H. The earliest genuinely playable, fun vertical slice

A **~250 m corner of the home region**: a 1950s suburban street of 6–8
houses, with one out-of-place Roman fragment, and static visible at the
slice's edges.

- **Salvage**: wire, wood, scrap and brick from houses and yards. Stripping
  wire can earn a NICE jab ("Where's your bike?").
- **Fabricate** a pry bar; salvage visibly faster with it.
- **Build** a small shelter from snap pieces mixing two eras. NICE rates it.
- **Pehlichi** reveals 4–5 hidden glitches:
  - one inside a house wall;
  - one behind a salvageable blocker;
  - one needing a delivered part;
  - one with a short riddle.
- **Repairs push the static back**, opening a storm-drain entrance to a
  one-room sewer proof with a hidden glitch.
- **Creatures**: one passive glitched animal, and one territorial one that can
  be avoided (the non-combat path works from day one).
- **One harmless Glitch Storm**: raining glitched cats and dogs.
- **~40 exchanges** of NICE/Pehlichi banter, covering death, overencumbrance,
  first repair and silence, with the frequency setting working.
- **Save and reload** keep everything.

Why this slice: it exercises every pillar at small scale (salvage, build,
Pehlichi-only repair, soft barrier, peaceful routine, avoidable threat, humor)
without needing combat depth, many cells or large content volume.

## I. What to design now so Ikbi and Abaiya can inherit it

Agents do best with **pure rules + JSON + tests**. Design these first:

1. **The content schemas and validator (M2)**. With them, an agent can add
   items, pieces, glitches, knowledge and dialogue without the editor, and
   invariant violations (NC-2, era-not-power, unresolved refs) fail lint
   instead of review.
2. **Per-cell JSON placement manifests (E2)**, so agents can place glitches,
   salvageables and spawn sources, not just define them.
3. **The pure Core rule sets**: glitch lifecycle (done), stability/
   interference/composure, dialogue selection, yield math, support
   propagation. Each is deterministic, seeded where random, and exhaustively
   testable, the same pattern M1 proved.
4. **The gameplay event vocabulary** (`Event.*` tags), so dialogue and later
   objectives are content, not code.
5. **A requirement-kind registry**: new glitch requirements are one C++ class
   + JSON kind + spec, following a written recipe in HANDOFF.
6. **Invariant-guarding tests in `Tools/required-tests.txt`** for every ADR
   with a mechanical rule (P-1, S-1, T-1, D-2/D-3, E-1). Abaiya challenges diffs
   against them, and the acceptance verifier runs them.
7. **Dialogue authoring lint**: ids unique, triggers valid, no story-critical
   exchange unreachable, and repetition caps set.

---

**Stop:** this report ends the design-reconciliation pass. M2 waits for the
operator's review and the section E decisions.
