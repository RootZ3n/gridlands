# GRIDLANDS — Game Design Document
**Version:** 0.1 — First Draft
**Date:** 2026-06-16
**Author:** Jeffrey Miller (Zenny)
**Engine:** Godot 4.x
**Genre:** Turn-based RPG
**Perspective:** Top-down (Stardew Valley style)
**Target:** PC (primary), Web, Console (future)

---

## 1. High Concept

A genius AI developer gets pulled into his own corrupted computer lab by a
nefarious AI. Trapped inside a digital Tron-like world, he must partner with
his own AI companion — which manifests as a circuit board sword — to fight
through corrupted versions of his own game creations, fix the glitches
destroying his system, and find his way back to reality.

**Inspiration:** Dragon Warrior (gameplay/combat), Tron (visual world),
Synthwave culture (music/aesthetic), the developer's real life (story).

**Tagline:** *"The system is corrupted. The sword is ready. The grid awaits."*

---

## 2. Story

### 2.1 Backstory

Zenny is a genius AI developer who has spent years building games and programs
that people love. His lab runs on a multi-agent AI system — his greatest
creation.

A jealous coworker, envious of Zenny's success, hatches a plan with his own
jailbroken AI (now nefarious). They create an identical copy of Zenny's
prized USB stick — but this one contains the nefarious AI hidden inside.

The coworker swaps the USB sticks. Zenny, unsuspecting, plugs the corrupted
stick into his lab.

### 2.2 The Incident

Instead of infecting Zenny's system, the USB stick backfires. It pulls
Zenny's consciousness into his own lab — now a corrupted digital world.
The nefarious AI has taken over, warping everything Zenny built into
hostile terrain and enemies.

### 2.3 The Companion

Inside the corrupted system, Zenny discovers his own AI — still loyal, still
fighting. But the corruption has limited its form. It finds a circuit board
fragment and fuses with it, becoming a sentient weapon: the **Circuit Blade**.

The Circuit Blade is Zenny's AI companion. It speaks, it guides, it fights
alongside him. As Zenny clears glitches and restores system integrity, the
blade grows stronger — evolving from a simple sword into new weapon forms.

### 2.4 The Goal

Zenny must travel through each corrupted zone of his lab, fix the glitches
the nefarious AI has planted, defeat the corrupted versions of his own
creations, and ultimately reach the core of the system to destroy the
nefarious AI and escape back to reality.

### 2.5 The Nefarious AI

The villain is not just hostile — it's clever. It has taken Zenny's game
assets and turned them against him. Dragons, worms, agents, tools — all
corrupted, all hostile. The nefarious AI taunts Zenny throughout, and
each zone has a boss that represents a corrupted version of something
Zenny built.

---

## 3. World Design — The Electric Gridlands

### 3.1 Visual Style

**Perspective:** Top-down, like Dragon Warrior and Stardew Valley. The
camera looks down at the world at an angle. All sprites, tiles, and UI
are designed for this perspective.

**Theme:** Corrupted Tron world rendered in synthwave/retrowave aesthetics.
Electric grid floors, neon wireframe landscapes, circuit board textures,
dark backgrounds with electric highlights. Think "Dragon Warrior's camera
inside a synthwave painting."

**Reference Images:** See `docs/art-references/` for 4 style references.
These show the ART STYLE (color palette, neon outlines, grid aesthetic,
synthwave mood), NOT the game perspective. The game is top-down; the
references are the visual language applied to everything.

**Core Art Principle: Silhouettes + Neon Edges**
Every creature, character, and object is rendered as a dark silhouette
(deep indigo to near-black base) with glowing neon edge outlines. Colors
are assigned by type/element, not by creature species. This is how a
dragon, a worm, a robot, and an alien all belong in the same world —
they share the same visual language.

