# P2 evidence: full-cell terrain scaling

**Question:** can the approved chunked runtime heightfield (ADR-0022) plausibly support Grid cells
of about 1 km on this machine, and what limits should the architecture assume?

## Method
Harness: `gl.Perf.Terrain <metres> [spacing] [chunk]` (`Source/GridlandsGame/Private/Debug/GLPerfCommands.cpp`),
driven by `Tools/perf-terrain.sh`. Each size ran in a fresh game process, in the real origin map
(houses, glitches, Zenny, Pehlichi), with real rendering.

**Machine and render settings**

| | |
|---|---|
| Machine | Linux, Vulkan, AMD Radeon RX 6800 (RADV), Intel i7-11700K, 32 GB RAM |
| Rendering | 1920×1080 offscreen, vsync off, frame cap off |
| Engine | Unreal 5.8.3 |

**The ground is representative, not a best case.** It is authored rolling hills: four value-noise
octaves at about ±15 m, plus steep ridges, and a new per-vertex authored base. It is built through
the real `UGLTerrainSubsystem` path:
- dynamic mesh chunks;
- complex-as-simple collision;
- navigation-relevant geometry;
- the cell's runtime navigation bounds, with Recast `RuntimeGeneration=Dynamic`.

**What is measured:**
1. Build: mesh plus collision.
2. A full navmesh build over the whole cell.
3. Memory: process used-physical, before and after each stage.
4. 400 frames from a low view looking across the whole cell ("ground").
5. 400 frames from a steep overlook ("overlook").
   - For both views: frame time, game thread, render thread and GPU time.
6. 60 random dig and raise strokes: each stroke's cost, including chunk rebuild, collision and
   navigation dirtying.
7. 8 single strokes, timed from the edit until navigation has fully settled.
8. Saved-delta size.
9. The time to restore that delta.

## Results (1920×1080)

| file | cell | spacing | chunk | chunks | tris | res | build s (mesh/coll) | nav build s | +mem MB (terrain/nav) | ground fps / GT / RT / GPU ms | overlook fps / GPU ms | edit ms mean/p95 | nav update ms mean/max | reload s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| terrain-256m.json | 256 m | 1 m | 64 m | 16 | 0.13 M | 1920x1080 | 0.19 (0.10/0.08) | 0.26 | 58 (16/42) | 317 / 1.41 / 2.13 / 2.74 | 315 / 2.76 | 11.7 / 13.6 | 31 / 46 | 0.17 |
| terrain-512m.json | 512 m | 1 m | 64 m | 64 | 0.52 M | 1920x1080 | 0.76 (0.42/0.32) | 1.07 | 258 (111/148) | 292 / 1.71 / 2.40 / 2.95 | 287 / 3.04 | 12.9 / 23.3 | 32 / 50 | 0.71 |
| terrain-1024m.json | 1024 m | 1 m | 64 m | 256 | 2.10 M | 1920x1080 | 2.98 (1.66/1.27) | 4.40 | 998 (538/460) | 286 / 1.59 / 2.32 / 3.03 | 253 / 3.48 | 12.1 / 12.8 | 33 / 50 | 2.77 |
| terrain-1024m-s1-c128.json | 1024 m | 1 m | 128 m | 64 | 2.10 M | 1920x1080 | 3.30 (1.84/1.45) | 7.78 | 1016 (583/433) | 280 / 1.63 / 2.23 / 3.10 | 243 / 3.66 | 51.7 / 56.0 | 171 / 299 | 3.12 |
| terrain-1024m-s2-c64.json | 1024 m | 2 m | 64 m | 256 | 0.52 M | 1920x1080 | 0.74 (0.41/0.30) | 3.76 | 297 (113/183) | 306 / 1.88 / 2.64 / 2.75 | 287 / 2.95 | 3.8 / 4.4 | 30 / 48 | 0.69 |
| terrain-2048m.json | 2048 m | 1 m | 64 m | 1024 | 8.39 M | 1920x1080 | 11.88 (6.59/5.06) | 20.18 | 3956 (2245/1710) | 259 / 1.82 / 2.75 / 3.31 | 256 / 3.44 | 12.7 / 13.6 | 31 / 52 | 11.46 |

Raw results: `terrain-*.json` in this folder.

## What the evidence says

**Rendering is not the constraint.**
- One whole cell costs about 3 ms of GPU time at 1080p, from 0.13 M to 8.4 M triangles, even with
  no terrain LOD.
- Frame rate stays at 250 FPS or more from 256 m to 2 km.
- The game and render threads stay under 3 ms.
- LOD can wait until real materials, foliage or weaker target hardware demand it.

**Memory is the constraint, and it scales linearly with vertex count.**
- At 1 m spacing a cell costs about 0.5 GB of terrain per km² (dynamic-mesh CPU copy, render
  buffers and Chaos trimesh collision), plus about 0.45 GB of navmesh per km².
- A 1 km cell is **about 1 GB**; a 2 km cell is about 4 GB.
- At 2 m spacing the same 1 km cell costs 0.3 GB.

**Load time scales the same way.**
- A 1 km cell takes about 3 s to build and 4.4 s for navigation.
- That is fine behind a load screen, but a seamless neighbour load would hitch today. Building
  and navigation happen all at once.

**Runtime edits are local and constant.**
- A stroke costs about 12 ms per 64 m chunk touched, and navigation settles about 30–50 ms later,
  at every cell size.
- 128 m chunks make each edit about 4× more expensive (52 ms, and 171 ms for navigation), so
  **keep 64 m chunks**.

**Saves are small.**
- About 15 bytes per edited vertex (7 KB for 60 strokes).
- Restoring rebuilt every chunk: 2.8 s at 1 km. It needs to rebuild only the chunks the delta
  touches, and on cell load the delta should be applied before the first build. This is fixed as
  part of P3.

## Limits the architecture should assume (until measured otherwise)
- About 1 GB per loaded 1 km cell at 1 m spacing. Budget for the loaded set: the cell you are in,
  plus neighbours near a boundary.
- Chunk builds must be **amortised across frames** (about 11 ms per 64 m chunk) for seamless
  streaming.
- Navigation will need a limit before large cells stream seamlessly. Options:
  - navigation only around agents (invokers);
  - coarser navmesh cells;
  - navigation only in authored "places".
- The levers that would make 1 km at 1 m spacing cheap, none of them implemented (no premature
  optimisation):
  - heightfield collision instead of trimesh;
  - releasing the CPU mesh copy after build;
  - coarser spacing away from built-up areas.
