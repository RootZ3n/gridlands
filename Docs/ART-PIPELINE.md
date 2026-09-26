# Art pipeline (P7 visual spike)

Status: **proven for the P7 style slice**; **not yet a production pipeline.** Visual direction is
**AWAITING OPERATOR REVIEW**. Decision record: [ADR-0032](ADR/0032-visual-pipeline-and-stylization.md).

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
   - **vertex colours** are painted, not all white.

It writes `Saved/Art/Export/import-report.json`.

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

**Variants are data:**
- a `tint` makes a dynamic instance of the master, with no new asset;
- `corruption` adds cubes to any visual;
- a variant is a second visual on the same mesh, for example `visual.prop.rotary_phone_glitched`.

**Outlines are opt-in per visual** (`outline`). Grass and flowers opt out, so they add no noise.

**Art is resident before streaming.** `GLVisuals::Preload` loads every visual's mesh and material
at world start. A cell's runtime layer must never load art synchronously: that broke the P5
streaming budget, see ADR-0032. A future large catalogue needs per-cell async loading ahead of the
runtime layer instead.

## 5. Adding an asset
1. **Write a recipe** in `Art/Source/build_assets.py`, reusing `gl_art` helpers and the palette, and
   append it to `RECIPES`.
2. **Run `Tools/art.sh`.** It must pass validation.
3. **Add `Data/visual/<group>/<name>.json`**, then reference it from a `buildpiece.visual`,
   `creature.visual` or a `scatter` placement.
4. **Run `Tools/data.sh validate`** (VIS-1/VIS-2) and `Tools/test.sh`.

## What is automated, what is not
- **Automated:** modelling from recipes, export, import, master materials, slot binding, validation,
  runtime attachment, data validation, and the review screenshots (`gl.Style.Tour`).
- **Not yet:**
  - authoring outside code: an artist-facing Blender add-on, or hand-sculpted source files with the
    same export contract;
  - textures and hand-painted detail maps;
  - skeletal meshes and animation (the character proxies are static);
  - LODs;
  - per-asset triangle budgets as data;
  - Unreal-side modular structure assembly and export (see STRUCTURE-AUTHORING.md).
- **Hand-authored source is compatible.** A hand-made `.blend` exported through `gl_art.export_fbx`
  with a manifest entry goes through the same import and validation.
