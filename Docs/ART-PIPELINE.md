# Art pipeline (P7 visual spike)

Status: **proven for the P7 style slice**; **not yet a production pipeline.**
- **The canonical look is P7.1 variant A** (operator, 2026-09-26; section 4a). The P7 direction is
  superseded.
- **LODs are required before production density.**
- **The P7.1 assets are the approved visual LANGUAGE, not the production quality target**
  ([VISUAL-DIRECTION](VISUAL-DIRECTION.md), operator clarification 2026-09-26).
  - **Open art and pipeline debt:** the procedural P7.1 proofs fall short of the WildStar-like
    target in authored geometric detail, silhouette sophistication, texture and material richness,
    environmental, vegetation and prop density, architectural personality, character and animation
    quality, and polish.
  - Production refinement starts from P7.1 and moves toward more WildStar-like richness: never back
    toward the P7 cartoon look, never toward photorealism.
  - Closing the gap likely needs artist-authored sources (hand-made meshes, textures or trim sheets)
    alongside the recipes. The export and validation contract already accepts them.
- Decision record: [ADR-0032](ADR/0032-visual-pipeline-and-stylization.md).

## Goal
**Every asset is produced from source by one repeatable command.** It is validated before runtime,
and the game reads it through data. No asset is hand-fixed inside Unreal.

## The flow

```
Art/Source/*.py (Blender recipes: the source of truth)
   |  blender -b --python Art/Source/build_assets.py
   v
Saved/Art/Export/<Name>.fbx + manifest.json (triangles, bounds, pivot, material slots)
   |  UnrealEditor -run=GLImportArt (Tools/art.sh runs both steps)
   v
/Game/Gridlands/Art/Materials/  master materials (regenerated every run)
/Game/Gridlands/Art/Meshes/SM_* imported, slots bound by name, validated against the manifest
   |  Data/visual/*.json (visual.*: mesh, tint, outline, corruption cubes, light)
   v
runtime: GLVisuals::Attach on build pieces, structure parts, creatures, Zenny, Pehlichi, scatter
```

**One command rebuilds everything:** `Tools/art.sh` (add `--preview` for Blender contact renders).
It fails on any validation problem.

## 1. Source (Blender recipes, `Art/Source`)
**`gl_art.py` holds the shared stylization**, so every asset gets the same language by
construction:
- **Chunky forms:** generous bevels. Recipes choose exaggerated proportions: oversized trims, big
  hands and feet, thick roofs.
- **Illustrated imperfection:** a seeded, size-relative vertex irregularity.
  - Deterministic: seeds are stable, and `zlib.crc32` is used, never Python's randomised `hash`.
  - **Never applied to NICE's corruption.** Corruption is the engine's exact cube.
- **Painted colour baked into vertex colours:**
  - a palette colour per part;
  - a top-to-bottom painted gradient;
  - up-facing faces a little brighter;
  - small per-face variation.
  - No textures are needed at this stage. Textures can be added later without changing the flow.
