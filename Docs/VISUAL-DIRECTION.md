# Visual direction

> **Status legend used in the design docs.**
> - **LOCKED DESIGN INTENT**: an operator decision. Changing it needs the operator.
> - **UNPROVEN / NOT IMPLEMENTED**: nothing in the build does this yet.
> - A brainstormed mechanic is never described as existing.

## CANONICAL VISUAL DIRECTION: P7.1 (LOCKED DESIGN INTENT, operator, 2026-09-26)
**The operator approved P7.1 variant A** as the visual baseline: the dimensional, highly colourful,
WildStar-leaning treatment ([evidence](Evidence/P7.1-visual-refinement/README.md); frames under
`screenshots/A/`).
- Richer volumetric trees and vegetation, rounded and chunky storefront geometry, richer rocks and
  material response.
- **No environment outlines.**
- It is the game's default look (`UGLStyleSubsystem`, variant `A`).
- **The rejected variants are not averaged in.** B (environment silhouettes) and C (grounded) exist
  only as the review record.

**The target: A COLOURFUL, RICHLY STYLIZED, DIMENSIONAL 3D WORLD.**
- **WildStar is PRIMARY** for:
  - degree of stylization and dimensionality;
  - exaggerated geometry and playful shape language;
  - environmental richness and material separation;
  - strong silhouettes;
  - the willingness to use lots of colour.
- **RuneScape: Dragonwilds is SECONDARY** for:
  - an inhabitable-world feeling;
  - readable survival environments;
  - dimensional environmental presentation;
  - visual longevity over long sessions.
- **These are references, never assets or designs to copy.** Gridlands develops its own identity.

**Colour is canonical.** Dimensionality is never permission to:
- desaturate, or move toward brown or grey realism;
- mute vegetation;
- apply generic cinematic desaturation;
- become visually conservative.

> **LOTS OF COLOUR + DIMENSIONAL GEOMETRY + RICH STYLIZED MATERIAL RESPONSE + STRONG LIGHTING AND SHADOW = GRIDLANDS.**
> **COLOURFUL DOES NOT MEAN FLAT.**

Zones may take very different palettes and moods; Gridlands stays comfortable with the full colour
wheel.

**The P7 illustrated treatment is superseded as the production target.** P7 remains valid
engineering history (`p7-visual-green`). Do not return to:
- heavy environment outlines;
- paper-cutout vegetation;
- extremely flat materials;
- primitive environmental geometry;
- "a 2D cartoon rendered in 3D".

**Nor move toward photorealism:** P7.1 is still highly stylized.

**Environment outlines:**
- **The baseline is none.** Geometry, materials, lighting and silhouette carry the stylization.
- **The outline system stays** (cheap and useful). It may serve selectively:
  - Zenny, Pehlichi and creatures;
  - interactables;
  - special readability cases;
  - particular effects.
- **None of those categories is assumed to need outlines either.** Judge them visually once
  production assets exist.
- Environment outlines are never reintroduced globally without operator visual review.

**Production assets may be much richer.** The P7.1 assets are style proofs; they set the visual
**language**, not the detail ceiling.
- Production assets may carry far more:
  - authored detail and shape personality;
  - geometric sophistication;
  - material and texture work;
  - silhouette design and architectural ornament;
  - animation and environmental layering.
- A production Roman statue, Victorian house, Japanese dwelling, 1920s carnival, 1950s
  university, museum, municipal library, dungeon or legendary item should have considerably more
  identity than these proofs, while clearly belonging to the same world.
- **Consequence: LODs are required before substantial production content density.** The P7.1
  proofs are already 3–9× the P7 triangle counts.

**NICE corruption:** the principle holds, and the richer world strengthens the contrast.
- *Normal Gridlands:* dimensional, colourful, organic, exaggerated, imperfect, expressive.
- *NICE:* unnaturally precise, exact, geometric, digital, artificial.
- Keep it sparse: never cover the environment in cubes because corruption exists.

**Characters remain unapproved proxies.** P7.1 approves the environmental and rendering language
only, not the final Zenny, Pehlichi or creature designs, proportions, animation, faces or emotes.
Characters will gain far more personality and sophistication while fitting this world.

*History below is kept as it was decided: the 2026-09-25 north star, the P7 review and the P7.1
brief. Where it conflicts with this section, this section wins.*

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

### P7 visual review: the direction was APPROVED (operator, 2026-09-26; superseded as the production target by P7.1)
The P7 screenshots ([evidence](Evidence/P7-visual-spike/README.md), [ADR-0032](ADR/0032-visual-pipeline-and-stylization.md),
[ART-PIPELINE](ART-PIPELINE.md)) establish **the correct visual family** for Gridlands.

**The environment and art language are locked around:**
- saturated, deliberate colour;
- strong, irregular dark outlines;
- simplified and exaggerated 3D forms;
- graphic, cel-like shading;
- strong, readable silhouettes;
- playful proportions;
- a "playable animated cartoon" presentation;
- modern rendering underneath the stylization, never photorealism.

**Inspirations, not sources.** WildStar remains the primary game-art inspiration. Gorillaz remains
secondary, for character attitude, expressive posing, graphic silhouette and illustrated-character
personality. Gridlands keeps its own identity and never copies protected assets or designs.

