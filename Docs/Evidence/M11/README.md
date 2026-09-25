# M11 evidence: the first vertical slice

- Verified commit: `a9428fa91096a04a11ea462c41cb4a11237fd3bb`.
- Engine: Unreal 5.8.3 CL 58210709.
- Decisions: [ADR-0025](../../ADR/0025-slice-creature-combat-storm-v0.md), plus the earlier ADRs
  for every system the slice integrates.

| File | What it proves | Verdict |
|---|---|---|
| `00-fresh-clone.summary.txt`, `00-tests-fresh-clone-a9428fa.index.json` | Fresh clone: 81 tooling tests, 143 entities valid, build, **78/78** automation tests, 0 warnings. | PASS |
| `01-session1-integrated-slice.log-excerpt.txt`, `01-session1-save-summary.txt` | **One world, the whole slice (real game).** See the session 1 list below. | PASS |
| `02-session2-world-still-changed.log-excerpt.txt`, `.jpg` | **Fresh launch:** "restored 5 glitches, 2 salvaged placements, 16 build pieces, 10 edited ground vertices". The storm does not repeat and the opening does not replay. The frame shows Grid Clear (0.00), NICE composure 0% and no static, with the shelter, mound and pit.\* | PASS |
| `03-real-game-gremlin-chase-and-distraction.log-excerpt.txt` | **The creature in the real game**, on the drain room's navmesh. It sees Zenny and chases (800 cm to 335 cm in 1.5 s). Pehlichi's noise sends it away: 622 cm to 143 cm from the noise in 3 s, in state Investigate. Zenny is untouched and the gremlin keeps full health (zero damage). | PASS |
| `04-mutations.diff` + `04-mutation-*.index.json` | (a) A chase ignores lures: `LureBeatsChase…` and `PehlichiDistractionIsNonLethal` fail, with Zenny left at 40 health. (b) The storm skips clean-up: `RainingCatsAndDogs…` fails, with 10 artifacts left in the world. | rejected |
| `05-storm-glitch-sprites.jpg`, `05-drain-room-gremlin-closing-in.jpg` | The storm's neon voxel "cats and dogs" reading as broken sprites, not animals; the drain room (pillars, side-channel wall, lights) with the violet gremlin approaching.\* | — |

\* The screenshots were taken on the working tree just before the final storm fixes (the
concurrency cap and the test counting world actors). Those fixes change neither geometry nor
rendering.

**Session 1** (`01-*` files), in order:
1. The riddle is answered by placing the map.
2. All **5** glitches are repaired, including the storm drain's echo loop.
3. **NICE's storm starts on the second repair.** It rains for its full 40 s (119 sprites) and
   cleans up the 10 still in the air at the end.
4. The shelter is built (16/16).
5. Dig and raise conserve soil; digging under the shelter is refused.
6. 24 dialogue lines play.
7. The save holds: the relaxed yield preset, 5/5 repaired, the storm marked as happened,
   16 pieces, the ground delta, the solved riddle, and Pehlichi's analysis, distract and scan
   each at level 2.

## Acceptance map (the operator's list)

| Slice item | Where it is shown |
|---|---|
| **The place** | |
| Suburban start, 1950s fragment, smaller Roman fragment | Blockout (`OriginMatchesBrief`). Discoveries: `DiscoveriesSilenceAndPersistence`, plus the diner and colonnade exchanges. |
| Storm-drain/sewer room | Authored blockout room with baked nav bounds and travel points. `OriginMatchesBrief` counts 2 travel points; row 03 runs there. |
| **Economy and building** | |
| Physical salvage, inventory, fabrication and pry bar | M4–M7 tests; `FMakesWhatZennyLacks`. |
| Resource-yield setting | `-GLSettings=`, saved with the world (row 01: relaxed); `WorldSettingsScaleRepeatableYields`. |
| Shelter construction, terrain manipulation | M10 gates; rows 01 and 02. |
| **Glitches, Pehlichi, puzzle** | |
| 4–5 glitches, Pehlichi scan/detect/repair | 5 glitches, row 01. `GremlinCanBeAvoidedAndTheDrainFixedWithoutAFight`. |
| The physical riddle | P1 tests; row 01. |
| Static receding, save/load | M8 and M9; row 02 (Clear 0.00 after reload). |
| **The creature** | |
| One creature that can be fought | `ZennyCanFightTheGremlin`: 3 pry-bar swings, drop, death and respawn. |
| It can be avoided | Cone, line of sight and leash (`CreatureSeesOnlyWhatItShould`). The drain glitch is fixed without a fight. |
| A non-combat solution | Pehlichi's distraction: `PehlichiDistractionIsNonLethal` and row 03. |
| Pehlichi deals zero damage | P-3 static rule. Gremlin health unchanged (row 03, tests). |
| No activity-triggered spawning | Only the placement subsystem spawns creatures (static rule); CR-2. |
| **The storm** | |
| Raining Cats and Dogs Glitch Storm | `RainingCatsAndDogsIsBoundedAndCleansUp`: trigger, bounded, rains all storm long, harmless, cleaned up, never repeats, not raining after a load. Row 05 image. |
| **Dialogue** | |
| Contextual NICE/Pehlichi dialogue, about 40 exchanges | 49 exchanges. `SliceDialogueTests`: 12 families, at least 15 sparring pairs, every non-critical line rate-limited. Cooldowns, priority, queueing and repetition protection are unchanged (M5 and P1). |

## Defects found and fixed in M11
- **The discovery line lost to a generic line.** Learning a discovery's knowledge fired "first
  unlock", and the more specific discovery line was then dropped as busy. The discovery event now
  comes first.
- **A false green in the storm test.** It counted only the artifacts the storm still tracked, so
  orphaned artifacts were invisible to it. It now counts world actors, and the mutation fails it.
- **The rain stopped halfway.** `maxArtifacts` acted as a lifetime total, so the rain stopped at
  20 s of 40. It now means "at most this many at once".
- **Build and terrain edits waited for quit or a repair to be saved.** A crash would have lost
  them. They now autosave, debounced by 5 s (row 01's save includes the shelter).
- **Two process slips in my own work, recovered:**
  - A `-ExecCmds` screenshot helper used `UEngine::Exec` and never ran; it now uses deferred
    commands.
  - Restoring after a mutation with `git checkout` once reverted uncommitted work. It was
    recovered from a verified backup, and backups are now checked before every mutation.
