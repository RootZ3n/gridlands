# P7.1 evidence: visual-direction refinement spike

**Status: APPROVED (operator, 2026-09-26). Variant A is the canonical look** and the game's
default: dimensional, highly colourful, WildStar-leaning, no environment outlines.
- B and C were rejected and are kept only as the review record. Characters remain unapproved
  proxies.
- P7 remains GREEN at its historical checkpoint (`p7-visual-green`), and `playtest-baseline-2` is
  untouched.

**Purpose** (operator, 2026-09-26): keep P7's engineering and its colour, but move from "a
2D/cartoon illustration rendered in 3D" toward **"a richly stylized 3D game world"**.
- WildStar is the primary reference; RuneScape: Dragonwilds is secondary.
- **Colourful does not mean flat.** The principle and the limits are in
  [VISUAL-DIRECTION](../../VISUAL-DIRECTION.md).

## How to review
- **Compare:** `comparisons/<view>.jpg`, one 1920x1080 sheet per view in four quadrants: **P7
  (accepted reference)**, **A**, **B**, **C**.
- **Full size:** `screenshots/<P7|A|B|C>/<view>.jpg` at 1600x900.
- **Same scene, same cameras:**
  - The P7 frames were rendered from the P7 code (`p7-visual-green`) in a throwaway worktree. It
    was patched only to add the two new camera positions (08, 09), so every view has a P7
    counterpart.
- **Reproduce:**
  - `Tools/art.sh`, then `Tools/p7-review.sh --variant A|B|C --evidence DIR`;
  - in game: `gl.Style.Variant P7|A|B|C`.

| # | View | Required |
|---|---|---|
| 01 | day overview (also `01b` without the stylize pass) | 1 |
| 02a / 02b | dusk / night overview (also `02c` night without the stylize pass) | 2 |
| 08 | closer structure view (new) | 3 |
| 09 | vegetation and terrain detail (new); 06 terrain after a real dig and raise | 4 |
| 04, 05, 05b | creature, Zenny and Pehlichi readability | 5 |
| 03, 04 | NICE corruption (one cube on the phone, three on the creature) | 6 |
| 07a / 07b | collapse and felled pine in the new style (unchanged gameplay) | — |

## The variants (presentation over the same assets, `gl.Style.Variant`)
| | Outlines | Light banding | Lighting |
|---|---|---|---|
| **P7** | everything outlined, with interior creases (reference) | on | P7 presets |
| **A** | environment none; characters and creatures light (0.85) | off | stronger warm key, cooler fill, sun-coloured atmospheric glow, deeper AO; saturation +0.1 |
| **B** | environment **silhouettes only** (no interior creases), lighter (0.8), fading 15–45 m; characters 0.9 | off | as A |
| **C** | environment none; characters faint (0.55) | off | "grounded": warmer, hazier sun glow, softer exposure; P7's saturation kept |

**A, B and C share all the new geometry and materials.** They differ only in outlines, banding and
light, so a choice between them is cheap to change later.

## What changed from P7 (A, B and C)
**Geometry** (`Art/Source`; same sizes, pivots and data boxes):
- **Pine:** a curved trunk with root lobes. Drooping, scalloped tier skirts, each ringed with lumpy
  foliage clusters: volume and self-shadow, not paper cones.
- **Rocks:**
  - chiselled flat facets on a smooth form;
  - moss painted on the up-facing faces;
  - satellite pebbles;
  - a more saturated lavender palette.
- **Grass:** 14 blades per tuft, subdivided and arcing outward, not flat cards. **Flowers:** cupped
  petals and curved stems. **Bush:** 7 rounder clumps.
- **1950s storefront kit (streamline moderne):**
  - rounded pilasters and stepped cornices;
  - chunky chrome window frames with a mullion;
  - planters;
  - an awning swelling into a barrel canopy with a curled, scalloped front;
  - a rounded sign board with a chrome rim;
  - bigger bevels throughout.
- **Props:** smoother, with more segments. **Characters:** smoother subdivision only (still **style
  proxies**).

**Materials:** a new painted master with six material classes as instances: painted, plastic,
metal, stone, wood and foliage.
- **Baked ambient occlusion** comes from the recipes (vertex alpha): creases, the underside of the
  awning and the base of trunks darken.
- **Procedural surface detail** in world space varies colour and roughness at two scales: stone
  speckle, vertical wood grain.
- **Per-instance hue and value** for scattered vegetation.
- **Metal and plastic** get real specular response.

