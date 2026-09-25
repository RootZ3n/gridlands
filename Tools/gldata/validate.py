"""Validate Data/ against Docs/CONTENT-IDS-AND-TAGS.md and the design invariants.

Every problem is a Problem(rule, file, where, message). Rule codes match the
documents: ID-*, TAG-*, ERA-1, NC-2, E-1, P-3, DLG-1, PLC-*, GEN-1, SCHEMA.
Output is sorted, so two runs over the same data print the same thing.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

from . import grammar, schema, tagfiles

SKIP_TOP = {"_registry", "README.md", "_aliases.json"}  # _registry holds registries and generated exports
ERA_POWER_KEYS = {"strength", "damage", "tier", "support", "yieldMultiplier", "power", "level"}
DAMAGE_WORDS = ("damage", "attack", "hurt", "kill", "harm")


@dataclass(frozen=True, order=True)
class Problem:
    rule: str
    file: str
    where: str
    message: str

    def __str__(self) -> str:
        where = f" {self.where}" if self.where else ""
        return f"{self.rule:<7} {self.file}{where}: {self.message}"


@dataclass
class Entity:
    id: str
    kind: str
    file: str
    data: dict
    found: schema.Found


@dataclass
class Dataset:
    root: Path
    kinds: list[str]
    namespaces: dict[str, str]
    combat_sources: set[str]
    entities: dict[str, Entity] = field(default_factory=dict)
    anchors: dict[str, str] = field(default_factory=dict)  # anchor id -> file
    aliases: dict[str, str] = field(default_factory=dict)
    problems: list[Problem] = field(default_factory=list)

    def problem(self, rule: str, file: str, where: str, message: str) -> None:
        self.problems.append(Problem(rule, file, where, message))


def load_json(path: Path, ds: Dataset, rel: str):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        ds.problem("SCHEMA", rel, "", f"unreadable JSON: {error}")
        return None


def load(repo_root: Path) -> Dataset:
    data_root = repo_root / "Data"
    registry = json.loads((data_root / "_registry" / "kinds.json").read_text(encoding="utf-8"))
    tag_registry = json.loads((data_root / "_registry" / "tag-namespaces.json").read_text(encoding="utf-8"))
    ds = Dataset(root=repo_root, kinds=registry["kinds"], namespaces=tag_registry["namespaces"],
                 combat_sources=set(tag_registry.get("combatSources", [])))

    aliases_path = data_root / "_aliases.json"
    if aliases_path.exists():
        aliases = load_json(aliases_path, ds, "Data/_aliases.json")
        if isinstance(aliases, dict):
            ds.aliases = aliases.get("aliases", {})

    for path in sorted(data_root.rglob("*.json")):
        rel = path.relative_to(repo_root).as_posix()
        parts = path.relative_to(data_root).parts
        if parts[0] in SKIP_TOP:
            continue
        kind = parts[0]
        if kind not in ds.kinds:
            ds.problem("ID-9", rel, "", f"top-level folder '{kind}' is not a registered kind")
            continue
        if kind == "anchor":
            load_anchor_file(path, rel, parts, ds)
            continue
        data = load_json(path, ds, rel)
        if data is None:
            continue
        load_entity(path, rel, parts, kind, data, ds)
    return ds


def load_entity(path: Path, rel: str, parts: tuple[str, ...], kind: str, data, ds: Dataset) -> None:
    if not isinstance(data, dict):
        ds.problem("SCHEMA", rel, "", "an entity file must contain one JSON object")
        return
    if "schemaVersion" not in data or "id" not in data:
        ds.problem("ID-8", rel, "", "entity needs 'schemaVersion' and 'id'")
        return
    entity_id = data["id"]
    problem = grammar.id_problem(entity_id)
    if problem:
        ds.problem("ID-1", rel, ".id", f"{entity_id!r}: {problem}")
        return
    if grammar.id_kind(entity_id) != kind:
        ds.problem("ID-2", rel, ".id", f"id kind '{grammar.id_kind(entity_id)}' does not match folder kind '{kind}'")
    expected = "Data/" + "/".join(entity_id.split(".")) + ".json"
    if rel != expected:
        ds.problem("ID-4", rel, "", f"file path must mirror id: expected {expected}")
    if entity_id in ds.entities:
        ds.problem("ID-3", rel, ".id", f"duplicate id, also defined in {ds.entities[entity_id].file}")
        return
    if kind == "era":
        power = sorted(ERA_POWER_KEYS & set(data))
        if power:
            ds.problem("ERA-1", rel, "", f"eras carry no gameplay power; remove {', '.join(power)}")
    if kind == "capability":
        check_no_damage(data, rel, ds)
    found = schema.Found()
    spec = schema.SCHEMAS.get(kind)
    if spec is None:
        ds.problem("SCHEMA", rel, "", f"kind '{kind}' has no schema yet; content of this kind cannot be validated")
    else:
        spec.check(data, "", found)
    ds.entities[entity_id] = Entity(entity_id, kind, rel, data, found)


def load_anchor_file(path: Path, rel: str, parts: tuple[str, ...], ds: Dataset) -> None:
    if len(parts) != 2 or not parts[1].endswith(".generated.json"):
        ds.problem("ID-4", rel, "", "anchors live only in Data/anchor/<cell>.generated.json")
        return
    cell_short = parts[1][: -len(".generated.json")]
    data = load_json(path, ds, rel)
    if not isinstance(data, dict) or not isinstance(data.get("anchors"), list):
        ds.problem("SCHEMA", rel, "", "anchor file needs an 'anchors' list")
        return
    for index, anchor in enumerate(data["anchors"]):
        anchor_id = anchor.get("id") if isinstance(anchor, dict) else None
        problem = grammar.id_problem(anchor_id)
        if problem:
            ds.problem("ID-1", rel, f".anchors[{index}]", f"{anchor_id!r}: {problem}")
            continue
        if grammar.id_kind(anchor_id) != "anchor" or anchor_id.split(".")[1] != cell_short:
            ds.problem("ID-10", rel, f".anchors[{index}]", f"{anchor_id} must be anchor.{cell_short}.<name>")
        ds.anchors[anchor_id] = rel


def check_no_damage(data: dict, rel: str, ds: Dataset) -> None:
    """P-3 / ADR-0017: Pehlichi deals zero direct damage. Named explicitly, before generic schema errors."""
    for li, level in enumerate(data.get("levels", []) if isinstance(data.get("levels"), list) else []):
        for ei, effect in enumerate(level.get("effects", []) if isinstance(level, dict) else []):
            kind = str(effect.get("kind", "")) if isinstance(effect, dict) else ""
            if any(word in kind.lower() for word in DAMAGE_WORDS):
                ds.problem("P-3", rel, f".levels[{li}].effects[{ei}]",
                           f"effect '{kind}': Pehlichi deals zero direct damage (ADR-0017); changing that needs a new operator ADR")


def cross_check(ds: Dataset) -> None:
    declared_tags = tagfiles.declared_tags(ds.root)
    cell_shorts: dict[str, str] = {}
    for entity in ds.entities.values():
        if entity.kind == "cell":
            short = entity.id.split(".")[-1]
            if short in cell_shorts:
                ds.problem("ID-10", entity.file, ".id", f"cell short name '{short}' also used by {cell_shorts[short]}")
            cell_shorts[short] = entity.id

    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        for rule, where, message in entity.found.errors:
            ds.problem(rule, entity.file, where, message)
        for where, target, kinds in entity.found.refs:
            resolved = ds.aliases.get(target, target)
            if grammar.id_kind(resolved) == "anchor":
                if resolved not in ds.anchors:
                    ds.problem("PLC-3", entity.file, where, f"anchor {target} not in any Data/anchor/*.generated.json")
                continue
            if resolved not in ds.entities:
                ds.problem("ID-5", entity.file, where, f"reference {target} does not resolve")
            elif kinds and grammar.id_kind(resolved) not in kinds:
                ds.problem("ID-5", entity.file, where, f"reference {target} must be a {' or '.join(kinds)}")
        for where, tag, namespace in entity.found.tags:
            if grammar.tag_namespace(tag) not in ds.namespaces:
                ds.problem("TAG-2", entity.file, where, f"{tag}: namespace '{grammar.tag_namespace(tag)}' is not registered")
            elif grammar.tag_namespace(tag) != namespace:
                ds.problem("TAG-2", entity.file, where, f"{tag}: expected a {namespace}.* tag")
            elif tag not in declared_tags and not declares_tag(entity, where):
                ds.problem("TAG-3", entity.file, where, f"{tag} is not declared in Config/Tags/*.ini")
        if entity.kind == "placement" and entity.id.split(".")[1] not in cell_shorts:
            ds.problem("ID-10", entity.file, ".id", f"no cell with short name '{entity.id.split('.')[1]}'")

    check_aliases(ds)
    check_non_combat(ds)
    check_knowledge_sources(ds)
    check_yields(ds)
    check_dialogue(ds)
    check_placements(ds)
    check_puzzles(ds)
    check_building(ds)
    check_terraform(ds)
    check_creatures(ds)
    check_grid(ds)
    check_knowledge_domains(ds)
    check_generated_tags(ds)


def declares_tag(entity: Entity, where: str) -> bool:
    """An era/band/yield entity's own `tag` field declares it (generated into Config/Tags, GEN-1)."""
    return entity.kind in tagfiles.GENERATING_KINDS and where == ".tag"


