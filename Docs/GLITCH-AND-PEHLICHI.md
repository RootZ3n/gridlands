# Glitches and Pehlichi

This is the defining mechanic of Gridlands. Read it before changing anything in
`Glitch/`, `Pehlichi/` or `GridlandsCore/Glitch/`.
Decision record: [ADR-0005](ADR/0005-pehlichi-sole-repair-authority.md).

## 1. Roles

**Pehlichi repairs glitches. The player cannot.** Pehlichi is a curious
scientist/hacker (a former Neurolink scientist in a squirrel body), not a
combat pet. Zenny is silent; Pehlichi does the talking
([STORY-AND-DIALOGUE.md](STORY-AND-DIALOGUE.md)).

| The player... | Pehlichi... |
|---|---|
| explores and notices signs that something is wrong | scans, which exposes the underlying Grid |
| commands Pehlichi (follow, stay, scan, repair, go-to) | accepts or refuses each command with a typed reason |
| creates physical access (salvages blockers, opens paths, terraforms) | must physically reach the repair point |
| gathers and delivers required resources; solves riddles and puzzles | performs the repair over time |
| removes, lures, distracts or evades Glitch Guards and other interference | can be interrupted, then resumes |
| protects Pehlichi during repair (fighting, hiding, distracting, timing) | receives permanent capability upgrades |
| decides where to push into interference | detects creature weak points; later disrupts or pacifies creatures non-lethally |

The in-game Pehlichi is **entirely separate from the real Pehlichi lab agent**.
It never calls a model, agent API, lab service or Pehverse runtime
([ADR-0006](ADR/0006-companion-isolated-from-lab-agent.md)). Its classes use
the `GL` prefix.

## 2. Glitch lifecycle

States (`EGLGlitchState`, in `GridlandsCore`):

| State | Meaning | Player can see it? |
|---|---|---|
| `Latent` | exists in the simulation, unknown to the player | no |
| `Detected` | revealed by a Pehlichi scan; one or more requirements unmet | yes, Grid-revealed |
| `Repairable` | detected and every requirement currently met | yes |
| `Repairing` | Pehlichi is actively repairing | yes |
| `Interrupted` | a repair stopped part-way; progress kept per the definition's policy | yes |
| `Repaired` | terminal; persistent world change applied, reward granted | as restored world |

Every transition names an **authority** (`EGLGlitchAuthority`):

| From -> To | Allowed authority | Example |
|---|---|---|
| Latent -> Detected | `PehlichiScan` | scan strength meets the glitch's detection requirement |
| Detected -> Latent | `Hostile` | the hostile AI jams/obscures it again *(later)* |
| Repairable -> Latent | `Hostile` | as above *(later)* |
| Detected -> Repairable | `World` | the last unmet requirement becomes satisfied |
| Repairable -> Detected | `World` | a requirement is lost (a guard returns, a path closes) |
| Repairable -> Repairing | `PehlichiRepair` | Pehlichi reached the repair point and began |
| Repairing -> Interrupted | `PehlichiRepair`, `Hostile`, `World` | recalled by the player / attacked / requirement lost |
| Interrupted -> Repairing | `PehlichiRepair` | Pehlichi resumes |
| Interrupted -> Detected | `World` | requirements lost while interrupted |
| Repairing -> Repaired | `PehlichiRepair` | repair completed |

Anything not in this table is illegal. In particular:

- **`Player` authority has no legal transition at all.** The player changes a
  glitch only indirectly: by commanding Pehlichi, or by changing the world
  that requirements observe.
- `Repaired` is terminal and is reachable only from `Repairing` by
  `PehlichiRepair`.

The table lives in one place (`FGLGlitchLifecycle`, Core) and is covered
exhaustively by `Gridlands.Core.Glitch.Lifecycle.*` tests. Game code calls
`FGLGlitchLifecycle::IsTransitionAllowed` and has no transition logic of its own.

### Why "Accessible" is not a state

Access is a **requirement** (`RepairPointReachable`), not a lifecycle state.
Access, resources, guards and jamming change independently of one another.
Encoding each as a state would multiply states and transitions, while one
`Repairable` gate over a list of requirements covers every combination.

## 3. Runtime shape (GridlandsGame)

**Pehlichi (`AGLPehlichi`)**, built from explicit components rather than a
generic follower:

| Component | Concept |
|---|---|
| `UGLPehlichiCommandComponent` | receives player commands (`Command.Pehlichi.*` tags); accepts or rejects with `EGLCommandRejection` (Busy, OutOfRange, Unreachable, CapabilityTooLow, NotRevealed, RequirementsUnmet) |
| `UGLCompanionPositioningComponent` | follow / stay / move-to; the only part that could be a generic companion base |
| `UGLScanComponent` | runs a scan; produces `FGLScanResult` = list of `FGLScanFinding {Kind, SubjectId, Location, Confidence}`; kinds include Glitch now and Trace/Node/Decoy later |
| `UGLCapabilityComponent` | permanent capability levels keyed by tag; generic, so the player can hold one too |
| `UGLRepairComponent` | **the only holder of repair authority**; walks to the repair point, performs timed repair, handles interruption |