**Terrain:** more lawn hues (a teal "lush" tone), cooler slope transitions, banded rock, speckled
earth, micro variation and roughness variation.

**NICE's corruption:** brighter electric blue with crisper, thinner edges, so the precise
intrusion stands out more against the richer world. Still the engine's exact cube, never outlined,
at most 4 per visual.

**Lighting (A/B/C):**
- a stronger, warmer key against a cooler sky fill;
- a sun-coloured directional glow in the height fog, for layering;
- screen-space AO stronger and tighter;
- day bloom slightly lower, so neon still reads at night;
- saturation kept or raised.

## What did not change (architecture)
- **Visuals are presentation only:** collision, support, salvage and all gameplay stay on the data
  (ADR-0024, ADR-0030).
- **Unchanged systems:**
  - multi-frame presentation with saved state before gameplay (ADR-0033);
  - streaming;
  - the structure contract;
  - persistence;
  - the outline architecture (still opt-in per visual; now with categories).
- **No data changes:** no `visual.*` or placement changes; the composition is P7's.

## Performance
All measured on the reference machine (RX 6800, 1920x1080) with the machine quiet (CPU load < 3,
GPU idle before each run).
- Runs spoiled by other sessions, or by my own overlapping runs, were discarded and repeated.
- Their logs record the load.

**Style slice cost** (`gl.Perf.Style`, the day overview, medians; `perf/style-*.json`):

| Build / variant | Frame | GPU | Game thread | Stylize pass | Vegetation | Shadows |
|---|---|---|---|---|---|---|
| P7 code | 3.66 ms | 3.24 ms | 1.41 ms | 0.15 ms | 0.04 ms | 0.46 ms |
| P7.1 A | 3.86 ms | 3.44 ms | 1.38 ms | 0.16 ms | 0.15 ms | 0.44 ms |
| P7.1 B | 3.84 ms | 3.44 ms | 1.38 ms | 0.16 ms | 0.14 ms | 0.44 ms |
| P7.1 C | 3.86 ms | 3.44 ms | 1.37 ms | 0.17 ms | 0.15 ms | 0.45 ms |

- **The richer style costs +0.20 ms GPU in the slice**, mostly vegetation (0.04 → 0.15 ms: grass
  tufts went from 108 to 504 triangles).
- The variants cost the same: they differ in post and light parameters, not in content.

**Triangles:** 14,348 → 38,804 over the 24 proof assets (`perf/triangles.json`).
- Rocks ×9 (180 → 1,680).
- Storefront walls ×3.5–5.4.
- Awning ×6.
- Pine ×3.7.
- Grass ×4.7 (instanced).

**P5 regression budgets with the P7.1 assets** (`perf/`; P7 final → **P7.1**, budget in
brackets):

| Run | Worst frame | p99 | Streaming worst (budget 12) | GPU mean | Peak memory MB |
|---|---|---|---|---|---|
| local straight | 23.4 → **19.8** (30) | 4.9 → **5.0** | 7.20 → **8.56** | 3.39 → **3.44** | 4486 |
| local sprint | 18.0 → **18.0** (30) | 8.7 → **8.8** | 6.92 → **7.74** | 3.42 → **3.48** | 4504 |
| local reversal | 29.7 → **33.8** (40) | 8.4 → **8.5** | 7.07 → **9.05** | 3.38 → **3.43** | 4589 |
| local teleport | 24.5 → **25.4** (40) | 5.6 → **5.7** | 9.70 → **6.79** | 3.37 → **3.42** | 4377 |
| local resume | 14.2 → **17.2** (25) | 13.6 → **13.5** | 6.78 → **7.96** | 4.63 → **4.69** | 3320 |
| local round trips | memory growth 70 → **76** MB (400) | | | | 4331 |
| dense straight | 23.3 → **22.3** (30) | 5.0 → **5.5** | 8.55 → **7.89** | 3.46 → **3.73** | 4483 |
| dense sprint | 18.9 → **18.3** (30) | 9.0 → **9.0** | 8.93 → **11.87** | 3.49 → **3.75** | 4529 |
| dense reversal | 36.2 → **36.8** (40) | 8.8 → **8.8** | 8.16 → **7.33** | 3.42 → **3.58** | 4578 |
| dense teleport | 24.7 → **24.6** (40) | 5.6 → **6.1** | 6.85 → **6.89** | 3.42 → **3.60** | 4365 |
| dense resume | 13.6 → **13.4** (25) | 12.3 → **12.4** | 6.57 → **6.84** | 4.65 → **4.70** | 3324 |
| dense round trips | memory growth 54 → **76** MB (400) | | | | 4368 |

