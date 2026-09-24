# ADR-0021: The engine loads Data/ JSON directly into typed definitions; no generated DataAssets

- Status: Accepted
- Date: 2026-09-24
- Decider: operator (M2 question "Data -> UE", option "Load JSON directly")
- Amends: [ADR-0002](0002-json-source-of-truth.md) ("produce or validate DataAssets")

## Context
ADR-0002 left open whether the JSON becomes generated `.uasset` DataAssets.
Committing generated assets means opaque binary diffs, agent merge conflicts
and non-deterministic bytes. Generating uncommitted assets adds a
regeneration step to every clone and package.

## Decision
- C++ loads `Data/**/*.json` at startup into **typed, read-only definitions**
  (USTRUCTs) in a content registry, keyed by id.
- Maps, Blueprints and code refer to content **by id** (consistent with
  placements and anchors, ADR-0018). No generated `.uasset` holds game data,
  and git stays text for content.
- Two independent checks guard fidelity between the Python validator and C++:
  1. **Closed keys in both languages.** C++ rejects JSON keys that map to no
     struct property. The validator exports each kind's key paths
     (`Data/_registry/schema.generated.json`), and a C++ test proves every one
     maps to a property.
  2. **Round-trip.** Every entity is parsed and re-serialized, and every source
     field must survive unchanged (no silently dropped or coerced field).
- Packaged builds stage `Data/` as non-asset files. Packaging itself is
  verified when first needed.
- If designers later need editor asset pickers, DataAssets can be generated
  from the same parsed definitions. That would be an additive decision.

## Consequences
- `Tools/import-data.sh` (planned in ADR-0002) is not built. Content changes
  take effect at the next launch, with no editor step.
- The id grammar exists in Python and C++; a shared corpus
  (`Tools/tests/id-corpus.json`) proves they agree.

## Reversal cost
Low to moderate. Generation can be layered on top later.
