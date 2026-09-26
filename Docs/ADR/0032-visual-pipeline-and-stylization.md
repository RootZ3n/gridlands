# ADR-0032: Visual pipeline and stylization architecture (P7 spike)

- Status: **Proposed by the P7 spike. Engineering proven; visual direction AWAITING OPERATOR REVIEW.**
  The operator decides whether the style is correct. Nothing here approves the look.
- Date: 2026-09-26
- Evidence: [Docs/Evidence/P7-visual-spike](../Evidence/P7-visual-spike/README.md). Pipeline guide:
  [ART-PIPELINE.md](../ART-PIPELINE.md). Authoring requirements:
  [STRUCTURE-AUTHORING.md](../STRUCTURE-AUTHORING.md).

## Decision (what the spike built and proposes to keep)
1. **Source to runtime is one repeatable command** (`Tools/art.sh`):
   - Blender recipes (the source of truth);
   - FBX plus manifest;
   - an import commandlet that also generates the master materials;
   - validation against the manifest.
2. **Presentation is data (`visual.*`), never gameplay.**
   - A visual is attached as collision-free, navigation-free components.
   - Collision, support, salvage and every gameplay answer stay on the authoritative data
     (ADR-0024, ADR-0030).
   - Build pieces and creatures reference visuals; Zenny and Pehlichi use fixed visual ids.
3. **Colour is painted into vertex colours** from a saturated palette:
   - a small set of master materials (painted, glow, glass, corruption, terrain);
   - variants by parameter (tint), not by new assets.
4. **Stylized ground reads masks, not colours.**
   - Terrain chunks write masks: flatness, exposed earth, variation.
   - `M_GLTerrain` owns the palette.
   - Dug or raised ground shows exposed earth, and vegetation will not grow there.
5. **Outlines are a post-process, opted into per visual.**
   - `PP_GLStylize` finds depth and normal edges only where an outlined object is.
   - "Outlined" means the object writes custom depth: `r.CustomDepth=3`, `visual.outline`.
   - Lines fade with distance.
   - Optional light banding quantises lit surfaces.
6. **NICE's corruption is a protected language:**
   - engine cubes (mathematically exact) in `M_GLCorruption` (unlit electric blue, crisp edges);
   - never outlined, never irregular;
   - sparse by validation (VIS-2: at most 4 cubes of at most 0.35 m per visual).
7. **Lighting presets (day, dusk, night) keep colour in the dark:** coloured key and fill, and a
   coloured fog tint. Fog density stays the stability system's.

## Outline techniques evaluated

| Technique | Verdict |
|---|---|
| **Post-process edges gated by custom depth (chosen)** | **One screen pass: 0.14 ms GPU for the whole stylize pass at 1080p on the RX 6800.** Works for any mesh, including future skeletal characters (custom depth is per component). Terrain and grass opt out, which avoids noise; the silhouettes of outlined objects against terrain still draw. Normal creases give selective internal lines through a threshold. |
| Inverted hull (a back-face-scaled copy of each mesh) | Not built. It is one extra draw per mesh, needs closed meshes, gives thickness control per object and is independent of resolution. Kept as a candidate for hero characters if the operator wants thicker, controlled lines. |
| Toon shading model / Substrate stylization | Not used. UE 5.8 has no stock toon shading model; a custom shading model means engine changes. |

## Limitations (honest)
- **Translucency.** Translucent effects are not outlined. Outlines of opaque objects are composed
  before bloom, so lines can show through translucent surfaces in front of them, such as future
  telegraphs and VFX.
- **Line width** is in pixels: constant on screen, relatively thinner up close. Lines fade out
  between 35 and 90 m.
- **Thin geometry** (fences, wires) can alias.
- **Light banding** uses a lighting-ratio trick. It is skipped on unlit surfaces and the sky, and
  can band oddly on strongly coloured light. It is one parameter, and can be switched off.
- **Outlines and banding share one shader,** so their individual costs are not separable (both are
  inside the 0.14 ms).
- **Character proxies are static meshes.** Semantic reactions need a rig later.
- **Vertex colour is the only colour source.** Fine detail (faces, labels, patterns) needs
  geometry, or textures later.

## Streaming (the P5 budget, P7-K)
When the styled content first went in, the P5 regression budget broke in all six runs:
- straight: worst frame 87.7 ms, streaming game thread up to 77 ms;
- the other runs: streaming game thread 25 ms, against a 12 ms budget.

Found and fixed without removing anything visual:
1. **Art loaded synchronously inside the cell's runtime-layer frame.** `GLVisuals::Preload` now
   loads every visual's mesh and material once at world start (29 objects, ~50 ms, before the first
   cell) and keeps them resident.
2. **A no-op ground restore.** A streaming cell re-applies the ground delta it captured a moment
   earlier, and doing that copied, reset and compared the whole 1 km heightfield (~5 ms).
   `RestoreCellDelta` now returns at once when the ground already is that delta. A planted defect
   (M4) shows the persistence test still guards the real restore.
3. **Build-piece blockout boxes that the art hides** are now registered hidden and unpainted,
   instead of getting a render state and a material instance that were thrown away.

**After the fixes, all six runs are within budget.**
- Straight and teleport, three repeats: streaming game thread worst 9.7–11.4 ms (P6 reference
  9.3).
- The margin is thin, and **the storefront's 16 parts are the largest single cost** (~5–7 ms for
  its cell's structure spawn).
- More authored buildings per cell will need the runtime layer spread over frames. That changes
  when saved state is applied relative to spawning, so it is a persistence decision and not made
  here.

## Memory across round trips (found while gating P7)
**The full test gate was killed by the kernel OOM killer** (the editor reached 21.8 GB). Test worlds
were destroyed but never garbage-collected, so every test's 1 km world stayed resident until exit.
`FTestWorld` now collects on teardown: the suite's peak is now ~7 GB.

**That exposed a pre-existing masked check.** `StreamingNeverLosesDuplicatesOrReplays` asserted
bounded *process memory* across 5 round trips.
- Run alone, it fails on P6 master too (4254 → 6397 MB, measured in a worktree at 4197b19). It grows
  ~0.5 GB per round trip, linearly, with no UObject growth: 1148 classes, counts identical.
- Ticking the world, flushing rendering, and removing scatter and visuals did not change it.
- Skipping chunk collision halved it: it is the physics and render scenes' deferred release, which a
  non-ticking editor automation world never performs.
- It passed in the old suite only because the test's own GC freed the earlier tests' worlds and the
  allocator reused that memory.

**The real game does not grow.** `gl.Perf.RoundTrips` makes 8 round trips between the cells: memory
levels off (+143 MB after the first round trip, ~5 MB per move and falling).

**Resolution:**
- The test now asserts what an automation world can measure: **no live object class grows** across
  round trips. Planted defect M6, where unload leaves vegetation behind, is caught
  (`GLScatterPatch 4 -> 20`).
- The real game's memory is budgeted in the P5 harness: `Tools/perf-crossing.sh roundtrips`,
  `memGrowthMb <= 400`.

## Consequences
- **Adding an asset** means a recipe, one command, a visual definition and a reference. There is no
  per-asset Unreal work.
- **The look is concentrated in a handful of masters and one post pass.** Restyling after the
  operator's review changes those, not hundreds of assets.
