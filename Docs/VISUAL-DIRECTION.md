# Visual direction

## Core rule

**NORMAL THINGS LOOK PHYSICAL. GLITCHED THINGS REVEAL THE GRID.**

- Ordinary physical spaces (houses, salvageable objects, tools, props, era
  fragments) are **stylized 3D** and read as real environments.
- Corruption exposes the neon digital structure beneath: cyan wireframe
  volumes, magenta circuit traces, grid planes.
- Pehlichi's scan will eventually let the player *see* that underlying Grid.
  In bootstrap this is a clear debug visualization, not a finished shader.

## Target fidelity

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

Neon colours belong to the Grid layer. Physical-layer materials use muted,
believable colours so the contrast carries meaning.

## Bootstrap limits

Placeholder meshes (engine primitives or simple low-poly shapes), one debug
"Grid" material (emissive cyan wire / magenta lines), no post-process
pipeline, no finished shaders.
