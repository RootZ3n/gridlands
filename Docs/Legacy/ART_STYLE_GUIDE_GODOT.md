# GRIDLANDS — Art Style Guide
**Version:** 1.0
**Date:** 2026-06-16

---

## The Golden Rule

**This is a retro NES game with synthwave aesthetics.**
Every pixel decision serves two masters: NES constraints AND synthwave style.

---

## 1. Resolution & Canvas

| Spec | Value | Why |
|------|-------|-----|
| **Viewport** | 256×240 px | NES native resolution |
| **Scale mode** | Integer scaling | Crisp pixels, no blur |
| **Stretch mode** | Viewport | Renders at native res, scales up |
| **Aspect** | Keep | NES correct aspect ratio |

The game renders at 256×240 and scales up to fill the window. Every art
asset is designed for this canvas. No sub-pixel positioning. No anti-aliasing.
Pixel-perfect placement.

---

## 2. Tile System

### 2.1 Tile Size

| Tile Type | Size | Notes |
|-----------|------|-------|
| **Overworld tiles** | 16×16 px | Grid floor, walls, obstacles |
| **Town/interior tiles** | 16×16 px | Building interiors, NPCs |
| **Battle background tiles** | 16×16 px | Battle scene environment |

### 2.2 Overworld Tileset — The Grid

The overworld IS the grid. Every ground tile is a magenta wireframe grid
on dark indigo/black. Different zones have different grid variations:

