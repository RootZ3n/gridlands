# P7 evidence: Gridlands visual language and authoring pipeline spike

**Status: P7 GREEN** (awarded at closure, see the end of this page).
- **Visual direction: APPROVED by the operator, 2026-09-26** ([VISUAL-DIRECTION](../../VISUAL-DIRECTION.md)).
  Zenny, Pehlichi and the creature remain style proxies.
- **Engineering:** the pipeline is repeatable and the systems work in the styled scene.
- **Closure work the operator required:** multi-frame cell presentation, authoritative state first
  ([ADR-0033](../../ADR/0033-multi-frame-cell-presentation.md)), proven against a dense authored
  stress fixture ([below](#dense-spawn-proof-adr-0033)).

**Setup:**
- Engine: Unreal 5.8.3, Linux, Vulkan.
- Hardware: RX 6800, i7-11700K, 1920x1080. Development evidence, not a minimum specification.
- Decision records: [ADR-0032](../../ADR/0032-visual-pipeline-and-stylization.md) (Accepted) and
  [ADR-0033](../../ADR/0033-multi-frame-cell-presentation.md) (Accepted).
- Guides: [ART-PIPELINE](../../ART-PIPELINE.md), [STRUCTURE-AUTHORING](../../STRUCTURE-AUTHORING.md),
  [VISUAL-DIRECTION](../../VISUAL-DIRECTION.md), [DUNGEONS-AND-LEGENDARIES](../../DUNGEONS-AND-LEGENDARIES.md) (P7-M).

**Reproduce:**
- `Tools/art.sh`: source → meshes, materials and validation. Needs Blender 5.2; the imported assets
  are committed.
- `Tools/p7-review.sh --evidence`: the screenshots.
- `Tools/test.sh`: automation.
- `Tools/perf-crossing.sh -t`: the P5 budgets, plus round trips.
- `Tools/perf-crossing.sh -d`: the same runs with the dense fixture (dev only, `-GLDenseProof`).
- `Tools/p7-dense-proof.sh`: real-game persistence of the dense fixture across a restart.
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

## Dense-spawn proof (ADR-0033)
**The operator's question:** does the spawning architecture scale when a cell holds many buildings,
props and structural parts rather than one storefront?

**The fixture** (`GLDenseProof`, dev only, `-GLDenseProof`):
- in-memory placements in the diner lots, never `Data/` or shipping;
- 84 structures, **308 structural parts** (the lots' authored content is 26 parts: ~12×):
  - 10 1950s storefronts (16 parts each);
  - 10 carports;
  - 24 pines;
  - 20 phone tables;
  - 20 boulders;
- 8 vegetation patches (5,600 instances);
- every site passes the authored-structure support rule, re-checked by
  `DenseFixtureStandsAndPresentsCompletely`.

**Architecture** (ADR-0033):
- The authoritative model and the saved state are resolved in one frame.
- Structure part actors and vegetation are presented over the following frames: nearest first,
  1.5 ms per frame, only under a 6 ms streaming-frame ceiling, and everything within 20 m at once.
- On unload, gameplay leaves at once; retired actors are destroyed within the budget.

### Measured, the same P5 harness and budgets (`perf/dense-*`, final binary, quiet machine)
Final binary. Every run started with CPU load < 3 and the GPU idle (`perf/perf-quiet-*.log`).
Columns: P6 → P7 before ADR-0033 → **P7 final** → **P7 final with the dense fixture**.

| Run | Worst frame (budget) | p99 (budget) | Streaming game thread worst (budget 12) |
|---|---|---|---|
| straight | 23.6 → 25.4 → **23.4** → **23.3** (30) | 4.5 → 4.9 → **4.9** → **5.0** (7) | 9.31 → 10.24 → **7.20** → **8.55** |
| sprint | 18.4 → 22.9 → **18.0** → **18.9** (30) | 8.6 → 8.7 → **8.7** → **9.0** (11) | 8.76 → 10.37 → **6.92** → **8.93** |
| reversal | 37.0 → 36.1 → **29.7** → **36.2** (40, the accepted ~34 ms unload hitch) | 8.2 → 8.4 → **8.4** → **8.8** (11) | 10.01 → 10.59 → **7.07** → **8.16** |
| teleport | 25.3 → 26.3 → **24.5** → **24.7** (40) | 5.0 → 5.6 → **5.6** → **5.6** (8) | 9.35 → 10.82 → **9.70** → **6.85** |
| resume | 12.8 → 14.0 → **14.2** → **13.6** (25) | — | 6.79 → 6.40 → **6.78** → **6.57** |

**Dense presentation** (straight / sprint / reversal / teleport / resume):

| Measure | Value |
|---|---|
| presentation per frame, worst | 1.87 / 1.56 / 1.69 / 1.74 / 0.88 ms (budget 1.5 + one unit) |
| presentation per frame, mean (frames that presented) | 0.44–0.56 ms |
| units presented | 346 per load (308 parts + 8 patches + the lots' own 30); reversal 1038 over 3 loads |
| frames over which a load is presented | ~210–230 (under the 6 ms ceiling) |
| authoritative layer frame (the lots' model, placements and saved state), worst | 3.75–4.67 ms (no presentation in that frame) |
| lots authoritative ready | 0.10 s after load start (resume 0.55 s, a launch) |
| lots fully presented | 2.43–2.51 s (the same time as their ground); 1.3–1.4 s on reloads |

**Memory and emergencies:**
- **Memory peak:** 4.31–4.55 GB, the same as the normal game's (budget 5.2 GB).
- **Round trips** (8 each way, real game): memory growth after the first round trip is 54 MB with
  the dense fixture and 70 MB without (budget 400).
- **Emergency chunks:** 0 in every run.

**An A/B** on the same machine, back to back (P7 before ADR-0033 against final, straight crossing,
two rounds): frame mean, p99 and GPU are identical; streaming worst goes from 10.5–10.7 to 6.9–7.2
ms (`perf/ab/`).

**Earlier runs made while other sessions loaded the machine breached p99 and worst-frame budgets**
(load 5–19, the GPU shared with video playback), in both the normal and the dense game.
- One such run showed a 15.4 ms streaming frame, which was a single terrain chunk.
- The A/B above shows those were the machine's, not the build's.
- The harness now records the machine's load with every result.

### What the fixture exposed and what was fixed
- **Vegetation checked every structure part in the world for every tuft.** One patch cost ~10 ms,
  and presentation frames hit 12 ms. It now collects nearby footprints once (`CollectFootprints`).
- **Unload destroyed ~300 actors in one frame** (6.4 ms, and one 18 ms streaming frame). Actors are
  now retired (inert at once) and destroyed over frames, and the unload frame is ~3 ms.
- **Remaining streaming spikes are the P5 terrain's**, not presentation's. Every freshly spawned
  64 m chunk actor's first `SetMesh` costs ~5 ms, and 9–15 ms when other work loads the machine.
  - That is the same in the normal game (instrumented: 513 of 513 first builds; `Grid: slow
    streaming frame` log lines name the step).
  - Presentation never adds to such a frame: it only uses what is left under the 6 ms ceiling.

### Correctness, automated (`Gridlands.Game.Streaming`, 4 tests)
| Proof | Test |
|---|---|
| Authoritative state before presentation: at the first frame of a reload, the model already holds every kept fact while 300+ units still wait; **every frame** of a 0.05 ms-per-frame presentation (hundreds of frames) shows no resurrected part, no duplicate, no displaced debris | `PresentationNeverRunsAheadOfSavedState` |
| Save mid-presentation keeps every fact; restart from that save: the same facts, one actor per present part, no collapse event | same |
| No gameplay events and no impacts from streaming | same, and `UnloadDuringPresentationCancelsCleanly` |
| Unload mid-presentation, 3 cycles: waiting work cancelled, made actors retired (inert, hidden, no collision, refuse salvage), no stale unit lands afterwards, retired actors destroyed within the budget, **no live object class grows**, and the facts survive | `UnloadDuringPresentationCancelsCleanly` |
| A collapse decided while its parts wait to be presented: a part presented mid-fall shows the plan's pose, not solid; one never presented until after landing appears as debris at its rest; one collapse event, each impact once | `ACollapseDecidedBeforePresentationShowsTheDecision` |
| The fixture stands, and presents completely (one actor per part, vegetation too) | `DenseFixtureStandsAndPresentsCompletely` |

### Real game (`Tools/p7-dense-proof.sh`, `dense-proof/`)
With `-GLDenseProof`:
1. Zenny damages the fixture through the real salvage pipeline:
   - a carport's two posts, so both decks fall;
   - a pine's stump, so the trunk topples;
   - a boulder;
   - a storefront's three awning posts;
   - then one deck's debris, salvaged.
2. The game quits (autosave) and relaunches from that save, starting inside the dense lots.

| | Before quit | After restart |
|---|---|---|
| dense structures | 84 | 84 |
| changed facts (digest of the lots' structure facts) | 12 (`83671a12`) | 12 (`83671a12`) |
| present parts / part actors | 300 / 300 | 300 / 300 |
| duplicates / resurrected | 0 / 0 | 0 / 0 |

`RESULT: PASS`. Logs are in `dense-proof/`.

**Worst-frame note (honest).** The straight route's worst frame is a GPU-bound frame at x = 352 m.
- Streaming there is 0.01 ms, the game thread 2.6 ms and the render thread 2.9 ms.
- It has been the worst frame since P5 (22.3 ms in P5, 23.6 in P6).
- With the dense fixture it measured 23.3, 25.3 and 24.1 ms on a quiet machine. The normal game
  measured 23.4–27.4 there.
- It reached 34.5 ms once, in a sanity run where the normal game also rose (budget 30).
- It is not streaming or presentation work, and it is recorded as debt.

## Automation (`30-full-gate.index.json`)
**109 tests, all green** (40 requirements). Peak editor memory for the whole suite: 7.0 GB.

**New in P7:**
- `Gridlands.Game.Presentation`:
  - `VisualsCarryNoCollisionAndFollowCollapse`;
  - `VegetationFollowsTheGround` (now also pins the layout seed to the id's text);
  - `CorruptionIsSparseAndPrecise`.
- `Gridlands.Game.Streaming` (ADR-0033, the dense fixture):
  - `DenseFixtureStandsAndPresentsCompletely`;
  - `PresentationNeverRunsAheadOfSavedState`;
  - `UnloadDuringPresentationCancelsCleanly`;
  - `ACollapseDecidedBeforePresentationShowsTheDecision`.
- Core `LandsOnlyWhereItsCentreIsSupported` (the collapse sliver fix).
- The round-trip streaming test now asserts that no live object class grows.

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
| M7 presentation makes a part the saved state says is removed (a zombie) | never runs ahead of saved state; unload cancels cleanly |
| M8 debris is presented where the part stood, not at its authoritative rest | the same two |
| M9 a retired actor (its cell unloaded) still accepts salvage and stays solid | unload cancels cleanly |
| M10 presentation ignores its per-frame budget | never runs ahead (it must take many frames); unload cancels cleanly |

## Defects found in P7
1. **Import scale and ignored import options:** validation failed the run; fixed.
2. **Double gamma, which made colours pastel:** linear export; fixed.
3. **Nondeterministic recipe seeds:** Python `hash()` replaced with `crc32`.
4. **A falling slab landed on a 10 cm sliver of a wall top:** it now needs its centre of mass over
   the surface. A core test covers it.
5. **The P5 budget breach on first styling:** fixed (ADR-0032).
6. **The OOM killer ended the full gate, and a masked memory check was exposed** (ADR-0032).
   - Test worlds are now collected on teardown: the peak dropped from 21.8 GB to ~7 GB.
   - The round-trip test now counts live objects.
   - Real-game memory is budgeted by `roundtrips`.
7. **The mutation harness kept a mutant binary** (the restored source was older than the object).
   Found because the gate reproduced M6 exactly. The harness now touches restored files, and every
   mutation was rerun.
8. **Vegetation scanned every structure part in the world, per tuft.** The dense fixture made one
   patch cost ~10 ms. It now uses nearby footprints, once per patch.
9. **Unloading a dense cell destroyed every part actor in one frame** (6.4 ms). Actors are now
   retired at once and destroyed within the budget (ADR-0033).
10. **Vegetation layout was not deterministic across builds.**
    - The seed was `GetTypeHash(FName)`, the name's index in the process's name table, so the same
      patch grew differently in another build.
    - Found by comparing review frames across builds; the seed is now the id's text.
    - Every other part of the frames matches the approved ones within noise (below).
11. **Measurement, not code: other sessions and video playback shared the machine.** Several perf
    runs breached p99 and worst-frame budgets in both the normal and the dense game.
    - An A/B against the pre-ADR-0033 build showed identical frame times.
    - The final evidence was run on an idle GPU and quiet CPU (`perf/perf-quiet-*.log`), and every
      result log records the machine's load.

## Visual presentation unchanged (`visual-unchanged/`)
The final review frames are compared with the approved ones (3e7efcd).
- **Rendered with vegetation hidden in both,** they differ by 0.15–2.76 (mean absolute grey
  difference out of 255). That is at or below the run-to-run noise of one build (0.6–2.9).
- **As rendered,** only the vegetation layout differs, because its seed was fixed (defect 10).
- The screenshots in `screenshots/` are the final build's.

## Remaining debt
- **Characters and creature are style proxies** (operator): static meshes, no faces, no rigs, no
  emotes. Production characters need silhouette design, expression, animation and semantic
  emotes, in the approved language.
- **Vertex colour is the only colour source.** Faces, labels and patterns need geometry or
  textures.
- **Recipes are code.** There is no artist-facing authoring yet, though hand-made sources fit the
  same export and validation.
- **No LODs; no per-asset triangle budgets as data.**
- **Outlines are unevaluated for translucency, sight cones, telegraphs, particles, water,
  projectiles, markers and placement ghosts.** None exist yet; each is to be evaluated before it
  is outlined (operator).
- **Terrain: a fresh chunk actor's first `SetMesh` costs ~5 ms** (9–15 ms under load). This is now
  the largest streaming cost, from P5; a warmed chunk pool is the obvious remedy.
- **A GPU-bound frame at x = 352 m on the straight route** has been its worst frame since P5 (22–27
  ms). Denser content in view adds ~1–2 ms, and once 34.5 ms. It is uninvestigated: something
  entering view; a GPU capture is the next step.
- **Actor-state placements** (creatures, glitches, salvage nodes) are still made in the
  authoritative frame. They need a model/actor split before they become dense (ADR-0033).
- **The mid-fall save is still GAMEPLAY CONSISTENCY DEBT** (P6), untouched by ADR-0033.
- **Unreal-side modular assembly and export is not built** (STRUCTURE-AUTHORING), and neither is
  exported floor support.
- **Locked design intent recorded, not built:**
  - death without corpse runs, where failure teaches;
  - use-based mastery without grind, kept separate from Chukka, Ofi and Hoponi
    ([SURVIVAL-AND-THREAT §4, §4a](../../SURVIVAL-AND-THREAT.md)).

## Fresh clone
**PASS.** A fresh clone of `b199ff2`, the final P7 code, built and passed **109/109** tests (40
requirements).
- It used only tracked inputs, including the LFS art content, plus the pinned engine (`00-*`).
- Blender is not needed: the imported assets are committed.
- The earlier clone of `1fb9845` (before closure) also passed, with 105/105.

## P7 closure
- **Visual direction:** approved by the operator.
- **Engineering:**
  - the pipeline is repeatable;
  - the systems work in the styled scene;
  - multi-frame presentation holds the authoritative-state-first invariant against a 12× dense
    fixture;
  - the P5, P6 and P7 regression gates pass;
  - memory is bounded;
  - save, restart and cancellation are proven;
  - the fresh clone passes.
- **P7 GREEN.** The mid-fall save remains GAMEPLAY CONSISTENCY DEBT, as the operator directed.
