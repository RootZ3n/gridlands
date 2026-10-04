# P12 operator session: can a human actually build it?

This is the **DAILY-DRIVER ACCEPTED** gate. It comes after ENGINEERING GREEN, and P13 does not start until you have done it.
It should take about an hour. Nothing here needs the implementation report.

## 1. Launch
From the repository root, on the P12 branch (built with `Tools/build.sh`):
```
/pehverse/engines/UE_5.8.3/Engine/Binaries/Linux/UnrealEditor "$PWD/Gridlands.uproject" -game -GLNewWorld
```
- `-GLNewWorld` starts a fresh world. Later launches without it continue the same world.
- Open the console with `~` and type:
  ```
  gl.Dev.BuildingStarterKit
  ```
  It teaches every building knowledge and gives 300 studs, 300 planks, 30 logs and 30 cut stone (dev only; it does not
  exist in a shipping build).

## 2. Cheat sheet

| Key | What it does |
|---|---|
| B | build mode on/off |
| left mouse | what the build bar says: PLACE / choose (BROWSE) / FINISH / REMOVE |
| Tab | piece browser. Wheel moves, Shift+wheel changes category, left mouse chooses |
| wheel / Shift+wheel | (placing) next piece in this category / next category |
| 1-8, Ctrl+1-8 | favorite / pin the current piece as a favorite |
| Z, Shift+Z, C, Ctrl+wheel | +90, -90, +15, ±2.5 degrees |
| Y | finish mode: wheel picks the finish, left mouse installs it |
| X | remove mode. N switches careful/smash. If others would fall, hold left mouse until the ring fills |
| Alt (hold) | build camera (pulled back). Shift+wheel sets its height |
| Esc / right mouse | back to placing, then out of build mode |
| E / L | on a storage crate: store all materials / take everything |
| F5 / F9 | quick save / quick load |
| F8 | friction note |

Accessibility (console):
- `gl.Build.CameraMode toggle` makes Alt a toggle.
- `gl.Build.ConfirmMode toggle` turns the hold into press twice.
- `gl.Build.TextScale 1.5` enlarges the build text.

## 3. Friction notes: F8, any time
- Pick a category, type a note if you like, then press Enter. From the console: `gl.Friction SNAP the wall jumped`.
- Notes go to `Saved/Playtest/friction.jsonl`, one line each, with the build context (mode, piece, finish, angle,
  snapped, structural state, reason, time) and nothing else.
- Note anything that grates, even if it seems small. Not every note becomes code.

## 4. The manual WINCHESTER sequence
Mark each step **PASS** (fine), **FRICTION** (works, but grates: note why) or **FAIL** (wrong or impossible).

| # | Do this | PASS / FRICTION / FAIL | Notes |
|---|---|---|---|
| 1 | Pick level ground near the start. Place a floor, then a **base core** on it (Base & Utility). A ring shows the base area | | |
| 2 | Place a **storage crate** on a floor and press E on it to store your materials. The build bar's cost line should now say "from base storage" first | | |
| 3 | Enter build mode (B) and open the browser (Tab). Find a piece in each category | | |
| 4 | Choose frame pieces: floors, stud walls, a doorway, a window wall | | |
| 5 | Build **two rooms** (floors, walls on the edges, a doorway between them). Use repeat placement without reopening the browser | | |
| 6 | Make a **45-degree bay**: an angle post, then bay walls (the build bar says "angle from the socket") | | |
| 7 | **Mix vocabularies** on purpose: a log wing, Victorian clapboard and a Roman element. Nothing should refuse because of the era | | |
| 8 | Build a **porch** with a porch roof held by a **Roman column** | | |
| 9 | Build an **upper floor** on wall tops and a **straight stair** up to it. Walk up the stair | | |
| 10 | **Finish** walls deliberately (Y; wheel to choose, e.g. clapboard; then click 10 walls in a row) | | |
| 11 | Read the **structural feedback**: build out until you see LIMIT and NO, and read the reason line | | |
| 12 | Point at a support in remove mode (X): what would fall is highlighted red, with both yields | | |
| 13 | Remove a **redundant** support: one click, nothing else falls | | |
| 14 | Remove a **final** support (e.g. the porch column): hold to confirm, and the predicted pieces fall | | |
| 15 | **Carefully dismantle** another piece, and smash one more (N): compare what you got back with the preview | | |
| 16 | Save (F5), quit, relaunch **without** `-GLNewWorld` | | |
| 17 | The house is back as you left it, including finishes, the stair and the collapse debris | | |
| 18 | **Stream away:** walk east past the cyan posts and about 400 m on into the Diner Lots (roughly 900 m from the start; the home cell unloads), then walk back | | |
| 19 | The house is intact on return. **Keep building:** add a room, finish it, remove something | | |

Interaction counts (the scripted proof measured these with the same controls; your sense of them is what counts):
- placing 10 identical snapped walls;
- finishing 10 walls with the same finish;
- carefully dismantling 10 independent pieces.

Do any of these feel tedious? ______________________

## 5. Verdict
- [ ] **ACCEPTED**: the building experience is good enough to build P13 authoring on.
- [ ] **FRICTION FIRST**: these must be corrected before P13: ______________________
- [ ] **FAIL**: ______________________

Notes:

&nbsp;

&nbsp;