**All 13 results are within budget** (`check_budgets.py`).
- The tightest is dense sprint's streaming worst, 11.87 ms. That frame is the P5 terrain's first
  `SetMesh` of a fresh chunk actor (11.4 ms, logged), not presentation.
  - Presentation worst is unchanged: 1.56 → 1.62 ms in dense sprint, 1.87 → 1.88 in dense straight.
  - The scheduled warm chunk pool addresses that frame.
- The authoritative-layer frame rose by ~0.9 ms (4.2 → 5.1 ms, dense sprint). The heavier creature
  and prop visuals are made in that frame.
- **No pathological cost, and no conflict with a budget.**

## Closure gates (variant A as the default; `perf-final/`)
These were rerun after the approval, on the final default, with the machine quiet:
- **109/109 tests pass** (40 requirements).
- **12 of 13 P5 results pass on the first run.** The exception is dense sprint: 15.25 ms streaming
  worst against 12.
  - Its slow-frame log names a single fresh terrain chunk's first `SetMesh` (14.4 ms).
    Presentation in that frame was 0.01 ms.
  - This is the known P5 terrain spike, and the warm-pool task that follows targets it.
  - Rerun twice on a quiet machine: 6.84 and 6.64 ms (`dense-sprint-rerun*.json`).

| Run | Worst frame | p99 | Streaming worst | Peak MB |
|---|---|---|---|---|
| local straight | 23.4 | 4.9 | 6.72 | 4482 |
| local sprint | 18.0 | 8.7 | 6.41 | 4507 |
| local reversal | 35.1 | 8.4 | 7.01 | 4572 |
| local teleport | 24.1 | 5.7 | 6.76 | 4355 |
| local resume | 15.3 | 13.4 | 7.22 | 3332 |
| dense straight | 23.2 | 5.5 | 6.88 | 4482 |
| dense sprint | 23.4 | 8.9 | 6.64 | 4528 |
| dense reversal | 38.4 | 8.8 | 7.47 | 4548 |
| dense teleport | 25.4 | 6.4 | 10.43 | 4371 |
| dense resume | 13.3 | 12.6 | 6.82 | 3333 |

## Tests
- **109/109 automation tests pass** (40 requirements; `30-full-gate.index.json`), on the P7.1
  assets.
- **Tooling self-tests and data validation pass** (223 entities).
- **All 24 assets import and validate** (bounds, pivot, triangles, slots, vertex colour).
  Validation caught two real issues on the way:
  - replaced meshes keeping stale material slots, fixed with clean reimports;
  - a bevel larger than half a board's thickness producing degenerate triangles.

## Pipeline implications if this direction is selected
- **Adopting a variant is cheap.** A, B and C share all content; the choice is a style preset.
  `gl.Style.Variant` and the presets would be folded into the shipping defaults.
- **Material classes become the vocabulary:** every recipe part names its class (paint, plastic,
  metal, stone, wood, foliage). New eras add classes (marble, lacquer, brass, canvas, ice) as
  instances, not new masters.
- **Baked AO is part of every export.** It is automatic, and costs ~9 s of Blender time for the whole
  kit.
- **LODs become a requirement** before production density. Rocks, walls and vegetation are 3–9×
  heavier; the instanced grass is the first candidate (LOD or impostor).
- **Textures are not required yet.** Procedural detail and vertex colour carry this look. A texture
  and normal-map path (stylized tiling detail, trim sheets) is the natural next step for
  hero-quality surfaces, and needs UVs from the recipes.
- **Hand-authored meshes fit the same contract:** named class slots, AO in vertex alpha, split
  normals, bottom-centre pivot, manifest.
- **Recipe-built geometry has a ceiling.** Code recipes reached this proof, but production assets
  at this richness want an artist-facing authoring path. That was already recorded as pipeline
  maturity debt.
- **Global illumination is not in this proof.** The project runs without dynamic GI. Lumen, or a
  baked fill, would add depth (bounce light, contact darkening) and is a project-wide lighting and
  performance decision, not a spike decision.

## Architectural conflicts
**None found.** The richer style needed no change to gameplay, streaming, persistence or
structural contracts.

## Fresh clone
**PASS.** A fresh clone of `bcf3e72` (P7.1 with variant A as the default) built and passed
**109/109** tests (40 requirements), from tracked inputs, including the LFS art, plus the pinned
engine (`00-*`).
