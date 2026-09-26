"""Declarative schemas for Data/ entities (stdlib only).

Each kind has a closed object schema: unknown keys are errors, so a field the
importer does not understand cannot be silently ignored. References and tags
found while checking are recorded for the cross-entity rules (ID-5, TAG-2/3).
"""

from __future__ import annotations

from dataclasses import dataclass, field

from . import grammar


@dataclass
class Found:
    """Everything a schema walk records besides errors."""

    refs: list[tuple[str, str, tuple[str, ...]]] = field(default_factory=list)  # (json path, id, allowed kinds)
    tags: list[tuple[str, str, str]] = field(default_factory=list)  # (json path, tag, required namespace)
    errors: list[tuple[str, str, str]] = field(default_factory=list)  # (rule, json path, message)

    def error(self, rule: str, path: str, message: str) -> None:
        self.errors.append((rule, path, message))


class Spec:
    def check(self, value, path: str, found: Found) -> None:
        raise NotImplementedError


class Str(Spec):
    def __init__(self, min_len: int = 1, max_len: int = 2000):
        self.min_len, self.max_len = min_len, max_len

    def check(self, value, path, found):
        if not isinstance(value, str):
            found.error("SCHEMA", path, "expected a string")
        elif not self.min_len <= len(value) <= self.max_len:
            found.error("SCHEMA", path, f"string length must be {self.min_len}..{self.max_len}")


class Bool(Spec):
    def check(self, value, path, found):
        if not isinstance(value, bool):
            found.error("SCHEMA", path, "expected true or false")


class Int(Spec):
    def __init__(self, minimum: int | None = None, maximum: int | None = None):
        self.minimum, self.maximum = minimum, maximum

    def check(self, value, path, found):
        if not isinstance(value, int) or isinstance(value, bool):
            found.error("SCHEMA", path, "expected an integer")
        elif (self.minimum is not None and value < self.minimum) or (self.maximum is not None and value > self.maximum):
            found.error("SCHEMA", path, f"integer must be in [{self.minimum}, {self.maximum}]")


class Num(Spec):
    def __init__(self, minimum: float | None = None, maximum: float | None = None, positive: bool = False):
        self.minimum, self.maximum, self.positive = minimum, maximum, positive

    def check(self, value, path, found):
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            found.error("SCHEMA", path, "expected a number")
        elif self.positive and value <= 0:
            found.error("SCHEMA", path, "number must be > 0")
        elif (self.minimum is not None and value < self.minimum) or (self.maximum is not None and value > self.maximum):
            found.error("SCHEMA", path, f"number must be in [{self.minimum}, {self.maximum}]")


class Enum(Spec):
    def __init__(self, *values: str):
        self.values = values

    def check(self, value, path, found):
        if value not in self.values:
            found.error("SCHEMA", path, f"expected one of {', '.join(self.values)}")


class Ref(Spec):
    """A reference to another entity by full id (ID-5)."""

    def __init__(self, *kinds: str):
        self.kinds = kinds

    def check(self, value, path, found):
        problem = grammar.id_problem(value)
        if problem:
            found.error("ID-1", path, f"reference {value!r}: {problem}")
            return
        found.refs.append((path, value, self.kinds))


class Tag(Spec):
    def __init__(self, namespace: str):
        self.namespace = namespace

    def check(self, value, path, found):
        problem = grammar.tag_problem(value)
        if problem:
            found.error("TAG-1", path, f"{value!r}: {problem}")
            return
        found.tags.append((path, value, self.namespace))


class List(Spec):
    def __init__(self, item: Spec, min_items: int = 0, unique: bool = False):
        self.item, self.min_items, self.unique = item, min_items, unique

    def check(self, value, path, found):
        if not isinstance(value, list):
            found.error("SCHEMA", path, "expected a list")
            return
        if len(value) < self.min_items:
            found.error("SCHEMA", path, f"needs at least {self.min_items} item(s)")
        if self.unique:
            seen = [json_key(v) for v in value]
            if len(set(seen)) != len(seen):
                found.error("SCHEMA", path, "items must be unique")
        for index, item in enumerate(value):
            self.item.check(item, f"{path}[{index}]", found)


