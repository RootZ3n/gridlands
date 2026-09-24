# Gridlands architecture

Status: **bootstrap (GRIDLANDS_BOOTSTRAP)**. This document is the contract that
later agents inherit. Decisions behind it are recorded in [`ADR/`](ADR/). When
code and this document disagree, the code is wrong or this document is stale,
and either way that is a defect to report, not to paper over.

## 1. The game in one paragraph

Gridlands is a third-person survival/crafting/building game set in a corrupted
digital town. The player salvages physical-looking objects for materials,
fabricates tools, builds and repairs structures, and explores outward from a
stabilized home into increasingly dangerous territory. The defining mechanic is
**glitch repair, which only Pehlichi, the player's AI squirrel companion, can
perform.** The player finds signs of trouble, commands Pehlichi to scan,
creates the conditions for a safe repair, and protects Pehlichi while it
works. The world is divided into large Grid cells (about 1 km each; one zone
per cell). Repairing a quota of a zone's glitches lets Pehlichi open the next
boundary. **The whole game can be completed without combat.** See
[`DESIGN-PILLARS.md`](DESIGN-PILLARS.md) and
[`ZONES-AND-PROGRESSION.md`](ZONES-AND-PROGRESSION.md).

## 2. Layering

```
GridlandsEditor   (editor-only)  JSON importer/validator, data tests
      |
GridlandsGame     (runtime)      actors, components, subsystems, Pehlichi, glitches, save
      |
GridlandsCore     (runtime)      pure types and rules: ids, lifecycle tables, stack math,
                                 recipe evaluation, save schema -- no actors, no world
```

- **Core has no dependency on a world.** Anything that can be decided without
  spawning an actor belongs in Core, so it can be tested headless in
  milliseconds. Game code calls Core rules; it does not re-implement them.
- **Game never reaches into Editor.** Editor may depend on Game and Core.
- Systems inside Game are separated by folder (`Interaction/`, `Salvage/`,
  `Inventory/`, `Fabrication/`, `Building/`, `Glitch/`, `Pehlichi/`,
  `Creatures/`, `Persistence/`). A system talks to another through an
  interface, a gameplay tag or a subsystem, never by casting to its concrete
  class.

## 3. Content: JSON is the source of truth

Structured content (items, recipes, salvage yields, build pieces, glitch
definitions, capability levels, creatures) is authored as JSON in `Data/`. The
`GridlandsEditor` importer deterministically generates or validates the
matching `UPrimaryDataAsset`s under `Content/Gridlands/Data/`. An agent adds an
item by editing JSON and running `Tools/import-data.sh`; it never edits a
`.uasset`. Blueprints are thin visual subclasses (mesh, material, sound) with
no gameplay logic. See [ADR-0002](ADR/0002-json-source-of-truth.md).

## 4. Systems

| System | Runtime shape (C++) | Definition (data) |
|---|---|---|
| Character | `AGLCharacter`: third-person, Enhanced Input | movement tuning, input mapping |
| Interaction | `UGLInteractorComponent` (trace then focus) + `IGLInteractable` returning `FGLInteractionOption {VerbTag, bEnabled, Reason}` | verb tags |
| Salvage | `UGLSalvageableComponent`: integrity, yields, tool gating | `UGLSalvageDefinition` |
| Inventory | `UGLInventoryComponent` over Core stack math (`FGLItemStack {ItemId, Count}`) | `UGLItemDefinition` |
| Fabrication | `UGLFabricationSubsystem`: Core `CanCraft`/`Craft` + station proximity | `UGLRecipeDefinition` |
| Building/repair | `AGLBuildPiece` with snap sockets; states Intact / Damaged / Ghost | `UGLBuildPieceDefinition` |
| **Glitches** | `AGLGlitch` + `UGLGlitchComponent`, `UGLGlitchSubsystem`, requirement objects | `UGLGlitchDefinition` |
| **Pehlichi** | `AGLPehlichi` + command, positioning, scan, capability and **repair** components | `UGLCompanionCapabilityDefinition` |
| Creatures | `AGLCreature` + StateTree; disposition is data | `UGLCreatureDefinition` (`Passive/Territorial/Guarding/Hunting`) |
| Persistence | `UGLSaveSubsystem`; `UGLPersistentIdComponent` (stable `FGuid`); save = authored world + delta | versioned schema |
| Zones *(design only)* | zone = one Grid cell; progress derived from glitch states; boundary stabilized by Pehlichi | `UGLZoneDefinition`: cell, quota, danger profile |
| Stats/effects/damage | small interfaces (`IGLDamageable`, capability component); **no GAS** | tuning |

