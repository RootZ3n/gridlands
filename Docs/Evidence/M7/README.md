# M7 evidence: Pehlichi scans, detects and repairs glitches

Verified commit: `dbf96092960e3bb068f1aa2d58f3771744957e1e`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-dbf9609.index.json` | fresh clone: 62 tooling tests, 81 entities valid, build, **43/43** automation tests, 0 warnings | PASS |
| `02-mutation.diff` + `02-mutation-scan-ignores-level.index.json` | the scan ignoring a glitch's required level fails `ScanRevealsByRangeAndLevel` | rejected |
| (same diff, tooling) | making `FGLRepairAuthority`'s constructor public ("anyone can repair") fails the static P-1 rule `test_passkeys_are_private_and_befriend_one_class` | rejected |
| `03-pehlichi-beside-zenny.jpg`, `04-game-run.log-excerpt.txt` | the real game: Pehlichi spawns beside Zenny; 6 placements (3 salvage, 3 glitches); the story-critical opening plays | rendered |

New tests:
- `Gridlands.Game.Pehlichi.PlayerHasNoWayToRepair`: glitches (actor and every
  component) are not interactable, have no collision, spawn Latent and invisible.
- `Gridlands.Game.Pehlichi.ScanRevealsByRangeAndLevel`: level 1 reveals the
  lamp in range; the transformer out of range stays hidden; the level-2 glitch
  stays hidden even point-blank.
- `Gridlands.Game.Pehlichi.LampLoopRevealPrepareRepairUpgrade`: **first-playable
  loop steps 5-12**. Hidden; repair refused before a scan; scan reveals;
  refused with `RequirementsUnmet` (a banter event); Zenny salvages the blocker;
  repairable; repair; busy refusal; repaired; scan permanently level 2, which
  reveals the buried signal.
- `Gridlands.Game.Pehlichi.ItemDeliveryInterruptAndResume`: the fuse makes it
  repairable; Pehlichi takes the fuse; recalling him interrupts; the delivered fuse
  still counts; progress is kept; resume completes; the knowledge reward is granted.
- Tooling P-1: every glitch state mutator takes an authority passkey; passkey
  constructors are private and befriend exactly one class; there is no player
  authority.

Found and fixed while building M7:
- `FScene` collided with Unreal's renderer class in a unity build (caught at
  compile time thanks to `-DisableAdaptiveUnity`).
- Glitch visuals were bound in `BeginPlay`, which doesn't run for pre-play spawns;
  they now bind in `PostInitializeComponents`.
- A P-1 static check once failed for the wrong reason under mutation (it parsed
  the wrong `public:`). It now parses the component class.
