extends Node
## SFX Manager — autoloaded singleton.
## Plays sound effects. Placeholder until real audio is integrated.

func play_sfx(sfx_name: String) -> void:
	## Play a sound effect by name.
	## TODO: Load actual audio files when Luna produces them.
	print("[SFX] %s" % sfx_name)

func play_music(track_name: String) -> void:
	## Play a music track by name.
	## TODO: Load actual music tracks when Luna produces them.
	print("[MUSIC] Playing: %s" % track_name)

func stop_music() -> void:
	## Stop the current music track.
	print("[MUSIC] Stopped.")
