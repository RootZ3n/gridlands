# Data/: content source of truth (ADR-0002)

Structured game content is authored here as JSON, one folder per kind
(`items/`, `recipes/`, `salvage/`, `buildpieces/`, `glitches/`,
`capabilities/`, `creatures/`). `Tools/import-data.sh` (M2) validates it and
deterministically generates the DataAssets under `Content/Gridlands/Data/`.

Never hand-edit the generated `.uasset`s. Change the JSON and re-import.
Schemas are fixed in M2; examples are in `Docs/GLITCH-AND-PEHLICHI.md` section 4.