def check_aliases(ds: Dataset) -> None:
    for old, new in sorted(ds.aliases.items()):
        if new in ds.aliases:
            ds.problem("ID-6", "Data/_aliases.json", old, f"alias chains through {new}; point directly at the final id")
        elif new not in ds.entities:
            ds.problem("ID-6", "Data/_aliases.json", old, f"alias target {new} does not exist")
        if old in ds.entities:
            ds.problem("ID-6", "Data/_aliases.json", old, "an aliased id must not also exist as an entity")


def check_non_combat(ds: Dataset) -> None:
    """NC-2: everything on the critical path has at least one non-combat source."""
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.data.get("criticalPath") is True:
            sources = set(entity.data.get("sources", []))
            if sources and sources <= ds.combat_sources:
                ds.problem("NC-2", entity.file, ".sources",
                           f"critical-path {entity.kind} has only combat sources ({', '.join(sorted(sources))}); add a non-combat source")


# A knowledge source tag is only real if some content actually grants the knowledge that way.
KNOWLEDGE_SOURCE_BACKING = {
    "Source.Salvage": ("salvage", "onSalvageUnlocks"),
    "Source.Discovery": ("item", "onAcquireUnlocks"),
}


def check_knowledge_sources(ds: Dataset) -> None:
    """KN-1: declared knowledge sources are backed by content (so NC-2 cannot pass on paper only)."""
    granted: dict[str, set[str]] = {tag: set() for tag in KNOWLEDGE_SOURCE_BACKING}
    for entity in ds.entities.values():
        for tag, (kind, field_name) in KNOWLEDGE_SOURCE_BACKING.items():
            if entity.kind == kind:
                granted[tag].update(entity.data.get(field_name, []))
    for entity in ds.entities.values():
        if entity.kind == "placement" and entity.data.get("kind") == "discovery":
            granted["Source.Discovery"].add(entity.data.get("definition", ""))
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind == "glitch":
            for reward in entity.data.get("rewards", []):
                if isinstance(reward, dict) and "knowledge" in reward:
                    granted.setdefault("Source.GlitchReward", set()).add(reward["knowledge"])
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "knowledge":
            continue
        for tag in entity.data.get("sources", []):
            if tag in granted and entity.id not in granted[tag]:
                ds.problem("KN-1", entity.file, ".sources", f"declares {tag} but nothing grants {entity.id} that way")


