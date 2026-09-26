"""Gridlands art kit for Blender (P7 visual spike; P7.1 dimensional refinement): the shared stylization
every asset recipe uses.

P7.1 adds dimensionality underneath the colour: rounder bevels that catch light, curved and bent forms,
chiselled rocks, ambient occlusion baked into the vertex colour's alpha (the material decides how much
of it to show), and material classes (the slot names; the importer binds each to a master instance).

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


def subdivide_z(obj, cuts):
    """Cuts every mostly-vertical edge into cuts+1 pieces (so blades and walls can bend and bulge)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    edges = [e for e in bm.edges if abs((e.verts[0].co - e.verts[1].co).normalized().z) > 0.7]
    bmesh.ops.subdivide_edges(bm, edges=edges, cuts=cuts, use_grid_fill=True)
    bm.to_mesh(obj.data)
    bm.free()


def subdivide_axis(obj, axis, cuts):
    """Cuts every edge that runs mostly along an axis (0 x, 1 y, 2 z) into cuts+1 pieces."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    edges = [e for e in bm.edges if (e.verts[0].co - e.verts[1].co).length > 1e-6 and abs((e.verts[0].co - e.verts[1].co).normalized()[axis]) > 0.7]
    bmesh.ops.subdivide_edges(bm, edges=edges, cuts=cuts, use_grid_fill=True)
    bm.to_mesh(obj.data)
    bm.free()


def bend(obj, amount, axis=0, power=2.0):
    """Leans the top over along an axis (local metres at the top), curving from the base: blades, awnings."""
    zs = [v.co.z for v in obj.data.vertices]
    z0, z1 = min(zs), max(zs)
    for v in obj.data.vertices:
        t = (v.co.z - z0) / max(1e-4, z1 - z0)
        v.co[axis] += amount * (t ** power)


def bulge(obj, amount, axes=(0, 1)):
    """Swells the middle of a form outward (a gently curved wall or trunk): amount in metres at mid height."""
    zs = [v.co.z for v in obj.data.vertices]
    z0, z1 = min(zs), max(zs)
    cx = sum(v.co.x for v in obj.data.vertices) / len(obj.data.vertices)
    cy = sum(v.co.y for v in obj.data.vertices) / len(obj.data.vertices)
    for v in obj.data.vertices:
        t = (v.co.z - z0) / max(1e-4, z1 - z0)
        k = amount * math.sin(math.pi * t)
        d = Vector((v.co.x - cx if 0 in axes else 0, v.co.y - cy if 1 in axes else 0, 0))
        if d.length > 1e-5:
            v.co += d.normalized() * k


def chisel(obj, planes, depth, seed, up_bias=0.5):
    """Cuts flat facets into a rounded form (chunky rocks): vertices beyond a random plane are pushed onto it."""
    rng = random.Random(seed)
    centre = sum((v.co for v in obj.data.vertices), Vector()) / len(obj.data.vertices)
    radius = max((v.co - centre).length for v in obj.data.vertices)
    for _ in range(planes):
        n = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-up_bias, 1))).normalized()
        d = radius * (1.0 - rng.uniform(depth * 0.4, depth))
        for v in obj.data.vertices:
            k = (v.co - centre).dot(n)
            if k > d:
                v.co -= n * (k - d)


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


def paint_up(obj, colour, threshold=0.55, softness=0.25, top=1.1):
    """Paints up-facing faces (moss on rock, snow, grass on a ledge), blending by how much they face up."""
    mesh = obj.data
    attr = mesh.color_attributes["Col"]
    world = obj.matrix_world.to_3x3()
    for poly in mesh.polygons:
        nz = (world @ poly.normal).normalized().z
        t = max(0.0, min(1.0, (nz - threshold) / softness))
        if t <= 0:
            continue
        for li in poly.loop_indices:
            c = attr.data[li].color
            attr.data[li].color = (c[0] + (colour[0] * top - c[0]) * t, c[1] + (colour[1] * top - c[1]) * t, c[2] + (colour[2] * top - c[2]) * t, c[3])
    return obj


def bake_ao(obj, samples=24, distance=0.8, strength=0.9, seed=7):
    """Ambient occlusion baked into the colour attribute's ALPHA (1 = open, 0 = fully occluded).

    Rays from each vertex over its normal's hemisphere against the whole joined mesh: crevices, the
    underside of an awning, the base of a trunk darken; the master material decides how much shows."""
    from mathutils.bvhtree import BVHTree
    mesh = obj.data
    attr = mesh.color_attributes.get("Col")
    if attr is None:
        return obj
    tree = BVHTree.FromPolygons([v.co.copy() for v in mesh.vertices], [p.vertices[:] for p in mesh.polygons])
    rng = random.Random(seed)
    dirs = []
    for i in range(samples):  # a fixed cosine-weighted set, rotated per vertex by the normal frame
        u, w = (i + 0.5) / samples, rng.random()
        r, a = math.sqrt(u), math.tau * w
        dirs.append(Vector((r * math.cos(a), r * math.sin(a), math.sqrt(max(0.0, 1.0 - u)))))
    mesh.calc_normals_split() if hasattr(mesh, "calc_normals_split") else None
    ao = []
    for v in mesh.vertices:
        n = v.normal.normalized()
        t = n.orthogonal().normalized()
        b = n.cross(t)
        origin = v.co + n * 0.004
        hit = 0
        for d in dirs:
            world_d = (t * d.x + b * d.y + n * d.z).normalized()
            loc, _, _, dist = tree.ray_cast(origin, world_d, distance)
            if loc is not None:
                hit += 1.0 - (dist / distance) * 0.5
        ao.append(max(0.0, 1.0 - strength * hit / samples))
    for li, loop in enumerate(mesh.loops):
        c = attr.data[li].color
        attr.data[li].color = (c[0], c[1], c[2], ao[loop.vertex_index])
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
        mesh_smooth_type="OFF",  # P7.1: export Blender's split normals (smooth forms, sharp creases) as-is
        use_mesh_modifiers=True,
        colors_type="LINEAR",  # the FBX importer gamma-encodes once; sRGB here would double it (pastel colours)
        add_leaf_bones=False,
        bake_anim=False,
    )


def write_manifest(entries, path: Path):
    path.write_text(json.dumps({"schemaVersion": 1, "comment": "GENERATED by Art/Source/build_assets.py", "assets": entries}, indent=1, sort_keys=True) + "\n")
