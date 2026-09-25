# ADR-0023: Zenny answers puzzles through gameplay, never through dialogue or text

- Status: Accepted
- Date: 2026-09-25
- Decider: operator

## Decision
- **Zenny never answers a riddle or puzzle through spoken dialogue, free-text input,
  or a multiple-choice dialogue response.** His silence is part of the character.
  The world is the input device.
- NICE poses riddles; NICE and Pehlichi may discuss them. **Their dialogue never
  completes a puzzle.** Only a gameplay action does.
- A data-driven puzzle interaction model with four **answer modes**:

  | Mode | Zenny... | Example |
  |---|---|---|
  | **PRESENT** | brings, shows or places the right item | "cities but no houses..." → find a map, place it at the glitch |
  | **MANIPULATE** | operates something | switches, valves, a clock's hands, redirecting a light |
  | **PERFORM** | does something with his body | walk a revealed path ("footsteps"), stand still, extinguish a light |
  | **CONSTRUCT** | builds or terraforms the answer | *reserved until building/terraforming exist (M10)* |

- **Pehlichi is the optional, escalating hint system**: vague, then stronger, then
  explicit. His hint quality may improve with an analysis capability, so players who
  invest in Pehlichi earn a better puzzle companion. NICE mocks excessive
  hint-asking.
- **Riddles are one puzzle family among many** (environmental logic, signal,
  construction, observation, navigation, electrical, memory, ...), so that "hidden
  glitch" never becomes "answer another riddle".
- **Puzzle completion is world state** (a persisted fact), compatible with the
  world save. A glitch's `Requirement.PuzzleSolved` observes it like any other
  requirement.
- Build the **smallest representative proof** first (one PRESENT riddle, with
  escalating hints), not a large puzzle framework.

## Consequences
- No text-matching problems ("footstep" vs "footsteps" vs "tracks").
- Puzzles reuse existing systems: items and inventory (PRESENT), interaction
  (MANIPULATE), the event bus (PERFORM), and later building (CONSTRUCT).
- Hints are exchanges in the dialogue system, gated by hint level and capability.

## Proof (2026-09-25): the map riddle, PRESENT
[Evidence](../Evidence/P1-puzzle/README.md). Data: `puzzle.home.map_riddle` (answer
`PRESENT item.misc.paper_map`), `glitch.home.cartographer_error` (requires it), a `puzzle_site`
placement (the stand) and a glovebox that yields the map. Runtime: `UGLPuzzleSubsystem` (posed /
solved / hint level; hints capped by `capability.pehlichi.analysis` `puzzle_insight`),
`AGLPuzzleSite` (the only caller of `Solve`, enforced by a static rule; dialogue code never calls
it), `Requirement.PuzzleSolved` in `GLGlitchRules`, and puzzle state in the world save.
Validator: PZ-1 (answer item must exist), PZ-2 (TEXT answers and the reserved CONSTRUCT are
refused). MANIPULATE and PERFORM exist in the schema only; each gets its runtime with its first puzzle.

Found by playing it: a riddle posed while another story-critical exchange played was never heard,
though its hint level still advanced. Story-critical exchanges are now **deferred, never dropped**
(`FGLDialogueChoice::bDeferred`, the director's queue).

## Reversal cost
Identity-level for the "no dialogue answers" rule; the answer modes are extensible data.
