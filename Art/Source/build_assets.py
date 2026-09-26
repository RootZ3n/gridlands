"""Gridlands P7 style-proof assets: every mesh is a recipe here (the source of truth), rebuilt headless:
(Axes: Blender +Y becomes Unreal -Y on import (a handedness change, not a mirror). A piece's
"front" faces Unreal -Y, so recipes put fronts at Blender +Y.)

    blender -b --factory-startup --python Art/Source/build_assets.py -- <out_dir>

Writes <out_dir>/<Name>.fbx, <out_dir>/manifest.json (triangles, bounds, slots) and, with --preview,
<out_dir>/preview/<Name>.png. Style proofs, not production assets (P7). Metres; +X is forward; the
pivot is the bottom centre. Material slots name the master material the importer binds:
GL_Painted (vertex-colour stylized), GL_Glow (emissive), GL_Glass.
"""

import math
import random
import sys
import zlib
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gl_art as A  # noqa: E402

# A saturated palette (linear-ish sRGB 0..1). Colour is identity: nothing here is desaturated.
P = {
    "grass": (0.36, 0.80, 0.22), "grass_tip": (0.88, 0.92, 0.32), "leaf": (0.10, 0.58, 0.42), "leaf_hi": (0.38, 0.84, 0.46),
    "bark": (0.58, 0.30, 0.14), "cut": (0.98, 0.84, 0.56), "rock": (0.50, 0.49, 0.70), "rock_hi": (0.72, 0.70, 0.90), "moss": (0.45, 0.76, 0.28),
    "mint": (0.42, 0.86, 0.74), "cherry": (0.92, 0.16, 0.22), "cream": (0.99, 0.92, 0.74), "chrome": (0.80, 0.86, 0.94),
    "tile_dark": (0.12, 0.11, 0.16), "glass": (0.18, 0.26, 0.52), "glow_pink": (1.0, 0.30, 0.70), "glow_warm": (1.0, 0.78, 0.40),
    "wood": (0.80, 0.48, 0.22), "wood_dark": (0.58, 0.32, 0.14), "flower": (0.96, 0.28, 0.62), "flower_mid": (1.0, 0.86, 0.20),
    "skin": (0.90, 0.64, 0.46), "hair": (0.20, 0.10, 0.08), "hoodie": (1.0, 0.52, 0.10), "pants": (0.10, 0.44, 0.60),
    "shoe": (0.98, 0.98, 0.96), "shoe_trim": (0.92, 0.18, 0.24), "white": (0.98, 0.98, 0.98), "pupil": (0.08, 0.06, 0.10),
    "peh": (1.0, 0.82, 0.14), "peh_fin": (0.92, 0.20, 0.62), "visor": (0.10, 0.07, 0.16), "peh_eye": (0.72, 1.0, 0.86),
    "fur": (0.48, 0.44, 0.60), "fur_dark": (0.16, 0.13, 0.22), "fur_light": (0.92, 0.88, 0.80), "nose": (0.12, 0.08, 0.12),
    "timber": (0.80, 0.50, 0.24), "timber_dark": (0.56, 0.32, 0.15),
}


