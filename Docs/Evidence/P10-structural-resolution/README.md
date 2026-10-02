# P10: structural environmental resolution

**Built 2026-10-02 under the operator-approved proposal and decisions 1–5. READY FOR OPERATOR REVIEW: not merged,
not tagged.**
- Decision record: [ADR-0038](../../ADR/0038-structural-environmental-resolution.md). It amends
  [ADR-0030](../../ADR/0030-structural-salvage-and-deterministic-collapse.md); ADR-0033 and ADR-0037 carry notes.
- This is system work, not content. The fixture is the ordinary P6 carport (`structure.modern.carport`, structure data
  unchanged), placed in the P9 dev proof room. P9's warden gains the `Neutralize.Pinned` susceptibility, as data.

## The rule
1. **SUPPORT FAILED** decides the fall: trajectory, rest, impact time and volume, severity, and one collapse event.
   It never decides who is affected.
2. **IN FLIGHT** the target is free to move.
3. **IMPACT** decides from the world at that instant. For each active creature model touching the volume, exactly one
   of:
   - **pinned** (`Neutralize.Pinned`), if the impact's severity ≥ `collapse.pinMinSeverity` (2.0, provisional) and the
     creature is susceptible: no damage, no death, no kill, health kept;
   - the **ordinary environmental damage** path, which can DEFEAT it with the usual drops, credit and events.

   Already defeated or neutralized creatures are skipped.

## Real-game proof (`Tools/p9-dungeon-proof.sh structural …`, results in [`proof/`](proof/))
The route is played in the real game: real ticking, navigation and streaming, with Zenny's own movement input.
1. Through the west gap and the north corridor into the arena (detected 5 times; the script cannot hide, as in P9).
2. Zenny takes the carport's **first post**. It stands: the redundancy rule holds and nothing is in flight.
3. He takes Pehlichi under the hanging deck, orders him to **Stay**, walks back to the posts, and orders a
   **Distract**.
4. The warden comes to investigate. Once it stands under the decks, Zenny takes the **last post** (6 salvage hits
   through the ordinary pipeline).
5. **Support fails**, and both decks fall (about 1 s). The warden keeps moving: its distance to the east deck's centre
   drifts from 186 to about 200 cm during the fall.

### `structural` ([json](proof/dungeon-structural.json))
| Measure | Value |
|---|---|
| Warden outcome | **NEUTRALIZED, `Neutralize.Pinned`** (by the west deck, which it was under at impact) |
| Damage to the warden | **0**; health 240 of 240 |
| `Event.Creature.Defeated` / kill credit | **0 / none** |
| `Event.Creature.Neutralized` / `Event.Encounter.Resolved` | 1 / 1 (the reward, +5 residue, once) |
| Same impact on the two non-susceptible gremlins under it | **ordinary damage, 59 each** (60 hp: they survive with 1) |
| Impacts | 2 (one per deck): severity 2.45 / 2.46, damage 59 / 59.2 |
| Warden actor | present, inert |
| Restart ([json](proof/dungeon-structural-restart.json)) | still Pinned, inert, full health; **zero events**; residue unchanged |

### `structural-control` ([json](proof/dungeon-structural-control.json)): the non-susceptible control
The same route, except that Zenny first wounds the arena patrol in an ordinary fight (it has 52 of 60 hp left).
The same impact then:
- **pins the warden** (0 damage);
- **DEFEATS the wounded patrol through its health**: one `Event.Creature.Defeated`, and its 2 drops go to Zenny, who
  removed the support (residue 7 = the 5 reward + 2 drops);
- **wounds the full-health gremlin** to 1 hp.

The restart replays nothing. P10 did not replace structural damage with a boss-specific neutralization.

### `structural-midfall`: quit half a second into the fall
1. **`structural-midfall`** ([json](proof/dungeon-structural-midfall.json)): the run quits mid-fall, and the autosave
   holds **2 collapses in flight**. No impact has happened yet; the debris is decided but not landed.
2. **`structural-midfall-resume`** ([json](proof/dungeon-structural-midfall-resume.json)): a restart from that save.
   - The fall resumes and lands **once**: `Event.Structure.Collapsed` 0, `Event.Structure.Impact` 2.
   - The warden is **Pinned**, and the two gremlins are wounded to 1 hp.
   - Neutralized 1, Resolved 1, residue 5.

   This is the same outcome as the uninterrupted run.
3. **`structural-midfall-restart`**: zero events, nothing replays.

### `structural-unload`: leave the lots mid-fall
1. 0.4 s into the fall Zenny leaves for the origin, and the lots stream out with the fall frozen in their record.
2. Five seconds later he comes back.
3. The fall resumes where it stopped and lands once: the warden **Pinned**, gremlins wounded, the same outcome.
4. The restart replays nothing.

## P9 regression (results in [`p9-regression/`](p9-regression/))
The same scripts, re-run unchanged:

| Route | Result | Same as P9 evidence |
|---|---|---|
| direct | warden **Defeated** through 240 damage; 5 defeats; residue 13; Zenny took 252 (2 dev heals) | yes, identical |
| environmental | warden **Neutralized / Contained**, 0 damage; residue 5; Zenny took 432 (5 dev heals); 47 s | yes, identical |
| restarts | zero events; outcomes kept | yes |
| navscale | region 190 (inside 164) active tiles; **1 → 16 extra active creatures add 0 tiles**; without the region 225 | the carport adds 2 tiles to the room's baseline (188 → 190, 162 → 164) |
