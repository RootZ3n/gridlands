# M8 evidence: Pehlichi fixes the world (stability, interference, static)

Verified commit: `0001b726e1b549437dfcfe3e5b87f068c53f81c4`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-0001b72.index.json` | fresh clone: 64 tooling tests, 83 entities valid, build, **46/46** automation tests, 0 warnings | PASS |
| `02-mutation.diff` + `02-mutation-repairs-worsen-interference.index.json` | making repairs raise interference fails the Core monotonicity test and the real-loop game test (0.72 -> 0.72) | rejected |
| `03-before-repairs.jpg` / `03-after-repairs.jpg` | the real game at spawn. **Before:** `Grid: Hazy (0.37)`, static veil and specks, NICE composure 100%. **After** Pehlichi repairs the three glitches: `Grid: Clear (0.00)`, NICE composure 0% | rendered |
| `04-sun-brightness.txt` | **the sun is up**: measured brightness of Zenny and house fronts at spawn | measured |
| `05-demo-run.log-excerpt.txt` | the dev command drove the real loop: every glitch -> Repaired via Pehlichi; sun rotation as intended | log |

New tests:
- `Gridlands.Core.Stability.DerivedDeterministicAndMonotone`: same states give
  the same numbers; over a 25x25 grid, no repair ever raises interference and
  each repair clears the air around it; clamped 0..1.
- `Gridlands.Core.Stability.NiceComposureAndTiers`: composure 1 -> weight share
  -> 0; tier thresholds.
- `Gridlands.Game.Stability.StaticRecedesWhenPehlichiRepairs`: the real loop on
  the lamp. Interference drops by more than 0.15, the tier drops, composure falls,
  `Event.World.Stabilized` carries it, and the next band applies beyond the slice.
- Tooling S-1: no runtime UPROPERTY stores interference, composure or stability.

Fixed along the way:
- The sun was nearly overhead because `SpawnActor` composed the spawn rotation
  with the light's default -46 degree pitch.
- Regenerating the blockout over an existing map crashed; it is now idempotent.
