"""Tests for Tools/gldata: each rule is proven to fire by breaking the real Data/ once.

Every test copies the repository's Data/ and Config/Tags into a temporary root,
applies one targeted mutation, and asserts the expected rule code appears. The
clean copy must validate with zero problems, so a rule that fires spuriously
is caught too.
"""

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
REPO = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from gldata import grammar, tagfiles, validate  # noqa: E402


class Sandbox:
    def __init__(self):
        self.root = Path(tempfile.mkdtemp())
        shutil.copytree(REPO / "Data", self.root / "Data")
        shutil.copytree(REPO / "Config" / "Tags", self.root / "Config" / "Tags")
        # VIS-1 looks for imported art meshes: mirror their names (empty files; content is irrelevant).
        meshes = self.root / "Content" / "Gridlands" / "Art" / "Meshes"
        meshes.mkdir(parents=True)
        for mesh in (REPO / "Content" / "Gridlands" / "Art" / "Meshes").glob("*.uasset"):
            (meshes / mesh.name).touch()

    def path(self, entity_id: str) -> Path:
        return self.root / "Data" / Path(*entity_id.split(".")).with_suffix(".json")

    def read(self, entity_id: str) -> dict:
        return json.loads(self.path(entity_id).read_text())

    def write(self, entity_id: str, data: dict, at: Path | None = None) -> None:
        target = at or self.path(entity_id)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(data))

    def edit(self, entity_id: str, change) -> None:
        data = self.read(entity_id)
        change(data)
        self.write(entity_id, data)

    def rules(self) -> set[str]:
        return {p.rule for p in validate.run(self.root).problems}

    def problems(self):
        return validate.run(self.root).problems


class CleanDataTests(unittest.TestCase):
    def test_repository_data_is_valid(self):
        problems = validate.run(REPO).problems
        self.assertEqual(problems, [], "\n".join(map(str, problems)))

    def test_sandbox_copy_is_valid(self):
        self.assertEqual(Sandbox().problems(), [])

    def test_all_ten_eras_are_data(self):
        eras = sorted(p.stem for p in (REPO / "Data" / "era" / "memory").glob("*.json"))
        self.assertEqual(len(eras), 10, eras)

    def test_validation_is_deterministic(self):
        first = [str(p) for p in validate.run(REPO).problems]
        second = [str(p) for p in validate.run(REPO).problems]
        self.assertEqual(first, second)


class GrammarTests(unittest.TestCase):
    def test_good_ids(self):
        for good in ("item.material.copper_wire", "era.memory.native_american_1800s", "a.b.c", "a.b.c.d.e"):
            self.assertIsNone(grammar.id_problem(good), good)

    def test_bad_ids(self):
        for bad in ("item.copper", "Item.material.x", "item.material.Copper", "item.material.copper__wire",
                    "item.material.copper_", "item.material.1copper", "a.b.c.d.e.f", "item.material." + "x" * 33, 5):
            self.assertIsNotNone(grammar.id_problem(bad), bad)

    def test_shared_id_corpus(self):
        corpus = json.loads((TOOLS / "tests" / "id-corpus.json").read_text())
        for good in corpus["valid"]:
            self.assertIsNone(grammar.id_problem(good), good)
        for bad in corpus["invalid"]:
            self.assertIsNotNone(grammar.id_problem(bad), bad)

    def test_tags(self):
        self.assertIsNone(grammar.tag_problem("Event.Salvage.WireStripped"))
        for bad in ("event.salvage", "Event.salvage", "A.B.C.D.E", "Era.Roman_Age"):
            self.assertIsNotNone(grammar.tag_problem(bad), bad)


