# Milestones: GRIDLANDS_BOOTSTRAP

Each milestone ends green on `Tools/test.sh` from a fresh clone, or it isn't done.
The operator reviews after M1 before anything further begins.

| # | Milestone | Proof | Status |
|---|---|---|---|
| M0 | Engine 5.8.3 prebuilt + git-lfs installed; `Tools/doctor.sh` | doctor passes | **done**: 5.8.3 CL 58210709 at `/pehverse/engines/UE_5.8.3` |
| M1 | Skeleton: 3 modules, tooling, docs, ADRs, one real headless test | `build.sh` + `test.sh` green from a fresh clone | **green, awaiting operator review**: 7/7 at `5b446c0`, fresh clone included |
| **Gate** | Design reconciliation (docs only) + operator decisions E1–E8 | operator approval | **approved 2026-09-24** |

### Roadmap (operator-approved 2026-09-24; dialogue moved earlier as a core feature)

Every milestone ends green on `Tools/test.sh` **and** `Tools/verify-fresh-clone.sh`,
with evidence under `Docs/Evidence/<milestone>/`. Gates are not traded for speed.

| # | Milestone | Proof |
|---|---|---|
| M2 | **Data pipeline v1** (JSON loaded directly, ADR-0021): ID/tag grammar validator ([CONTENT-IDS-AND-TAGS](CONTENT-IDS-AND-TAGS.md)); schemas for items, materials, recipes, salvage, yield categories, knowledge, build pieces (era + material + support), the 10 eras, bands, cells, glitches (stability weight), placements, capabilities, exchanges; C++ typed loader with closed keys and round-trip; lints for NC-2, E1 non-scaling, ERA-1 and P-3 no-damage | **DONE 2026-09-24**: 57 entities valid, 55 tooling tests, 15/15 automation tests from a fresh clone ([evidence](Evidence/M2/README.md)) | 
| S1 | **Terrain spike** alongside M2 (time-boxed prototypes, not production) | **DONE; [ADR-0022](ADR/0022-terrain-chunked-heightfield.md) approved: chunked runtime heightfield** ([evidence](Evidence/S1-terrain/README.md)) |
| M3 | Character, interaction, **gameplay event bus**, anchor component + export commandlet, modern-suburbia slice blockout | **DONE 2026-09-24**: 20/20 automation tests; rendered game run loads L_Origin with 15 anchors ([evidence](Evidence/M3/README.md)); keyboard walk-around is the operator's to try |
| M4 | Salvage + inventory + world-settings yields | **DONE 2026-09-24**: 28/28 automation tests; E-1 mutation caught ([evidence](Evidence/M4/README.md)) |
| M5 | **Dialogue director v0** + frequency setting + first ~15 exchanges on real events (salvage, overencumbrance, death, idle) | **DONE 2026-09-24**: 34/34 automation + 58 tooling tests; 15 exchanges ([evidence](Evidence/M5/README.md)). Idle has no event source yet |
| M6 | Fabrication + knowledge unlocks | **DONE 2026-09-24**: loop steps 2-4 automated; 39/39 automation + 59 tooling tests ([evidence](Evidence/M6/README.md)) |
| M7 | Pehlichi command/scan/repair from placements; requirements (salvaged blocker, delivered item, riddle) | **DONE 2026-09-24**: loop steps 5-12 automated; 43/43 + 62 tooling tests ([evidence](Evidence/M7/README.md)). Riddles deferred: design question for the operator |
| M8 | Stability model + interference tiers; static recedes after repair | **DONE 2026-09-25**: static and fog visibly recede (before/after frames); the sun is up; 46/46 + 64 tooling tests ([evidence](Evidence/M8/README.md)) |
| M9 | World save of everything above (ADR-0019) | **DONE 2026-09-25**: loop steps 13-15 automated; real quit/relaunch keeps the world changed; 50/50 + 64 tooling tests ([evidence](Evidence/M9/README.md)) |
| P1 | Puzzle proof (ADR-0023): one PRESENT riddle, capped escalating hints, persisted | **DONE 2026-09-25**: 54/54 + tooling; real game plays it end to end; story-critical deferral fix ([evidence](Evidence/P1-puzzle/README.md)) |
| M10 | Building v0 (snap, small mixed-era set, simple support) + terraform v0 per the approved terrain ADR | **GREEN 2026-09-25**: navigation hard gate proven (automated, with a control and a mutation, plus the real game); shelter from inventory; demolition and collapse with full refunds; dig, raise and flatten with conservation; building on edited ground; everything persists ([evidence](Evidence/M10/README.md), [ADR-0024](ADR/0024-building-v0-structural-model.md)) |
| M11 | **Vertical slice**: avoidable creature + one demonstrated non-combat solution, Raining Cats and Dogs storm, one-room storm drain, ~40 exchanges, automated first-playable loop | full loop green headless |

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
