# M6 evidence: fabrication and knowledge unlocks

Verified commit: `21c0982f7266d79d29e50c17f8bc805af5e022ca`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-21c0982.index.json` | fresh clone: 59 tooling tests, 73 entities valid, build, **39/39** automation tests, 0 warnings | PASS |
| `02-mutation.diff` + `02-mutation-craft-not-transactional.index.json` | consuming inputs on a refused craft fails `CraftIsTransactional` (47 scrap left instead of 50) | rejected |

New tests:
- `Gridlands.Core.Fabrication.CraftIsTransactional`: short inputs are refused and
  nothing is consumed; exact consumption and output; no room for the output means
  nothing is consumed; consuming the inputs can make the room.
- `Gridlands.Core.Fabrication.KnowledgeAndStationGates`: AND semantics with the
  missing ids reported; learning is idempotent.
- `Gridlands.Game.Fabrication.PryBarFromSalvagedScrap`: **first-playable loop
  steps 2-4**. Salvage by hand, fabricate the pry bar from salvaged scrap
  (`Event.Item.Fabricated`), then the next fence takes 2 hits instead of 4.
- `Gridlands.Game.Fabrication.StationsInReach`: a bench counts only within reach.
- `Gridlands.Game.Knowledge.UnlocksFromDataDrivenSources`: the first copper wire
  teaches its uses and salvaging a fence teaches timber framing, each announced
  once; unknown ids are refused.
- Tooling: `KN-1` fires when a knowledge entry declares a source nothing provides.

Behaviour change noted: salvaging the wiring run now also teaches copper-wire uses,
so the dialogue burst's single reaction can be the knowledge line. The M5 burst
test was changed to assert the invariant (one exchange, answering the burst)
rather than a specific exchange.