def check_yields(ds: Dataset) -> None:
    """E-1 / ADR-0016: scaling follows the yield class; settings cover every scalable setting key."""
    settings_keys: set[str] = set()
    presets = [e for e in ds.entities.values() if e.kind == "settings"]
    for preset in presets:
        settings_keys |= set(preset.data.get("yieldMultipliers", {}))
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "yield":
            continue
        klass, scalable = entity.data.get("yieldClass"), entity.data.get("scalable")
        if klass in schema.NON_SCALING_CLASSES and scalable is not False:
            ds.problem("E-1", entity.file, ".scalable", f"class '{klass}' must never scale (ADR-0016)")
        if klass not in schema.NON_SCALING_CLASSES and klass is not None and scalable is not True:
            ds.problem("E-1", entity.file, ".scalable", f"repeatable class '{klass}' scales with the player's setting (ADR-0016)")
        if scalable is True and not entity.data.get("setting"):
            ds.problem("E-1", entity.file, ".setting", "a scalable yield category must name its world setting")
        if scalable is True and presets and entity.data.get("setting") not in settings_keys:
            ds.problem("E-1", entity.file, ".setting", f"setting '{entity.data.get('setting')}' is not defined by any settings preset")
        if scalable is False and "setting" in entity.data:
            ds.problem("E-1", entity.file, ".setting", "a non-scalable category must not name a setting")
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "glitch":
            continue
        for index, reward in enumerate(entity.data.get("rewards", [])):
            if not isinstance(reward, dict) or "item" not in reward:
                continue
            category = ds.entities.get(reward.get("yieldCategory", ""))
            if category is None:
                ds.problem("E-1", entity.file, f".rewards[{index}]", "item rewards must name a yieldCategory")
            elif category.data.get("scalable") is not False:
                ds.problem("E-1", entity.file, f".rewards[{index}].yieldCategory", "glitch rewards never scale; use a non-scalable category")


