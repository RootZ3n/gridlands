# Handoff: inheriting Gridlands as an autonomous coding agent

Read in this order: `CLAUDE.md` (repo root), `Docs/DESIGN-BIBLE.md`,
`Docs/GLOSSARY.md`, `Docs/ARCHITECTURE.md`, `Docs/GLITCH-AND-PEHLICHI.md`,
`Docs/DESIGN-PILLARS.md`, the topic docs (`WORLD-AND-PROGRESSION`,
`STORY-AND-DIALOGUE`, `SURVIVAL-AND-THREAT`, `BUILDING-SALVAGE-TERRAIN`),
`Docs/ADR/`,
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
| `Tools/selftest.sh` | no | tests the tooling itself (report parser, data validator) |
| `Tools/data.sh validate` | no | validates `Data/` against the id/tag grammar and invariants (rule codes in `Docs/CONTENT-IDS-AND-TAGS.md`) |
| `Tools/data.sh generate` | no | regenerates `Config/Tags/GeneratedFromData.ini` from era/band/yield data |
| `Tools/export-anchors.sh` | yes | writes `Data/anchor/<cell>.generated.json` from each cell's map (ADR-0018) |
| `Tools/build-blockout.sh` | yes | regenerates the origin blockout map from its C++ generator, then exports anchors |
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
8. Non-combat completion (ADR-0009, amended by ADR-0011): no content may
   make a kill the only way to advance. No combat-only critical-path item,
   knowledge or capability; combat-free stabilization must make each next
   band traversable; no required boss.
9. Grid lines define regions, not surfaces (ADR-0010). Never use "zone": say
   cell, band or era (ADR-0012).
10. No repair-count locks or invisible walls; interference is the barrier (ADR-0011).
11. Stability, interference and NICE composure are derived, never stored (ADR-0013).
12. Routine activity never summons threat (ADR-0014).
13. Dialogue is authored JSON selected by the director; systems emit events and
    never call dialogue directly (ADR-0015).
14. Every yield passes through its world-settings category (ADR-0016).

15. **Pehlichi deals zero direct damage** (ADR-0017). No capability, item or
    balance change may give him one. That needs a new operator ADR.
16. Gameplay placement is JSON per cell; never put gameplay logic or
    gameplay-critical placement only in a `.umap` (ADR-0018).
17. Ids and tags follow `Docs/CONTENT-IDS-AND-TAGS.md`; eras are data (ADR-0020).
18. All progression lives in the world save (ADR-0019).

The full invariant table is in `Docs/ARCHITECTURE.md` section 8.

## Recipes

- **Add a test:** put it next to the code in `Private/Tests/`, name it
  `Gridlands.<Layer>.<System>.<Case>`, and add it to `Tools/required-tests.txt`
  when it guards an invariant. **Unity builds merge .cpp files**: never define
  file-local helpers with generic names (`Flags`, `FTestWorld`, `Tag`). In
  GridlandsGame use `Tests/GLTestUtils.h`, and give anything file-specific a
  unique name.
- **Add content:** create `Data/<kind>/<domain>/<name>.json` whose `id` mirrors
  the path, run `Tools/data.sh generate` (if you added an era, band or yield) and
  `Tools/data.sh validate`, then `Tools/test.sh`.
- **Add a field to a content kind:** add it to the kind's schema in
  `Tools/gldata/schema.py` **and** to the matching struct in
  `Source/GridlandsCore/Public/Content/GLContentDefinitions.h` (the JSON key is the
  property name with a lower-case first letter; use `double` for numbers). Run
  `Tools/data.sh generate`, which refreshes `Data/_registry/schema.generated.json`,
  then `Tools/test.sh`. `Gridlands.Core.Content.SchemaKeysMatchValidator` fails
  if either side is missing the field.
- **Add a content kind:** register it in `Data/_registry/kinds.json`; add a
  schema in `schema.py`, a struct in `GLContentDefinitions.h` and a row in
  `KindStructs` (`GLContentRegistry.cpp`); document it in
  `Docs/CONTENT-IDS-AND-TAGS.md` section 2; then generate and test.
- **Change the origin blockout (until art takes the map over):** edit
  `Source/GridlandsEditor/Private/Commandlets/GLBuildBlockoutCommandlet.cpp`, run
  `Tools/build.sh` then `Tools/build-blockout.sh` (it regenerates
  `Content/Gridlands/Maps/L_Origin.umap` and re-exports anchors), then `Tools/test.sh`.
- **Anchors changed in a map:** run `Tools/export-anchors.sh`;
  `Gridlands.Editor.Anchors.ExportMatchesCommitted` fails until you do.
- **Change key bindings:** edit `AGLCharacter::BuildInput` (input is C++, not assets).
- **React to gameplay:** subscribe to an `Event.*` tag on `UGLEventSubsystem`, and emit
  events with `UGLEventSubsystem::Emit`. Never call another system's listener directly.
- **Add salvage to the world:** write `Data/salvage/<domain>/<name>.json` (yields
  name a `yield.*` category; optional `onSalvageEvents` for dialogue hooks) and a
  `salvage_node` placement in `Data/placement/<cell>/` anchored to a visual actor or at a
  transform. The node spawns at play; salvaging hides the anchored visual.
- **Add a glitch to the world:** write `Data/placement/<cell>/<name>.json`
  naming a `glitch.*` definition and an anchor (see
  `Data/anchor/<cell>.generated.json`) or a cell-local transform; run the
  importer and the tests.
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
