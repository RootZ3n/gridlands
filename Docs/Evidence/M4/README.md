# M4 evidence: salvage, inventory, world-settings yields

Verified commit: `8b98e845247b214312d9fa916f992290e851a54a`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-8b98e84.index.json` | fresh clone: validate (59 entities), build, **28/28** tests, 0 warnings, tree unchanged | PASS |
| `02-mutation.diff` + `02-mutation-e1-scale-everything.index.json` | scaling *every* yield category (ADR-0016 broken) fails `YieldScalingFollowsCategory` with 111 errors | rejected |

New tests:
- `Gridlands.Core.Inventory.StacksWeightAndCapacity`: stack sizes, slot limits
  (overflow refused, not lost), remove across stacks, overencumbrance is allowed
  and exact.
- `Gridlands.Core.Economy.YieldScalingFollowsCategory`: every real category ×
  every preset plus extreme multipliers. Non-scalable is never scaled; scalable
  gives max(1, round). Rare repeatable scales; glitch rewards don't.
- `Gridlands.Core.Salvage.ToolEfficiencyAndGating`: junk pile 6→3 hits, fence
  4→2, wiring 3→2 with the pry bar; required-tool gating with a reason; tools
  below minTier get no bonus.
- `Gridlands.Game.Salvage.CompletesGrantsYieldsAndEvents`: 3 hits; 5 copper
  wire and 1 fuse; `Event.Salvage.Completed`, data-driven
  `Event.Salvage.WireStripped`, `Event.Item.Acquired`; the node is gone and
  can't be salvaged twice.
- `Gridlands.Game.Salvage.WorldSettingsScaleRepeatableYields`: the relaxed
  preset doubles scrap (4 → 8); unknown presets are refused.
- `Gridlands.Game.Salvage.PryBarSalvagesFaster`: 6 hits bare vs 3 with the pry
  bar, same yield.
- `Gridlands.Game.Inventory.OverencumbranceIsAnnouncedOnce`: announced on
  crossing, not repeated, re-announced after relief.
- `Gridlands.Game.Placement.SpawnsSalvageNodesFromData`: the origin's three
  salvage placements spawn at their transform or anchor (+ offset) and carry
  their placement ids.

Found during M4: a unity-build name collision in test helpers that only a clean
build exposed. Fixed; `Tools/build.sh` now passes `-DisableAdaptiveUnity`, so
every build groups files like a clean clone.
