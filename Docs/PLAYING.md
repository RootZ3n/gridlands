# Playing the current build

What exists: the first vertical slice. The modern-suburbia origin cell sits on runtime ground you can
dig, raise and flatten. You can build a small timber shelter. There's a storm drain with a gremlin
you can fight, sneak past, or let Pehlichi distract, and NICE's Raining Cats and Dogs storm., NICE's static, salvage and fabrication, Pehlichi's
scan and repair, one riddle answered through gameplay, and a world that stays changed across quits.

## Launch
```
/pehverse/engines/UE_5.8.3/Engine/Binaries/Linux/UnrealEditor "$PWD/Gridlands.uproject" -game
```
The game loads `Saved/SaveGames/Gridlands/world.json` if it exists. To start a new world, add
`-GLNewWorld` or delete that file.
- It autosaves after every repair, a few seconds after building or terraforming, and on quit.
- A new world can take a resource-yield setting: `-GLSettings=settings.preset.relaxed` doubles
  repeatable yields. The default is `settings.preset.default`.

## Controls
| Key | What it does |
|---|---|
| WASD, mouse, Space | move, look, jump |
| E | use what you're looking at (salvage, pick up, place something; on your storage crate: store every material you carry, tools stay in hand) |
| L | take everything out of the storage crate you're looking at (what doesn't fit stays in the crate) |
| F | fabricate |
| Q | ask Pehlichi to scan (reveals glitches in range) |
| R | ask Pehlichi to repair the revealed glitch nearby |
| G | Pehlichi follows / stays |
| H | ask Pehlichi for a hint on NICE's current puzzle (escalating; capped by his Analysis) |
| F5 / F9 | quick save / quick load |
| B | build mode on/off (see **Build mode** below) |
| left mouse | in build mode: what the build bar's mode says (place, choose, finish, remove). Terraform mode: use the shovel |
| T | terraform mode: dig, then raise, then flatten, then off (needs a shovel) |
| left mouse (no tool out) | swing at a creature in front of you (best weapon you carry: pry bar > shovel > fists) |
| V | Pehlichi makes a glitchy noise where he is; creatures nearby go and look (he never hurts anything) |

Building and terraforming bindings are provisional; tell me what feels wrong (F8, below).

