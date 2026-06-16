extends Node
## Game Manager — autoloaded singleton.
## Handles scene transitions, game state, and global coordination.

signal scene_transition_started
signal scene_transition_finished

enum GameState { OVERWORLD, BATTLE, MENU, DIALOG, CUTSCENE, LOADING }

var current_state: GameState = GameState.OVERWORLD
var current_zone: String = "home"
var player_position: Vector2i = Vector2i.ZERO
var step_count: int = 0

# Encounter tracking (random encounter system)
var steps_since_encounter: int = 0
var encounter_rate: int = 15  # base steps between encounters (per zone)

func _ready() -> void:
	print("[GameManager] Gridlands initializing...")

func change_state(new_state: GameState) -> void:
	var old_state := current_state
	current_state = new_state
	print("[GameManager] State: %s -> %s" % [GameState.keys()[old_state], GameState.keys()[new_state]])

func register_step() -> void:
	## Called every time the player moves one tile on the overworld.
	step_count += 1
	steps_since_encounter += 1

func should_encounter() -> bool:
	## Classic Dragon Warrior random encounter check.
	## Returns true if a random encounter should trigger.
	if steps_since_encounter < 5:
		return false  # grace period — no encounters in first 5 steps
	var roll := randi() % encounter_rate
	return roll == 0

func trigger_encounter(enemy_id: String = "") -> void:
	## Start a battle. enemy_id is optional — if empty, pick from zone table.
	steps_since_encounter = 0
	change_state(GameState.BATTLE)
	print("[GameManager] Encounter! Enemy: %s" % (enemy_id if enemy_id else "random"))
	# TODO: Transition to battle scene

func end_battle(victory: bool, exp_gained: int = 0, gold_gained: int = 0) -> void:
	change_state(GameState.OVERWORLD)
	if victory:
		print("[GameManager] Victory! +%d EXP, +%d Gold" % [exp_gained, gold_gained])
	else:
		print("[GameManager] Fled from battle.")

func get_zone_encounter_rate() -> int:
	## Returns the encounter rate for the current zone.
	## Lower = more frequent encounters.
	match current_zone:
		"home":
			return 0  # no encounters in tutorial
		"zone_1_arena":
			return 18
		"zone_2_settlement":
			return 16
		"zone_3_engine":
			return 14
		"zone_4_academy":
			return 12
		"zone_5_workshop":
			return 10
		"zone_6_grounds":
			return 8
		"core":
			return 6
		_:
			return 15
