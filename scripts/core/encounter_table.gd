extends Node
## Encounter Table — autoloaded singleton.
## Defines which enemies appear in each zone and their spawn weights.

## Zone encounter data.
## Each zone maps to an array of { "id": String, "weight": int, "min_level": int }
var encounter_data: Dictionary = {
	"home": [],
	"zone_1_arena": [
		{ "id": "worm_drone", "weight": 40, "min_level": 1 },
		{ "id": "glitched_egg", "weight": 30, "min_level": 1 },
		{ "id": "corrupted_dragon", "weight": 20, "min_level": 2 },
		{ "id": "worm_swarm", "weight": 10, "min_level": 3 },
	],
	"zone_2_settlement": [
		{ "id": "rogue_agent", "weight": 35, "min_level": 3 },
		{ "id": "corrupted_tool", "weight": 30, "min_level": 3 },
		{ "id": "glitched_session", "weight": 20, "min_level": 4 },
		{ "id": "shadow_node", "weight": 15, "min_level": 5 },
	],
	"zone_3_engine": [
		{ "id": "malformed_data", "weight": 35, "min_level": 5 },
		{ "id": "hostile_process", "weight": 30, "min_level": 5 },
		{ "id": "corrupted_resume", "weight": 20, "min_level": 6 },
		{ "id": "error_spirit", "weight": 15, "min_level": 7 },
	],
	"zone_4_academy": [
		{ "id": "corrupted_lesson", "weight": 35, "min_level": 7 },
		{ "id": "false_teacher", "weight": 30, "min_level": 7 },
		{ "id": "broken_textbook", "weight": 20, "min_level": 8 },
		{ "id": "exam_phantom", "weight": 15, "min_level": 9 },
	],
	"zone_5_workshop": [
		{ "id": "compile_error", "weight": 35, "min_level": 9 },
		{ "id": "broken_construct", "weight": 30, "min_level": 9 },
		{ "id": "null_reference", "weight": 20, "min_level": 10 },
		{ "id": "stack_overflow", "weight": 15, "min_level": 11 },
	],
	"zone_6_grounds": [
		{ "id": "adversarial_probe", "weight": 35, "min_level": 11 },
		{ "id": "security_exploit", "weight": 30, "min_level": 11 },
		{ "id": "data_leech", "weight": 20, "min_level": 12 },
		{ "id": "intrusion_wraith", "weight": 15, "min_level": 13 },
	],
	"core": [
		{ "id": "shadow_dragon", "weight": 25, "min_level": 14 },
		{ "id": "corrupted_worm_king", "weight": 25, "min_level": 14 },
		{ "id": "rogue_ai_fragment", "weight": 25, "min_level": 15 },
		{ "id": "data_storm", "weight": 25, "min_level": 15 },
	],
}

func pick_encounter(zone: String) -> String:
	## Pick a random enemy for the given zone using weighted random selection.
	var entries: Array = encounter_data.get(zone, [])
	if entries.is_empty():
		return ""

	var total_weight := 0
	for entry in entries:
		total_weight += entry["weight"]

	var roll := randi() % total_weight
	var cumulative := 0
	for entry in entries:
		cumulative += entry["weight"]
		if roll < cumulative:
			return entry["id"]

	return entries[-1]["id"]

func get_enemy_data(enemy_id: String) -> Dictionary:
	## Load enemy stats from data/enemies.json.
	## Returns empty dict if not found.
	var file := FileAccess.open("res://data/enemies.json", FileAccess.READ)
	if not file:
		push_error("[EncounterTable] Could not open enemies.json")
		return {}

	var json := JSON.new()
	var error := json.parse(file.get_as_text())
	if error != OK:
		push_error("[EncounterTable] JSON parse error: %s" % json.get_error_message())
		return {}

	var data: Dictionary = json.data
	return data.get(enemy_id, {})
