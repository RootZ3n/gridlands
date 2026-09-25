# M9 evidence: the world save

Verified commit: `850cd2252d981bcd2cc35de41af69fe812387134`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-850cd22.index.json` | fresh clone: 64 tooling tests, 83 entities valid, build, **50/50** automation tests, 0 warnings | PASS |
| `02-mutation.diff` + `02-mutation-capabilities-not-restored.index.json` | a load that forgets Pehlichi's upgrades fails `WorldStaysChangedAcrossReload` (scan level 1 instead of 2) | rejected |
| `03-second-launch-world-still-changed.jpg`, `04-second-launch.log-excerpt.txt` | **the real game, two sessions.** Session 1: Pehlichi repaired every glitch through the real loop, autosaving after each, then quit. Session 2, a fresh launch: "Load: restored 3 glitches, 1 salvaged placements"; the frame shows `Grid: Clear (0.00)`, NICE composure 0%; the one-time opening line did not replay | PASS |

New tests:
- `Gridlands.Core.Save.CodecRoundTripAndVersions`: stable, human-readable JSON;
  Repairing is never written (it comes back Interrupted with progress and the
  delivered flag); newer, unversioned and garbage saves are refused.
- `Gridlands.Game.Save.WorldStaysChangedAcrossReload`: **first-playable loop
  steps 13-15** in a fresh world. The lamp stays repaired, the scan stays level 2,
  and salvage, inventory and dialogue history persist; recomputed interference is
  identical to before the save (derived, S-1).
- `Gridlands.Game.Save.MidRepairLoadsAsInterrupted`: progress is kept and the
  fuse isn't demanded twice or duplicated.
- `Gridlands.Game.Save.StaleIdsAreReportedNotFatal`: ids removed from content
  are reported; everything else applies.

The save file after session 1 (paths removed):
`schemaVersion 1`, `cell.home.origin`; the three glitches `Repaired`;
`salvagedPlacements: [placement.origin.junk_pile_01]`;
`pehlichiCapabilities: scan 2, distract 1`. No stability or interference fields.
