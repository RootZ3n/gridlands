# P7 evidence: Gridlands visual language and authoring pipeline spike

**Status:**
- **P7_ENGINEERING: GREEN.** The pipeline is repeatable, the systems work in the styled scene,
  performance is measured, tests pass and the fresh clone succeeds.
- **P7_VISUAL: AWAITING OPERATOR.** Nothing here approves the look.
- **Final P7 GREEN is not awarded.**

**Setup:**
- Engine: Unreal 5.8.3, Linux, Vulkan.
- Hardware: RX 6800, i7-11700K, 1920x1080. Development evidence, not a minimum specification.
- Decision record: [ADR-0032](../../ADR/0032-visual-pipeline-and-stylization.md) (Proposed).
- Guides: [ART-PIPELINE](../../ART-PIPELINE.md), [STRUCTURE-AUTHORING](../../STRUCTURE-AUTHORING.md),
  [VISUAL-DIRECTION](../../VISUAL-DIRECTION.md), [DUNGEONS-AND-LEGENDARIES](../../DUNGEONS-AND-LEGENDARIES.md) (P7-M).

**Reproduce:**
- `Tools/art.sh`: source → meshes, materials and validation. Needs Blender 5.2; the imported assets
  are committed.
- `Tools/p7-review.sh --evidence`: the screenshots.
- `Tools/test.sh`: automation.
- `Tools/perf-crossing.sh -t`: the P5 budgets, plus round trips.
- In game, `gl.Perf.Style`: the stylization cost.

