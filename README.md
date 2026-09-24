# Gridlands

A third-person survival/crafting/building game set inside a corrupted digital
town, built in **Unreal Engine 5.8** (C++ core, data-driven content).

Salvage the physical world for materials, fabricate tools, build and repair a
home, and push outward into danger. You can't fix the simulation yourself.
**Pehlichi**, your AI squirrel companion, can: it scans to reveal glitches in
the Grid and repairs them while you clear the way and keep it safe.

> *Normal things look physical. Glitched things reveal the Grid.*

Status: **GRIDLANDS_BOOTSTRAP**. Architecture foundation, not a game yet. See
[`Docs/MILESTONES.md`](Docs/MILESTONES.md).

## Quick start

```sh
git lfs install            # once per machine
Tools/doctor.sh            # checks git-lfs, python, the pinned Unreal engine
Tools/selftest.sh          # tooling tests (no engine needed)
Tools/build.sh             # compile the editor target
Tools/test.sh              # headless automation tests
```

The engine is Epic's prebuilt Linux binary at the exact version in
`Tools/engine-pin.env`. Point the tools at it with `GRIDLANDS_UE_ROOT=/path`, or
write that path into an untracked `.engine-root` file.

## Documentation

- [Architecture](Docs/ARCHITECTURE.md)
- [Glitches and Pehlichi](Docs/GLITCH-AND-PEHLICHI.md), the defining mechanic
- [Design pillars](Docs/DESIGN-PILLARS.md) and [visual direction](Docs/VISUAL-DIRECTION.md)
- [Zones and progression](Docs/ZONES-AND-PROGRESSION.md): Grid cells, quotas, the non-combat path
- [Decision records](Docs/ADR/)
- [Handoff for coding agents](Docs/HANDOFF.md)
- [The Godot prototype this replaced](Docs/LEGACY-GODOT.md)