def slot(obj, name):
    """Assigns every face of obj to the material slot `name` (the importer binds it to a master)."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    return obj


def painted(obj, colour, **kw):
    return slot(A.paint(obj, colour, **kw), "GL_Painted")


# --- nature -----------------------------------------------------------------------------------

def pine():
    parts = [painted(A.cylinder("trunk", 0.30, 3.4, sides=9, radius_top=0.17), P["bark"], bottom=0.6)]
    parts.append(painted(A.cylinder("flare", 0.42, 0.35, sides=9, radius_top=0.28), P["bark"], top=0.8, bottom=0.55))
    tiers = [(2.1, 1.9, 2.0), (3.5, 1.55, 1.9), (4.8, 1.2, 1.7), (5.9, 0.85, 1.6), (6.9, 0.5, 1.2)]
    for i, (z, r, h) in enumerate(tiers):
        t = A.cylinder(f"tier{i}", r, h, at=(0.08 * math.sin(i * 2.1), 0.08 * math.cos(i * 1.7), z), sides=8, radius_top=r * 0.18, bevel=0.06, rot=(4 * math.sin(i), 4 * math.cos(i), i * 17))
        A.irregular(t, 0.07, 30 + i)
        parts.append(painted(t, P["leaf"] if i % 2 == 0 else P["leaf_hi"], top=1.3, bottom=0.62, seed=i))
    obj = A.join("SM_Pine", parts)
    A.pivot_bottom_centre(obj)
    return obj


def pine_stump():
    s = A.cylinder("stump", 0.30, 0.34, sides=9, radius_top=0.27, bevel=0.03)
    A.irregular(s, 0.02, 7)
    cap = A.cylinder("cap", 0.26, 0.06, at=(0, 0, 0.34), sides=9, bevel=0.02)
    obj = A.join("SM_PineStump", [painted(s, P["bark"], bottom=0.6), painted(cap, P["cut"], up_boost=0.0)])
    A.pivot_bottom_centre(obj)
    return obj


def boulder(name, seed, scale):
    b = A.blob("rock", 0.9 * scale, squash=(1.25, 1.0, 0.72), subdiv=2, lump=0.2, seed=seed)
    for v in b.data.vertices:  # sit flat on the ground
        v.co.z = max(v.co.z, -0.25 * scale)
    moss = A.blob("moss", 0.55 * scale, at=(0.1 * scale, 0.05 * scale, 0.45 * scale), squash=(1.2, 1.0, 0.35), subdiv=2, lump=0.15, seed=seed + 1)
    small = A.blob("pebble", 0.35 * scale, at=(0.95 * scale, -0.55 * scale, -0.1 * scale), squash=(1.2, 1, 0.8), subdiv=1, lump=0.2, seed=seed + 2)
    obj = A.join(name, [painted(b, P["rock"], top=1.25, bottom=0.6, up_boost=0.22, variation=0.08, seed=seed),
                        painted(moss, P["moss"], top=1.2, bottom=0.8), painted(small, P["rock"], top=1.2, bottom=0.7, up_boost=0.2)])
    A.pivot_bottom_centre(obj)
    return obj


def grass_tuft():
    rng = random.Random(11)
    parts = []
    for i in range(9):
        a = i / 9 * math.tau + rng.uniform(-0.3, 0.3)
        h = rng.uniform(0.32, 0.52)
        blade = A.box(f"b{i}", (0.07, 0.025, h), at=(0.06 * math.cos(a), 0.06 * math.sin(a), 0), taper=0.12, rot=(22 * math.sin(a), -22 * math.cos(a), math.degrees(a)))
        parts.append(slot(A.paint(blade, P["grass"], top=1.35, bottom=0.55, up_boost=0.0, seed=i), "GL_Painted"))
        # tips go yellow-green: repaint the top half warmer
    obj = A.join("SM_GrassTuft", parts)
    _tint_tips(obj, P["grass_tip"], 0.6)
    A.pivot_bottom_centre(obj)
    return obj


def _tint_tips(obj, colour, from_h):
    mesh = obj.data
    attr = mesh.color_attributes["Col"]
    zs = [v.co.z for v in mesh.vertices]
    z0, z1 = min(zs), max(zs)
    for li, loop in enumerate(mesh.loops):
        h = (mesh.vertices[loop.vertex_index].co.z - z0) / max(1e-4, z1 - z0)
        if h > from_h:
            t = (h - from_h) / (1 - from_h)
            c = attr.data[li].color
            attr.data[li].color = (c[0] + (colour[0] - c[0]) * t, c[1] + (colour[1] - c[1]) * t, c[2] + (colour[2] - c[2]) * t, 1.0)


def flower():
    stem = A.box("stem", (0.025, 0.025, 0.34), taper=0.6)
    parts = [painted(stem, P["grass"])]
    for i in range(5):
        a = i / 5 * math.tau
        petal = A.blob(f"p{i}", 0.06, at=(0.07 * math.cos(a), 0.07 * math.sin(a), 0.36), squash=(1.4, 0.8, 0.35), subdiv=1, lump=0.05, seed=i)
        petal.rotation_euler = (0, 0, a)
        parts.append(painted(petal, P["flower"], up_boost=0.15))
    parts.append(painted(A.blob("mid", 0.045, at=(0, 0, 0.37), squash=(1, 1, 0.6), subdiv=1, lump=0.0), P["flower_mid"]))
    obj = A.join("SM_Flower", parts)
    A.pivot_bottom_centre(obj)
    return obj


def bush():
    parts = []
    for i, (x, y, r) in enumerate([(0, 0, 0.55), (0.45, 0.2, 0.42), (-0.4, 0.15, 0.4), (0.1, -0.35, 0.38)]):
        b = A.blob(f"bush{i}", r, at=(x, y, r * 0.8), squash=(1.1, 1.0, 0.85), subdiv=2, lump=0.14, seed=40 + i)
        parts.append(painted(b, (0.40, 0.78, 0.20) if i % 2 else (0.20, 0.60, 0.16), top=1.3, bottom=0.55, up_boost=0.15, seed=i))
    obj = A.join("SM_Bush", parts)
    A.pivot_bottom_centre(obj)
    return obj


# --- 1950s kit (2 m modules; walls 2 x 0.2 x 2.5 like the timber pieces; overhang is trim) ----------

def k50_foundation():
    base = painted(A.box("base", (2.0, 2.0, 0.26), bevel=0.04), P["cream"], top=0.95, bottom=0.6)
    tiles = []
    for ix in range(4):
        for iy in range(4):
            t = A.box(f"t{ix}{iy}", (0.46, 0.46, 0.05), at=(-0.75 + ix * 0.5, -0.75 + iy * 0.5, 0.25), bevel=0.01)
            tiles.append(painted(t, P["tile_dark"] if (ix + iy) % 2 else P["cream"], up_boost=0.05, variation=0.02))
    obj = A.join("SM_K50_Foundation", [base] + tiles)
    A.pivot_bottom_centre(obj)
    return obj


def _wall_frame(name, body_parts):
    trim_low = painted(A.box("trim_low", (2.04, 0.28, 0.32), bevel=0.04), P["cream"], top=1.0, bottom=0.75)
    cornice = painted(A.box("cornice", (2.08, 0.32, 0.24), at=(0, 0, 2.26), bevel=0.05), P["cream"], top=1.1, bottom=0.8)
    obj = A.join(name, body_parts + [trim_low, cornice])
    A.irregular(obj, 0.008, zlib.crc32(name.encode()) % 1000)  # stable seed: rebuilds are identical
    A.pivot_bottom_centre(obj)
    return obj


def k50_wall_plain():
    return _wall_frame("SM_K50_WallPlain", [painted(A.box("wall", (2.0, 0.2, 2.5), bevel=0.03), P["mint"])])


def k50_wall_window():
    parts = [painted(A.box("sill", (2.0, 0.2, 0.9), bevel=0.03), P["mint"]),
             painted(A.box("head", (2.0, 0.2, 0.5), at=(0, 0, 2.0), bevel=0.03), P["mint"]),
             painted(A.box("jambL", (0.22, 0.2, 1.1), at=(-0.89, 0, 0.9)), P["mint"]),
             painted(A.box("jambR", (0.22, 0.2, 1.1), at=(0.89, 0, 0.9)), P["mint"]),
             painted(A.box("frame", (1.62, 0.26, 0.1), at=(0, 0, 0.86), bevel=0.02), P["cherry"]),
             painted(A.box("frame2", (1.62, 0.26, 0.1), at=(0, 0, 1.96), bevel=0.02), P["cherry"])]
    glass = slot(A.paint(A.box("glass", (1.56, 0.06, 1.1), at=(0, 0, 0.9)), P["glass"], top=1.6, bottom=0.7, up_boost=0), "GL_Glass")
    return _wall_frame("SM_K50_WallWindow", parts + [glass])


def k50_wall_door():
    parts = [painted(A.box("left", (0.55, 0.2, 2.5), at=(-0.725, 0, 0), bevel=0.03), P["mint"]),
             painted(A.box("right", (0.55, 0.2, 2.5), at=(0.725, 0, 0), bevel=0.03), P["mint"]),
             painted(A.box("over", (0.9, 0.2, 0.4), at=(0, 0, 2.1), bevel=0.03), P["mint"]),
             painted(A.box("door", (0.86, 0.1, 2.06), at=(0, 0.02, 0.02), bevel=0.03), P["cherry"])]
    port = slot(A.paint(A.cylinder("port", 0.17, 0.14, at=(0, 0.05, 1.45), sides=14, rot=(90, 0, 0)), P["glass"], top=1.5, bottom=0.8, up_boost=0), "GL_Glass")
    ring = painted(A.cylinder("ring", 0.21, 0.1, at=(0, 0.07, 1.45), sides=14, rot=(90, 0, 0)), P["chrome"])
    return _wall_frame("SM_K50_WallDoor", parts + [ring, port])


def k50_post():
    parts = [painted(A.cylinder("pole", 0.07, 2.8, sides=10), P["cherry"], top=1.15, bottom=0.8),
             painted(A.cylinder("foot", 0.15, 0.12, sides=10, bevel=0.02), P["chrome"]),
             painted(A.cylinder("capital", 0.13, 0.1, at=(0, 0, 2.7), sides=10, bevel=0.02), P["chrome"])]
    obj = A.join("SM_K50_Post", parts)
    A.pivot_bottom_centre(obj)
    return obj


def k50_awning():
    parts = []
    for i in range(8):
        s = A.box(f"s{i}", (0.25, 2.0, 0.14), at=(-0.875 + i * 0.25, 0, 0.03), rot=(0, 0, 0))
        parts.append(painted(s, P["cherry"] if i % 2 == 0 else P["cream"], up_boost=0.08))
    for i in range(8):
        sc = A.cylinder(f"sc{i}", 0.125, 0.14, at=(-0.875 + i * 0.25, 1.0, 0.03), sides=10)  # the scalloped edge is the front
        parts.append(painted(sc, P["cherry"] if i % 2 == 0 else P["cream"]))
    rail = painted(A.box("rail", (2.0, 0.1, 0.06), at=(0, -1.0, 0.0)), P["chrome"])
    obj = A.join("SM_K50_Awning", parts + [rail])
    A.pivot_bottom_centre(obj)
    return obj


def k50_roof():
    slab = painted(A.box("slab", (2.0, 2.0, 0.16), bevel=0.02), P["cream"], top=0.9, bottom=0.7)
    lip = painted(A.box("lip", (2.06, 2.06, 0.08), at=(0, 0, 0.12), bevel=0.03), P["cherry"], up_boost=0.1)
    obj = A.join("SM_K50_Roof", [slab, lip])
    A.pivot_bottom_centre(obj)
    return obj


def k50_sign():
    post = painted(A.box("mast", (0.14, 0.14, 1.2)), P["chrome"])
    board = painted(A.box("board", (1.8, 0.22, 0.8), at=(0, 0, 1.2), bevel=0.06), P["cherry"])
    face = slot(A.paint(A.box("face", (1.5, 0.05, 0.52), at=(0, 0.12, 1.34), bevel=0.02), P["glow_pink"], top=1.0, bottom=1.0, up_boost=0), "GL_Glow")
    star = slot(A.paint(A.blob("star", 0.22, at=(0.95, -0.02, 2.05), squash=(1, 0.35, 1), subdiv=1, lump=0.35, seed=5), P["glow_warm"], top=1, bottom=1, up_boost=0), "GL_Glow")
    obj = A.join("SM_K50_Sign", [post, board, face, star])
    A.pivot_bottom_centre(obj)
    return obj


# --- timber kit for the player's building pieces (ADR-0024 sizes) --------------------------------

def timber_wall(name="SM_KT_Wall", door=False):
    parts = []
    for i in range(5):
        x = -0.8 + i * 0.4
        if door and abs(x) < 0.5:
            parts.append(painted(A.box(f"over{i}", (0.38, 0.2, 0.45), at=(x, 0, 2.05), bevel=0.03), P["timber"] if i % 2 else P["timber_dark"], seed=i))
            continue
        board = A.box(f"b{i}", (0.38, 0.2, 2.5), at=(x, 0, 0), bevel=0.03)
        A.irregular(board, 0.012, i + (7 if door else 0))
        parts.append(painted(board, P["timber"] if i % 2 else P["timber_dark"], seed=i))
    if door:  # the rail stops at the door opening
        for x in (-0.76, 0.76):
            parts.append(painted(A.box(f"rail{x}", (0.5, 0.24, 0.16), at=(x, 0, 1.6), bevel=0.02), P["timber_dark"]))
    else:
        parts.append(painted(A.box("rail", (2.02, 0.24, 0.16), at=(0, 0, 1.6), bevel=0.02), P["timber_dark"]))
    obj = A.join(name, parts)
    A.pivot_bottom_centre(obj)
    return obj


def timber_foundation():
    parts = []
    for i in range(5):
        p = A.box(f"plank{i}", (2.0, 0.38, 0.3), at=(0, -0.8 + i * 0.4, 0), bevel=0.03)
        A.irregular(p, 0.01, 60 + i)
        parts.append(painted(p, P["timber"] if i % 2 else P["timber_dark"], seed=i))
    obj = A.join("SM_KT_Foundation", parts)
    A.pivot_bottom_centre(obj)
    return obj


def timber_roof():
    parts = []
    pitch = math.degrees(math.atan2(1.0, 2.0))
    for i in range(5):
        s = A.box(f"sh{i}", (0.42, 2.24, 0.1), at=(-0.8 + i * 0.4, 0, 0.5), bevel=0.02, rot=(-pitch, 0, 0))  # rises toward Unreal +Y (ADR-0024 roof)
        parts.append(painted(s, P["cherry"] if i % 2 else (0.72, 0.12, 0.18), up_boost=0.12, seed=i))
    obj = A.join("SM_KT_Roof", parts)
    A.pivot_bottom_centre(obj)
    return obj


# --- props --------------------------------------------------------------------------------------

def side_table():
    top = painted(A.cylinder("top", 0.32, 0.06, at=(0, 0, 0.69), sides=16, bevel=0.02), P["mint"], up_boost=0.1)
    rim = painted(A.cylinder("rim", 0.33, 0.03, at=(0, 0, 0.67), sides=16), P["chrome"])
    stem = painted(A.cylinder("stem", 0.05, 0.67, sides=10), P["chrome"])
    foot = painted(A.cylinder("foot", 0.22, 0.05, sides=16, bevel=0.015), P["chrome"])
    obj = A.join("SM_SideTable", [top, rim, stem, foot])
    A.pivot_bottom_centre(obj)
    return obj


def rotary_phone():
    base = painted(A.box("base", (0.26, 0.22, 0.09), taper=0.82, bevel=0.025), P["cherry"])
    dial = painted(A.cylinder("dial", 0.075, 0.02, at=(0.04, 0, 0.085), sides=18, rot=(0, -18, 0)), P["cream"], up_boost=0.05)
    hub = painted(A.cylinder("hub", 0.03, 0.03, at=(0.045, 0, 0.09), sides=12, rot=(0, -18, 0)), P["tile_dark"])
    holes = []
    for i in range(8):
        a = i / 10 * math.tau + 0.6
        holes.append(painted(A.cylinder(f"h{i}", 0.011, 0.025, at=(0.04 + 0.05 * math.cos(a) * math.cos(math.radians(18)), 0.05 * math.sin(a), 0.093), sides=6), P["tile_dark"]))
    cradle = [painted(A.box(f"c{i}", (0.035, 0.05, 0.05), at=(-0.06, y, 0.075), bevel=0.01), P["cherry"]) for i, y in enumerate((-0.085, 0.085))]
    hand = painted(A.box("handle", (0.05, 0.22, 0.035), at=(-0.06, 0, 0.13), bevel=0.012), P["cherry"])
    ear = painted(A.blob("ear", 0.042, at=(-0.06, -0.11, 0.125), squash=(1.1, 1, 0.8), subdiv=1, lump=0.0), P["cherry"])
    mouth = painted(A.blob("mouth", 0.042, at=(-0.06, 0.11, 0.125), squash=(1.1, 1, 0.8), subdiv=1, lump=0.0), P["cherry"])
    obj = A.join("SM_RotaryPhone", [base, dial, hub, hand, ear, mouth] + holes + cradle)
    A.pivot_bottom_centre(obj)
    return obj


# --- characters and creatures (static style proxies; semantic reactions need a rig later) ---------

def raccoon():
    body = painted(A.blob("body", 0.26, at=(0, 0, 0.3), squash=(1.45, 0.95, 0.9), subdiv=2, lump=0.05, seed=3), P["fur"], top=1.2, bottom=0.6)
    belly = painted(A.blob("belly", 0.18, at=(0.08, 0, 0.22), squash=(1.3, 0.9, 0.7), subdiv=2, lump=0.03, seed=4), P["fur_light"])
    head = painted(A.blob("head", 0.2, at=(0.38, 0, 0.46), squash=(1.05, 1.1, 0.95), subdiv=2, lump=0.04, seed=5), P["fur"], top=1.25, bottom=0.75)
    mask = painted(A.box("mask", (0.12, 0.36, 0.09), at=(0.5, 0, 0.47), bevel=0.03), P["fur_dark"])
    snout = painted(A.blob("snout", 0.08, at=(0.56, 0, 0.42), squash=(1.4, 1, 0.8), subdiv=1, lump=0.0), P["fur_light"])
    nose = painted(A.blob("nose", 0.03, at=(0.66, 0, 0.44), subdiv=1, lump=0.0), P["nose"])
    parts = [body, belly, head, mask, snout, nose]
    for s in (-1, 1):
        parts.append(painted(A.cylinder(f"ear{s}", 0.06, 0.12, at=(0.33, 0.12 * s, 0.6), sides=6, radius_top=0.01), P["fur_dark"]))
        parts.append(painted(A.blob(f"eye{s}", 0.045, at=(0.55, 0.075 * s, 0.5), subdiv=1, lump=0.0), P["white"]))
        parts.append(painted(A.blob(f"pupil{s}", 0.025, at=(0.585, 0.08 * s, 0.505), subdiv=1, lump=0.0), P["pupil"]))
        for fx in (0.22, -0.2):
            parts.append(painted(A.cylinder(f"leg{s}{fx}", 0.055, 0.22, at=(fx, 0.14 * s, 0), sides=8, radius_top=0.06), P["fur_dark"], top=1.0, bottom=0.8))
    for i in range(6):  # a ringed tail curling up behind
        a = i / 6
        seg = A.cylinder(f"tail{i}", 0.09 - 0.008 * i, 0.12, at=(-0.34 - 0.1 * i + 0.012 * i * i, 0, 0.3 + 0.012 * i * i * 1.6), sides=8, rot=(0, -80 + 9 * i, 0))
        parts.append(painted(seg, P["fur_dark"] if i % 2 else P["fur_light"], seed=i))
    obj = A.join("SM_Raccoon", parts)
    A.pivot_bottom_centre(obj)
    return obj


def zenny():
    parts = []
    for s in (-1, 1):
        parts.append(painted(A.box(f"leg{s}", (0.2, 0.2, 0.72), at=(0, 0.13 * s, 0.1), taper=1.15, bevel=0.05), P["pants"], top=1.1, bottom=0.7))
        shoe = A.blob(f"shoe{s}", 0.16, at=(0.07, 0.13 * s, 0.08), squash=(1.6, 0.9, 0.62), subdiv=2, lump=0.02, seed=s + 3)
        parts.append(painted(shoe, P["shoe"], up_boost=0.05))
        parts.append(painted(A.box(f"stripe{s}", (0.3, 0.2, 0.04), at=(0.05, 0.13 * s, 0.06)), P["shoe_trim"]))
        arm = A.box(f"arm{s}", (0.16, 0.16, 0.6), at=(0.02, 0.33 * s, 0.82), taper=0.85, bevel=0.05, rot=(-8 * s, 0, 0))
        parts.append(painted(arm, P["hoodie"], top=1.1, bottom=0.75))
        parts.append(painted(A.blob(f"hand{s}", 0.1, at=(0.03, 0.37 * s, 0.78), subdiv=2, lump=0.03, seed=s), P["skin"]))
        parts.append(painted(A.blob(f"eye{s}", 0.07, at=(0.32, 0.11 * s, 1.66), squash=(0.5, 0.9, 1.3), subdiv=1, lump=0), P["white"]))
        parts.append(painted(A.blob(f"pupil{s}", 0.038, at=(0.35, 0.11 * s, 1.66), squash=(0.5, 0.9, 1.2), subdiv=1, lump=0), P["pupil"]))
    torso = A.box("torso", (0.4, 0.56, 0.66), at=(0, 0, 0.78), taper=0.82, bevel=0.1)
    parts.append(painted(torso, P["hoodie"], top=1.15, bottom=0.75))
    parts.append(painted(A.box("pocket", (0.06, 0.34, 0.16), at=(0.19, 0, 0.9), bevel=0.03), (0.92, 0.42, 0.06)))
    parts.append(painted(A.blob("hood", 0.26, at=(-0.14, 0, 1.44), squash=(0.8, 1.1, 0.8), subdiv=2, lump=0.04, seed=8), P["hoodie"]))
    parts.append(painted(A.blob("head", 0.34, at=(0.04, 0, 1.66), squash=(0.95, 0.92, 1.0), subdiv=2, lump=0.02, seed=9), P["skin"], top=1.1, bottom=0.8))
    for i, (x, y, z, r) in enumerate([(0.0, 0, 1.95, 0.22), (0.14, 0.1, 1.93, 0.15), (0.12, -0.13, 1.92, 0.14), (-0.13, 0, 1.9, 0.18)]):
        parts.append(painted(A.blob(f"hair{i}", r, at=(x, y, z), squash=(1.1, 1, 0.65), subdiv=1, lump=0.12, seed=20 + i), P["hair"], top=1.3, bottom=0.8))
    parts.append(painted(A.box("pack", (0.2, 0.42, 0.46), at=(-0.28, 0, 0.9), bevel=0.06), (0.36, 0.22, 0.54)))
    obj = A.join("SM_Zenny", parts)
    A.pivot_bottom_centre(obj)
    return obj


def pehlichi():
    body = painted(A.blob("body", 0.3, squash=(1.0, 0.95, 0.9), subdiv=3, lump=0.0), P["peh"], top=1.15, bottom=0.7)
    visor = painted(A.blob("visor", 0.22, at=(0.2, 0, 0.03), squash=(0.55, 1.15, 0.72), subdiv=2, lump=0.0), P["visor"], up_boost=0.1)
    eye = slot(A.paint(A.blob("eye", 0.1, at=(0.31, 0, 0.05), squash=(0.4, 1.3, 1.0), subdiv=2, lump=0.0), P["peh_eye"], top=1, bottom=1, up_boost=0), "GL_Glow")
    parts = [body, visor, eye]
    parts.append(painted(A.cylinder("antenna", 0.018, 0.22, at=(-0.02, 0, 0.26), sides=6, rot=(0, -12, 0)), P["visor"]))
    parts.append(painted(A.blob("bulb", 0.055, at=(-0.07, 0, 0.5), subdiv=1, lump=0), P["peh_fin"]))
    for s in (-1, 1):
        fin = A.box(f"fin{s}", (0.16, 0.04, 0.22), at=(-0.05, 0.28 * s, 0.02), taper=0.4, bevel=0.02, rot=(35 * s, 0, 0))
        parts.append(painted(fin, P["peh_fin"]))
    for i in range(4):  # a little tail trailing behind
        parts.append(painted(A.blob(f"tail{i}", 0.09 - 0.018 * i, at=(-0.3 - 0.12 * i, 0, -0.08 - 0.04 * i), subdiv=1, lump=0.0), P["peh"] if i % 2 else P["peh_fin"]))
    obj = A.join("SM_Pehlichi", parts)
    A.pivot_bottom_centre(obj)
    return obj


RECIPES = [pine, pine_stump, lambda: boulder("SM_Boulder_A", 3, 1.0), lambda: boulder("SM_Boulder_B", 9, 0.7), grass_tuft, flower, bush,
           k50_foundation, k50_wall_plain, k50_wall_window, k50_wall_door, k50_post, k50_awning, k50_roof, k50_sign,
           timber_wall, lambda: timber_wall("SM_KT_Doorway", door=True), timber_foundation, timber_roof,
           side_table, rotary_phone, raccoon, zenny, pehlichi]


def preview(obj, path: Path):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "VERTEX"
    scene.display.shading.light = "STUDIO"
    scene.render.resolution_x = scene.render.resolution_y = 320
    scene.render.film_transparent = False
    try:
        scene.view_settings.view_transform = "Standard"
    except TypeError:
        pass
    size = max(obj.dimensions)
    cam_data = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    centre = obj.dimensions.z * 0.5
    cam.location = (size * 1.6, -size * 1.6, centre + size * 0.8)
    direction = (obj.location + __import__("mathutils").Vector((0, 0, centre))) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = Path(argv[0] if argv else "Saved/Art/Export").resolve()
    want_preview = "--preview" in argv
    entries = []
    for recipe in RECIPES:
        A.reset_scene()
        obj = recipe()
        A.smooth(obj)
        info = A.stats(obj)
        info["name"] = obj.name
        info["file"] = f"{obj.name}.fbx"
        info["slots"] = [m.name for m in obj.data.materials]
        A.export_fbx(obj, out / info["file"])
        if want_preview:
            preview(obj, out / "preview" / f"{obj.name}.png")
        entries.append(info)
        print(f"GLART {obj.name}: {info['triangles']} tris, slots {info['slots']}")
    A.write_manifest(entries, out / "manifest.json")
    print(f"GLART manifest: {len(entries)} assets -> {out}")


main()