Glitches and Pehlichi are the heart of the game and get their own document:
**[`GLITCH-AND-PEHLICHI.md`](GLITCH-AND-PEHLICHI.md)**. The short form:

```
Player --command--> Pehlichi
                      |  Scan
                      v
              Glitch detection / revelation     (Latent -> Detected)
                      |
              Requirements evaluated by the world
                      |  player salvages blockers, delivers materials,
                      |  clears or evades guards, opens a path
                      v
              Repairable
                      |  player commands Repair; Pehlichi must reach the repair point
                      v
              Pehlichi repairs (Repairing <-> Interrupted)
                      |
                      v
              Repaired: persistent world change + progression reward
```

**Invariant: only Pehlichi's repair system can perform a simulation repair.**
The player never has a "repair glitch" interaction. This is enforced in code by
the Core lifecycle table (the `Player` authority has no legal transitions) and
by a passkey type that only `UGLRepairComponent` can construct.

## 5. Tags as the shared vocabulary

Gameplay Tags (`Config/Tags/*.ini`, text) name verbs (`Interact.Salvage`),
tool classes (`Tool.Pry`), stations (`Station.Workbench`), companion commands
(`Command.Pehlichi.Scan`), capabilities (`Capability.Pehlichi.Scan`) and
requirement kinds. Systems match on tags, so adding content never means adding
a C++ enum value in another system.

## 6. Persistence

A save is the authored level plus a **delta** keyed by stable ids: objects
salvaged or destroyed, glitch lifecycle states, placed and repaired build
pieces, inventories, capability levels. Every mutable placed actor carries a
`UGLPersistentIdComponent` whose `FGuid` is validated for uniqueness by an
editor check. The save schema is versioned from day one, and every version bump
ships with a migration and a round-trip test.

Transient lifecycle states are not saved as-is: `Repairing` saves as
`Interrupted` with its progress, so a load never resumes a repair nobody is
performing.

## 7. Seams kept for later (not built)

| Later feature | Seam that exists now |
|---|---|
| GAS for combat | damage/effects go through `IGLDamageable` and capability components; nothing outside them touches stats directly |
| Structural support | `UGLBuildPieceDefinition` reserves support fields; snapping is socket-based |
| Stabilization suppressing spawns | glitch rewards carry an optional stabilization effect; spawn logic queries a `UGLStabilitySubsystem` interface, radius is data, never a constant |
| Hostile AI jamming / decoys | lifecycle has hostile-authority transitions; scan findings carry a kind and confidence |
| Pehlichi-only access and traversal | reachability is asked through `IGLReachability`, so specialized traversal replaces the default nav query without touching repair |
| Non-combat completion checks (ADR-0009) | items/capabilities tag acquisition sources; glitches declare guards; zones declare quotas, so the data validator can check NC-2/NC-3 |
| Zones as world-scale Grid cells (ADR-0010) | persistent ids are stable `FGuid`s, safe under level streaming; no system assumes one loaded level; streaming choice is a later ADR |
| Rewards to player, Pehlichi or both | `FGLReward` names its recipient; the capability component is generic and can sit on either |

## 8. Design invariants every system must preserve

- **Non-combat completion (ADR-0009):** no progression gate requires a kill;
  nothing on the critical path is combat-only; each zone's combat-free glitches
  cover its quota. *If a player can only advance by killing something, the
  non-combat path has failed.*
- **Grid lines define regions, not surfaces (ADR-0010).**
- **Only Pehlichi repairs (ADR-0005).**

## 9. Out of scope during bootstrap

Multiplayer, procedural city generation, final combat, large crafting trees,
large item counts, advanced enemy AI, any connection to the real Pehlichi lab
agent or Pehverse runtime, finished art, finished shaders, final building
system, narrative, voice, large environments, multi-zone worlds, zone
boundaries, level streaming, boss encounters.
