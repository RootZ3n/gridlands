# Handoff: inheriting Gridlands as an autonomous coding agent

Read in this order: `CLAUDE.md` (repo root), `Docs/ARCHITECTURE.md`,
`Docs/GLITCH-AND-PEHLICHI.md`, `Docs/DESIGN-PILLARS.md`, `Docs/ADR/`,
`Docs/MILESTONES.md`.

## Roles (Pehverse lab)

- **Ikbi (mechanic)** changes C++ under `Source/` and JSON under `Data/`. It
  never edits `.uasset`/`.umap` files.
- **Abaiya (challenger)** challenges diffs against the invariants below.
- **Acceptance** is `Tools/test.sh` passing on a clean checkout. Self-written
  acceptance is not acceptance.

## Checks (bare executables, run from the repo root)

| Command | Needs engine | Meaning |
|---|---|---|
| `Tools/doctor.sh` | reports if absent | environment matches the pin |
| `Tools/selftest.sh` | no | tests the tooling itself (report parser) |
| `Tools/build.sh` | yes | compiles `GridlandsEditor` (Linux, Development) |
| `Tools/test.sh` | yes | headless automation; pass only by parsed report (ADR-0008) |
| `Tools/verify-fresh-clone.sh [ref]` | yes | clone, build and test from tracked inputs + pinned engine only |

Exit codes: 0 pass, 1 check failed, 2 environment/usage problem (engine
missing, wrong version). The last line of output is always
`RESULT: PASS|FAIL|ENV <reason>`.

## Invariants a change must not break

1. Only `UGLRepairComponent` can move a glitch to `Repaired` (ADR-0005).
2. The glitch transition table lives only in `FGLGlitchLifecycle`.
3. Content is JSON-first (ADR-0002); Blueprints contain no gameplay logic.
4. Core has no world/actor dependency.
5. No network, model or lab calls from game code (ADR-0006).
6. No GAS (ADR-0003).
7. The save schema version bumps with a migration and a round-trip test.

## Recipes

- **Add a test:** put it next to the code in `Private/Tests/`, name it
  `Gridlands.<Layer>.<System>.<Case>`, and add it to `Tools/required-tests.txt`
  when it guards an invariant.
- **Add content (from M2):** edit `Data/<kind>/*.json`, run
  `Tools/import-data.sh`, then `Tools/test.sh`.
- **Add a glitch requirement kind:** subclass `UGLGlitchRequirement` in C++,
  register its JSON `kind`, add a spec, document it in
  `GLITCH-AND-PEHLICHI.md`.

## Known lab-side follow-ups (outside this repo)

- Johnny project PRJ-0048 maps to this repository. Its description predates the
  Unreal pivot.
- Ikbi's game-studio module validates Godot only; it needs an Unreal validator
  that calls `Tools/test.sh`.
- First compiles are long; budget agent turns and timeouts accordingly.
- Any agent campaign on this repo obeys the lab's DeepSeek cost window.