class Obj(Spec):
    """A closed object. `required` names mandatory keys; `one_of` groups need exactly one present."""

    def __init__(self, fields: dict[str, Spec], required: tuple[str, ...] = (), one_of: tuple[tuple[str, ...], ...] = ()):
        self.fields, self.required, self.one_of = fields, required, one_of

    def check(self, value, path, found):
        if not isinstance(value, dict):
            found.error("SCHEMA", path, "expected an object")
            return
        for key in value:
            if key not in self.fields:
                found.error("SCHEMA", f"{path}.{key}", "unknown field (schemas are closed)")
        for key in self.required:
            if key not in value:
                found.error("SCHEMA", f"{path}.{key}", "required field missing")
        for group in self.one_of:
            present = [k for k in group if k in value]
            if len(present) != 1:
                found.error("SCHEMA", path, f"exactly one of {', '.join(group)} is required")
        for key, spec in self.fields.items():
            if key in value:
                spec.check(value[key], f"{path}.{key}", found)


class Map(Spec):
    """An open object: string keys (1..64 chars), each value checked by `value`."""

    def __init__(self, value: Spec, key_max_len: int = 64):
        self.value, self.key_max_len = value, key_max_len

    def check(self, value, path, found):
        if not isinstance(value, dict):
            found.error("SCHEMA", path, "expected an object")
            return
        for key, item in value.items():
            if not isinstance(key, str) or not 1 <= len(key) <= self.key_max_len:
                found.error("SCHEMA", f"{path}.{key}", "map keys must be 1..64 character strings")
            self.value.check(item, f"{path}.{key}", found)


def json_key(value) -> str:
    import json

    return json.dumps(value, sort_keys=True)


# ---------------------------------------------------------------------------
# Kind schemas (schemaVersion 1)

COMMON = {"schemaVersion": Int(1, 1), "id": Str(), "notes": Str(max_len=4000)}
COMMON_REQUIRED = ("schemaVersion", "id")

YIELD_CLASSES = (
    "repeatable_common", "repeatable_rare", "repeatable_drop",  # scale with settings
    "unique", "glitch_reward", "knowledge", "reward_blueprint", "story", "artifact",  # never scale (ADR-0016)
)
NON_SCALING_CLASSES = ("unique", "glitch_reward", "knowledge", "reward_blueprint", "story", "artifact")

# Pehlichi's capability effects (ADR-0017: none of these deals damage).
CAPABILITY_EFFECTS = (
    "scan_range", "scan_strength", "weak_point_analysis", "signal_analysis", "puzzle_insight", "distract",
    "jam", "disable_temporary", "pacify", "escape_assist", "traversal",
)

REQUIREMENT_KINDS_NEEDING_TARGET = {"Requirement.ObjectSalvaged": "salvage_node", "Requirement.ItemDelivered": None}

COUNT = Int(1, 100000)
ITEM_STACK = Obj({"item": Ref("item"), "count": COUNT}, required=("item", "count"))
VEC3 = List(Num(), min_items=3)
COLLAPSE = Obj({"motion": Enum("drop", "topple"), "direction": Enum("awayFromInstigator", "pieceForward", "pieceBackward")},
               required=("motion",))


def kind(fields: dict[str, Spec], required: tuple[str, ...] = (), one_of=()) -> Obj:
    return Obj({**COMMON, **fields}, required=COMMON_REQUIRED + required, one_of=one_of)


