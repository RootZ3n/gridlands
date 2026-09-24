# Milestones: GRIDLANDS_BOOTSTRAP

Each milestone ends green on `Tools/test.sh` from a fresh clone, or it isn't done.
The operator reviews after M1 before anything further begins.

| # | Milestone | Proof | Status |
|---|---|---|---|
| M0 | Engine 5.8.3 prebuilt + git-lfs installed; `Tools/doctor.sh` | doctor passes | **done**: 5.8.3 CL 58210709 at `/pehverse/engines/UE_5.8.3` |
| M1 | Skeleton: 3 modules, tooling, docs, ADRs, one real headless test | `build.sh` + `test.sh` green from a fresh clone | **green, awaiting operator review**: 7/7 at `5b446c0`, fresh clone included |
| **Gate** | Design reconciliation (docs only), then operator review and the pre-M2 decisions in `DESIGN-RECONCILIATION.md` section E | operator approval | **in review** |

### Roadmap after the gate (revised 2026-09-24; see DESIGN-RECONCILIATION section G)

| # | Milestone | Proof |
|---|---|---|
| M2 | **Data pipeline v1**: JSON schema foundation (ids, tag namespaces, yield categories, knowledge, era/material/support fields, glitch stability weights, cells/bands/eras) + deterministic importer/validator (refs, NC-2 source check) | data tests green; re-import is byte-stable |
| S1 | **Terrain spike** (time-boxed): custom chunked heightfield vs landscape+overlay | measurements + ADR-0017; no production code |
| M3 | Character, interaction, home-slice blockout | interaction spec + manual walk |
| M4 | Salvage + inventory + world-settings yields (Core yield math) | per-category yield specs |
| M5 | Fabrication + knowledge unlocks | the tool salvages faster; discovery unlocks a recipe |
| M6 | Pehlichi command/scan/repair in the world; requirements (ObjectSalvaged, ItemDelivered, PuzzleSolved) | lifecycle/command/requirement specs |
| M7 | Stability model (Core) + interference tiers + static edge v0 | derived-value specs; static recedes after a repair |
| M8 | Persistence of all of the above (derived values rebuilt, not saved) | round-trip + migration spec |
| M9 | Event bus + dialogue director v0 + frequency setting + ~40 exchanges | selection specs; the silence gap holds |
| M10 | Building v0 (snap, a small mixed-era set, simple support) + terraform v0 per ADR-0017 | specs + manual build |
| M11 | First playable loop as one automated functional test; vertical-slice pass | full loop green headless |

## First playable loop (M9 functional test)

1. Player enters the home-region slice.
2. Player salvages a basic object for material.
3. Player fabricates a primitive tool.
4. The tool improves salvage efficiency on another object.
5. Player reaches an area containing a glitch that is **not initially visible**.
6. Player commands Pehlichi to scan.
7. Pehlichi reveals/detects the glitch.
8. Player satisfies a simple requirement, making the glitch repairable.
9. Player commands Pehlichi to repair it.
10. Pehlichi performs the repair.
11. The glitch becomes persistently repaired.
12. Pehlichi receives one permanent capability improvement.
13. Save.
14. Reload.
15. Repaired state and Pehlichi upgrade remain correct.