def check_dialogue(ds: Dataset) -> None:
    """DLG-1: Zenny is silent."""
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "exchange":
            continue
        for index, line in enumerate(entity.data.get("lines", [])):
            if isinstance(line, dict) and str(line.get("speaker", "")).lower() == "zenny":
                ds.problem("DLG-1", entity.file, f".lines[{index}].speaker", "Zenny is silent; only NICE and Pehlichi speak")


def check_placements(ds: Dataset) -> None:
    """PLC-1: placement kind matches its definition. PLC-2: glitch requirement bindings resolve correctly."""
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "placement":
            continue
        kind, definition = entity.data.get("kind"), entity.data.get("definition", "")
        expected = schema.PLACEMENT_KIND_DEFINITION.get(kind)
        if expected and isinstance(definition, str) and grammar.id_kind(definition) != expected:
            ds.problem("PLC-1", entity.file, ".definition", f"a {kind} placement needs a {expected}.* definition")
        bindings = entity.data.get("bindings", {}) if isinstance(entity.data.get("bindings"), dict) else {}
        if kind != "glitch":
            if bindings:
                ds.problem("PLC-2", entity.file, ".bindings", "only glitch placements have requirement bindings")
            continue
        glitch = ds.entities.get(definition)
        requirements = {r.get("name"): r for r in glitch.data.get("requirements", []) if isinstance(r, dict)} if glitch else {}
        for name in sorted(bindings):
            if name not in requirements:
                ds.problem("PLC-2", entity.file, f".bindings.{name}", f"{definition} has no requirement named '{name}'")
        for name, requirement in sorted(requirements.items(), key=lambda kv: str(kv[0])):
            target_kind = schema.REQUIREMENT_KINDS_NEEDING_TARGET.get(requirement.get("kind"))
            if target_kind is None:
                continue
            target = ds.entities.get(bindings.get(name, ""))
            if name not in bindings:
                ds.problem("PLC-2", entity.file, ".bindings", f"requirement '{name}' ({requirement.get('kind')}) needs a binding")
            elif target is not None and target.data.get("kind") != target_kind:
                ds.problem("PLC-2", entity.file, f".bindings.{name}", f"must bind a {target_kind} placement")