| Zone | Grid Color | Ground Color | Accent |
|------|-----------|--------------|--------|
| **Home** | Soft cyan (#66CCCC) | Dark navy (#0D1117) | Warm white |
| **Zone 1 — Arena** | Magenta (#FF00FF) | Deep indigo (#1A0A2E) | Red glow |
| **Zone 2 — Settlement** | Cyan (#00FFFF) | Dark blue (#0A1628) | Gold accent |
| **Zone 3 — Engine** | Green (#00FF66) | Near black (#0A0A14) | Orange |
| **Zone 4 — Academy** | Purple (#9933FF) | Deep purple (#1A0A2E) | White |
| **Zone 5 — Workshop** | Orange (#FF6600) | Dark brown (#1A1008) | Cyan |
| **Zone 6 — Grounds** | Red (#FF3333) | Dark red-black (#1A0A0A) | Magenta |
| **Core** | All colors (cycling) | Pure black (#000000) | White |

### 2.3 Tile Categories (per zone)

Each zone needs these tiles:

```
Ground:
  - grid_floor.png          (walkable, main grid)
  - grid_floor_damaged.png  (corrupted variant, glitchy)
  - grid_floor_restored.png (clean, healed variant)

Walls:
  - wall_circuit.png        (circuit board wall)
  - wall_corrupted.png      (glitched wall)
  - wall_data.png           (data stream wall)

Obstacles:
  - obstacle_chip.png       (circuit chip, blocks movement)
  - obstacle_wire.png       (wire bundle, blocks movement)
  - obstacle_glitch.png     (corruption zone, blocks until fixed)

Decorative:
  - decor_node.png          (data node, ambient)
  - decor_stream.png        (data flow, animated)
  - decor_spark.png         (electric spark, animated)
```

---

## 3. Character Sprites

### 3.1 Zenny (Player Character)

| Spec | Value |
|------|-------|
| **Sprite size** | 16×16 px |
| **Directions** | 4 (up, down, left, right) |
| **Frames per direction** | 2 (idle + walk) |
| **Sheet layout** | 4 rows × 2 cols = 128×64 px |
| **Style** | Dark silhouette, neon cyan outline |
| **Details** | Hoodie shape, jeans, sneakers. Small but readable. |

```
Zenny sprite sheet layout:
Row 1: Down idle, Down walk
Row 2: Left idle, Left walk
Row 3: Right idle, Right walk
Row 4: Up idle, Up walk
```

**Color palette:**
- Body fill: Near-black (#0D1117)
- Outline: Neon cyan (#00FFFF)
- Hoodie accent: Slightly lighter dark (#1A2332)
- Eyes: Bright white dot (1px)

### 3.2 NPC Sprites (Friendly AIs)

| Spec | Value |
|------|-------|
| **Sprite size** | 16×16 px |
| **Directions** | 4 (down-facing default) |
| **Frames** | 2 (idle + talk) |
| **Style** | Dark silhouette, neon outline (color per NPC) |

NPCs are visual variations on the same base — humanoid silhouettes with
different neon outline colors to indicate their role:
- Cyan outline: Information NPCs
- Green outline: Shop NPCs
- Gold outline: Save/rest NPCs
- White outline: Story NPCs

---

## 4. Enemy Sprites

### 4.1 Overworld Enemies

Enemies appear on the overworld as moving sprites (like Dragon Warrior).

| Spec | Value |
|------|-------|
| **Sprite size** | 16×16 px |
| **Movement** | Random patrol, 1-2 px/frame |
| **Style** | Dark silhouette, neon outline (color per zone) |
| **Frames** | 2 (idle + alert/attack) |

### 4.2 Battle Enemies

Battle sprites are larger and more detailed.

| Spec | Value |
|------|-------|
| **Sprite size** | 32×32 px (normal enemies) |
| **Boss sprite size** | 48×48 px or 64×64 px |
| **Frames** | 2-4 (idle, attack, hurt, death) |
| **Style** | Dark silhouette, neon outline, detailed shape |

### 4.3 Enemy Color Coding

Enemies are colored by their TYPE, not their species. This is how a dragon
and a robot look like they belong in the same battle:

| Enemy Type | Outline Color | Example |
|-----------|--------------|---------|
| **Biological** | Magenta (#FF00FF) | Dragons, worms, aliens |
| **Mechanical** | Cyan (#00FFFF) | Robots, drones, constructs |
| **Digital** | Green (#00FF66) | Glitches, data ghosts, corruptions |
| **Hybrid** | Yellow (#FFD700) | Bio-mechanical, corrupted AIs |
| **Boss** | White (#FFFFFF) + glow | All bosses get white outlines |

---

## 5. Battle Screen Layout

```
┌──────────────────────────────────┐
│                                  │
│     [Enemy Sprite 32×32 or      │
│      larger, centered]           │
│                                  │
│  ┌──────────────────────────┐   │
│  │ Battle Background        │   │
│  │ (synthwave landscape     │   │
│  │  at reduced detail)      │   │
│  └──────────────────────────┘   │
│                                  │
│  ┌──────────────────────────┐   │
│  │ ENEMY NAME         HP ███│   │
│  │─────────────────────────│   │
│  │ Zenny    HP ████  MP ██ │   │
│  │ Circuit  LV 5    EXP ██ │   │
│  │─────────────────────────│   │
│  │ ▶ FIGHT    SKILL        │   │
│  │   ITEM     RUN          │   │
│  └──────────────────────────┘   │
└──────────────────────────────────┘
```

**Battle background:** Each zone has a synthwave landscape as the battle
background — simplified, dark, with neon accents. The segmented sun,
wireframe mountains, and grid floor are visible but reduced in detail
so the enemy sprite stands out.

---

## 6. UI Design

### 6.1 General UI Rules

- **Font:** 8×8 pixel font (NES-style, monospace)
- **Borders:** Neon-colored single-pixel borders on dark backgrounds
- **Text color:** White (#FFFFFF) on dark (#0A0A0A)
- **Highlight color:** Neon magenta (#FF00FF) for selections
- **HP bar:** Green (#00FF66) → Yellow (#FFD700) → Red (#FF3333)
- **MP bar:** Cyan (#00FFFF)
- **Menu background:** Semi-transparent dark (#0A0A0A at 90% opacity)

### 6.2 Dialog Boxes

```
┌──────────────────────────────────┐
│ Circuit Blade:                   │
│ "Zenny, I detect corruption      │
│  ahead. Stay sharp."             │
│                            ▼     │
└──────────────────────────────────┘
```

- Single-pixel neon border
- Dark fill
- White text
- Blinking arrow indicator for "more text"
- Circuit Blade dialog has cyan border
- NPC dialog has white border
- Nefarious AI dialog has red border

### 6.3 Menu Screens

All menu screens follow the same pattern:
- Full-screen dark background
- Neon-bordered panels
- 8×8 pixel font
- Cursor: blinking neon arrow or highlight bar

---

## 7. Effects & Particles

### 7.1 Glitch Effect

When corruption is present:
- Random pixel displacement (1-3 px horizontal shift)
- Color channel splitting (RGB offset)
- Scanline intensification
- Static noise overlay (1-2 frames)

### 7.2 Restoration Effect

When a glitch is fixed:
- White flash (2 frames)
- Neon pulse outward from center
- Grid lines re-appear cleanly
- Sound chime

### 7.3 Battle Effects

- **Attack slash:** Neon trail (2-3 frames)
- **Hit:** White flash on enemy (1 frame)
- **Heal:** Rising cyan particles (3-4 frames)
- **Level up:** Gold burst + fanfare
- **Critical hit:** Screen shake + larger flash

---

## 8. Animation Timing (NES Feel)

| Action | Duration | Frames |
|--------|----------|--------|
| **Walk cycle** | 300ms per frame | 2 frames |
| **Idle blink** | 500ms | 1 frame toggle |
| **Battle intro** | 500ms | Screen wipe |
| **Enemy appear** | 300ms | Fade in |
| **Attack animation** | 200ms per frame | 2-3 frames |
| **Hit flash** | 100ms | 1 frame |
| **Dialog text** | 30ms per character | Typewriter |
| **Menu cursor blink** | 400ms | Toggle |

Everything should feel snappy and responsive. No long animations. NES games
were fast — the player should never be waiting for an animation to finish.

---

## 9. Color Palette Summary

### Primary Palette (use 80% of the time)
| Color | Hex | Use |
|-------|-----|-----|
| Near-black | #0A0A0A | Backgrounds, fills |
| Deep indigo | #1A0A2E | Secondary background |
| Dark navy | #0D1117 | UI backgrounds |
| White | #FFFFFF | Text, highlights |
| Neon magenta | #FF00FF | Grid, primary accent |
| Neon cyan | #00FFFF | UI, secondary accent |

### Element Palette (use for specific elements)
| Color | Hex | Use |
|-------|-----|-----|
| Neon green | #00FF66 | HP, digital enemies |
| Gold | #FFD700 | EXP, currency, highlights |
| Orange | #FF6600 | Fire, mechanical |
| Red | #FF3333 | Danger, low HP, corruption |
| Purple | #9933FF | Corruption, glitch |
| Electric blue | #0066FF | Ice, data |

---

## 10. What Luna Needs to Produce

### Per Zone:
- 1 overworld tileset (16×16 tiles, ~20-30 unique tiles)
- 3-5 enemy overworld sprites (16×16, 2 frames each)
- 3-5 enemy battle sprites (32×32, 2-4 frames each)
- 1 boss battle sprite (48×48 or 64×64, 4 frames)
- 1 battle background (256×240, reduced detail)

### Global:
- Zenny sprite sheet (16×16, 4 directions, 2 frames each = 128×64)
- NPC base sprite (16×16, recolorable)
- UI elements (borders, icons, cursors, bars)
- Effects (glitch, restore, attack trails)
- Title screen (256×240)

### File Format:
- PNG with transparency
- No anti-aliasing
- No compression artifacts
- Pixel-perfect (no sub-pixel positioning)

---

*This guide is the contract between art and code. Every sprite must follow
these specs exactly, or it won't look right in the game.*