## Screenshots (`screenshots/`, final binary, 1280x720 JPG)
| # | Required view | File |
|---|---|---|
| 1 | Day | `01-day-overview.jpg`; the same frame without the stylize pass: `01b-day-without-stylize-post.jpg` |
| 2 | Darker colorful | `02a-dusk-overview.jpg`, `02b-night-overview.jpg`; night without the stylize pass: `02c-night-without-stylize-post.jpg` |
| 3 | Ordinary vs corrupted prop | `03-prop-ordinary-vs-corrupted.jpg`: the rotary telephone, and its twin with ONE electric-blue cube |
| 4 | Ordinary vs corrupted creature | `04-creature-ordinary-vs-corrupted.jpg`: the raccoon, and its twin with 3 cubes (the gremlin's look) |
| 5 | Zenny and Pehlichi readability | `05-zenny-pehlichi-readability.jpg`, `05b-zenny-pehlichi-front.jpg` |
| 6 | Terrain after manipulation | `06-terrain-after-manipulation.jpg`: a real dig and a raise; the exposed earth has no vegetation |
| 7 | Collapse and debris | `07a-collapse-in-progress.jpg`, `07b-collapse-debris-and-felled-pine.jpg`: the awning falls when its posts are taken; the pine is felled |

The collapse and felling in 7 are the real structural pipeline (ADR-0030), triggered by salvaging
the supports and the stump in the running game: deterministic plans, debris and salvage. They are
not staged animation.

## What was built (P7-A … P7-J)
**Pipeline** (P7-I, [ART-PIPELINE](../../ART-PIPELINE.md)):
- Blender recipes: 24 assets from one shared stylization module.
- FBX plus a manifest.
- `GLImportArt`: generates 5 master materials and the post pass, imports, and validates bounds,
  pivot, triangles, slots and vertex colour.
- `visual.*` data (26 definitions; VIS-1/VIS-2).
- `GLVisuals::Attach` at runtime.
- The import report for the committed assets: **24 assets, 0 failures.**

**What validation caught:**
- the scale was 100× too small, then 100× too large;
- import options were being ignored;
- colours were gamma-encoded twice, which turned the whole palette pastel.

**Scene** (P7-A):
- stylized terrain, through masks;
- scatter vegetation: grass, flowers, bushes, 1565 instances;
- boulders and 2 pines;
- a 16-part 1950s storefront (world-only kit pieces), with a neon sign as a light;
- the rotary telephone and its glitched twin;
- the raccoon and its corrupted twin;
- Zenny and Pehlichi proxies;
- day, dusk and night presets.

**Colour** (P7-B):
- a saturated palette painted into vertex colours;
- dusk: a pink key with a violet-cyan fill;
- night: a blue-violet moon key, sky light and lit signs;
- saturation raised, never lowered.

**Shape** (P7-C): chunky bevels, exaggerated proportions and seeded irregularity, shared by the
1950s kit, the timber kit, the props and the characters.

**Outlines** (P7-D, ADR-0032): a custom-depth-gated post pass with depth and normal edges, distance
fade, and optional light banding. The alternatives and limitations are in the ADR.

**Corruption** (P7-E):
- engine cubes in an unlit electric-blue material;
- never outlined, never irregular;
- VIS-2 limits a visual to at most 4 cubes of at most 0.35 m.

**Characters** (P7-F/G): static proxies. Zenny has a big head and hands and a teal jacket. Pehlichi
is a round service drone with an antenna and a glowing eye.

**Structure authoring** (P7-J): the storefront proves the contract does not prevent visual
authoring. The exporter requirements and the limits are in
[STRUCTURE-AUTHORING](../../STRUCTURE-AUTHORING.md): quarter-turn, axis-aligned support; no load;
module grid.

## Compatibility (P7-H), in the styled scene
| System | Evidence |
|---|---|
| 1 m terrain, deformation, dirt | Screenshot 6: a real dig and raise through `Terraform`; exposed-earth masks; vegetation re-plants off dug ground (`VegetationFollowsTheGround`: 400 tufts → 330 after the pit, none on dug ground, all on the surface) |
| 64 m chunks, streaming | Chunk vertex masks are rebuilt with every chunk. The P5 budget suite and the round trips run with the full P7 content (below) |
| Part graphs, collapse, debris | Screenshot 7; `VisualsCarryNoCollisionAndFollowCollapse` (the visual follows the authoritative pose to rest, collision stays on the data boxes); `Tools/p6-acceptance.sh` A–T rerun with visuals (below) |
| Tree felling | Screenshot 7 (the slice pine); the P6 `tree` run |
| Building pieces | The timber kit's buildpieces carry visuals; ghosts keep the blockout (no visual) |
| Persistence | Visuals are never saved: they are derived from the data on spawn. `CollapsePersistsThroughStreamingSavesAndRestart` passes with visuals |

## Performance (P7-K)
**Stylization cost** (`perf/style.json`, the slice at 1080p, medians):

| Config | Frame | GPU | Game thread |
|---|---|---|---|
| Full | 3.63 ms | 3.24 ms | 1.31 ms |
| Stylize post off | 3.49 ms | 3.10 ms | 1.31 ms |
| Vegetation off | 3.60 ms | 3.20 ms | 1.31 ms |
| Corruption off | 3.63 ms | 3.23 ms | 1.30 ms |
| Shadows off | 3.08 ms | 2.78 ms | 1.29 ms |

- **The whole outline and banding pass costs 0.14 ms GPU.** Outline and banding share one shader,
  so they are not separable.
- Vegetation costs 0.04 ms (1565 instances); corruption is within noise.
- **Shadows are the largest single cost (0.46 ms)** and are kept: the look needs them.

**P5 regression budgets with P7** (`perf/`):

| Run | Worst frame (P6 → P7, budget) | p99 | Streaming game thread worst (budget 12) | Peak memory |
|---|---|---|---|---|
| Straight | 23.6 → 25.4 ms (30) | 4.5 → 4.9 ms | 9.3 → 10.2 ms | 4480 MB |
| Sprint | 18.4 → 22.9 ms (30) | 8.6 → 8.7 ms | 8.8 → 10.4 ms | 4503 MB |
| Reversal | 37.0 → 36.1 ms (40) | 8.2 → 8.4 ms | 10.0 → 10.6 ms | 4543 MB |
| Teleport | 25.3 → 26.3 ms (40) | 5.0 → 5.6 ms | 9.4 → 10.8 ms | 4352 MB |
| Resume | 12.8 → 14.0 ms (25) | 12.3 → 11.9 ms | 6.8 → 6.4 ms | 3317 MB |

- **Round trips** (new, `local-roundtrips.json`): 8 each way, memory 4267 → 4337 MB after the first round trip. Growth 112 MB (budget 400).
- **Emergency chunks:** 0 in every run.
- **Navigation:** localized (initial navigation 0.50 s).

**Found and fixed on the way** (details in ADR-0032):
- **The first P7 run breached the budget in all six runs.** The causes, all fixed without removing
  anything visual:
  - synchronous art loading in the streaming frame;
  - a no-op whole-heightfield restore;
  - hidden blockout boxes that were still getting render state.
- **Remaining margin is thin:** straight and teleport streaming worst is 9.7–11.4 ms in three
  repeats, against a 12 ms budget.
  - The largest single cost is the storefront's spawn.
  - More authored buildings per cell will need the runtime layer spread over frames. That is a
    persistence-ordering decision, left to the operator.

## P6 acceptance with visuals (`p6-acceptance/`)
**`Tools/p6-acceptance.sh` passes on the final binary with every structure visualized**: collapse,
kill, creature, tree, edge and noise.

**The restart runs report the same facts as P6:**
- the carport's posts are removed, its decks are debris at rest, and Zenny's health is 40;
- the pine's stump is removed and the log is gathered;
- the edge carport and edge pine are at their rests after relaunching in the neighbouring cell.

## Automation (`30-full-gate.index.json`)
**105 tests, all green** (39 requirements).

**New tests:**
- `Gridlands.Game.Presentation.VisualsCarryNoCollisionAndFollowCollapse`
- `…VegetationFollowsTheGround`
- `…CorruptionIsSparseAndPrecise`
- core `LandsOnlyWhereItsCentreIsSupported` (the collapse sliver fix)

**Tooling:** VIS-1/VIS-2 validator tests and the round-trip budget test.

## Planted defects (`mutations/`: diff plus report; each built and run in isolation)
| Defect | Caught by |
|---|---|
| M1 a visual carries collision | visuals-are-presentation, corruption |
| M2 a corruption cube is outlined | corruption is sparse and precise |
| M3 grass grows on dug earth | vegetation follows the ground |
| M4 a cell's saved ground is skipped on restore (the new fast path made too eager) | terraform persists |
| M5 a visual ignores the collapse pose | visuals-are-presentation |
| M6 unload leaves vegetation behind | streaming never loses or duplicates (`GLScatterPatch 4 -> 20`) |

## Defects found in P7
1. **Import scale and ignored import options:** validation failed the run; fixed.
2. **Double gamma, which made colours pastel:** linear export; fixed.
3. **Nondeterministic recipe seeds:** Python `hash()` replaced with `crc32`.
4. **A falling slab landed on a 10 cm sliver of a wall top:** it now needs its centre of mass over
   the surface. A core test covers it.
5. **The P5 budget breach:** fixed (above).
6. **The OOM killer ended the full gate, and a masked memory check was exposed** (ADR-0032).
   - Test worlds are now collected on teardown: the peak dropped from 21.8 GB to ~7 GB.
   - The round-trip test now counts live objects.
   - Real-game memory is budgeted by `roundtrips`.
7. **The mutation harness kept a mutant binary** (the restored source was older than the object).
   Found because the gate reproduced M6 exactly. The harness now touches restored files, and every
   mutation was rerun.

## Remaining debt
- **The visual direction is unapproved.** Every colour, the outline width and the proportions are
  provisional.
- **Character proxies are static meshes.** Semantic reactions (P7-F) need rigs and animation.
- **Vertex colour is the only colour source.** Faces, labels and patterns need geometry or
  textures.
- **Recipes are code.** There is no artist-facing authoring yet, though hand-made sources fit the
  same export and validation.
- **No LODs; no per-asset triangle budgets as data.**
- **Translucency and telegraph interaction with outlines is unproven:** no such effects exist yet.
- **The streaming margin is thin** (above).
- **The mid-fall save is still GAMEPLAY CONSISTENCY DEBT** (P6).
- **Unreal-side modular assembly and export is not built** (STRUCTURE-AUTHORING), and neither is
  exported floor support.

## Fresh clone
**PASS.** A fresh clone of `1fb9845` built and passed **105/105** tests (39 requirements) from
tracked inputs, including the LFS art content, plus the pinned engine (`00-*`). Blender is not
needed: the imported assets are committed.
