# P1 evidence: the puzzle proof (ADR-0023)

Verified commit: `0409098765dfda4f919a27b742f61a48be404e8e`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-0409098.index.json` | fresh clone: 69 tooling tests, 97 entities valid, build, **54/54** automation tests, 0 warnings | PASS |
| `02-real-game-before-fix.log-excerpt.txt` | **found by playing**: the riddle was solved, but NICE's riddle and all of Pehlichi's hints were never heard. Story-critical lines were dropped while another played (and a small queue then dropped the oldest) | defect |
| `03-real-game-riddle.log-excerpt.txt` | the real game after the fix: reveal, riddle, two hints, "that's all I can pull out of it", placing the map, NICE conceding, Pehlichi repairing, all heard in order | PASS |
| `04-mutation-deferral.diff` | dropping story-critical lines again fails `StoryCriticalIsDeferredNotDropped` | rejected |

Other mutations run and rejected (not kept as files):
- Hint cap ignoring Analysis (`Next > MaxHintTier`) fails `AnsweredByPresentingNotBySpeaking`
  (hint 3 given at Analysis 1).
- `Requirement.PuzzleSolved` always met fails the same test (the glitch became repairable before
  the map was placed).

New tests:
- `Gridlands.Game.Puzzle.AnsweredByPresentingNotBySpeaking`: revealing the glitch poses the riddle;
  hints 1 and 2, then exhausted at Analysis 1; talk solves nothing; the stand refuses empty hands
  and the wrong item; the glovebox yields the map; placing it solves the riddle and consumes the
  map; Pehlichi then repairs, and his Analysis grows to 2.
- `Gridlands.Game.Puzzle.HintCapFollowsAnalysis`: Analysis 2 reaches hint 3; there is no hint 4.
- `Gridlands.Game.Puzzle.SolvedStateSurvivesReload`: solved, posed and hint level persist; the
  glovebox stays empty; reload doesn't re-pose the riddle.
- `Gridlands.Game.Dialogue.StoryCriticalIsDeferredNotDropped`: a riddle posed during the opening
  plays after it, and its hint after that.
- Static (`test_architecture_rules.py`): no dialogue code calls `Solve`; only `GLPuzzleSite.cpp`
  does. Tooling: PZ-1, PZ-2 (TEXT answers and CONSTRUCT refused).