def check_puzzles(ds: Dataset) -> None:
    """PZ-1: a PRESENT answer names its item. PZ-2: a PuzzleSolved requirement names its puzzle."""
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind == "puzzle":
            answer = entity.data.get("answer", {})
            if isinstance(answer, dict) and answer.get("mode") == "PRESENT" and "item" not in answer:
                ds.problem("PZ-1", entity.file, ".answer", "a PRESENT answer must name the item Zenny presents")
        if entity.kind == "glitch":
            for index, requirement in enumerate(entity.data.get("requirements", [])):
                if isinstance(requirement, dict) and requirement.get("kind") == "Requirement.PuzzleSolved" and "puzzle" not in requirement:
                    ds.problem("PZ-2", entity.file, f".requirements[{index}]", "Requirement.PuzzleSolved must name its puzzle")


def check_building(ds: Dataset) -> None:
    """BLD-1 a piece's material is structural; BLD-2 a piece has a bottom socket at z = 0 (it rests on
    something); BLD-3 socket names are unique; BLD-4 every socket lies within the piece's bounds."""
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "buildpiece":
            continue
        data, rel = entity.data, entity.file
        material = ds.entities.get(data.get("material", ""))
        if material and "support" not in material.data:
            ds.problem("BLD-1", rel, ".material", f"{data['material']} has no support values; it cannot be built with")
        sockets = [s for s in data.get("sockets", []) if isinstance(s, dict)]
        if not any(s.get("role") == "bottom" and abs(s.get("offset", [0, 0, 1])[2]) < 1e-6 for s in sockets):
            ds.problem("BLD-2", rel, ".sockets", "needs a bottom socket at z = 0 (what the piece rests on)")
        names = [s.get("name") for s in sockets]
        if len(names) != len(set(names)):
            ds.problem("BLD-3", rel, ".sockets", "socket names must be unique")
        size = data.get("size", [0, 0, 0])
        for s in sockets:
            x, y, z = (s.get("offset", [0, 0, 0]) + [0, 0, 0])[:3]
            if abs(x) > size[0] / 2 + 1e-6 or abs(y) > size[1] / 2 + 1e-6 or z < -1e-6 or z > size[2] + 1e-6:
                ds.problem("BLD-4", rel, ".sockets", f"socket '{s.get('name')}' lies outside the piece's size {size}")


def check_terraform(ds: Dataset) -> None:
    """TF-1 no free ground: raising costs items and digging yields them (conservation)."""
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "terraform":
            continue
        data, rel = entity.data, entity.file
        if data.get("op") == "RAISE" and not data.get("cost"):
            ds.problem("TF-1", rel, ".cost", "RAISE must cost what DIG yields (no free ground)")
        if data.get("op") == "DIG" and not data.get("yields"):
            ds.problem("TF-1", rel, ".yields", "DIG must yield what RAISE costs")


def check_creatures(ds: Dataset) -> None:
    """CR-1 creature drops are combat sources: every dropped item must declare Source.CreatureDrop, and
    (with NC-2) never be critical-path by drops alone. CR-2 a creature exists only through a placement."""
    placed = {e.data.get("definition") for e in ds.entities.values() if e.kind == "placement"}
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind != "creature":
            continue
        for index, drop in enumerate(entity.data.get("drops", [])):
            item = ds.entities.get(drop.get("item", "")) if isinstance(drop, dict) else None
            if item and "Source.CreatureDrop" not in item.data.get("sources", []):
                ds.problem("CR-1", entity.file, f".drops[{index}]", f"{drop['item']} must list Source.CreatureDrop among its sources")
        if entity.id not in placed:
            ds.problem("CR-2", entity.file, "", "a creature must be placed somewhere (threat comes from places, ADR-0014)")


