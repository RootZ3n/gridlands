# Gridlands

A single-player survival / salvage / building / exploration game set inside a
corrupted digital world, built in **Unreal Engine 5.8** (C++ core, data-driven
content).

You are Zenny, a silent game developer and the favourite toy of **NICE**, the
AI Game Master who runs this world and never lets you forget it. Salvage,
build, terraform and push inward toward her core through static and
corruption. You can't fix the simulation yourself. **Pehlichi**, your
smart-mouthed scientist companion stuck in a squirrel body, can: it scans for
glitches and repairs them. Every repair steadies the world, and rattles NICE a
little more.

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

- [Design bible](Docs/DESIGN-BIBLE.md), the canonical game design; [glossary](Docs/GLOSSARY.md)
- [Architecture](Docs/ARCHITECTURE.md)
- [Glitches and Pehlichi](Docs/GLITCH-AND-PEHLICHI.md), the defining mechanic
- [Design pillars](Docs/DESIGN-PILLARS.md) and [visual direction](Docs/VISUAL-DIRECTION.md)
- [World and progression](Docs/WORLD-AND-PROGRESSION.md): cells, bands, eras, static, storms
- [Story and dialogue](Docs/STORY-AND-DIALOGUE.md), [survival and threat](Docs/SURVIVAL-AND-THREAT.md), [building, salvage and terrain](Docs/BUILDING-SALVAGE-TERRAIN.md)
- [Design reconciliation report](Docs/DESIGN-RECONCILIATION.md)
- [Decision records](Docs/ADR/)
- [Handoff for coding agents](Docs/HANDOFF.md)
- [The Godot prototype this replaced](Docs/LEGACY-GODOT.md)