Repair authority is enforced by a passkey: glitch-mutating functions on
`UGLGlitchComponent` take an `FGLRepairAuthority` argument whose constructor
is private and befriends only `UGLRepairComponent`. Scan authority works the
same way with `FGLScanAuthority`.

**Glitch (`AGLGlitch` + `UGLGlitchComponent`)**: holds lifecycle state,
repair progress and a reference to its `UGLGlitchDefinition`. It does **not**
implement `IGLInteractable`.

**`UGLGlitchSubsystem`** (world subsystem): registry and spatial query for
scans; evaluates requirements as the `World` authority; broadcasts
state-change events used by revelation visuals and persistence.

**Requirements (`UGLGlitchRequirement`)** are C++ classes parameterized from
JSON. The bootstrap needs one or two, for example:

- `ObjectSalvaged(PersistentId)`: a physical blocker must be salvaged first.
- `ItemDelivered(ItemId, Count)`: materials placed into a receptacle actor.

Later: `NoHostilesWithin(Radius)`, `RepairPointReachable(TraversalClass)`,
`NotJammed`, `PuzzleSolved(PuzzleId)` (riddles and environmental puzzles).

**Scans under interference.** Scan findings carry a `Confidence`. Interference
at the scan location ([WORLD-AND-PROGRESSION.md](WORLD-AND-PROGRESSION.md)
section 4) lowers confidence and range, can hide findings, and at high tiers
blocks weak-point analysis. Pehlichi's capability level pushes back against
it. Scan reliability is therefore a function of capability and interference,
not a constant.

**What a repair does.** A repair is a persistent world change and the
source of truth for progression. From the set of repaired glitches, the world
**derives** ([ADR-0013](ADR/0013-derived-world-stability.md)):
- local stability and reduced interference;
- reduced NICE control and NICE's falling composure;
- safer travel and settlement; returning traders.

The repair itself grants its definition's **rewards**: Pehlichi capabilities,
knowledge, and player rewards where defined.

**Revelation.** Revealed glitches show a debug Grid visualization (bootstrap:
an emissive wire mesh plus debug draw). The finished scan-overlay shader is
out of scope.

## 4. Definition data (`Data/glitches/*.json`)

```json
{
  "id": "glitch.test.flicker_lamp",
  "detection": { "capability": "Capability.Pehlichi.Scan", "minLevel": 1 },
  "requirements": [ { "kind": "ObjectSalvaged", "target": "<persistent-guid>" } ],
  "repair": { "seconds": 5.0, "interruptPolicy": "KeepProgress" },
  "rewards": [ { "recipient": "Pehlichi", "capability": "Capability.Pehlichi.Scan", "delta": 1 } ]
}
```

The exact schema is fixed in M2 and validated by `Tools/import-data.sh`.
Illustrative fields beyond the bootstrap (`hiddenIn`, `accessClass`,
`guards`, `decoy`, `stabilization`) are added when their feature is built, not
before.

## 5. Future behaviours and where they plug in

Every one of these must keep the non-combat path viable (ADR-0009): a glitch
that can *only* be reached by killing never counts toward the combat-free
stabilization that NC-3 requires.

| Behaviour | Extension point |
|---|---|
| hidden inside buildings/objects | detection requirement plus an occlusion factor in the scan query |
| needs higher scan capability | `detection.minLevel` |
| only Pehlichi can access / physically enter | `RepairPointReachable(TraversalClass)` + `IGLReachability` |
| defend Pehlichi during repair | `Hostile` -> `Interrupted`; creature `Hunting` targets the repairer; defending includes hiding, distracting and timing, not only fighting |
| Glitch Guards | guards are creatures bound to a glitch; `NoHostilesWithin` requirement, which must be satisfiable without a kill (lure, distract, evade, wait) for the glitch to count as combat-free (ADR-0009) |
| riddles and puzzles | `PuzzleSolved(PuzzleId)` requirement; puzzle state is a persisted world fact |
| stabilization pushes back static | repaired glitches feed the derived stability model; **no quota gates anything** ([ADR-0011](ADR/0011-radial-bands-and-soft-interference.md)) |
| Pehlichi's regional estimate ("roughly 35 signatures") | a region-level `FGLScanResult` summary; accuracy depends on scan capability and interference; informative, never a gate |
| NICE bribes Pehlichi to stop | a dialogue/story beat driven by NICE's derived composure; it changes nothing in the lifecycle table |
| jammed/obscured by the hostile AI | `Hostile` -> `Latent` transitions; `NotJammed` requirement |
| false signatures / decoys | `FGLScanFinding.Kind = Decoy`, low confidence; definition flag |
| stabilizing the local simulation | optional reward effect consumed by `UGLStabilitySubsystem` |
| upgrades to player, Pehlichi or both | `FGLReward.recipient` |
