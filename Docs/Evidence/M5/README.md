# M5 evidence: dialogue director v0

Verified commit: `f984ba7782bc07092901af1deb094b42a4615108`. Unreal 5.8.3 CL 58210709.

| File | What it proves | Verdict |
|---|---|---|
| `01-fresh-clone.summary.txt`, `01-tests-fresh-clone-f984ba7.index.json` | fresh clone: **58** tooling tests, 71 entities valid, build, **34/34** automation tests, 0 warnings | PASS |
| `02-mutation.diff` + `02-mutation-d2-no-story-bypass.index.json` | making story-critical lines obey the frequency setting fails `StoryCriticalIgnoresSetting` for all 50 seeds | rejected |

Honest note on the mutation: `Gridlands.Game.Dialogue.DirectorReactsToRealGameplay`
still passed under it, because its seed happened to win the Quiet roll. D-2 is
guarded by the Core test, not by the game test.

New tests:
- `Gridlands.Core.Dialogue.TriggersSubjectsAndHistory`: hierarchical triggers,
  subject filter, event-count requirements (the third-death line waits for death 3).
- `Gridlands.Core.Dialogue.StoryCriticalIgnoresSetting` (**D-2**): under Quiet,
  inside the silence gap, while chatter plays, story still plays. Story never
  interrupts story, and chatter never interrupts anything.
- `Gridlands.Core.Dialogue.SilenceGapAndRepetitionLimits` (**D-3**): silence gap,
  maxUses, per-exchange cooldowns; the unused exchange wins about 91% of draws
  against a worn one.
- `Gridlands.Core.Dialogue.DeterministicUnderSeed` (**D-5**): same seed and
  events give the same 300 choices; different seeds differ.
- `Gridlands.Core.Dialogue.FrequencyScalesOptionalOnly`: the ambient rate rises
  Quiet < Normal < Chatty < Unhinged (Quiet about 10%, Unhinged 100%); silence gaps shrink.
- `Gridlands.Game.Dialogue.DirectorReactsToRealGameplay`: the story opening under
  Quiet, never repeated; salvaging the wiring run gives one exchange (the fuse),
  not a pile-up; the wire joke later; history counts; only NICE and Pehlichi speak.
- Tooling (`test_architecture_rules.py`): **D-4**, no gameplay code includes
  the dialogue director; **P-3**, no damage calls in Pehlichi code.

Found during M5: two validator tests had gone stale since M3, when real anchors
were exported, because `test.sh` never ran the tooling tests. Fixed, and the gate
now runs them.
