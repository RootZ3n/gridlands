# Visual direction

> **Status legend used in the design docs.**
> - **LOCKED DESIGN INTENT**: an operator decision. Changing it needs the operator.
> - **UNPROVEN / NOT IMPLEMENTED**: nothing in the build does this yet.
> - A brainstormed mechanic is never described as existing.

## Visual north star (LOCKED DESIGN INTENT, operator, 2026-09-25; NOT IMPLEMENTED)
**Primary inspiration: WildStar.** This is inspiration, not imitation. Never reproduce WildStar
assets, characters, iconography, proprietary designs or specific content.

Gridlands should look like **a highly colorful, playable animated series**:
- expressive, exaggerated, high-quality stylized 3D;
- strong silhouettes and chunky, readable forms;
- saturated, deliberately colorful environments with rich environmental detail;
- expressive animation;
- graphic or dark character linework where appropriate;
- modern Unreal materials, lighting and atmosphere underneath the stylization.

**Secondary reference: Gorillaz**, for:
- character attitude and graphic presentation;
- expressive posing;
- illustrated characters inhabiting a dimensional world.

**Also locked:**
- **Retro design remains fundamental.** Synthwave remains fundamental but does **not** coat every
  object indiscriminately.
- **Color is strongly preferred.** Dark and night environments keep readable color and never
  collapse into grey or black.
- **Historical and era assets share one Gridlands rendering language** while keeping distinct
  architecture. Deliberately eclectic player construction is supported: a feudal Japanese
  dwelling beside a Victorian house, Roman beside 1950s.
- **Ofi is the authority for learned building knowledge.** Exploration lets the player bring
  architectural traditions home.

### NICE's corruption: a protected visual language (LOCKED)
**What it looks like:**
- mathematically clean geometry;
- **electric-blue corruption cubes**;
- cyan, magenta and violet digital interference where appropriate.
- Corruption geometry looks unnaturally precise against the illustrated, organic world.

**Blue cubes are used SPARINGLY.** Seeing them must mean *something is wrong*.
- A normal-looking creature carries only **3–4 cubes** intersecting or replacing small portions of it.
- An ordinary telephone may have **one** cube replacing part of the receiver.
- A glitch may be an otherwise normal object with one small, impossible geometric corruption.

### What this supersedes
- **Target fidelity:** "Valheim-level stylization is acceptable" and "muted, believable
  physical-layer colours" (sections below) are superseded by the north star. The physical world
  is now **saturated and colourful**.
- **The Grid layer's contrast:** it now comes from corruption's **precision and its protected
  palette**, not from a muted world.
- **The core rule stands:** normal things look physical; glitched things reveal the Grid.

### Art-direction spike: P7 DONE for engineering; VISUAL STATUS: AWAITING OPERATOR REVIEW
**What was built** ([evidence](Evidence/P7-visual-spike/README.md), [ADR-0032](ADR/0032-visual-pipeline-and-stylization.md),
[ART-PIPELINE](ART-PIPELINE.md)): a real-time slice in the diner lots with:
- stylized terrain and vegetation, rocks and pines;
- a modular 1950s storefront;
- a rotary telephone beside its one-cube glitched twin;
- an ordinary and a sparsely corrupted creature;
- Zenny and Pehlichi proxies;
- day, dusk and night presets;
- graphic outlines, gated by custom depth.

**Engineering compatibility** is proven with terrain edits, collapse and tree felling inside the
scene.

**Whether the look is right is the operator's call.** The spike exists so the answer is learned
before hundreds of assets.

### Art-direction spike (the plan it followed)
**No mass asset production until a real-time Unreal scene proves the pipeline** can produce the
"playable colorful animated series" look. The spike contains roughly:
- Zenny and Pehlichi proxies;
- an ordinary creature, and a creature with sparse blue-cube corruption;
- a rotary telephone, and its glitched version with one cube substitution;
- a small retro/1950s facade or street corner;
- vegetation and representative terrain;
- NICE corruption;
- colorful lighting and atmosphere.

