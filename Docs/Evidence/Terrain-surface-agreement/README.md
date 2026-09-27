# Terrain surface agreement: visible = collision = gameplay

**GREEN (2026-09-27).** Operator: "fix the HeightAt gap before P8". This is a narrow closure slice;
nothing else changed.

**The invariant (ADR-0035): for normal Gridlands heightfield terrain, the stored vertex field plus the
canonical triangle diagonal defines ONE terrain surface. Rendering, collision and gameplay queries all
represent it.**

## What changed
- **One primitive, `GLTerrainSurface`** (GridlandsCore, `Terrain/GLHeightfield.h`):
  - `Height`: the interpolation;
  - `Triangles`: the quad split.
- **`FGLHeightfield::HeightAt`** evaluates `GLTerrainSurface::Height`.
- **The render mesh and the worker-trimesh fallback** take their triangles from
  `GLTerrainSurface::Triangles` in every collision mode. The mode-dependent split
  (`RenderSplitsMainDiagonal`) is gone.
- **The Chaos heightfield's split** is fixed by Chaos. It is the independent anchor the gate compares
  against.

**Old and new interpolation** (quad corners A = (x, y), B = (x+1, y), C = (x, y+1), D = (x+1, y+1),
position FX, FY in [0, 1]):

| | Rule | Agrees with the drawn and colliding surface? |
|---|---|---|
| **Before** (bilinear) | `lerp(lerp(A, B, FX), lerp(C, D, FX), FY)` | only at vertices and on planar quads; up to ~72 cm off on steep edits (measured during ADR-0035) |
| **After** (triangle planes, split A–D) | FY ≥ FX: `A + FX·(D − C) + FY·(C − A)` (triangle A, C, D); otherwise `A + FX·(B − A) + FY·(D − B)` (triangle A, D, B) | everywhere |

- At every vertex both rules return the stored height exactly.
- The stored data is untouched.
- Saves are unchanged (below).

## Consumers audited
Every gameplay terrain-height query is `UGLTerrainSubsystem::HeightAt`, which calls
`FGLHeightfield::HeightAt`. No consumer interpolates heights itself.

| Consumer | Path | Change needed |
|---|---|---|
| Structure support, spawn base, unsupported-part checks, collapse landing (`GLCollapseRules::GroundUnder`, 9 samples) | `UGLStructureSubsystem::GroundAt` → HeightAt | none (inherits the one surface) |
| Player building: `Snap` (grounded pieces take `Ground(Aim)`), `CheckPlacement` (Buried), `RestsOnGround`, `ComputeSupport`, demolition collapse | `UGLBuildingSubsystem::GroundAt` → HeightAt | none |
| Terraform Flatten target and terraform noise position | `UGLTerrainSubsystem` → HeightAt | none |
| Vegetation and scatter placement | `AGLScatterPatch` → HeightAt | none |
| Storm ground | `UGLStormSubsystem` → HeightAt | none |
| Salvage and world placements | no terrain query (authored Z) | none |
| Creatures and pathing | navigation reads the collision export; no height query | none |
| Dev and perf commands (demo, perf, style, structure demo, terrain review) | HeightAt | none |
| `GLTerrainGen` lerp | relief-noise generation, not a surface query | none |
| Render mesh, worker trimesh | now take their split from `GLTerrainSubsystem::Triangles` | yes (one definition) |
| Tests | the agreement oracle keeps its own statement of the split and reads the mesh's actual triangles, so a defect in the primitive cannot also blind it | test only |

## Agreement proof (`Gridlands.Game.TerrainCollision`, 4 tests, required; `agreement-report.txt`)
**Scale:** 19 region checks with **44,314 probes**, of which 20,184 are per-quad probes.

**Where the probes are:**
- in every quad: the vertex, the centre, both sides of the diagonal, 1 mm either side of the diagonal,
  and (0.15, 0.85) / (0.85, 0.15);
- an off-grid 37 cm grid;
- on and 0.01–1 cm beside the seams.

**Scenes:**
- rolling natural terrain, and the steepest natural slope (31.9°) in the lots' 6 m relief;
- untouched ground;
- small, steep (4 m in 1.2 m), repeated and neighbouring digs;
- Raise, Flatten, and the 4 m dig limit reached and held;
- aggressive (30 rapid random edits);
- an edit on a seam, on a four-chunk corner, and edits crossing the west X and north Y seams;
- save/restore: a flushed reload, a streamed reload onto pooled chunks, and in-place
  `RestoreCellDelta` (to base, then the edits).

