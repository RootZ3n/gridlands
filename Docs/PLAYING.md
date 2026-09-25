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
| left mouse (no tool out) | swing at a creature in front of you (best weapon you carry: pry bar > shovel > fists) |
| V | Pehlichi makes a glitchy noise where he is; creatures nearby go and look (he never hurts anything) |

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
East of home, past the cyan posts (about 128 m east of the start), is a second Grid cell: the
Diner Lots. It is deeper (more static), mostly 1950s, and has one glitch and a junk pile. The
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