class RuleTests(unittest.TestCase):
    def setUp(self):
        self.box = Sandbox()

    def assertRule(self, rule):
        rules = self.box.rules()
        self.assertIn(rule, rules, f"expected {rule}, got {sorted(rules)}")

    def test_id1_bad_id(self):
        self.box.edit("item.material.copper_wire", lambda d: d.update(id="item.material.Copper"))
        self.assertRule("ID-1")

    def test_id2_kind_mismatch(self):
        data = self.box.read("item.material.scrap_metal")
        data["id"] = "recipe.material.scrap_metal"
        self.box.path("item.material.scrap_metal").unlink()
        self.box.write("", data, at=self.box.root / "Data/item/material/scrap_metal.json")
        self.assertRule("ID-2")

    def test_id3_duplicate(self):
        data = self.box.read("item.material.scrap_metal")
        self.box.write("", data, at=self.box.root / "Data/item/material/scrap_metal_copy.json")
        self.assertRule("ID-3")

    def test_id4_path_must_mirror_id(self):
        data = self.box.read("item.part.fuse")
        self.box.path("item.part.fuse").unlink()
        self.box.write("", data, at=self.box.root / "Data/item/material/fuse.json")
        self.assertRule("ID-4")

    def test_id5_dangling_reference(self):
        self.box.edit("recipe.tool.pry_bar", lambda d: d["inputs"][0].update(item="item.material.unobtainium"))
        self.assertRule("ID-5")

    def test_id5_wrong_kind_reference(self):
        self.box.edit("recipe.tool.pry_bar", lambda d: d["inputs"][0].update(item="material.metal.copper"))
        self.assertRule("ID-5")

    def test_id6_alias_to_nothing(self):
        (self.box.root / "Data/_aliases.json").write_text(json.dumps({"schemaVersion": 1, "aliases": {"item.material.old": "item.material.gone"}}))
        self.assertRule("ID-6")

    def test_id8_missing_schema_version(self):
        self.box.edit("item.part.fuse", lambda d: d.pop("schemaVersion"))
        self.assertRule("ID-8")

    def test_id9_unregistered_kind_folder(self):
        self.box.write("", {"schemaVersion": 1, "id": "spell.fire.ball"}, at=self.box.root / "Data/spell/fire/ball.json")
        self.assertRule("ID-9")

    def test_id10_placement_in_unknown_cell(self):
        data = self.box.read("placement.origin.junk_pile_01")
        data["id"] = "placement.nowhere.junk_pile_01"
        self.box.write("placement.nowhere.junk_pile_01", data)
        self.assertRule("ID-10")

    def test_schema_closed_objects(self):
        self.box.edit("item.part.fuse", lambda d: d.update(damage=5))
        self.assertRule("SCHEMA")

    def test_schema_kind_without_schema_is_refused(self):
        self.box.write("creature.glitched.raccoon", {"schemaVersion": 1, "id": "creature.glitched.raccoon"})
        self.assertRule("SCHEMA")

    def test_tag2_unregistered_namespace(self):
        self.box.edit("item.part.fuse", lambda d: d.update(sources=["Loot.Chest"]))
        self.assertRule("TAG-2")

    def test_tag3_undeclared_tag(self):
        self.box.edit("item.part.fuse", lambda d: d.update(sources=["Source.Stolen"]))
        self.assertRule("TAG-3")

    def test_gen1_stale_generated_tags(self):
        self.box.edit("era.memory.roman", lambda d: d.update(tag="Era.Rome"))
        self.assertRule("GEN-1")

    def test_era1_no_power_on_eras(self):
        self.box.edit("era.memory.roman", lambda d: d.update(tier=3))
        self.assertRule("ERA-1")

    def test_nc2_critical_path_needs_non_combat_source(self):
        self.box.edit("item.part.fuse", lambda d: d.update(sources=["Source.CreatureDrop", "Source.Boss"]))
        self.assertRule("NC-2")

    def test_nc2_combat_source_alongside_non_combat_is_fine(self):
        self.box.edit("item.part.fuse", lambda d: d.update(sources=["Source.Salvage", "Source.CreatureDrop"]))
        self.assertNotIn("NC-2", self.box.rules())

    def test_pz1_present_answer_needs_item(self):
        self.box.edit("puzzle.home.map_riddle", lambda d: d["answer"].pop("item"))
        self.assertRule("PZ-1")

    def test_puzzles_cannot_be_answered_by_text_or_choice(self):
        # ADR-0023: no dialogue / free-text / multiple-choice answer modes exist in the schema.
        self.box.edit("puzzle.home.map_riddle", lambda d: d["answer"].update(mode="TEXT"))
        self.assertRule("SCHEMA")

    def test_construct_is_reserved(self):
        self.box.edit("puzzle.home.map_riddle", lambda d: d["answer"].update(mode="CONSTRUCT"))
        self.assertRule("SCHEMA")

    def test_bld1_piece_material_must_be_structural(self):
        self.box.edit("material.timber.pine", lambda d: d.pop("support"))
        self.assertRule("BLD-1")

    def test_bld2_piece_must_rest_on_something(self):
        self.box.edit("buildpiece.modern.timber_wall", lambda d: d.update(sockets=[s for s in d["sockets"] if s["role"] != "bottom"]))
        self.assertRule("BLD-2")

    def test_bld3_socket_names_unique(self):
        self.box.edit("buildpiece.modern.timber_wall", lambda d: d["sockets"][1].update(name="bottom"))
        self.assertRule("BLD-3")

    def test_bld4_socket_inside_bounds(self):
        self.box.edit("buildpiece.modern.timber_roof", lambda d: d["sockets"][0].update(offset=[0, -3, 0]))
        self.assertRule("BLD-4")

    def test_tf1_raising_is_never_free(self):
        self.box.edit("terraform.shovel.raise", lambda d: d.pop("cost"))
        self.assertRule("TF-1")
        box = Sandbox()
        box.edit("terraform.shovel.dig", lambda d: d.pop("yields"))
        self.assertIn("TF-1", box.rules())

    def test_cr1_creature_drops_are_combat_sources(self):
        self.box.edit("item.material.static_residue", lambda d: d.update(sources=["Source.Salvage"]))
        self.assertRule("CR-1")

    def test_cr2_creatures_come_from_places(self):
        self.box.path("placement.origin.drain_gremlin_den").unlink()
        self.assertRule("CR-2")

    def test_storm_must_say_whether_it_harms(self):
        self.box.edit("storm.playful.cats_and_dogs", lambda d: d.pop("harmless"))
        self.assertRule("SCHEMA")

    def test_grid1_one_pitch(self):
        self.box.edit("cell.outer.diner_lots", lambda d: d.update(sizeMetres=512.0))
        self.assertRule("GRID-1")

    def test_grid2_unique_coords(self):
        self.box.edit("cell.outer.diner_lots", lambda d: d.update(coord={"x": 0, "y": 0}))
        self.assertRule("GRID-2")

    def test_grid3_terrain_tiles_the_cell(self):
        self.box.edit("cell.outer.diner_lots", lambda d: d["terrain"].update(chunkMetres=60))
        self.assertRule("GRID-3")

    def test_kn2_every_knowledge_has_a_book(self):
        self.box.edit("knowledge.memory.fifties_diner", lambda d: d.update(category="Knowledge.Unfiled"))
        self.assertRule("KN-2")

    def test_kn3_blueprints_come_from_ofi(self):
        self.box.edit("buildpiece.modern.timber_wall", lambda d: d.update(unlockedBy=["knowledge.memory.fifties_diner"]))
        self.assertRule("KN-3")

    def test_kn1_declared_source_must_be_backed(self):
        self.box.edit("knowledge.style.roman_masonry", lambda d: d.update(sources=["Source.Salvage"]))
        self.assertRule("KN-1")

    def test_e1_glitch_reward_category_must_not_scale(self):
        self.box.edit("yield.reward.glitch", lambda d: d.update(scalable=True, setting="resourceYield"))
        self.assertRule("E-1")

    def test_e1_repeatable_rare_must_scale(self):
        self.box.edit("yield.rare.material", lambda d: (d.update(scalable=False), d.pop("setting")))
        self.assertRule("E-1")

    def test_e1_glitch_item_reward_must_use_non_scaling_category(self):
        self.box.edit("glitch.home.flicker_lamp", lambda d: d["rewards"].append(
            {"recipient": "Zenny", "item": "item.part.fuse", "count": 1, "yieldCategory": "yield.common.salvage"}))
        self.assertRule("E-1")

    def test_p3_pehlichi_deals_zero_damage(self):
        self.box.edit("capability.pehlichi.distract", lambda d: d["levels"][0]["effects"].append({"kind": "bite_damage", "value": 5}))
        self.assertRule("P-3")

    def test_dlg1_zenny_is_silent(self):
        self.box.edit("exchange.player.died_again", lambda d: d["lines"].append({"speaker": "Zenny", "text": "..."}))
        self.assertRule("DLG-1")

    def test_plc1_placement_definition_kind(self):
        self.box.edit("placement.origin.junk_pile_01", lambda d: d.update(definition="glitch.home.flicker_lamp"))
        self.assertRule("PLC-1")

    def test_plc2_missing_requirement_binding(self):
        self.box.edit("placement.origin.glitch_flicker_lamp", lambda d: d.pop("bindings"))
        self.assertRule("PLC-2")

    def test_plc2_binding_to_wrong_placement_kind(self):
        self.box.edit("placement.origin.glitch_flicker_lamp", lambda d: d.update(bindings={"blocker": "placement.origin.glitch_flicker_lamp"}))
        self.assertRule("PLC-2")

    # Anchor tests must not depend on which anchors the real maps export today.
    UNKNOWN_ANCHOR = "anchor.origin.test_no_such_anchor"

    def test_plc3_unknown_anchor(self):
        self.box.edit("placement.origin.junk_pile_01", lambda d: (d.pop("transform"), d.update(anchor=self.UNKNOWN_ANCHOR)))
        self.assertRule("PLC-3")

    def test_plc3_known_anchor_resolves(self):
        path = self.box.root / "Data/anchor/origin.generated.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        anchors = json.loads(path.read_text()) if path.exists() else {"schemaVersion": 1, "cell": "cell.home.origin", "anchors": []}
        anchors["anchors"].append({"id": "anchor.origin.test_added_anchor", "transform": {"location": [0, 0, 0]}})
        path.write_text(json.dumps(anchors))
        self.box.edit("placement.origin.junk_pile_01", lambda d: (d.pop("transform"), d.update(anchor="anchor.origin.test_added_anchor")))
        self.assertEqual(self.box.problems(), [])


