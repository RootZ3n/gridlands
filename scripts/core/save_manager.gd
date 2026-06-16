extends Node
## Save Manager — autoloaded singleton.
## Handles save/load with JSON files. NES-style save system.

const SAVE_PATH := "user://save_data.json"

var save_data: Dictionary = {
	"player_name": "Zenny",
	"current_zone": "home",
	"player_position": { "x": 0, "y": 0 },
	"blade_level": 1,
	"blade_exp": 0,
	"blade_form": "circuit_sword",
	"blade_skills": [],
	"hp": 100,
	"max_hp": 100,
	"mp": 20,
	"max_mp": 20,
	"gold": 0,
	"inventory": [],
	"quests_completed": [],
	"glitches_fixed": [],
	"play_time_seconds": 0,
	"step_count": 0,
}

func _ready() -> void:
	if FileAccess.file_exists(SAVE_PATH):
		print("[SaveManager] Save file found.")
	else:
		print("[SaveManager] No save file. Starting fresh.")

func save_game() -> bool:
	## Write current state to disk.
	save_data["player_position"] = {
		"x": GameManager.player_position.x,
		"y": GameManager.player_position.y,
	}
	save_data["current_zone"] = GameManager.current_zone
	save_data["step_count"] = GameManager.step_count

	var file := FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	if not file:
		push_error("[SaveManager] Could not open save file for writing.")
		return false

	file.store_string(JSON.stringify(save_data, "\t"))
	file.close()
	print("[SaveManager] Game saved.")
	return true

func load_game() -> bool:
	## Load state from disk.
	if not FileAccess.file_exists(SAVE_PATH):
		push_error("[SaveManager] No save file found.")
		return false

	var file := FileAccess.open(SAVE_PATH, FileAccess.READ)
	if not file:
		push_error("[SaveManager] Could not open save file for reading.")
		return false

	var json := JSON.new()
	var error := json.parse(file.get_as_text())
	file.close()

	if error != OK:
		push_error("[SaveManager] JSON parse error: %s" % json.get_error_message())
		return false

	save_data = json.data
	GameManager.current_zone = save_data.get("current_zone", "home")
	GameManager.step_count = save_data.get("step_count", 0)

	var pos: Dictionary = save_data.get("player_position", { "x": 0, "y": 0 })
	GameManager.player_position = Vector2i(int(pos["x"]), int(pos["y"]))

	print("[SaveManager] Game loaded. Zone: %s" % GameManager.current_zone)
	return true

func has_save() -> bool:
	return FileAccess.file_exists(SAVE_PATH)

func delete_save() -> void:
	if FileAccess.file_exists(SAVE_PATH):
		DirAccess.remove_absolute(SAVE_PATH)
		print("[SaveManager] Save file deleted.")