| | Maximum |
|---|---|
| **Gameplay (HeightAt) vs render (the mesh's own triangles)** | **0.000 cm** |
| **Gameplay vs collision (trace)** | **0.009 cm** |
| Render vs the authoritative field | 0.000 cm |
| Collision (sweep contact, 3 m sweeps) vs render | 0.05 cm |
| Missing ground, wrong overlaps, character capsule misses | 0 |

**Why the sweeps are 3 m:** the gate first swept 1.2 km, and one contact read 1.001 cm. Chaos sweeps a
heightfield in single precision over the whole query length, so a 1.2 km sweep stops about 1 cm early
by itself; the sphere rested at z 0.002 over ground that a trace put at −1.001. Gameplay sweeps are
short. At 3 m the error is 0.05 cm, and the 1.0 cm limit is unchanged.

## Structure and building proof (`StructuresAndBuildingStandOnTheVisibleGround`)
The proof runs on steep edited ground where the **old interpolation was 60.8 cm off** the surface.
1. **Building placement:** 24 floors snapped to the ground at the worst points sit on the visible
   ground (0.000 cm) and the colliding ground (0.002 cm).
2. **Placement rules** (RestsOnGround and Buried), on **858** level floors across the steep area at six
   heights:
   - **0** decisions differ from the rules evaluated on the visible, colliding ground;
   - **the old interpolation would have got 10 of them wrong;**
   - the building subsystem's own `Check` agrees on every Buried decision.
3. **An authored structure** (the carport) spawned at the worst point stands on the visible ground
   (0.000 cm) and the colliding ground (0.001 cm), where the old interpolation was 60.8 cm off.
4. **Determinism:** in a fresh world, the same edits give the same structure base and identical decisions.
5. **Save/restore:** after capture, unload and reload, the same snap height and identical decisions.

**Existing authored content (reported, not asserted; `NaturalTerrainIsOneSurface`):**
- **Every authored structure's base is unchanged:** they stand on vertices. The origin cell moved
  0.000 cm everywhere (flat base), and `slice_phone_table_glitched` moved 0.1 cm.
- **The lots' authored relief moved by up to 16.9 cm** somewhere in the cell.
- **Near `slice_pine_02`,** the ground within 5 m moved by up to **10.75 cm**. Its base did not move.
  If felled, it lands on the visible ground (it did not before); this is still deterministic.
- **Everything else** within 5 m of an authored structure moved ≤ 0.5 cm.
- All existing structure, collapse and building tests pass unchanged.

## Save compatibility (`save-compatibility/`)
A **pre-change save** was written by the pre-change binary (master `4930c33`): the teleport run, 25
edited vertices in the lots cell. The new binary then loaded it through the resume path and saved it
again on quit.
- **The re-saved terrain indices and deltas are identical** to the pre-change save, in both cells.
- The saved mound reads 453 cm, as before.
- **No migration:** saves store vertex deltas, and only the interpretation between vertices changed.

## HeightAt cost (`heightat/`, 4 M random queries, three runs)
| Query | ns per query |
|---|---|
| Old bilinear (a reference copy, inlined) | 10.2–10.5 |
| **Canonical `FGLHeightfield::HeightAt`** | **12.8–13.8** |
| `UGLTerrainSubsystem::HeightAt` (what gameplay calls: cell lookup plus the field) | 22.2–23.7 |

About +2.6 ns per query, measured against an inlined reference, so it overstates the difference.
**Negligible:** 10,000 queries in one frame add ~0.03 ms. No consumer's cost changed measurably (below).

## Performance and memory (quiet-run, strictly sequential; `perf/`)
Both suites `QUIETDONE PASS`, every result within `budgets.json`, no retries (end loads 3.5–5.5).
Compared with the terrain-collision closure (the same heightfield code before this change):

| Run | Streaming worst (ms) | Frame p99 (ms) | Presentation worst (ms) | Memory peak (MB) | Cell complete (s) |
|---|---|---|---|---|---|
| local straight | 5.30 → 4.64 | 4.69 → 4.70 | 1.65 → 1.66 | 3604 → 3613 | 0.40 |
| local sprint | 5.03 → 4.21 | 4.93 → 4.97 | 1.82 → 1.63 | 3603 → 3596 | 0.40 |
| local reversal | 5.53 → 4.88 | 4.78 → 4.80 | 1.91 → 2.31 | 3748 → 3728 | 0.35 |
| local teleport | 5.58 → 4.46 | 4.71 → 4.71 | 1.70 → 1.70 | 3615 → 3607 | 0.40 |
| local resume | 3.97 → 4.21 | (17.38 → 16.33)* | 1.64 → 1.63 | 2862 → 2861 | 0.79 |
| dense straight | 6.16 → 4.92 | 5.32 → 5.30 | 2.38 → 1.71 | 3609 → 3619 | 0.51 |
| dense sprint | 4.82 → 4.45 | 5.44 → 5.52 | 1.74 → 1.71 | 3601 → 3608 | 0.50 |
| dense reversal | 6.61 → 6.45 | 5.32 → 5.36 | 2.57 → 2.96 | 3739 → 3768 | 0.45 |
| dense teleport | 4.25 → 4.43 | 5.27 → 5.24 | 1.72 → 1.85 | 3614 → 3617 | 0.54 |
| dense resume | 4.02 → 4.08 | (15.25 → 14.04)* | 1.72 → 1.77 | 2896 → 2892 | 0.91 |
| roundtrips local / dense | | | | growth 77 → 63 / 66 → 67 MB | |

\* Resume p99 is its single worst frame (about 26 frames until the cell completes).

- **Everything is within run-to-run noise.** Stale results 0, emergency chunks 0, the mound persists at
  453 cm, and teleport cancels and reloads.
- **The presentation worst in reversal rose by 0.4 ms.** Scatter placement's added HeightAt cost is
  microseconds, so this is noise, within budget.

## Planted defects (`planted-defects/`, `Tools/planted-defects/terrain_collision.py`): 27/27 caught
Each defect is demonstrated on its own.

**The one surface:**

| Defect | Caught by |
|---|---|
| S1 HeightAt back to the old bilinear | agreement (4 tests) |
| S2 the wrong triangle chosen inside a quad | agreement (4) |
| S3 the diagonal reversed everywhere (HeightAt and the render split) | agreement: collision (Chaos) disagrees |
| S4 a wrong plane (barycentric term) in one triangle | agreement (4) |
| S5 exact at vertices, wrong inside (smoothstep) | agreement (4) |
| S6 building's ground bypasses the terrain query (its own nearest-vertex ground) | structure/building proof |
| S6b structures' ground bypasses the terrain query | structure/building proof |
| S7 seam quads use the other diagonal | agreement (3) |
| S8 stale gameplay ground after an edit (a remembering HeightAt) | agreement (both main tests), terraform, navigation, pool (5) |
| S9 restore leaves gameplay on the pre-restore ground while visuals and collision restore | restore-in-place checks (2) |
| H9 the render mesh split differently from collision and gameplay | agreement (4) |

**Collision and pool (existing, rerun on this code):** H1–H8, H10 and T1, T1b, T2, T4, T6, T7b, T8, all
caught.

**What the defects exposed in the tests** (these are the reason the gate is trusted):
- **H9 first survived.** The oracle interpolated mesh *vertices* with an assumed split, so it could not
  see the mesh's triangulation. It now reads the mesh's actual triangles.
- **S8 first killed the run.** An older test (`TerraformConservesProtectsAndPersists`) crashed on a
  failure instead of failing. It now fails cleanly, and the tool reports a dead run as "RUN DIED", never
  as a result.
- The full run caught 26/27 before the terraform-test hardening; S8 was then run and caught on its own
  (`summary-full-run-1.txt`, `summary.txt`).
- Not counted separately: S2 and S3 are distinct (one triangle's plane vs the whole split), and S6 and
  S6b are distinct consumers.

## Regression
- **Tests:** 119/119, 42 requirements met (`00-full-gate.out.txt`); tooling self-tests; data validation
  (223 entities).
- **Fresh clone:** see the PR.

## Remaining terrain debt (unchanged)
- **Extreme 1 m edits look harsh:** terraforming presentation debt
  (`../Terrain-heightfield-spike/diagonal-review`, pair 05).
- **The render mesh** is ~2.33 MB per chunk, ~80% of terrain memory. Not optimized.
- **The ~90 ms synchronous whole-cell load** on restart and jumps (field generation).
- **Measurement-only collision modes** (0, 2) are not gated. In a full-suite run, mode 0/2 sweeps
  showed a 24.8 cm contact outlier in the rapid-edits step that the canonical heightfield does not
  show. Recorded, not investigated (not canonical).