SCHEMAS: dict[str, Obj] = {
    "era": kind(
        {"displayName": Str(), "tag": Tag("Era"), "description": Str()},
        required=("displayName", "tag"),
    ),
    "band": kind(
        {"displayName": Str(), "tag": Tag("Band"), "depth": Int(0, 99), "baselineInterference": Num(0, 1)},
        required=("displayName", "tag", "depth", "baselineInterference"),
    ),
    "yield": kind(
        {"displayName": Str(), "tag": Tag("Yield"), "yieldClass": Enum(*YIELD_CLASSES), "scalable": Bool(), "setting": Str()},
        required=("displayName", "tag", "yieldClass", "scalable"),
    ),
    "settings": kind(
        {"displayName": Str(), "yieldMultipliers": Obj({"resourceYield": Num(positive=True), "creatureDrops": Num(positive=True)},
                                                       required=("resourceYield", "creatureDrops"))},
        required=("displayName", "yieldMultipliers"),
    ),
    "material": kind(
        {
            "displayName": Str(),
            "tags": List(Tag("Material"), min_items=1, unique=True),
            "salvageHardness": Num(positive=True),
            "support": Obj({"strength": Num(positive=True), "maxHorizontalSpan": Num(positive=True), "maxStack": Int(1, 1000)},
                           required=("strength", "maxHorizontalSpan", "maxStack")),
            # P6: scales the noise radius of working this material, and collapse impact damage (default 1).
            "noiseScale": Num(0, 10),
            "impactScale": Num(0, 10),
        },
        required=("displayName", "tags", "salvageHardness"),
    ),
    "item": kind(
        {
            "displayName": Str(),
            "stackSize": Int(1, 10000),
            "weight": Num(0, 1000),
            "material": Ref("material"),
            "tool": Obj({"toolClass": Tag("Tool"), "tier": Int(1, 10)}, required=("toolClass", "tier")),
            # M11: Zenny can fight with it (Pehlichi never deals damage, ADR-0017). Metres, seconds.
            "weapon": Obj({"damage": Num(positive=True), "reach": Num(0.5, 10), "cooldownSeconds": Num(0.1, 10)},
                          required=("damage", "reach", "cooldownSeconds")),
            "criticalPath": Bool(),
            "sources": List(Tag("Source"), min_items=1, unique=True),
            "onAcquireUnlocks": List(Ref("knowledge"), unique=True),
        },
        required=("displayName", "stackSize", "weight", "criticalPath", "sources"),
    ),
    "knowledge": kind(
        {"displayName": Str(), "category": Tag("Knowledge"), "criticalPath": Bool(), "sources": List(Tag("Source"), min_items=1, unique=True)},
        required=("displayName", "category", "criticalPath", "sources"),
    ),
    "recipe": kind(
        {
            "displayName": Str(),
            "output": ITEM_STACK,
            "inputs": List(ITEM_STACK, min_items=1),
            "station": Tag("Station"),
            "craftSeconds": Num(0, 3600),
            "unlockedBy": List(Ref("knowledge"), unique=True),
        },
        required=("displayName", "output", "inputs", "craftSeconds"),
    ),
    "salvage": kind(
        {
            "displayName": Str(),
            "integrity": Num(positive=True),
            "material": Ref("material"),
            "requiresTool": Tag("Tool"),
            "yields": List(Obj({"item": Ref("item"), "count": COUNT, "yieldCategory": Ref("yield")},
                               required=("item", "count", "yieldCategory")), min_items=1),
            "toolEfficiency": List(Obj({"toolClass": Tag("Tool"), "minTier": Int(1, 10), "multiplier": Num(positive=True)},
                                       required=("toolClass", "minTier", "multiplier"))),
            "onSalvageUnlocks": List(Ref("knowledge"), unique=True),
            # Gameplay events emitted when salvage completes (dialogue hooks, ADR-0015).
            "onSalvageEvents": List(Tag("Event"), unique=True),
            # P6: the noise action each hit makes (default Noise.Salvage.Hit).
            "noise": Tag("Noise"),
        },
        required=("displayName", "integrity", "yields"),
    ),
    # M10 building v0 (Docs/ADR/0024). Metres, piece-local, origin at the bottom centre, +X along the piece.
    "buildpiece": kind(
        {
            "displayName": Str(),
            "era": Ref("era"),
            "material": Ref("material"),
            "role": Enum("foundation", "wall", "doorway", "roof", "post", "beam", "floor", "stump", "trunk"),
            # May rest directly on terrain (foundations, posts); otherwise it needs another piece.
            "grounded": Bool(),
            # Axis-aligned bounds in piece space (overlap tests, terrain protection).
            "size": VEC3,
            # Visual and collision boxes; pitch tilts a box about its local X axis (roofs).
            "shapes": List(Obj({"size": VEC3, "offset": VEC3, "pitch": Num(-89, 89)}, required=("size", "offset")), min_items=1),
            # bottom meets top: the upper piece rests on the lower; side meets side: a lateral link.
            "sockets": List(Obj({"name": Str(max_len=64), "role": Enum("bottom", "top", "side"), "offset": VEC3},
                                required=("name", "role", "offset")), min_items=1),
            "cost": List(ITEM_STACK, min_items=1),
            "unlockedBy": List(Ref("knowledge"), unique=True),
            # P6: false = world-only (authored structures, trees): no cost, never offered to the player (BLD-5).
            "buildable": Bool(),
            # P6: how it moves when unsupported (deterministic collapse).
            "collapse": COLLAPSE,
            # P7: how it looks (visual.*); collision and support stay on shapes/sockets.
            "visual": Ref("visual"),
        },
        required=("displayName", "era", "material", "role", "grounded", "size", "shapes", "sockets"),
    ),
    # M10 terraforming v0 (ADR-0022): one stroke of a heightfield tool. Metres.
    "terraform": kind(
        {
            "displayName": Str(),
            "op": Enum("DIG", "RAISE", "FLATTEN"),
            "radius": Num(0.5, 20),
            "amount": Num(0, 5),
            "requiresTool": Tag("Tool"),
            "cost": List(ITEM_STACK),
            "yields": List(ITEM_STACK),
        },
        required=("displayName", "op", "radius", "amount", "requiresTool"),
    ),
    "capability": kind(
        {
            "displayName": Str(),
            "owner": Enum("pehlichi"),
            "category": Tag("Capability"),
            "levels": List(Obj({"level": Int(1, 20), "effects": List(Obj({"kind": Enum(*CAPABILITY_EFFECTS), "value": Num()},
                                                                         required=("kind", "value")), min_items=1)},
                               required=("level", "effects")), min_items=1),
        },
        required=("displayName", "owner", "category", "levels"),
    ),
    "glitch": kind(
        {
            "displayName": Str(),
            "detection": Obj({"capability": Ref("capability"), "minLevel": Int(1, 20)}, required=("capability", "minLevel")),
            "requirements": List(Obj({"name": Str(max_len=64), "kind": Tag("Requirement"), "item": Ref("item"), "count": COUNT, "puzzle": Ref("puzzle")},
                                     required=("name", "kind"))),
            "repair": Obj({"seconds": Num(positive=True), "interruptPolicy": Enum("KeepProgress", "ResetProgress")},
                          required=("seconds", "interruptPolicy")),
            "stabilityWeight": Num(0, 1000),
            # Metres over which the glitch corrupts (unrepaired) or stabilizes (repaired); default 40.
            "influenceRadius": Num(1, 2000),
            "rewards": List(Obj({
                "recipient": Enum("Pehlichi", "Zenny"),
                "capability": Ref("capability"), "delta": Int(1, 10),
                "item": Ref("item"), "count": COUNT, "yieldCategory": Ref("yield"),
                "knowledge": Ref("knowledge"),
            }, required=("recipient",), one_of=(("capability", "item", "knowledge"),))),
        },
        required=("displayName", "detection", "repair", "stabilityWeight"),
    ),
    "cell": kind(
        {
            "displayName": Str(),
            "band": Ref("band"),
            "coord": Obj({"x": Int(-1000, 1000), "y": Int(-1000, 1000)}, required=("x", "y")),
            "eraComposition": List(Obj({"era": Ref("era"), "weight": Num(positive=True)}, required=("era", "weight")), min_items=1),
            "level": Str(),
            # Half-width of the playable area in metres; beyond it the next band's interference applies.
            "playableHalfExtent": Num(1, 5000),
            # Grid pitch (P3, ADR-0026): the cell is a square of this edge, centred on coord * sizeMetres.
            # Every cell uses the same pitch (GRID-1); it is data, not yet a final design decision.
            "sizeMetres": Num(64, 8192),
            # Local static intensity on top of the band's baseline (P3): independent of depth and era.
            "interferenceOffset": Num(-1, 1),
            # Runtime heightfield ground (ADR-0022); covers the whole cell (sizeMetres), in whole chunks.
            "terrain": Obj({"chunkMetres": Int(8, 256), "spacingMetres": Num(0.25, 4), "baseHeight": Num(-1000, 1000),
                            "maxDigDepth": Num(0, 50), "maxRaiseHeight": Num(0, 50),
                            # Authored relief (GLTerrainGen), flattened to baseHeight within edgeBlendMetres of the
                            # cell edge so neighbouring cells always meet seamlessly.
                            "relief": Obj({"seed": Int(0, 2147483647), "amplitudeMetres": Num(0, 200), "edgeBlendMetres": Num(1, 1000)},
                                          required=("seed", "amplitudeMetres", "edgeBlendMetres"))},
                           required=("chunkMetres", "spacingMetres", "baseHeight", "maxDigDepth", "maxRaiseHeight")),
        },
        required=("displayName", "band", "coord", "eraComposition"),
    ),
    "placement": kind(
        {
            "kind": Enum("glitch", "salvage_node", "spawn", "patrol", "discovery", "encounter", "puzzle_site", "structure", "scatter"),
            "definition": Ref("glitch", "salvage", "creature", "knowledge", "puzzle", "structure", "visual"),
            "anchor": Ref("anchor"),
            "offset": VEC3,
            "transform": Obj({"location": VEC3, "yaw": Num(-360, 360)}, required=("location",)),
            "bindings": Map(Ref("placement")),  # glitch requirement name -> target placement (rules PLC-2)
            # discovery: metres within which Zenny finds it (default 8). scatter: the patch radius.
            "radius": Num(0.5, 200),
            # scatter (P7): how many instances of the visual within the radius.
            "count": Int(1, 5000),
        },
        required=("kind", "definition"),
        one_of=(("anchor", "transform"),),
    ),
    # M11: a corrupted creature. Threat comes from places (ADR-0014): creatures exist only through
    # placements, never because of what the player is doing. Metres and seconds.
    "creature": kind(
        {
            "displayName": Str(),
            "health": Num(positive=True),
            "walkSpeed": Num(0.5, 20),
            "chaseSpeed": Num(0.5, 20),
            "perception": Obj({"sightRadius": Num(1, 200), "coneDegrees": Num(10, 360), "hearingRadius": Num(0, 200),
                               # P6: seconds it searches Zenny's last known position after losing sight (default: tuning).
                               "memorySeconds": Num(0, 600)},
                              required=("sightRadius", "coneDegrees", "hearingRadius")),
            "attack": Obj({"damage": Num(positive=True), "reach": Num(0.5, 10), "cooldownSeconds": Num(0.1, 30)},
                          required=("damage", "reach", "cooldownSeconds")),
            # How far from its home it will chase before giving up and going back.
            "leashRadius": Num(1, 500),
            "drops": List(Obj({"item": Ref("item"), "count": COUNT, "yieldCategory": Ref("yield")},
                              required=("item", "count", "yieldCategory"))),
            # P7: how it looks (visual.*).
            "visual": Ref("visual"),
        },
        required=("displayName", "health", "walkSpeed", "chaseSpeed", "perception", "attack", "leashRadius"),
    ),
    # M11: a bounded Glitch Storm NICE sets off (one representative event, not a weather system).
    "storm": kind(
        {
            "displayName": Str(),
            "durationSeconds": Num(1, 600),
            "radius": Num(1, 200),
            "spawnPerSecond": Num(0.1, 50),
            "maxArtifacts": Int(1, 500),  # at most this many falling at once
            "artifacts": List(Enum("cat", "dog"), min_items=1, unique=True),
            # Starts the first time this many Event.* have fired (e.g. NICE retaliates after repairs).
            "trigger": Obj({"eventCount": Tag("Event"), "min": Int(1, 1000)}, required=("eventCount", "min")),
            "harmless": Bool(),
        },
        required=("displayName", "durationSeconds", "radius", "spawnPerSecond", "maxArtifacts", "artifacts", "trigger", "harmless"),
    ),
    # P6: an authored world structure (salvageable building, tree): the canonical structural contract.
    # Parts are pieces in the shared structural language (buildpiece), placed in structure space (metres).
    "structure": kind(
        {
            "displayName": Str(),
            "parts": List(Obj({"name": Str(max_len=32), "piece": Ref("buildpiece"), "location": VEC3, "yawQuarter": Int(0, 3),
                               "salvage": Ref("salvage"), "collapse": COLLAPSE},
                              required=("name", "piece", "location", "salvage")), min_items=1),
        },
        required=("displayName", "parts"),
    ),
    # P7: how something looks (art pipeline runtime contract). Metres, degrees; presentation only.
    "visual": kind(
        {
            "displayName": Str(),
            "mesh": Str(max_len=64),
            "tint": List(Num(0, 4), min_items=3),
            "outline": Bool(),
            "castShadow": Bool(),
            "scale": Num(0.05, 20),
            "offset": VEC3,
            "yaw": Num(-360, 360),
            # NICE's corruption: electric-blue precise cubes, SPARSE (VIS-2).
            "corruption": List(Obj({"offset": VEC3, "size": Num(0.01, 2), "rotation": VEC3}, required=("offset", "size"))),
            "light": Obj({"color": List(Num(0, 1), min_items=3), "intensity": Num(0, 100000), "radius": Num(0.1, 200), "offset": VEC3},
                         required=("color", "intensity", "radius")),
        },
        required=("mesh",),
    ),
    # P6: provisional physical and noise tuning (data, never tuned by feel). Metres and seconds.
    "tuning": kind(
        {
            "displayName": Str(),
            "collapse": Obj({"gravity": Num(positive=True), "startDelaySeconds": Num(0, 10), "impactMarginMetres": Num(0, 5),
                             "impactHeightMetres": Num(0.1, 20), "damageBase": Num(0, 10000), "damagePerMetreFallen": Num(0, 10000),
                             "damageMax": Num(0, 10000), "toppleStartDegrees": Num(0.1, 45)},
                            required=("gravity", "startDelaySeconds", "impactMarginMetres", "impactHeightMetres", "damageBase",
                                      "damagePerMetreFallen", "damageMax", "toppleStartDegrees")),
            "noise": Obj({"investigateSeconds": Num(0, 600), "memorySeconds": Num(0, 600), "radius": Map(Num(0, 500))},
                         required=("investigateSeconds", "memorySeconds", "radius")),
        },
        required=("displayName", "collapse", "noise"),
    ),
    # ADR-0023: Zenny answers through gameplay. CONSTRUCT is reserved until building exists.
    "puzzle": kind(
        {
            "displayName": Str(),
            "family": Enum("riddle", "environmental", "signal", "construction", "observation", "navigation", "electrical", "memory"),
            "answer": Obj({"mode": Enum("PRESENT", "MANIPULATE", "PERFORM"), "item": Ref("item")}, required=("mode",)),
            "poseExchange": Ref("exchange"),
        },
        required=("displayName", "family", "answer"),
    ),
    "exchange": kind(
        {
            "trigger": List(Tag("Event"), min_items=1, unique=True),
            "category": Enum("StoryCritical", "Contextual", "Ambient"),
            "priority": Int(0, 100),
            "cooldownSeconds": Num(0, 1e7),
            "maxUses": Int(1, 1000000),
            "weight": Num(positive=True),
            # Only react when the event is about this content id (e.g. item.part.fuse).
            "subject": Ref(),
            # History-aware conditions: how often an Event.* tag (or its children) has fired so far.
            "requires": List(Obj({"eventCount": Tag("Event"), "min": Int(0, 1000000), "max": Int(0, 1000000)}, required=("eventCount",))),
            # voice (P4 hook): a sound asset path; when present, its length sets the subtitle duration.
            "lines": List(Obj({"speaker": Enum("NICE", "Pehlichi"), "text": Str(max_len=500), "voice": Str(max_len=256)},
                              required=("speaker", "text")), min_items=1),
        },
        required=("trigger", "category", "priority", "cooldownSeconds", "weight", "lines"),
    ),
}

def key_paths(spec: Spec, prefix: str = "") -> list[str]:
    """Every key path a schema allows, e.g. 'tool.toolClass', 'yields[].item', 'bindings{}'."""
    paths: list[str] = []
    if isinstance(spec, Obj):
        for key, sub in spec.fields.items():
            path = f"{prefix}.{key}" if prefix else key
            paths.append(path)
            paths.extend(key_paths(sub, path))
    elif isinstance(spec, List):
        paths.extend(key_paths(spec.item, prefix + "[]"))
    elif isinstance(spec, Map):
        paths.append(prefix + "{}")
    return paths


PLACEMENT_KIND_DEFINITION = {"glitch": "glitch", "salvage_node": "salvage", "spawn": "creature", "patrol": "creature",
                             "discovery": "knowledge", "encounter": "creature", "puzzle_site": "puzzle",
                             "structure": "structure", "scatter": "visual"}