- **Pivot and units:** bottom-centre pivot (the buildpiece/structure convention), metres.
- **Material slots name the master material:**
  - `GL_Painted`: vertex-colour stylized;
  - `GL_Glow`: emissive (signs, Pehlichi's eye);
  - `GL_Glass`.
- **Axes:** Blender +Y becomes Unreal −Y. A piece's front faces Unreal −Y, so recipes put fronts at
  Blender +Y.

## 2. Export
- FBX in metres (`FBX_SCALE_NONE`): Unreal converts to centimetres.
- Colours are exported **linear**. The importer gamma-encodes once; sRGB here doubled it and
  turned every colour pastel (defect found in P7).
- A manifest records the triangles, bounds and slots Blender produced.

## 3. Import and validation (`GLImportArtCommandlet`)
**It regenerates the master materials first:**
- `M_GLPainted`: vertex colour × `Tint`, a fresnel rim, soft specular;
- `M_GLGlow`;
- `M_GLGlass`;
- `M_GLCorruption`: unlit electric blue, crisp face-UV edges, a slow scan;
- `M_GLTerrain`: vertex masks plus a painterly world-space palette;
- `PP_GLStylize`: outlines and light banding (see ADR-0032).

**Then, for each asset, it:**
1. imports with fixed settings: static mesh, no materials or textures, vertex colours replaced, no
   collision, no lightmap UVs, no Nanite;
2. binds slots to the masters by name;
3. validates, and **fails the run** on any problem:
   - **bounds** equal the manifest (converted), within 1.5 cm;
   - **pivot** is the bottom centre;
   - **triangles** equal the manifest, within 5%;
   - **material slots** are exactly the manifest's;
   - **vertex colours** are painted, not all white;
4. **builds the LODs (P8)** the mesh's data budget declares (`visual.*.lod`), with the engine's
   reduction, and **fails the run** if any LOD is over its triangle budget (see §4b).

It writes `Saved/Art/Export/import-report.json`, with a row per LOD (triangles, budget, screen size).

**What validation caught in P7:**
- the scale 100× too small, then 100× too large;
- import options being ignored;
- the triangle loss caused by the scale.

## 4. Metadata and runtime (`visual.*`, ADR-0032)
**A visual is presentation only.** Collision, support, salvage and gameplay stay on the
authoritative data: `buildpiece` shapes and sockets, `structure` parts, creature definitions.

**Validator rules:**
- **VIS-1:** the mesh is an imported art mesh.
- **VIS-2:** NICE's corruption stays **sparse**: at most 4 cubes per visual, each at most 0.35 m.
- **VIS-3 (P8):** the LOD budget is well formed (§4b).
- **VIS-4 (P8):** a visual that a scatter placement instances declares a `cullDistance`.
- **VIS-5 (P8):** visuals of the same mesh declare the same LOD budget (LODs belong to the mesh).

**Variants are data:**
- a `tint` makes a dynamic instance of the master, with no new asset;
- `corruption` adds cubes to any visual;
- a variant is a second visual on the same mesh, for example `visual.prop.rotary_phone_glitched`.

**Outlines are opt-in per visual** (`outline`). Grass and flowers opt out, so they add no noise.

**Art is resident before streaming.** `GLVisuals::Preload` loads every visual's mesh and material
at world start. A cell's runtime layer must never load art synchronously: that broke the P5
streaming budget, see ADR-0032. A future large catalogue needs per-cell async loading ahead of the
runtime layer instead.

## 4a. P7.1: what the pipeline gained (variant A approved as canonical, 2026-09-26)
The flow is unchanged (recipe → FBX + manifest → import + validation → `visual.*`). These additions
make the style dimensional under the same colour:
- **Material classes:**
  - Slots `GL_Painted`, `GL_Plastic`, `GL_Metal`, `GL_Stone`, `GL_Wood` and `GL_Foliage` bind to
    instances (`MI_GL*`) of one painted master.
  - Each class has its own roughness, metallic and specular, and a procedural surface detail in
    world space at two scales (colour and roughness variation; stretched for wood grain).
  - Foliage adds a per-instance hue and value shift for scattered vegetation.
  - Colour stays in the vertex colours; textures can come later without changing the flow.
- **Baked ambient occlusion:**
  - `gl_art.bake_ao` ray-casts each vertex's hemisphere against the joined mesh and stores the
    result in the colour attribute's alpha.
  - The master uses it for base-colour depth (`AOStrength`) and indirect light.
- **Split normals exported as authored** (`mesh_smooth_type="OFF"`): smooth forms with crisp
  creases, not faceted flat shading.
- **Recipe helpers for volume:**
  - `subdivide_z` / `subdivide_axis`;
  - `bend` (grass blades, stems);
  - `bulge`;
  - `chisel` (chunky rock facets);
  - `paint_up` (moss on up-facing faces);
  - more bevel segments.
- **Clean reimports:** every mesh package is deleted before import. Replacing an existing mesh kept
  its old material slots, and validation caught it as `unexpected material slot` on 15 assets.
- **Outline categories:**
  - Stencil 1 is the environment; stencil 2 is characters and creatures (pawns).
  - The stylize post weights each category (`EnvOutline`, `CharOutline`), and can keep the
    environment's silhouettes while dropping its interior creases (`EnvCreases`).
- **Triangles:** 14,348 → 38,804 over the 24 proof assets. The grass tuft goes from 108 to 504,
  instanced by the thousand. **LODs become necessary** before production density.

## 4b. P8: LODs and triangle budgets as data
**Every visual declares its mesh's LOD budget** (schema-required):

```json
"lod": { "maxTriangles": [5000, 2000, 750], "screenSize": [1.0, 0.35, 0.12] },
"cullDistance": 60
```

- `maxTriangles[i]` is the most triangles LOD *i* may have; `screenSize[i]` is where it takes over.
- `cullDistance` (metres, optional) stops drawing the visual beyond it. Scattered vegetation must
  declare one (VIS-4): grass and flowers 60 m, bushes 150 m.
- **VIS-3:**
  - 1 to 4 levels, one screen size per level;
  - triangles strictly decreasing;
  - screen sizes start at 1.0 and strictly decrease (above 0);
  - every reduced LOD at least 64 triangles. That is the engine reducer's floor, measured on
    SM_GrassTuft, SM_Flower and SM_K50_Roof; a smaller budget can never be met.

**The importer builds the LODs.**
- It asks the reducer for about 92% of each budget, keeping LOD0's build settings. Painted vertex
  colours, baked AO and material slots survive the reduction.
- If a LOD lands over budget, it retries harder (up to 6 times).
- What still does not fit **fails the import**. The first run caught SM_GrassTuft and SM_K50_Roof
  at 64 > 60. That is how the reducer floor was found; their LOD2 budgets became 70, and the floor
  became a VIS-3 rule.

**The P8 budgets:**
- LOD0 is the P7.1 mesh plus about 15% headroom, rounded up to 100.
- LOD1 is 40% of that and LOD2 15%. Grass and flowers use 30% and 10%, since they are instanced by
  the thousand.
- Screen sizes: characters and creatures 1 / 0.4 / 0.15; foliage 1 / 0.25 / 0.08; the rest
  1 / 0.35 / 0.12.
- The built result: 24 meshes, 3 LODs each, all within budget, 78 levels checked. Examples:

| Mesh | LOD0 | LOD1 | LOD2 |
|---|---|---|---|
| SM_GrassTuft | 504 / 600 | 166 / 180 | 64 / 70 |
| SM_Pine | 4274 / 5000 | 1840 / 2000 | 690 / 750 |
| SM_K50_WallDoor | 2604 / 3100 | 1141 / 1240 | 424 / 460 |
| SM_Zenny | 3316 / 3900 | 1436 / 1560 | 534 / 580 |

**At runtime,** LODs come with the mesh. Visual components take `cullDistance`. Scatter patches set
their instances' cull distances: faded from 75% of it, gone at it.

**LODs are presentation only.** No gameplay rule may depend on a mesh or LOD level (north star).
`Gridlands.Game.Lod` checks three things:
1. every visual's mesh holds its budget: LOD count, triangles per LOD, and screen sizes as data;
2. cull distances come from data;
3. forcing a mesh's lightest LOD changes no gameplay trace.

**Changing a budget means re-running `Tools/art.sh`.** The C++ test fails when data and the built
meshes drift apart: a tighter budget, a moved screen size, or an extra level.

## 5. Adding an asset
1. **Write a recipe** in `Art/Source/build_assets.py`, reusing `gl_art` helpers and the palette, and
   append it to `RECIPES`.
2. **Run `Tools/art.sh`.** It must pass validation.
3. **Add `Data/visual/<group>/<name>.json`** with its `lod` budget (and a `cullDistance` if it is
   scattered), re-run `Tools/art.sh` so the LODs are built to it, then reference it from a `buildpiece.visual`,
   `creature.visual` or a `scatter` placement.
4. **Run `Tools/data.sh validate`** (VIS-1..VIS-5) and `Tools/test.sh`.

## What is automated, what is not
- **Automated:** modelling from recipes, export, import, master materials, slot binding, validation,
  LODs built and checked against per-asset triangle budgets in data (P8), cull distances, runtime
  attachment, data validation, and the review screenshots (`gl.Style.Tour`).
- **Not yet:**
  - authoring outside code: an artist-facing Blender add-on, or hand-sculpted source files with the
    same export contract;
  - textures and hand-painted detail maps;
  - skeletal meshes and animation (the character proxies are static);
  - Nanite and HLOD (not needed at the P8 budgets; see ADR-0036);
  - Unreal-side modular structure assembly and export (see STRUCTURE-AUTHORING.md).
- **Hand-authored source is compatible.** A hand-made `.blend` exported through `gl_art.export_fbx`
  with a manifest entry goes through the same import and validation.
