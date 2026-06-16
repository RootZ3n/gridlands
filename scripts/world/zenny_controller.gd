extends CharacterBody2D
## Zenny — Player character. NES-style tile-based movement.
## Moves one tile at a time, grid-aligned, with walk animation.

@export var move_speed: float = 4.0  # tiles per second
@export var tile_size: int = 16       # pixels per tile

@onready var sprite: Sprite2D = $Sprite2D
@onready var animation: AnimationPlayer = $AnimationPlayer

var facing: Vector2i = Vector2i.DOWN
var is_moving: bool = false
var move_timer: float = 0.0
var move_duration: float = 0.0
var target_position: Vector2 = Vector2.ZERO
var start_position: Vector2 = Vector2.ZERO

# NES-style input buffering
var input_buffer: Vector2i = Vector2i.ZERO
var can_move: bool = true

func _ready() -> void:
	# Snap to grid on spawn
	position = position.snapped(Vector2(tile_size, tile_size))
	target_position = position
	print("[Zenny] Spawned at %s" % str(position))

func _process(delta: float) -> void:
	if GameManager.current_state != GameManager.GameState.OVERWORLD:
		return

	if is_moving:
		_animate_movement(delta)
	else:
		_read_input()
		if input_buffer != Vector2i.ZERO:
			_try_move(input_buffer)

func _read_input() -> void:
	## Read directional input (NES-style: one direction at a time)
	input_buffer = Vector2i.ZERO

	if Input.is_action_pressed("move_up"):
		input_buffer = Vector2i.UP
	elif Input.is_action_pressed("move_down"):
		input_buffer = Vector2i.DOWN
	elif Input.is_action_pressed("move_left"):
		input_buffer = Vector2i.LEFT
	elif Input.is_action_pressed("move_right"):
		input_buffer = Vector2i.RIGHT

func _try_move(direction: Vector2i) -> void:
	## Attempt to move one tile in the given direction.
	## Checks for collisions before committing.
	if not can_move:
		return

	facing = direction
	_update_sprite_direction()

	var next_pos := position + Vector2(direction * tile_size)

	# Check for collision at target position
	if _is_blocked(next_pos):
		# Can't move — but still face that direction
		return

	# Start movement
	is_moving = true
	start_position = position
	target_position = next_pos
	move_duration = 1.0 / move_speed
	move_timer = 0.0

func _animate_movement(delta: float) -> void:
	## Smoothly interpolate between tiles (NES feel: snappy, not smooth)
	move_timer += delta
	var t := move_timer / move_duration

	if t >= 1.0:
		# Arrived at target tile
		position = target_position
		is_moving = false
		move_timer = 0.0
		_on_tile_arrived()
	else:
		# NES-style: snap to positions, no smooth interpolation
		# Use integer steps for authentic retro feel
		var step := floor(t * 4.0) / 4.0
		position = start_position.lerp(target_position, step)

func _on_tile_arrived() -> void:
	## Called when Zenny lands on a new tile.
	GameManager.register_step()

	# Check for random encounter
	if GameManager.should_encounter():
		GameManager.trigger_encounter()

func _is_blocked(next_pos: Vector2) -> bool:
	## Check if the target position is blocked by a wall or obstacle.
	## Uses a raycast or tilemap check.
	var space_state := get_world_2d().direct_space_state
	var query := PhysicsRayQueryParameters2D.create(
		position,
		next_pos,
		0b0001  # collision layer 1 (walls)
	)
	var result := space_state.intersect_ray(query)
	return result.size() > 0

func _update_sprite_direction() -> void:
	## Update sprite frame based on facing direction.
	## Sprite sheet: 4 rows (down, left, right, up) × 2 cols (idle, walk)
	var row := 0
	match facing:
		Vector2i.DOWN:
			row = 0
		Vector2i.LEFT:
			row = 1
		Vector2i.RIGHT:
			row = 2
		Vector2i.UP:
			row = 3

	# Set sprite region for the correct direction
	if sprite and sprite.region_enabled:
		sprite.region_rect = Rect2(0, row * tile_size, tile_size * 2, tile_size)
