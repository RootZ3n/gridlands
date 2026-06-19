# CLAUDE.md — Gridlands

Project context for AI agents working in this repo. Keep it accurate; base
changes on what is actually in the codebase.

## What this is

**Gridlands** is a turn-based RPG built in **Godot 4.x** (GDScript). A genius
AI developer ("Zenny") is pulled into his own corrupted digital lab and must
fight through corrupted versions of his creations alongside a sentient weapon,
the **Circuit Blade**. The look is a corrupted Tron world rendered in
**synthwave / retrowave** aesthetics; the gameplay is a **Dragon Warrior**
blueprint (top-down overworld, random encounters, turn-based menu combat).
It is part of the Pehverse lab (a multi-agent AI development ecosystem).

The project is early — Phase 0/1. Core scripts, autoloads, the Overworld
scene, and the enemy data file exist; battle scenes, UI, and art assets are
not yet implemented (many functions log to console with `# TODO` markers).

## Running / exporting

The repo has no checked-in export presets, so run from source with the editor
or headless binary:

```sh
# Open in the editor
godot --path .

# Run the main scene directly (res://scenes/world/Overworld.tscn)
godot --path . --main-pack-disabled
# or simply:
godot --path . scenes/world/Overworld.tscn
```

Exports require creating an `export_presets.cfg` first (it is gitignored).
Use `godot --headless --export-release "<preset>" <output>` once a preset exists.

## Engine / display facts (verified)

- **Engine features:** `project.godot` declares `4.5`, Forward Plus; renderer
  is set to **mobile**. Docs refer to "Godot 4.x".
- **Viewport: 256×240** (NES native), `window/stretch/mode="viewport"`,
  `scale_mode="integer"`, `aspect="keep"`. NOTE: this is NES resolution, not
  320×180 — do not "correct" it to 320×180.
- **Tiles / sprites: 16×16 px** overworld; battle enemies 32×32; bosses 48–64.
- Pixel-perfect: no anti-aliasing, no sub-pixel positioning.

## Conventions

- **Language:** GDScript, tabs for indentation, static typing throughout
  (`var x: int`, typed func signatures, `:=` inference). Use `##` doc comments
  on classes/functions, `#` for inline notes.
- **Autoloads are singletons** referenced by name (e.g. `GameManager`,
  `SaveManager`). Do not instance them.
- **Data-driven design:** game content lives in JSON under `data/`
  (`enemies.json` exists). Scripts read data; data defines content. Add new
  enemies/items/skills as JSON, not hardcoded.
- **Scene organization** (target layout from the GDD):
  `scenes/{world,battle,ui,zones}`, `scripts/{core,battle,world,companion,zones,ui}`,
  `assets/{sprites,tilesets,backgrounds,audio,fonts}`, `data/`, `docs/`.
  Currently only `scenes/world/`, `scripts/core/`, `scripts/world/`, `data/`,
  `docs/` are populated.
- **Asset naming:** snake_case PNGs by category (e.g. `grid_floor.png`,
  `wall_circuit.png`). See `docs/ART_STYLE_GUIDE.md` for the full contract.
- **Synthwave palette:** near-black `#0A0A0A` / deep indigo `#1A0A2E` bases,
  neon magenta `#FF00FF` and cyan `#00FFFF` accents, dark silhouettes with
  neon edge outlines color-coded by type. Full palette in the art guide.

## Architecture

### Autoloads (`scripts/core/`, registered in `project.godot`)

- **GameManager** (`game_manager.gd`) — global state machine
  (`GameState` enum: OVERWORLD, BATTLE, MENU, DIALOG, CUTSCENE, LOADING),
  scene-transition signals, step counting, and the **random encounter** check
  (`should_encounter()` with a 5-step grace period; per-zone encounter rate via
  `get_zone_encounter_rate()`). `trigger_encounter()` / `end_battle()` are
  stubs awaiting the battle scene.
- **SaveManager** (`save_manager.gd`) — JSON save/load to
  `user://save_data.json`. `save_data` dictionary is the canonical schema
  (player, blade level/exp/form/skills, hp/mp, gold, inventory, quests,
  glitches_fixed, play time, steps). NES "talk to a save NPC" style.
- **EncounterTable** (`encounter_table.gd`) — per-zone weighted enemy spawn
  tables (`pick_encounter(zone)`); `get_enemy_data(id)` reads
  `res://data/enemies.json`.
- **SfxManager** (`sfx_manager.gd`) — `play_sfx` / `play_music` / `stop_music`,
  currently print stubs until audio assets exist.

### Overworld

- Main scene: **`res://scenes/world/Overworld.tscn`** (Node2D + Camera2D
  zoom 3× + Zenny CharacterBody2D).
- **`scripts/world/zenny_controller.gd`** — NES-style tile-based movement:
  one tile at a time, grid-aligned (16px), input one direction at a time,
  quantized lerp for a snappy retro feel. On arriving at a tile it calls
  `GameManager.register_step()` and checks for an encounter. Collision via a
  raycast against collision layer 1 (walls).

### Combat (designed, not yet built)

Dragon Warrior turn-based menu combat (Fight / Skill / Item / Run).
**Zenny has no stats** — all progression lives in the **Circuit Blade**
companion (levels, weapon forms, skills). See GDD §4 for the full design and
the weapon-form table.

## Key docs

- `docs/GDD_GRIDLANDS.md` — full game design document (story, zones, combat,
  progression, phases, lab pipeline).
- `docs/ART_STYLE_GUIDE.md` — the binding art contract (resolution, tiles,
  sprites, palette, UI, effects, animation timing).
- `docs/art-references/` — 4 synthwave style reference images.

## Guardrails

- Do not change the 256×240 viewport or integer scaling without intent.
- Keep new content data-driven (JSON) where the design calls for it.
- Honor the static-typing + `##` doc-comment style already established.