def check_grid(ds: Dataset) -> None:
    """GRID-1 every cell declares the same sizeMetres (one Grid pitch); GRID-2 no two cells share a coord;
    GRID-3 a cell's terrain tiles its size exactly in whole chunks (neighbours meet edge to edge)."""
    cells = sorted((e for e in ds.entities.values() if e.kind == "cell"), key=lambda e: e.id)
    sizes = {e.data.get("sizeMetres") for e in cells}
    if len(sizes) > 1 or None in sizes:
        for e in cells:
            ds.problem("GRID-1", e.file, ".sizeMetres", f"every cell needs the same sizeMetres (found {sorted(map(str, sizes))})")
    seen: dict[tuple, str] = {}
    for e in cells:
        coord = e.data.get("coord", {})
        key = (coord.get("x"), coord.get("y"))
        if key in seen:
            ds.problem("GRID-2", e.file, ".coord", f"coord {key} already used by {seen[key]}")
        seen[key] = e.id
        terrain, size = e.data.get("terrain"), e.data.get("sizeMetres")
        if terrain and size:
            chunk, spacing = terrain.get("chunkMetres", 0), terrain.get("spacingMetres", 1)
            if chunk <= 0 or abs(size / chunk - round(size / chunk)) > 1e-9 or abs(chunk / spacing - round(chunk / spacing)) > 1e-9:
                ds.problem("GRID-3", e.file, ".terrain", f"{chunk} m chunks at {spacing} m spacing must tile a {size} m cell exactly")


def check_knowledge_domains(ds: Dataset) -> None:
    """KN-2 every knowledge category belongs to exactly one book (Chukka / Ofi / Hoponi); KN-3 what unlocks
    a recipe or a build piece is Ofi knowledge (Docs/KNOWLEDGE-AND-DISCOVERY.md: never one generic unlock DB)."""
    path = ds.root / "Data" / "_registry" / "knowledge-domains.json"
    domains = json.loads(path.read_text(encoding="utf-8")).get("domains", {}) if path.exists() else {}
    for category, book in domains.items():
        if book not in ("chukka", "ofi", "hoponi"):
            ds.problem("KN-2", "Data/_registry/knowledge-domains.json", f".domains.{category}", f"unknown book '{book}'")
    for entity in sorted(ds.entities.values(), key=lambda e: e.id):
        if entity.kind == "knowledge" and entity.data.get("category") not in domains:
            ds.problem("KN-2", entity.file, ".category", f"{entity.data.get('category')} belongs to no knowledge book")
        if entity.kind in ("recipe", "buildpiece"):
            for index, ref in enumerate(entity.data.get("unlockedBy", [])):
                known = ds.entities.get(ref)
                if known and domains.get(known.data.get("category")) != "ofi":
                    ds.problem("KN-3", entity.file, f".unlockedBy[{index}]", f"{ref} is not Ofi (blueprint) knowledge")


def check_generated_tags(ds: Dataset) -> None:
    for relative, expected in ((tagfiles.GENERATED_FILE, tagfiles.render_generated(ds)),
                               (tagfiles.SCHEMA_EXPORT, tagfiles.render_schema_export())):
        path = ds.root / relative
        actual = path.read_text(encoding="utf-8") if path.exists() else ""
        if actual != expected:
            ds.problem("GEN-1", relative, "", "out of date; run `Tools/data.sh generate`")


def run(repo_root: Path) -> Dataset:
    ds = load(repo_root)
    cross_check(ds)
    ds.problems = sorted(set(ds.problems))
    return ds
