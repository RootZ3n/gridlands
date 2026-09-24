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
    "scan_range", "scan_strength", "weak_point_analysis", "signal_analysis", "distract",
    "jam", "disable_temporary", "pacify", "escape_assist", "traversal",
)

REQUIREMENT_KINDS_NEEDING_TARGET = {"Requirement.ObjectSalvaged": "salvage_node", "Requirement.ItemDelivered": None}

COUNT = Int(1, 100000)
ITEM_STACK = Obj({"item": Ref("item"), "count": COUNT}, required=("item", "count"))
VEC3 = List(Num(), min_items=3)


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
        },
        required=("displayName", "integrity", "yields"),
    ),
    "buildpiece": kind(
        {
            "displayName": Str(),
            "era": Ref("era"),
            "material": Ref("material"),
            "sockets": List(Str(max_len=64), min_items=1, unique=True),
            "cost": List(ITEM_STACK, min_items=1),
            "unlockedBy": List(Ref("knowledge"), unique=True),
        },
        required=("displayName", "era", "material", "sockets", "cost"),
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
            "requirements": List(Obj({"name": Str(max_len=64), "kind": Tag("Requirement"), "item": Ref("item"), "count": COUNT},
                                     required=("name", "kind"))),
            "repair": Obj({"seconds": Num(positive=True), "interruptPolicy": Enum("KeepProgress", "ResetProgress")},
                          required=("seconds", "interruptPolicy")),
            "stabilityWeight": Num(0, 1000),
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
        },
        required=("displayName", "band", "coord", "eraComposition"),
    ),
    "placement": kind(
        {
            "kind": Enum("glitch", "salvage_node", "spawn", "patrol", "discovery", "encounter"),
            "definition": Ref("glitch", "salvage", "creature", "knowledge", "puzzle"),
            "anchor": Ref("anchor"),
            "offset": VEC3,
            "transform": Obj({"location": VEC3, "yaw": Num(-360, 360)}, required=("location",)),
            "bindings": Map(Ref("placement")),  # glitch requirement name -> target placement (rules PLC-2)
        },
        required=("kind", "definition"),
        one_of=(("anchor", "transform"),),
    ),
    "exchange": kind(
        {
            "trigger": List(Tag("Event"), min_items=1, unique=True),
            "category": Enum("StoryCritical", "Contextual", "Ambient"),
            "priority": Int(0, 100),
            "cooldownSeconds": Num(0, 1e7),
            "maxUses": Int(1, 1000000),
            "weight": Num(positive=True),
            "lines": List(Obj({"speaker": Enum("NICE", "Pehlichi"), "text": Str(max_len=500)}, required=("speaker", "text")), min_items=1),
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
                             "discovery": "knowledge", "encounter": "creature"}
