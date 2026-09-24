# Glossary

Terms as used in code, data and documentation. Agents: use these words and
these meanings.

| Term | Meaning |
|---|---|
| **Zenny** | the silent player avatar |
| **Pehlichi** | the player's AI companion in a squirrel body; the sole glitch-repair authority. Game character only, unrelated to any real AI agent |
| **NICE** | the AI Game Master of the world; antagonist; her composure falls as the world is repaired |
| **Grid** | the underlying simulation structure, visible at boundaries, in scans, around glitches and in NICE's phenomena |
| **Grid cell** | a large physical world region (planning assumption ~1 km); the unit of streaming and physical boundaries |
| **Progression band** | depth toward NICE's core: Home, Outer Gridlands, Fractured Gridlands, City outskirts, Inner city, NICE core. Sets baseline danger/corruption/control |
| **Era / world memory** | project fragments (Roman, 1950s, ...) that bleed into cells. Visual, material and knowledge flavour. **Not a tech tier** |
| **Glitch** | a fault in the simulation with a lifecycle (Latent...Repaired); only Pehlichi repairs one |
| **Requirement** | a condition that must hold before a detected glitch is repairable (a blocker salvaged, items delivered, a puzzle solved, the repair point reachable, guards absent, not jammed) |
| **Stability** | a derived measure of how repaired a place is, computed from persisted glitch states |
| **Interference / static** | a derived measure of NICE's disruption at a place: map, minimap, visibility, scan reliability. The soft progression barrier |
| **NICE composure** | a derived global measure: falls as the world is repaired; drives her dialogue and the core's chaos |
| **Glitch Storm** | a NICE-controlled world event replacing most weather |
| **Knowledge** | an unlock record (material uses, building styles, recipes) earned by discovery, scanning, NPCs or rewards |
| **Skill** | Zenny's use-based physical proficiency (running, jumping, weapon families, ...) |
| **Capability** | Pehlichi's permanent ability level (scan, weak-point analysis, disruption, ...) |
| **Exchange** | a unit of dialogue: one or more NICE/Pehlichi lines selected by the dialogue director |
| **World settings** | per-world configuration, e.g. yield multipliers by category and commentary frequency |
| **Combat-free** | achievable without killing: by stealth, avoidance, distraction, non-lethal disabling, terrain or waiting |
| **Id** | a stable namespaced entity id, e.g. `item.material.copper_wire` ([CONTENT-IDS-AND-TAGS.md](CONTENT-IDS-AND-TAGS.md)) |
| **Placement** | a JSON gameplay object in a cell (glitch, salvage node, spawn, discovery, encounter), `placement.<cell>.<name>` (ADR-0018) |
| **Anchor** | a stable handle on a visual-world actor in an Unreal map, `anchor.<cell>.<name>`; how placements attach to houses, walls and grates (ADR-0018) |
| **Zone** | *deprecated*. Do not use. Say **cell** (place), **band** (depth) or **era** (memory) |
