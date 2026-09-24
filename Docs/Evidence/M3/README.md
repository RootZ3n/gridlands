# M3 evidence: character, interaction, event bus, anchors, origin blockout

Verified commit: `30e72b3cd4ab913a3cf084dc366fa9fc75ef4708`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-30e72b3.index.json` | fresh clone (including the Git LFS map): validate, build, **20/20** automation tests, 0 warnings, tree unchanged | PASS |
| `02-origin-game-start.jpg` | the real game (Vulkan, `-RenderOffscreen`) at the player start: Zenny's placeholder body, two debug interactables, the street, a house with its garage | rendered |
| `03-game-run.log-excerpt.txt` | the same run: content loaded (57 entities, 15 anchors, 0 problems), `L_Origin` loaded, game class `GLGameMode`, screenshot taken | PASS |

New tests:
- `Gridlands.Game.Events.SubscribersReceiveMatchingTags`: parent-tag subscribers
  get child events; unsubscribe works; history is bounded; emitting without a
  world is safe.
- `Gridlands.Game.Interaction.FocusAndInteract`: focus within reach; use; the
  interaction event is emitted; disabled options explain themselves and can't be
  performed; nothing is focused when looking away or out of reach.
- `Gridlands.Game.Character.InputIsDefinedInCode`: Move, Look, Jump and Interact
  actions with the right value types; WASD, mouse, space, E and gamepad mapped;
  the game mode spawns Zenny.
- `Gridlands.Editor.Anchors.ExportMatchesCommitted`: the committed anchor file
  equals a fresh export.
- `Gridlands.Editor.Blockout.OriginMatchesBrief`: at least 8 modern houses, one
  1950s fragment, one Roman fragment, one storm drain, one player start, the
  Gridlands game mode.

Not done by the agent: a keyboard-and-mouse walk-around. That's for the operator.
Open the project and press Play, or run the game.
Blockout lighting reads dark from the start view (sun behind the buildings);
it's cosmetic and left for art.