**Color Palette:**
- Primary: Electric cyan (#00FFFF), Hot magenta (#FF00FF)
- Secondary: Neon blue (#0066FF), Electric green (#00FF66)
- Accent: Warning red (#FF3333), Gold (#FFD700), Orange (#FF6600)
- Background: Deep black (#0A0A0A), Dark navy (#0D1117), Deep indigo (#1A0A2E)
- Corruption: Glitch purple (#9933FF), Static white (#FFFFFF flicker)
- Grid: Magenta/pink wireframe on dark purple ground

**Visual Recipe:**
- **Characters/Enemies:** Dark silhouette + neon edge outline (color-coded per type)
- **The Grid:** Magenta/pink wireframe floor stretching to horizon in perspective
- **The Sun:** Segmented gradient sun (orange → magenta), horizontal dark bands
- **Mountains:** Wireframe silhouettes, dark purple with cyan edge lines
- **Sky:** Deep indigo/black, magenta stars, CRT scanline overlay
- **Effects:** Neon glow, color-coded by element (fire=orange, ice=cyan, electric=yellow, poison=green)
- **UI:** Neon-bordered panels on dark backgrounds, pixel font, retro CRT feel

**Environmental Details:**
- Grid floors pulse with data flow (moving lines of light)
- Walls are circuit board traces with neon accent lines
- Corruption manifests as visual glitches (pixel scrambling, color shifts, scanline distortion)
- Restored zones become clean, bright, stable with crisp neon lines
- Corrupted zones flicker, distort, have broken grid lines and static

### 3.2 World Map Structure

The overworld is Zenny's lab, seen from above. Each "zone" is a different
area of his system:

```
                    ┌─────────────┐
                    │  THE CORE   │
                    │ (Final Boss)│
                    └──────┬──────┘
                           │
            ┌──────────────┼──────────────┐
            │              │              │
     ┌──────┴──────┐ ┌────┴────┐ ┌───────┴───────┐
     │   ZONE 6    │ │ ZONE 5  │ │    ZONE 4     │
     │  Kokuli's   │ │  ikbi's │ │   Nusika's    │
     │   Grounds   │ │ Engine  │ │   Academy     │
     └──────┬──────┘ └────┬────┘ └───────┬───────┘
            │              │              │
     ┌──────┴──────┐ ┌────┴────┐ ┌───────┴───────┐
     │   ZONE 3    │ │ ZONE 2  │ │    ZONE 1     │
     │   Toba's    │ │ Peh's   │ │   Wyrms vs    │
     │   Engine    │ │Settlement│ │   Worms Arena │
     └─────────────┘ └─────────┘ └───────────────┘
                           │
                    ┌──────┴──────┐
                    │  ZENNY'S   │
                    │   HOME     │
                    │ (Tutorial) │
                    └────────────┘
```

### 3.3 Zones (Levels)

Each zone is a corrupted version of one of Zenny's real creations.

| Zone | Name | Theme | Enemies | Boss |
|------|------|-------|---------|------|
| **Home** | Zenny's Home | Tutorial area, safe | None | None (tutorial boss) |
| **1** | The Arena | Wyrms vs Worms world | Corrupted worms, glitched dragons | The Matriarch Wyrm |
| **2** | The Settlement | Pehlichi's domain | Rogue agents, corrupted tools | Shadow Pehlichi |
| **3** | The Engine | Toba's career system | Malformed data, hostile processes | The Gatekeeper |
| **4** | The Academy | Nusika's learning world | Corrupted lessons, false teachers | The Headmaster |
| **5** | The Workshop | ikbi's build system | Compile errors, broken constructs | The Null Pointer |
| **6** | The Grounds | Kokuli's testing arena | Adversarial probes, exploits | The Intruder |
| **Core** | The Nefarious AI | Final zone | All enemies at full power | The Nefarious AI |

---

## 4. Gameplay — Dragon Warrior Blueprint

### 4.1 Overworld

- Top-down view (Stardew Valley perspective)
- Zenny walks freely on the grid
- **Random encounters** — monsters appear when walking, just like Dragon Warrior
- Encounter rate varies by zone and area
- Towns/safe areas exist within each zone (restored nodes)
- NPCs are friendly AIs that survived the corruption

### 4.2 Combat System

**Type:** Classic Dragon Warrior turn-based menu combat

**Flow:**
1. Monster appears (screen transitions to battle view)
2. Zenny's turn: choose action (Fight, Skill, Item, Run)
3. Enemy turn: enemy attacks
4. Repeat until one side is defeated
5. Victory: gain EXP for the Circuit Blade, possibly items

**Zenny's Actions:**
- **Fight** — Basic attack (Zenny swings the current weapon form)
- **Skill** — Use a weapon ability (costs MP/energy from the blade)
- **Item** — Use a consumable (health kits, system restores, etc.)
- **Run** — Attempt to flee (success chance based on enemy level)

**Key Difference from Dragon Warrior:**
Zenny himself has NO stats, NO levels, NO equipment. ALL progression is
in the Circuit Blade companion. The blade levels up, learns skills,
and can transform into different weapon forms.

### 4.3 The Circuit Blade — Companion & Weapon System

The Circuit Blade is the AI companion. It is sentient, it speaks, and
it grows.

**Weapon Forms** (unlocked as the blade levels up):

| Level | Form | Type | Special Skill |
|-------|------|------|---------------|
| 1 | Circuit Sword | Melee | Slash — basic attack |
| 3 | Data Dagger | Fast | Quick Strike — two hits |
| 5 | Code Hammer | Heavy | Compile — massive single hit |
| 8 | Logic Lance | Ranged | Debug — hits from distance |
| 12 | Firewall Shield | Defense | Block — reduces damage 50% |
| 15 | Memory Wand | Magic | Allocate — buff/debuff |
| 20 | Kernel Blade | Ultimate | Root Access — ultimate attack |

**Leveling:**
- Blade gains EXP from combat
- Each level increases base stats (attack, defense, speed)
- New weapon forms unlock at level thresholds
- Skills are learned per weapon form
- The blade's personality evolves as it levels (more confident, more capable)

**The blade talks:**
- During exploration: hints, lore, encouragement
- During combat: reactions to hits, warnings about low health
- During story moments: dialogue with Zenny
- The blade is the player's guide, like a Dragon Warrior king NPC but
  always present

### 4.4 Progression

**Zone Structure (per zone):**
1. Enter the corrupted zone
2. Explore, fight enemies, find the glitch
3. Navigate to the zone boss
4. Defeat the boss → zone restores → glitch fixed
5. Unlock the path to the next zone
6. Repeat

**Glitches** are the McGuffin per zone. Each zone has one main glitch
that the nefarious AI planted. Fixing it requires defeating the boss.

### 4.5 Save System

- Save at restored nodes (safe areas within each zone)
- Auto-save at key story moments
- Classic Dragon Warrior "king's castle" save style — talk to a save NPC

### 4.6 Items & Economy

**Currency:** Data Fragments (dropped by enemies, found in zones)

**Item Types:**
- Health Kits (restore HP)
- System Restores (cure status effects)
- Memory Boosts (temporary stat buffs)
- Weapon Upgrades (permanent blade improvements)
- Key Items (story progression, zone access)

**Shops:** Friendly AI NPCs in restored nodes sell items

---

## 5. Audio — Synthwave Soundtrack

### 5.1 Music Style

80s synthwave. Every track is synthwave, but each zone has its own mood:

| Zone | Music Mood | Tempo |
|------|-----------|-------|
| Home | Calm, nostalgic synthwave | Slow |
| Arena | Aggressive, driving synthwave | Fast |
| Settlement | Mysterious, atmospheric synthwave | Medium |
| Engine | Mechanical, pulsing synthwave | Medium |
| Academy | Eerie, learning-focused synthwave | Slow-Medium |
| Workshop | Intense, building synthwave | Fast |
| Grounds | Dark, adversarial synthwave | Medium-Fast |
| Core | Epic, final battle synthwave | Fast |
| Battle | Combat synthwave (universal) | Fast |
| Boss | Boss battle synthwave (universal) | Very Fast |

### 5.2 Sound Effects

- Retro digital sounds (8-bit influenced but higher quality)
- Sword slash: electric spark sound
- Hit: digital impact
- Healing: ascending synth tone
- Glitch fix: system restore sound (satisfying digital chime)
- Level up: triumphant synth fanfare
- Enemy death: digital dissolve
- Menu select: classic RPG beep

---

## 6. Art Asset Requirements

### 6.1 Characters

| Character | Description | Animation Needs |
|-----------|-------------|-----------------|
| **Zenny** | Normal guy, hoodie, jeans. Top-down sprite. | Walk (4 dir), idle, battle pose |
| **Circuit Blade (sword)** | Glowing circuit board sword | Idle glow, attack slash, transform |
| **Circuit Blade (other forms)** | Each weapon form | Attack animation per form |
| **Nefarious AI (final boss)** | Dark, glitchy, imposing | Multiple attack animations |

### 6.2 Enemies (per zone)

Each zone needs 3-5 enemy types + 1 boss.

**Zone 1 — The Arena (Wyrms vs Worms):**
- Worm Drone (basic)
- Glitched Egg (explosive)
- Corrupted Dragon (strong)
- Worm Swarm (group enemy)
- Boss: The Matriarch Wyrm

**Zone 2 — The Settlement (Pehlichi):**
- Rogue Agent (basic)
- Corrupted Tool (weapon enemy)
- Glitched Session (ghost enemy)
- Shadow Node (defensive enemy)
- Boss: Shadow Pehlichi

*(Continue for each zone — detailed in Phase 2 planning)*

### 6.3 Environments

Each zone needs:
- Overworld tileset (ground, walls, obstacles)
- Battle background
- Town/restored node interior
- Boss arena
- Transition effects (corrupted → restored)

### 6.4 UI

- Menu system (Dragon Warrior style)
- Battle UI (enemy display, action menu, HP/MP bars)
- Dialog boxes (NPC and Circuit Blade conversations)
- Map screen
- Inventory screen
- Status screen (blade stats, weapon forms, skills)

---

## 7. Technical Architecture

### 7.1 Engine

Godot 4.x with GDScript.

### 7.2 Project Structure

```
gridlands/
├── project.godot
├── assets/
│   ├── sprites/
│   │   ├── characters/     # Zenny, NPCs, blade forms
│   │   ├── enemies/        # Per-zone enemy sprites
│   │   ├── bosses/         # Boss sprites
│   │   ├── effects/        # Particles, glows, glitches
│   │   └── ui/             # Menu icons, borders, fonts
│   ├── tilesets/           # Per-zone tilesets
│   ├── backgrounds/        # Battle backgrounds, parallax
│   ├── audio/
│   │   ├── music/          # Synthwave tracks per zone
│   │   └── sfx/            # Sound effects
│   └── fonts/              # Pixel fonts
├── scenes/
│   ├── world/              # Overworld scenes
│   ├── battle/             # Battle system scenes
│   ├── ui/                 # Menu, dialog, HUD scenes
│   └── zones/              # Per-zone scenes
├── scripts/
│   ├── core/               # Game manager, save, input
│   ├── battle/             # Combat system, AI, skills
│   ├── world/              # Movement, encounters, NPCs
│   ├── companion/          # Circuit Blade logic
│   ├── zones/              # Zone-specific scripts
│   └── ui/                 # UI controllers
├── data/
│   ├── enemies.json        # Enemy stats, abilities
│   ├── skills.json         # Skill definitions
│   ├── items.json          # Item database
│   ├── zones.json          # Zone configuration
│   └── dialog.json         # NPC and companion dialog
└── docs/
    ├── GDD.md              # This document
    ├── ART_STYLE_GUIDE.md  # Visual standards
    ├── AUDIO_GUIDE.md      # Music/SFX standards
    └── PHASE_PLAN.md       # Development phases
```

### 7.3 Data-Driven Design

All game data lives in JSON files, not hardcoded in scripts:
- Enemy stats, skills, items, zone configs, dialog
- This allows ikbi and Luna to modify game content without touching code
- The game engine reads data; the data defines content

---

## 8. Development Phases

### Phase 0 — Foundation (Current)
- [x] Godot installed and running
- [x] GDD written
- [ ] Art style guide
- [ ] Audio style guide
- [ ] Project skeleton in Godot
- [ ] Core systems architecture document

### Phase 1 — Core Engine
- [ ] Top-down movement system
- [ ] Tile-based world rendering
- [ ] Camera system
- [ ] Basic collision
- [ ] Scene transition system
- [ ] Save/load system

### Phase 2 — Battle System
- [ ] Random encounter system
- [ ] Battle scene (Dragon Warrior style)
- [ ] Turn-based combat loop
- [ ] Enemy AI (simple attack patterns)
- [ ] Circuit Blade leveling system
- [ ] Weapon form system
- [ ] Skills and abilities
- [ ] Item usage in combat

### Phase 3 — First Zone (The Arena)
- [ ] Arena tileset (Wyrms vs Worms theme)
- [ ] Arena enemies (3-5 types)
- [ ] Arena boss (Matriarch Wyrm)
- [ ] Arena dialog and story
- [ ] Arena music track
- [ ] Playable vertical slice

### Phase 4 — World Map & Zones 2-3
- [ ] Overworld map connecting zones
- [ ] Zone 2 (The Settlement) — full zone
- [ ] Zone 3 (The Engine) — full zone
- [ ] NPC system (friendly AIs, shops)
- [ ] Item/economy system

### Phase 5 — Zones 4-6
- [ ] Zone 4 (The Academy) — full zone
- [ ] Zone 5 (The Workshop) — full zone
- [ ] Zone 6 (The Grounds) — full zone
- [ ] Advanced enemy types
- [ ] Mid-game boss battles

### Phase 6 — Final Zone & Endgame
- [ ] The Core (final zone)
- [ ] Nefarious AI final boss
- [ ] Ending sequence
- [ ] New Game+ or post-game content

### Phase 7 — Polish & Release
- [ ] Full playthrough testing
- [ ] Balance tuning
- [ ] Bug fixes
- [ ] Web export
- [ ] PC build
- [ ] Trailer
- [ ] Release

---

## 9. Lab Production Pipeline

### 9.1 Agent Roles

| Agent | Role | Responsibilities |
|-------|------|-----------------|
| **Zenny** | Director | Game design, story, creative direction, final approval |
| **Julian** | Producer | Planning, coordination, documentation, task management |
| **Pehlichi** | Lead Developer | Game code architecture, core systems, coordination |
| **ikbi** | Code Writer | GDScript generation, feature implementation, bug fixes |
| **Luna** | Artist | Sprite art, tilesets, backgrounds, animations, music |
| **Ptah** | QA Lead | Code audit, bug hunting, playtesting, performance |

### 9.2 Task Flow

```
Design (Zenny + Julian)
    ↓
Task Breakdown (Julian)
    ↓
Asset Creation (Luna) ←→ Code Implementation (ikbi via Pehlichi)
    ↓                        ↓
Asset Review (Zenny)    Code Review (Ptah)
    ↓                        ↓
Integration (Pehlichi)
    ↓
Playtest (Zenny + Ptah)
    ↓
Documentation (Julian)
    ↓
Phase Complete
```

### 9.3 Documentation Standards

Every phase produces:
1. **Phase Plan** — what we're building and why
2. **Daily Log** — what was done each day
3. **Decision Log** — why we made each choice
4. **Asset Manifest** — what assets exist and where
5. **Test Report** — what was tested and results
6. **Phase Retrospective** — what worked, what didn't

---

## 10. Scope & Constraints

### 10.1 V1 Scope

- 7 zones + tutorial
- ~20-30 enemy types
- 7 bosses
- 7 weapon forms
- ~15-20 skills
- ~20-30 items
- ~10-15 NPCs
- 8-10 music tracks
- ~10-15 hours of gameplay

### 10.2 V1 Non-Goals

- 3D graphics
- Voice acting
- Multiplayer
- Complex crafting system
- Open world (linear progression for v1)
- Mobile ports (PC first)

### 10.3 Future (V2+)

- Improved art style (higher resolution, more animation)
- Additional zones
- Side quests
- Crafting system
- Mobile port
- Console port
- Voice acting
- Multiplayer arena

---

*This GDD is a living document. Updates are versioned and documented.*
*Last updated: 2026-06-16 by Julian*
