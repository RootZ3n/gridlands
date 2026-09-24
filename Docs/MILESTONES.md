# Milestones: GRIDLANDS_BOOTSTRAP

Each milestone ends green on `Tools/test.sh` from a fresh clone, or it isn't done.
The operator reviews after M1 before anything further begins.

| # | Milestone | Proof | Status |
|---|---|---|---|
| M0 | Engine 5.8.3 prebuilt + git-lfs installed; `Tools/doctor.sh` | doctor passes | git-lfs done; engine awaits operator download |
| M1 | Skeleton: 3 modules, tooling, docs, ADRs, one real headless test | `build.sh` + `test.sh` green from a fresh clone | written; blocked on M0 engine |
| M2 | JSON -> DataAsset importer/validator | data tests green, deterministic re-import | - |
| M3 | Character, interaction, `L_TestNeighborhood` | interaction spec + manual walk-around | - |
| M4 | Salvage + inventory | specs | - |
| M5 | Fabrication: one recipe, one primitive tool, efficiency gating | spec: the tool salvages faster | - |
| M6 | Pehlichi commands/scan/repair + glitch lifecycle in the world | lifecycle, command-rejection, capability specs | - |
| M7 | Save/load of all of the above | round-trip + migration spec | - |
| M8 | One snap-socket build piece with Damaged->Intact repair; one passive creature | specs + visible in map | - |
| M9 | First-playable loop as one automated functional test; handoff pass | full loop green headless | - |

## First playable loop (M9 functional test)

1. Player enters the test neighborhood.
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
