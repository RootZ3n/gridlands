# Knowledge, discovery, gear and food (design direction)

Operator decisions, 2026-09-25. **Design only.** None of the UI, farming, cooking, rare gear,
hidden quests or digestive mechanics is implemented yet. This document exists so the architecture
does not contradict them.

## 1. Three knowledge books, one investigator

| | Name | "What Zenny..." | Owns |
|---|---|---|---|
| **Chukka** | knowledge compendium | ...knows | creatures, plants, materials, locations, eras and history, people, glitches, NICE, Pehlichi's memories, environmental phenomena, **rumours and clues**, other world knowledge |
| **Ofi** | blueprints | ...knows how to make | fabrication patterns, construction blueprints, furniture, tools, weapon and armour patterns, machines, workstations, structural techniques, decorative pieces, restoration patterns for rare equipment |
| **Hoponi** | recipes | ...knows how to cook | recipes, culinary preparation, ingredient substitutions, recipe variants, food effects, cooking quality, dietary compatibility and sensitivity |

**Pehlichi investigates and analyses.** He helps Zenny discover and understand, and what they
learn propagates into whichever book owns it:

```
WORLD
  -> Zenny / Pehlichi discovery and analysis
  -> Chukka records general knowledge
  -> Ofi records applicable fabrication / building knowledge
  -> Hoponi records applicable culinary knowledge
```

Rules:
- **Never merge the three into one generic unlock or engram database.** Cross-dependencies are
  fine; ownership is never ambiguous.
  - Example: Ofi knows how to build a smoker, Hoponi knows the recipes that need a smoker, and
    Chukka may hold the smoker's history.
- **Chukka reflects only what was actually discovered.**
  - No completion counters ("38/52 creatures", "12/31 secrets"); unknown stays genuinely
    unknown.
  - Entries can be incomplete or uncertain, and grow through observation and Pehlichi's
    analysis.
  - Rumours live in Chukka, not in a quest log.

**Architecture today (enforced).** Every `knowledge.*` entry's category belongs to exactly one book
(`Data/_registry/knowledge-domains.json`; validator KN-2):
- `Knowledge.BuildingStyle` and `Knowledge.MaterialUse` belong to Ofi.
- `Knowledge.Memory` and `Knowledge.Place` belong to Chukka.
- Anything that unlocks a recipe or a build piece must be Ofi knowledge (KN-3).

## 2. Discovery-driven progression
Knowledge comes mainly from **discovering and understanding the world**, not from character
levels. Skills still improve through use.

Ways knowledge arrives:
- examining or analysing a structure can teach Ofi knowledge;
- finding a blueprint or manual teaches Ofi;
- finding a recipe card or cookbook teaches Hoponi;
- finding an ingredient teaches Chukka and may open Hoponi possibilities;
- an NPC's story becomes a Chukka rumour, and investigating a rumour turns up clues;
- rare artefact fragments build a Chukka and Ofi correlation.

Two players at the same point may know different recipes and blueprints because they explored
differently. Exploration rewards curiosity, not map completion. **Rare discoveries can be
completely missable.**

## 3. Rare gear and hidden quests
**Quality is separate from rarity.**
- **Quality:** ordinary effectiveness, craftsmanship, durability.
- **Rarity:** scarcity, provenance and unusual capability.

Rare gear gives *distinctive properties* that enable or reinforce a playstyle (combat,
exploration, stealth, salvage, Pehlichi-assisted and non-combat play, building and crafting). It
does not simply give bigger numbers. **No rarity treadmill** in which each colour tier invalidates
the last.

Rare items are found through:
- NPC rumours, environmental clues and hidden locations;
- puzzles, and trails spanning multiple cells;
- item fragments, Pehlichi's analysis, and Ofi reconstruction;
- rare fabrication requirements.

Sparse NPCs tell stories casually; they are not quest dispensers. **No automatic map markers, quest
arrows or "0/5 clues".** Working example: **"My Ex's Caliber"**, a named rare weapon found through
rumour and clues. Humorous and crass names are on-brand.

## 4. Farming, cooking and expedition food
**Food and water are preparation, not a survival tax.** Ignoring food while safely building,
decorating, farming or organising at base must not meaningfully punish the player.

Food mainly grants temporary benefits for:
- exploration and combat;
- stealth and salvage;
- hazardous and static environments;
- Pehlichi-assisted play.

Buffs support combat **and** non-combat play. When a buff expires, Zenny returns toward baseline.

## 5. Digestion (canonical design; not implemented)
**The chain.** Some foods add a temporary chance of flatulence while their effect lasts. The
flatulence is emitted through **the same world-sound and perception architecture as every other
noise**:

```
digestive event -> world sound stimulus -> normal creature hearing/perception -> normal response
```

It must never be special-cased as "fart, therefore aggro". Today's hearing
(`perception.hearingRadius`, and Pehlichi's lure travelling as `Event.Pehlichi.Lure`) is the seed
of that stimulus channel. Generalising it into one world-sound stimulus that any source can emit
is the intended path.

**The recipe UI may show "Flatulence Chance: X%".** X derives from:
- the recipe and its ingredients;
- food quality;
- Zenny's cooking ability;
- the relevant preparation knowledge (Hoponi);
- dietary sensitivities.

A skilled cook's normal, high-quality food approaches **0%**. By late game, a fart takes deliberate
low-quality or volatile food, so the joke thins out naturally as mastery grows.

**Food sensitivities.** Optional randomised food sensitivities or intolerances at world creation
(Random / Choose / None). They change recipe suitability and digestive volatility. There are **no
medically dangerous allergies** unless designed separately. Hoponi owns this knowledge.

**Setting.** Digestive humour is optional. Turning it off removes the events and their gameplay and
dialogue consequences, without penalty.

## 6. Writing requirements for digestive comedy
Recorded here and in [STORY-AND-DIALOGUE §6b](STORY-AND-DIALOGUE.md).
- **Zenny is "silent but deadly".** It is a canonical recurring joke.
- **Pehlichi escalates across the playthrough:**
  - a suppressed snicker;
  - "Seriously?";
  - shushing;
  - "You're gonna have to learn how to control that thing.";
  - "Dude, put a plug in that thing.";
  - eventually just "Bro."
- **NICE uses context:**
  - the food, its ingredient category and its quality;
  - how recently it happened before, and the stealth state;
  - whether a creature heard it, whether it caused detection, and whether Zenny died of it.
  - Example: "Zenny, are you lactose intolerant?"
- **Restraint.** No comment after every event; silence is a valid outcome. Escalation and
  callbacks, not a random joke bucket.
- **Achievement idea.** A possible hidden achievement, "SILENT BUT DEADLY". Details not designed.
