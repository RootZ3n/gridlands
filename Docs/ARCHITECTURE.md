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
works. See [`DESIGN-PILLARS.md`](DESIGN-PILLARS.md).

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
                      |  clears guards, opens a path
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
| Rewards to player, Pehlichi or both | `FGLReward` names its recipient; the capability component is generic and can sit on either |

## 8. Out of scope during bootstrap

Multiplayer, procedural city generation, final combat, large crafting trees,
large item counts, advanced enemy AI, any connection to the real Pehlichi lab
agent or Pehverse runtime, finished art, finished shaders, final building
system, narrative, voice, large environments.
