# CLAUDE.md: Gridlands

Instructions for AI agents working in this repo. Keep them accurate. Base
changes on what is actually in the codebase.

## What this is

Gridlands is an **Unreal Engine 5.8.3** (prebuilt Linux binary) single-player
third-person survival / salvage / building / exploration game with a C++
gameplay core and JSON-first content. Zenny (silent) is trapped in a world run
by NICE, its AI Game Master; progression is radial toward her core. The defining mechanic: **only Pehlichi, the AI squirrel companion,
repairs simulation glitches.** The player scans via Pehlichi, satisfies
requirements, and protects it. The in-game Pehlichi has nothing to do with the
real Pehlichi lab agent and must never call any model, API or lab service.

Current phase: **GRIDLANDS_BOOTSTRAP**. Check `Docs/MILESTONES.md` for what is
built. Anything on the out-of-scope list in `Docs/ARCHITECTURE.md` section 8
is refused, not started.

## Read before changing code

`Docs/DESIGN-BIBLE.md` -> `Docs/GLOSSARY.md` -> `Docs/ARCHITECTURE.md` ->
`Docs/GLITCH-AND-PEHLICHI.md` -> topic docs -> `Docs/ADR/` ->
`Docs/HANDOFF.md` (invariants + recipes).

## Commands (repo root)

```sh
Tools/doctor.sh      # environment vs pin (Tools/engine-pin.env)
Tools/selftest.sh    # tooling tests, no engine
Tools/build.sh       # compile GridlandsEditor, Linux Development
Tools/test.sh        # headless automation; PASS only by parsed report
Tools/verify-fresh-clone.sh  # reproducibility proof from a clean clone
```

The last output line is `RESULT: PASS|FAIL|ENV ...`. Exit status alone is
never evidence (ADR-0008). Zero tests run is a failure.

## Layout

- `Source/GridlandsCore/`: pure rules and types, no world dependency. Tests
  are in `Private/Tests/`.
- `Source/GridlandsGame/`: primary game module; actors, components,
  subsystems, one folder per system.
- `Source/GridlandsEditor/`: editor-only; JSON importer/validator (M2).
- `Data/`: JSON source of truth for content (ADR-0002).
- `Content/`: binary assets (LFS). Agents do not edit `.uasset`/`.umap`.
- `Config/`: engine/game ini, gameplay tags (text).
- `Docs/Legacy/`: the retired Godot prototype's design docs, for reference only.

## Rules

- Naming: `GL` prefix on all game types (`AGLPehlichi`, `UGLGlitchComponent`).
- Glitch transitions only through `FGLGlitchLifecycle`; never add a player
  repair interaction.
- The game must stay completable without combat (ADR-0009): never make a kill,
  a boss or a mob drop required for progression.
- Never add a repair-count lock or an invisible wall (ADR-0011); never store
  stability/interference/NICE composure as counters (ADR-0013); never spawn
  threat from routine activity (ADR-0014); never call dialogue from gameplay
  code; emit events instead (ADR-0015).
- Do not use the word "zone"; say cell, band or era (ADR-0012, `Docs/GLOSSARY.md`).
- No GAS. No gameplay logic in Blueprints.
- Close the editor before `Tools/build.sh` (Live Coding and hot reload
  corrupt Blueprints).
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`
  or the engine.
- Record an ADR for any decision that is expensive to reverse.
