# ADR-0007: Same repository; Godot prototype preserved by tag, not migrated

- Status: Accepted
- Date: 2026-09-24
- Decider: operator

## Decision
The Unreal project replaces the Godot prototype in `RootZ3n/gridlands`.
The Godot state is preserved by the annotated tag `godot-prototype-final`
(commit `f695f17e`). Removal is an ordinary commit; no history is rewritten
and no objects are deleted. GDScript is not migrated. Design and art material
moved to `Docs/Legacy/` and `Docs/ArtReference/`. See
[LEGACY-GODOT.md](../LEGACY-GODOT.md).
