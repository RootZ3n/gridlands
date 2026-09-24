# ADR-0002: JSON in Data/ is the source of truth for structured content

- Status: Accepted (core architectural requirement)
- Date: 2026-09-24
- Decider: operator

## Context
Autonomous coding agents (Ikbi, Abaiya, verifiers) cannot read, diff or merge
binary `.uasset` files. If content lives only in assets, every content change
needs a human in the editor.

## Decision
- Items, recipes, salvage, build pieces, glitches, capabilities and creatures are
  authored as JSON in `Data/`.
- `GridlandsEditor` provides an importer/validator commandlet that
  **deterministically** produces or validates the matching `UPrimaryDataAsset`s.
  Running it twice gives identical assets. A test fails if JSON and assets
  disagree.
- Gameplay logic an agent may need to reason about lives in C++ or text/data.
- Blueprints are allowed only as thin visual subclasses (mesh, material, VFX,
  sound), with no gameplay logic.
- Gameplay tags live in text `.ini` files.

## Consequences
- Adding content = edit JSON, run `Tools/import-data.sh`, run tests.
- The importer is a critical tool with its own tests (M2).
- Art assets (meshes, textures) stay binary in LFS. They are referenced from
  JSON by soft object path.

## Reversal cost
High. Everything downstream assumes it, which is why it is decided first.