**Never move toward photorealism.**

#### Colour: aggressive, but deliberate
The strong colour direction is approved. It does **not** mean everything is always equally bright
and saturated.

**Places and moments may have their own palette, value range and mood,** while staying
recognisably Gridlands: zones, historical periods, dungeons, weather, times of day, interiors and
story moments.

**Examples of the principle** (not palette specifications):
- a home region that is extremely green and cheerful;
- a carnival pushing saturated reds, yellows, purples and neon;
- an Ice Age museum leaning into cyan, blue and cold artificial light;
- a municipal-library dungeon in warmer amber and dark interior values;
- a Roman area of sun-baked stone against strongly coloured vegetation and sky.

**Neon and synthwave accents work with this language** (the dusk and night proof); preserve that
capability. **Night stays colourful and readable,** never the daytime scene desaturated.

#### Characters and creatures are NOT final
The current Zenny, Pehlichi and creature assets are **style proxies**. Appearing in an approved
scene is not character-art approval.

**Production characters need substantially more:**
- silhouette design;
- personality;
- facial expression where applicable;
- animation quality;
- semantic emote capability;
- exaggerated posing;
- readable reactions.

They stay within the approved rendering language, and are never realistic. **The stylized
environment and the characters must look like they belong to the same animated world.**

#### Outlines: selective, presentation-only (architecture approved)
- Outlines stay **presentation-only** and **opt-in per visual**. Nothing is outlined automatically.
- **Each future system is evaluated on its own before it is outlined:**
  - translucent effects;
  - stealth sight cones;
  - AoE telegraphs;
  - NICE corruption (today: never outlined);
  - particles;
  - water;
  - rainbow and projectile effects;
  - UI and world markers;
  - ghost and placement previews.
- **Outlines exist for readability and identity, not visual noise.**

#### Corruption stays sparse (approved)
The contrast is the language:
- **Normal Gridlands:** organic, imperfect, colourful, illustrated, exaggerated.
- **NICE corruption:** unnaturally exact, geometric, digital and precise.

Never "cover everything in blue cubes": sparse corruption is stronger. **An otherwise normal prop
or creature with a few impossible geometric intrusions stays the default.** VIS-2 enforces the
budget: at most 4 cubes of at most 0.35 m per visual.

### P7.1 visual-direction refinement brief (operator, 2026-09-26; RESOLVED: variant A approved, see the canonical section above)
**P7 stays accepted and GREEN.** Its engineering results stand: the pipeline, strong colour,
selective outlines, the corruption language, compatibility and performance. What changed is the
desired **degree and type** of stylization.

**The target is not "a 2D/cartoon illustration rendered in 3D" but "a richly stylized 3D game
world."**
- **WildStar is now the PRIMARY north star** for:
  - degree of stylization and dimensionality;
  - exaggerated geometry and shape language;
  - environmental and material richness;
  - strong colour and playful personality.
- **RuneScape: Dragonwilds is SECONDARY:** dimensional world presentation, readable survival
  environments, longevity over long sessions, a world that feels inhabitable.
- **These are references only.** Copy no assets, characters, environments, textures or proprietary
  designs. Gridlands keeps its own identity inside this territory.

**Canonical principle: COLOURFUL DOES NOT MEAN FLAT.**
- **Keep** the aggressive colour:
  - vivid greens and strong blues;
  - purples and pinks;
  - warm/cool contrast;
  - neon and synthwave accents;
  - colourful vegetation;
  - regional palettes;
  - colourful nights.
- **Move away from:**
  - universal heavy comic outlines;
  - paper-cutout looks and flat surfaces;
  - overly primitive geometry;
  - card-like vegetation;
  - flat-colour-only presentation.
- **Move toward:**
  - chunky, volumetric, exaggerated forms;
  - stronger (and curved) architectural silhouettes;
  - richer stylized materials and controlled surface variation;
  - bevels that catch light;
  - stronger lighting response;
  - dimensional vegetation, rocks and terrain;
  - environmental layering and depth.
- **Neither photorealism nor the loss of stylization.**

**Gridlands identity stays:**
- synthwave;
- NICE's precise geometric corruption;
- historical architectural collisions;
- a broad palette and humour;
- Pehlichi and Zenny;
- terrain manipulation and structural destruction;
- readable telegraphs.

**NICE should read even more distinct** against the richer world:
- *normal world:* dimensional, colourful, imperfect, organic, expressive;
- *NICE:* precise, geometric, artificial, digitally impossible.

**Historical coherence:** Roman, feudal Japanese, Victorian, 1920s carnival, 1950s, modern,
museums, universities, libraries and fantasy must look like one game. The common language comes
from proportion, exaggeration, materials, colour, lighting and presentation, never from identical
architecture.

**Outlines are re-evaluated, not removed.** The opt-in outline architecture stays (it is cheap);
how strongly the environment is outlined is a presentation decision.

**The P7.1 proof** ([evidence](Evidence/P7.1-visual-refinement/README.md)) is the same scene and
cameras as P7. It came in three variants; **A was approved** (2026-09-26):
- **A:** dimensional, with no environment outlines and lightly outlined characters.
- **B:** dimensional, with environment silhouettes only, lighter and fading with distance.
- **C:** a more grounded dimensional treatment, keeping P7's saturation.

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