## Build mode
(P12 candidate. Numbers are provisional.) Press B. The build bar at the bottom always says which of four modes you are
in, what the piece is, its angle, what it costs and where that comes from, and the structural state as a word, an icon
and a colour: **OK [+]** (green, stands with margin), **LIMIT [!]** (yellow, stands but at its material's limit),
**NO [x]** (red). The line under it says why, in the same words the game used to decide.

| Key | PLACE (default) | BROWSE (Tab) | FINISH (Y) | REMOVE (X) |
|---|---|---|---|---|
| left mouse | place the piece | choose the highlighted piece, back to placing | install the chosen finish on the frame you aim at | take the piece apart (if others would fall: hold it, see below) |
| mouse wheel | next/previous piece in this category | move down/up the list | next/previous finish that fits | - |
| Shift+wheel | next/previous category | next/previous category | - | - |
| Esc / right mouse | leave build mode | back to placing | back to placing | back to placing |

Everywhere in build mode:

| Key | What it does |
|---|---|
| Z / Shift+Z | rotate +90 / -90 degrees |
| C | rotate +15 degrees |
| Ctrl+wheel | rotate 2.5 degrees (the finest step; every angle is a multiple of it) |
| Tab | open/close the piece browser (categories; the era filter only narrows the list, it never forbids anything) |
| 1-8 / Ctrl+1-8 | pick / pin a favorite. The browser also shows your recent pieces |
| X / Y | remove mode / finish mode on and off |
| N | in remove mode: careful (components back) or smash (quicker, fewer studs, more scrap). Both yields are shown before you click |
| Alt (hold) | build camera: pulled back and up around Zenny. Shift+wheel raises or lowers it. You still aim with the centre of the screen |
| F8 | note what's annoying (see below) |

- **Snapping.** A small marker shows the socket the piece is snapping to; the build bar says "angle from the socket" when the
  socket sets the angle (angle posts). Snapped and placed are the same position.
- **Removing.** The piece and everything that would fall with it are highlighted in red, with what you'd get back. If
  nothing else falls, one click removes it. If something else would fall, hold the button until the ring fills.
- **Base area.** In build mode a ring shows your base area (32 m around the base core, provisional).
- **Accessibility.** In the console (`~`): `gl.Build.CameraMode toggle` makes Alt a toggle; `gl.Build.ConfirmMode toggle`
  turns the hold into press-twice; `gl.Build.TextScale 1.5` enlarges the build text. Saved in `Saved/Profile/build-profile.json`
  with your favorites, recents and finish choices (not in the world save).
- **New pieces:** a straight timber stair (Stairs), a window wall (a stud wall with an opening, finished like any wall) and an
  upper floor that rests on wall tops.

### F8: the friction log
F8 opens a small picker: choose what grated (CAMERA, SNAP, ROTATION, BROWSER, PIECE_FIND, FINISH, STRUCTURE_FEEDBACK,
REMOVAL, SALVAGE, RESOURCE, CLAIM, INPUT, REPETITION, VISIBILITY, OTHER), optionally type a note, Enter. From the
console: `gl.Friction SNAP walls jump at the corner`. Each note is one line in `Saved/Playtest/friction.jsonl` with the
build context (mode, piece, finish, angle, snapped, structural state, reason, time) and nothing else. It is not a save.

### Dev only: the starter kit
`gl.Dev.BuildingStarterKit` (console) teaches every building knowledge and gives 300 studs, 300 planks, 30 logs and 30 cut
stone, so a session can test building rather than gathering. It does not exist in a shipping build.

Zenny never talks. Pehlichi is the only one who repairs anything; you make repairs possible.

## Building a shelter
(Building v1, P11; build mode, P12 candidate. Numbers are provisional.)
1. Learn timber framing. Salvage a backyard fence panel, or let Pehlichi fix the dead transformer.
2. Floors cost planks; walls, doorways and roof frames are framed from **studs**. Fell a pine for logs and saw them into
   studs at a sawhorse (F), or dismantle framing carefully to get studs back. Finishes (clapboard, boards, shingles) cost planks.
3. **A base:** place a base core. Within 32 m of it your storage crates (they stand on a floor) supply building and
   crafting while you are there, before your own pockets; what you take apart goes to your pockets first, then the crates.
   There is no carrying weight: only slots and stacks (100 of most building materials per stack).
4. Put floors down on level ground. On a slope the bottom line says "not on firm, level ground",
   so flatten it with the shovel first. Walls snap to floor edges and roof slopes snap to wall
   tops. A stair stands on a floor and reaches a wall-top upper floor 2.7 m up.
5. Support weakens as you build up and out. Timber stands a floor plus three walls high, and a
   floor can hang one piece out over a drop.

Shovel: press F with 2 scrap metal and a plank (F makes a tool you don't already have).
Digging gives soil; raising spends it. You can't dig under your own floors.

## The storm drain and its gremlin (spoilers)
The culvert is at the north end of the cross street. Press E on the manhole to climb down, and E
on the one below to come back up. A static gremlin guards the drain's glitch, and there are three
ways past it:
- **Sneak:** it faces the way you came in. The walled side channel on your right leads around it.
- **Distract:** tell Pehlichi to stay somewhere (G), walk away, then press V. It goes to look.
- **Fight:** three hits with the pry bar. It hits back (12 a strike); if you go down, you wake at
  the start with everything you carried.

Your health bar is top left.

## NICE's storm (spoilers)
After Pehlichi's second repair, NICE loses her temper and it rains cats and dogs for 40 seconds.
They're glitch sprites and they don't hurt.

## A second cell (architecture proof, not a new zone)
East of home, past the cyan posts (about 512 m east of the start: cells are 1 km, ADR-0027), is a
second Grid cell: the Diner Lots. It streams in while you walk, with no loading screen (ADR-0028). It is deeper (more static), mostly 1950s, and has one glitch and a junk pile. The
posts and the "Cell:" line on the HUD are temporary development markers. Walk back and forth:
whatever you change on either side should still be there when you return.

## Things to find (spoilers)
- The flickering lamp: clear the junk pile blocking it, then scan and repair.
- The dead transformer needs a fuse (fabricate one).
- The buried signal only shows up once Pehlichi's scan has improved.
- The echo loop, in the storm drain (see above).
- The diner and the Roman columns: go and look, and NICE and Pehlichi have opinions.
- The cartographer's error, east along the road (about 90 m): scanning it makes NICE pose a riddle.
  Talk solves nothing. Answer by bringing her what she describes and placing it on the stand
  beside the glitch. Look in the car glovebox on the way there.
