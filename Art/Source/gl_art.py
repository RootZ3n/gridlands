"""Gridlands art kit for Blender (P7 visual spike): the shared stylization every asset recipe uses.

The look is built from a few reusable decisions, not hand-fixing per asset:
  - chunky forms: generous bevels, exaggerated proportions (recipes choose sizes);
  - illustrated imperfection: a seeded, size-relative irregularity (never on NICE corruption, which
    must stay mathematically precise and is not made here at all);
  - painted colour baked into vertex colours: a palette colour per part, lit top-to-bottom like a
    painted gradient, up-facing faces a little brighter, a little per-face variation;
  - pivot at the bottom centre (the buildpiece/structure convention, ADR-0024), metres.

Run by Art/Source/build_assets.py inside `blender -b --python`. Pure bpy/bmesh, no add-ons.
"""

import json
import math
import random
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def _object_from_bmesh(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def box(name, size, at=(0, 0, 0), bevel=0.0, segments=2, taper=1.0, rot=(0, 0, 0)):
    """An axis-aligned box of `size` metres whose BOTTOM centre sits at `at`. taper < 1 narrows the top."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= size[0]
        v.co.y *= size[1]
        v.co.z = (v.co.z + 0.5) * size[2]
        if v.co.z > size[2] * 0.5 and taper != 1.0:
            v.co.x *= taper
            v.co.y *= taper
    obj = _object_from_bmesh(name, bm)
    if bevel > 0:
        _bevel(obj, bevel, segments)
    obj.rotation_euler = [math.radians(a) for a in rot]
    obj.location = at
    return obj


def cylinder(name, radius, height, at=(0, 0, 0), sides=12, bevel=0.0, radius_top=None, rot=(0, 0, 0)):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=sides, radius1=radius, radius2=radius if radius_top is None else radius_top, depth=height)
    for v in bm.verts:
        v.co.z += height * 0.5
    obj = _object_from_bmesh(name, bm)
    if bevel > 0:
        _bevel(obj, bevel, 2)
    obj.rotation_euler = [math.radians(a) for a in rot]
    obj.location = at
    return obj


def blob(name, radius, at=(0, 0, 0), squash=(1, 1, 1), subdiv=2, lump=0.12, seed=1):
    """A lumpy rounded form (rocks, bodies, canopies)."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=radius)
    rng = random.Random(seed)
    for v in bm.verts:
        v.co.x *= squash[0]
        v.co.y *= squash[1]
        v.co.z *= squash[2]
        v.co *= 1.0 + rng.uniform(-lump, lump)
    obj = _object_from_bmesh(name, bm)
    obj.location = at
    return obj


def _bevel(obj, width, segments):
    mod = obj.modifiers.new("bevel", "BEVEL")
    mod.width = width
    mod.segments = segments
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(35)
    _apply(obj, mod)


def _apply(obj, mod):
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)


def irregular(obj, amount, seed):
    """Illustrated imperfection: displace vertices by a seeded, smooth-ish offset (metres)."""
    rng = random.Random(seed)
    offsets = {}
    for v in obj.data.vertices:
        key = (round(v.co.x, 2), round(v.co.y, 2), round(v.co.z, 2))  # coincident verts move together
        if key not in offsets:
            offsets[key] = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.5, 0.5))) * amount
        v.co += offsets[key]


def paint(obj, colour, top=1.12, bottom=0.72, up_boost=0.10, variation=0.05, seed=0):
    """Bakes a painted gradient into a CORNER colour attribute ('Col', sRGB bytes)."""
    mesh = obj.data
    attr = mesh.color_attributes.get("Col") or mesh.color_attributes.new(name="Col", type="BYTE_COLOR", domain="CORNER")
    world = obj.matrix_world
    zs = [(world @ v.co).z for v in mesh.vertices]
    z0, z1 = min(zs), max(zs)
    span = max(1e-4, z1 - z0)
    rng = random.Random(seed)
    for poly in mesh.polygons:
        vary = 1.0 + rng.uniform(-variation, variation)
        normal_z = (world.to_3x3() @ poly.normal).normalized().z
        for li in poly.loop_indices:
            z = (world @ mesh.vertices[mesh.loops[li].vertex_index].co).z
            h = (z - z0) / span
            k = (bottom + (top - bottom) * h) * vary + (up_boost if normal_z > 0.7 else 0.0)
            attr.data[li].color = (min(1, colour[0] * k), min(1, colour[1] * k), min(1, colour[2] * k), 1.0)
    return obj


def join(name, parts):
    """Joins parts (applying their transforms) into one object named `name`."""
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
        bpy.context.view_layer.objects.active = p
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.data.name = name
    return obj


def pivot_bottom_centre(obj, centre_xy=True):
    """Moves the mesh so its bounds' bottom centre is the origin (buildpiece convention)."""
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    shift = Vector(((min(xs) + max(xs)) / 2 if centre_xy else 0, (min(ys) + max(ys)) / 2 if centre_xy else 0, min(zs)))
    obj.data.transform(Matrix.Translation(-shift))
    obj.location = (0, 0, 0)


def smooth(obj, angle=40):
    for p in obj.data.polygons:
        p.use_smooth = True
    mod = obj.modifiers.new("smooth", "SMOOTH_BY_ANGLE") if hasattr(bpy.types, "SmoothByAngleModifier") else None
    if mod is None:
        try:
            obj.data.set_sharp_from_angle(angle=math.radians(angle))
        except AttributeError:
            pass


def stats(obj):
    obj.data.calc_loop_triangles()
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    return {
        "triangles": len(obj.data.loop_triangles),
        "boundsMin": [round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)],
        "boundsMax": [round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)],
    }


def export_fbx(obj, path: Path):
    """FBX in metres (FBX_SCALE_NONE): Unreal converts to cm; the importer checks bounds against the manifest."""
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=str(path),
        use_selection=True,
        global_scale=1.0,  # metres; with FBX_SCALE_NONE Unreal reads them as metres and converts to cm (validated below)
        apply_unit_scale=False,
        apply_scale_options="FBX_SCALE_NONE",
        axis_forward="-Y",
        axis_up="Z",
        mesh_smooth_type="FACE",
        use_mesh_modifiers=True,
        colors_type="LINEAR",  # the FBX importer gamma-encodes once; sRGB here would double it (pastel colours)
        add_leaf_bones=False,
        bake_anim=False,
    )


def write_manifest(entries, path: Path):
    path.write_text(json.dumps({"schemaVersion": 1, "comment": "GENERATED by Art/Source/build_assets.py", "assets": entries}, indent=1, sort_keys=True) + "\n")