class SliceDialogueTests(unittest.TestCase):
    """M11: smart-ass dialogue is a core feature; the slice carries ~40 exchanges across these categories."""

    FAMILIES = {
        "salvage": "Event.Salvage", "overencumbrance": "Event.Player.Overencumbered", "building": "Event.Building",
        "death": "Event.Player.Died", "exploration": "Event.Discovery", "glitches": "Event.Glitch",
        "riddle": "Event.Puzzle", "silence": "Event.Player.Silent", "ambient": "Event.Ambient",
        "creature": "Event.Creature", "storm": "Event.Storm", "terraform": "Event.Terrain",
    }

    def test_breadth(self):
        ds = validate.run(REPO)
        exchanges = [e for e in ds.entities.values() if e.kind == "exchange"]
        self.assertGreaterEqual(len(exchanges), 40)
        triggers = [t for e in exchanges for t in e.data.get("trigger", [])]
        for family, prefix in self.FAMILIES.items():
            self.assertTrue(any(t == prefix or t.startswith(prefix + ".") for t in triggers), f"no {family} exchange ({prefix})")
        antagonism = [e for e in exchanges if {"NICE", "Pehlichi"} <= {l.get("speaker") for l in e.data.get("lines", [])}]
        self.assertGreaterEqual(len(antagonism), 15, "NICE and Pehlichi should spar often")

    def test_every_exchange_is_rate_limited(self):
        ds = validate.run(REPO)
        for e in ds.entities.values():
            if e.kind == "exchange" and e.data.get("category") != "StoryCritical":
                self.assertTrue(e.data.get("maxUses") or e.data.get("cooldownSeconds", 0) > 0, f"{e.id} could repeat forever")


