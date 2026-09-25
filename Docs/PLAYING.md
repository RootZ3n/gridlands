# Playing the current build

What exists: the modern-suburbia origin cell on runtime ground you can dig, raise and flatten;
building a small timber shelter;, NICE's static, salvage and fabrication, Pehlichi's
scan and repair, one riddle answered through gameplay, and a world that stays changed across quits.

## Launch
```
/pehverse/engines/UE_5.8.3/Engine/Binaries/Linux/UnrealEditor "$PWD/Gridlands.uproject" -game
```
The game loads `Saved/SaveGames/Gridlands/world.json` if it exists. To start a new world, add
`-GLNewWorld` or delete that file. It autosaves after every repair and on quit.

## Controls
| Key | What it does |
|---|---|
| WASD, mouse, Space | move, look, jump |
| E | use what you're looking at (salvage, pick up, place something) |
| F | fabricate |
| Q | ask Pehlichi to scan (reveals glitches in range) |
| R | ask Pehlichi to repair the revealed glitch nearby |
| G | Pehlichi follows / stays |
| H | ask Pehlichi for a hint on NICE's current puzzle (escalating; capped by his Analysis) |
| F5 / F9 | quick save / quick load |
| B | build mode on/off (a ghost shows where the piece would go, green if it fits and red if not; the line at the bottom of the screen says why) |
| mouse wheel | choose a piece (floor, wall, doorway, roof, Roman wall) |
| Z | rotate the piece a quarter turn |
| left mouse | place the piece (build mode) or use the shovel (terraform mode) |
| X | demolish the piece you're aiming at; anything it held up falls, and you get everything back |
| T | terraform mode: dig, then raise, then flatten, then off (needs a shovel) |

Building and terraforming bindings are provisional; tell me what feels wrong.

Zenny never talks. Pehlichi is the only one who repairs anything; you make repairs possible.

## Building a shelter
1. Learn timber framing. Salvage a backyard fence panel, or let Pehlichi fix the dead transformer.
2. Gather planks: 4 fence panels (3 each) and 2 garden sheds behind the houses (12 each). A 4 m
   shelter takes 32: 4 floors, 7 walls, 1 doorway and 4 roof slopes, at 2 planks each.
3. Put floors down on level ground. On a slope the bottom line says "not on firm, level ground",
   so flatten it with the shovel first. Walls snap to floor edges and roof slopes snap to wall
   tops. Rotate with Z.
4. Support weakens as you build up and out. Timber stands a floor plus three walls high, and a
   floor can hang one piece out over a drop.

Shovel: press F with 2 scrap metal and a plank (F makes a tool you don't already have).
Digging gives soil; raising spends it. You can't dig under your own floors.

## Things to find (spoilers)
- The flickering lamp: clear the junk pile blocking it, then scan and repair.
- The dead transformer needs a fuse (fabricate one).
- The buried signal only shows up once Pehlichi's scan has improved.
- The cartographer's error, east along the road (about 90 m): scanning it makes NICE pose a riddle.
  Talk solves nothing. Answer by bringing her what she describes and placing it on the stand
  beside the glitch. Look in the car glovebox on the way there.
