# Gridlands

> *"The system is corrupted. The sword is ready. The grid awaits."*

**Gridlands** is a synthwave, turn-based RPG built in **Godot 4.x** (GDScript).
A genius AI developer named **Zenny** is pulled into his own corrupted digital
lab by a nefarious AI. Trapped in a Tron-like world rendered in neon retrowave
aesthetics, he partners with his loyal AI companion — manifested as the sentient
**Circuit Blade** — to fight corrupted versions of his own creations, fix the
glitches tearing his system apart, and find his way back to reality.

The gameplay follows a **Dragon Warrior** blueprint: a top-down overworld,
random monster encounters, and classic turn-based menu combat. The twist —
Zenny himself has no stats or levels. All progression lives in the Circuit
Blade, which levels up, learns skills, and transforms into new weapon forms.

Gridlands is a project of the **Pehverse** lab, a multi-agent AI development
ecosystem by Jeffrey Miller.

> **Project status:** early development (Phase 0/1). The core engine scaffolding
> exists — autoload singletons, the overworld movement system, and enemy data —
> but the battle scenes, UI, and art/audio assets are still to come.

## How to play

The game targets a **256×240** NES-resolution viewport with integer scaling
(crisp, pixel-perfect retro visuals).

### Controls (overworld)

| Action  | Keys                |
|---------|---------------------|
| Move    | Arrow keys / WASD   |
| Confirm | Enter / Z           |
| Cancel  | Backspace / X       |
| Menu    | Escape              |

### Objective

Travel through each corrupted zone of Zenny's lab, defeat the corrupted boss to
fix that zone's glitch and restore it, then unlock the path onward — culminating
at **The Core** and the final battle against the nefarious AI. (See the GDD for
the full zone map and story.)

## Run from source

You need [Godot 4.x](https://godotengine.org/) (the project declares 4.5
engine features).

```sh
# Clone, then open the project in the Godot editor
godot --path .

# Or run the main scene directly
godot --path . scenes/world/Overworld.tscn
```

The main scene is `res://scenes/world/Overworld.tscn`.

Exports require creating an export preset first (`export_presets.cfg` is
gitignored), then:

```sh
godot --headless --export-release "<preset-name>" <output-path>
```

## Project structure

```
gridlands/
├── project.godot              # Godot project config (autoloads, input, display)
├── data/
│   └── enemies.json           # Data-driven enemy stats, abilities, loot
├── scenes/
│   └── world/Overworld.tscn   # Main scene (player + camera)
├── scripts/
│   ├── core/                  # Autoload singletons
│   │   ├── game_manager.gd       # Global state + random encounters
│   │   ├── save_manager.gd       # JSON save/load (user://save_data.json)
│   │   ├── encounter_table.gd    # Per-zone weighted enemy tables
│   │   └── sfx_manager.gd        # Sound/music (stub)
│   └── world/
│       └── zenny_controller.gd   # NES-style tile-based movement
└── docs/
    ├── GDD_GRIDLANDS.md       # Game Design Document
    ├── ART_STYLE_GUIDE.md     # Art contract (resolution, palette, sprites)
    └── art-references/        # Synthwave style reference images
```

Architecture highlights:

- **Autoloads** (`GameManager`, `SaveManager`, `EncounterTable`, `SfxManager`)
  are global singletons coordinating state, saving, encounters, and audio.
- **Data-driven content** — enemies (and, by design, items/skills/zones/dialog)
  live in JSON under `data/`, so content can change without touching code.
- **Top-down movement** is grid-aligned (16×16 tiles), one tile at a time,
  with a quantized step for an authentic NES feel; each step rolls for a random
  encounter.

## Documentation

- **[Game Design Document](docs/GDD_GRIDLANDS.md)** — story, world, zones,
  combat system, the Circuit Blade weapon forms, progression, and dev phases.
- **[Art Style Guide](docs/ART_STYLE_GUIDE.md)** — the visual contract:
  resolution, tile/sprite specs, the synthwave color palette, UI, effects, and
  animation timing.

## Credits

- **Director / Design / Story:** Jeffrey Miller (Zenny)
- Produced within the **Pehverse** multi-agent AI lab (Julian, Pehlichi, ikbi,
  Luna, Ptah — see GDD §9 for the production pipeline).

## License

No license file is present in the repository. All rights reserved by the author
unless a license is added.