**Hardware.** The RX 6800 is development and performance evidence, **not** the minimum
specification. Do not optimise the look around it.

## Core rule

**NORMAL THINGS LOOK PHYSICAL. GLITCHED THINGS REVEAL THE GRID.**

- Ordinary physical spaces (houses, salvageable objects, tools, props, era
  fragments) are **stylized 3D** and read as real environments.
- Corruption exposes the neon digital structure beneath: cyan wireframe
  volumes, magenta circuit traces, grid planes.
- Pehlichi's scan will eventually let the player *see* that underlying Grid.
  In bootstrap this is a clear debug visualization, not a finished shader.

## Target fidelity (superseded 2026-09-25 by the visual north star above; kept for history)

Stylized 3D, roughly in this range:
- more detailed and cleaner than Stardew-style pixel presentation;
- Valheim-level stylization/pixelation is acceptable;
- ARK-like cleanliness is also acceptable;
- **photorealism is not required** and not a goal.

The world has a synthwave/digital-Grid identity, while ordinary spaces stay
readable as real places.

## Grid lines define regions, not surfaces

The "Grid" is world-scale: its lines divide the world into large cells
(about 1 km, planning assumption). **Do not cover streets, floors, houses or terrain in
neon graph-paper lines.** The Grid shows at cell boundaries, during Pehlichi
scans, around glitches, where the simulation is damaged, and in corruption
events. Crossing a boundary should look and feel physically meaningful. See
[WORLD-AND-PROGRESSION.md](WORLD-AND-PROGRESSION.md) and
[ADR-0010](ADR/0010-grid-cells-are-world-regions.md).

## Static, storms and NICE's phenomena

- **Static replaces map fog.** Unexplored and unstable areas show digital
  static / white noise on the map. In the world, interference scales from
  haze up to a full static blizzard.
- **Glitch Storms** are NICE's weather and should look authored by her:
  playful (glitched cats and dogs raining), creepy, or dangerous. They are one
  of the places the neon Grid is allowed to dominate.
- **Era collisions** near the core may look chaotic on purpose. Incompatible
  eras intersect because NICE is unraveling.

## Primary target

[`ArtReference/ref-01-digital-tree-grid.png`](ArtReference/ref-01-digital-tree-grid.png)
is the operator's chosen style target. Read it as **what a scan reveals**, the
simulation laid bare: a tree whose trunk has become magenta circuit traces
and whose canopy has become cyan wireframe cubes, standing on a magenta grid
under a synthwave sun. In play, the same tree would look like a physical,
low-poly tree until its glitch is exposed. The magenta ground grid in the image
is a *revealed* or boundary view, not how ordinary ground normally looks.

Other references (carried over from the Godot prototype):
`ref-02-dinosaurs-neon.png` (corrupted assets from other game worlds),
`ref-03-knight-vs-dragon.png`, `ref-04-delorean-grid.png`.

## Palette (carried from the Godot prototype)

| Role | Hex |
|---|---|
| Grid, primary corruption accent | neon magenta `#FF00FF` |
| Wireframe, scan, secondary accent | neon cyan `#00FFFF` |
| Night sky / deep background | deep indigo `#1A0A2E` |
| Void | near-black `#0A0A0A` |
| Sun gradient | hot pink to orange (sample from ref-01) |
| Corruption warning | purple `#9933FF` |
| Danger | red `#FF3333` |

Neon colours belong to the Grid layer. *(Superseded 2026-09-25:* physical-layer materials were
"muted, believable". The north star now makes the physical world saturated and colourful. The
corruption language (electric-blue cubes, cyan/magenta/violet interference) is protected, and
carries the contrast through precision and sparing use.*)*

## Bootstrap limits

Placeholder meshes (engine primitives or simple low-poly shapes), one debug
"Grid" material (emissive cyan wire / magenta lines), no post-process
pipeline, no finished shaders.
