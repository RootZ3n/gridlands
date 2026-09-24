# Legacy: the Godot prototype

Gridlands began as a **Godot 4 prototype**: a top-down, turn-based RPG built on
a Dragon Warrior blueprint (random encounters, menu combat, a sentient
"Circuit Blade" companion weapon). It was replaced on 2026-09-24 by the current
Unreal Engine 5 third-person survival/crafting design.

| | |
|---|---|
| Final Godot commit | `f695f17e5064d7182161e5c92ecf0a6afadccbf9` ("chore: add Peh intro to README") |
| Preservation tag | `godot-prototype-final` (annotated, tag object `1e015a40eda3ba97f6e814204ed742cd2983b2e8`) |
| Earlier restore point | `restore-point-2026-06-19` -> `df4c70d3` |

To inspect it: `git switch --detach godot-prototype-final`.

## What carries forward

- **Design and art material may be referenced.** The original GDD and art
  guide live in [`Legacy/`](Legacy/) unchanged except for their path. The
  concept art moved to [`ArtReference/`](ArtReference/).
- **The synthwave palette** (neon magenta `#FF00FF`, neon cyan `#00FFFF`, deep
  indigo `#1A0A2E`, near-black `#0A0A0A`) is carried into
  [`VISUAL-DIRECTION.md`](VISUAL-DIRECTION.md).
- The "corrupted digital lab / nefarious AI" premise remains background canon.

## What is retired

- **The code architecture is retired, not migrated.** No GDScript was ported.
  Turn-based combat, random encounters, the Dragon Warrior overworld, the
  16x16 sprite pipeline and the Circuit Blade companion do not constrain the
  Unreal design.
- `data/enemies.json` was Godot-specific and was not carried forward.

Nothing in history was rewritten; the Godot files were removed by an ordinary
commit on top of the tagged state.