class GenerateTests(unittest.TestCase):
    def test_generated_tag_file_is_in_sync(self):
        ds = validate.run(REPO)
        expected = tagfiles.render_generated(ds)
        self.assertEqual((REPO / tagfiles.GENERATED_FILE).read_text(), expected)

    def test_schema_export_in_sync_and_nonempty(self):
        text = (REPO / tagfiles.SCHEMA_EXPORT).read_text()
        self.assertEqual(text, tagfiles.render_schema_export())
        kinds = json.loads(text)["kinds"]
        self.assertIn("tool.toolClass", kinds["item"])
        self.assertIn("bindings{}", kinds["placement"])
        self.assertIn("yields[].yieldCategory", kinds["salvage"])

    def test_generated_tags_cover_eras_bands_yields(self):
        declared = tagfiles.declared_tags(REPO)
        for tag in ("Era.Roman", "Era.ModernDay", "Band.Home", "Band.NiceCore", "Yield.Rare.Material"):
            self.assertIn(tag, declared)


if __name__ == "__main__":
    unittest.main()


class VisualRuleTests(unittest.TestCase):
    """P7: VIS-1 visuals name imported meshes; VIS-2 NICE's corruption stays sparse."""

    def setUp(self):
        self.box = Sandbox()

    def test_vis1_unknown_mesh(self):
        self.box.edit("visual.prop.rotary_phone", lambda d: d.update(mesh="SM_NotImported"))
        self.assertIn("VIS-1", self.box.rules())

    def test_vis2_too_many_cubes(self):
        cube = {"offset": [0, 0, 0.1], "size": 0.05}
        self.box.edit("visual.prop.rotary_phone", lambda d: d.update(corruption=[cube] * 5))
        self.assertIn("VIS-2", self.box.rules())

    def test_vis2_cube_too_big(self):
        self.box.edit("visual.prop.rotary_phone", lambda d: d.update(corruption=[{"offset": [0, 0, 0], "size": 0.5}]))
        self.assertIn("VIS-2", self.box.rules())

