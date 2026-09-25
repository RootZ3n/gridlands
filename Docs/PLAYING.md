# Playing the current build

What exists: the modern-suburbia origin cell, NICE's static, salvage and fabrication, Pehlichi's
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

Zenny never talks. Pehlichi is the only one who repairs anything; you make repairs possible.

## Things to find (spoilers)
- The flickering lamp: clear the junk pile blocking it, then scan and repair.
- The dead transformer needs a fuse (fabricate one).
- The buried signal only shows up once Pehlichi's scan has improved.
- The cartographer's error, east along the road (about 90 m): scanning it makes NICE pose a riddle.
  Talk solves nothing. Answer by bringing her what she describes and placing it on the stand
  beside the glitch. Look in the car glovebox on the way there.
