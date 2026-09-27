# Gridlands architectural north star

> **LOCKED DESIGN INTENT** (operator, 2026-09-27), for the future building and architecture milestone.
> **NOT IMPLEMENTED.** Nothing on this page is built. It is recorded now so that infrastructure (P8's
> model/presentation split, LODs, content density, assets) does not make these goals impossible.
> Changing it needs the operator.

## 1. What Gridlands is
**Gridlands is architecture-first.** It is an architectural building game that also contains
exploration, combat, survival and adventure systems, an insane AI antagonist (NICE), and Pehlichi.

**The benchmark is not merely "good survival-game building".** A sufficiently skilled player should
eventually be able to look at a real building and reproduce a recognizable version of it in Gridlands.

### Future acceptance concepts
- **REAL HOUSE TEST.** Can a player reproduce a recognizable real house, including:
  - its floor plan and roof form;
  - its interior spaces;
  - its landscaping and appropriate supporting features?
- **HILLSIDE TEST.** Can a player build a sophisticated multi-level structure into difficult terrain,
  including excavation, retaining structures and elevation changes, without fighting the building
  controls?
- **WINCHESTER TEST.** Can a player continuously combine radically different historical architectural
  traditions into one functioning structure, without arbitrary era-compatibility restrictions?
  - Example: Victorian house + Roman columns + log-cabin addition + later technology retrofits.
  - This is legal if the resulting structure satisfies the gameplay structural rules.

## 2. Historical progression
**Zones are deliberately NON-CHRONOLOGICAL.** The player is not progressing through history from
primitive to modern: **the player is collecting pieces of history.**
- **No architectural era supersedes another.** Eras expand the player's architectural vocabulary
  sideways, by introducing:
  - materials and processed components;
  - construction techniques and forms;
  - finishes, technology and knowledge.
- **Architectural appearance emerges primarily from:**
  - construction technique;
  - structural material;
  - processed building components;
  - finishing material;
  - player design.
- **Eras are not cosmetic skins over identical finished pieces.**
  - Common structural concepts (wall, floor, pillar, beam, roof…) may exist.
  - What the player actually constructs is made from real game materials and components, appropriate
    to the learned technique.
  - Example: tree/raw wood → processing → studs, beams, boards → structural framing.
- **Salvage respects components.**
  - Carefully salvaging a framed wall may recover intact studs plus scrap.
  - Poor or destructive demolition recovers fewer and/or lower-quality reusable materials.
- **No unnecessary real-world granularity:** no simulating every nail, screw, wire gauge or
  dimensional-lumber SKU.

> **SIMULATE COMPONENTS WHEN THEY CREATE INTERESTING ARCHITECTURAL DECISIONS; ABSTRACT LABOR WHEN IT ONLY CREATES CLICKS.**

## 3. Future construction phases
The intended model is deliberately compact. **Foundation and support come first.**
1. **STRUCTURE / FRAME.** A framed wall first looks framed; it does not instantly become a finished
   wall.
2. **ELECTRICAL (optional).**
   - It is optional depth. A player who enjoys it may design lights, switches and circuits, and
     wiring behaviour.
   - A player who does not can choose automatic, wireless-style installation without being punished
     mechanically.
   - **Automatic installation abstracts interaction; it does NOT remove material requirements.**
   - Existing buildings from electrified periods contain recoverable electrical material.
   - **Electrical salvage avoids repetitive clicking.** Interacting with an appropriate wall can strip
     the recoverable wiring of the connected floor or network in one operation.
   - **Structural salvage stays localized,** because which structural component is removed has
     gameplay consequences.
3. **FINISH.**
   - It is made from actual finishing materials, not an arbitrary era skin.
   - Where two historical construction methods genuinely differ structurally, the difference is
     represented structurally, not pretended to be cosmetic.

## 4. Salvage and structural consequences
- **Buildings are structures, not generic resource nodes.**
- **Careful dismantling** preserves significantly more high-quality reusable material.
- **Recklessly removing critical support** may cause deterministic cascading collapse through the
  existing structural system (ADR-0030).
- **Collapse substantially reduces** the quantity and/or quality of recoverable materials, compared
  with careful dismantling.
  - This is intentional, consequence-driven gameplay.
  - Fast, destructive demolition may still be rational when the player values speed over material
    recovery.
- **Structural preview direction:**
  - green = safe/strong;
  - yellow = marginal or structurally risky;
  - red = predicted support failure or collapse.
- Exact semantics wait for the building-system design. The preview **must not imply a heavyweight
  engineering or load simulator** unless the operator explicitly approves one.

## 5. Pehlichi's stud finder (intended gameplay and comedy)
- Pehlichi can eventually provide a structural-investigation, **"stud finder"**, mode.
- It reveals useful framing information beneath finished surfaces: reusable studs and potentially
  important structural members.
- Using that information can improve careful salvage yield and quality.
- **Running joke:** the stud finder occasionally identifies Pehlichi himself as the stud.

## 6. Ofi and building education
- **Ofi teaches architecture;** Ofi does not merely dispense finished prefabs.
- **Future Ofi plans may appear as 3D ghost blueprints** assembled from ordinary player-buildable
  components.
  - A beginner follows the ghost structure like 3D paint-by-numbers.
  - An expert ignores it and free-builds.
- **Plans teach; they do not replace the underlying construction system.**
- **Long term:**
  - players learn techniques from Ofi, modify plans and combine traditions;
  - players may save their own structures as reusable plans;
  - the system is expressive enough that community creators can make Gridlands building tutorials,
    and other players can reproduce those structures by hand.

## 7. Base storage and inventory principles
Construction depends on these:
- **No ordinary character weight or encumbrance system.**
- **Inventory capacity and stack limits exist.**
  - About **100** is a useful initial baseline for common-resource stacks.
  - Stack limits are data, subject to playtesting.
- **Starting capacity** is generous enough not to manufacture tedium.
- **Established bases use shared storage** for building and crafting. Nearby owned base storage is
  available directly to appropriate workbenches and building operations.
- **Storage organization** may be useful and expressive, but never mandatory clerical work.

> **CAPACITY MAY CREATE CHOICES; IT SHOULD NOT EXIST PRIMARILY TO CREATE EXTRA WALKING.**

## 8. World and resource renewal
- **Buildings are themselves valuable salvage resources.** The future zone economy must account for
  renewable building and material supply, without needing huge numbers of simultaneous houses.
- **Not designed around real-world constraints.** Gridlands occurs inside a corrupted computer
  environment: NICE's world reconstruction can justify regeneration, remixing and other impossible
  behaviour.
- **Player-owned or claimed construction is never overwritten by regeneration.**
- Exact regeneration rules are a future economy and content decision.

## 9. Overall principle
> **Reality is reference material, not law.**

- Borrow real construction concepts when they create interesting decisions, expression, progression
  or comedy.
- Abstract them when realism would merely create repetitive labour.
- **Complexity is opt-in depth,** not mandatory friction.

## 10. Dungeons read and weaponize the same spaces (2026-09-27)
The dungeon decisions ([DUNGEONS-AND-LEGENDARIES §1.2–1.7](DUNGEONS-AND-LEGENDARIES.md)) strengthen
this north star. One systemic language:

**BUILD → UNDERSTAND → SALVAGE → TRAVERSE → MANIPULATE → WEAPONIZE** the environment.

- A balcony a player collapses onto a boss by removing its supports is a legitimate environmental
  resolution when the normal structural rules produce it.
- Dungeon systems reuse the structural, terrain, noise and environmental systems; they never
  duplicate or bypass them.

## Constraints this places on current work (P8 onward)
Infrastructure must not make the above impossible:
- **Structures are part graphs** in one structural language shared by authored and player building
  (ADR-0024, ADR-0030). Keep it that way: finished-surface, frame and component layers must remain
  addable per part.
- **Salvage outcomes are data per part and per path** (careful, destructive, collapse). Do not bake a
  single yield per piece into systems that cannot carry quality or intactness later.
- **Model and presentation stay separate.**
  - A building's gameplay state (parts, phases, components, damage, claims) lives in the authoritative
    model.
  - Presentation (meshes, LODs, finishes) is derived from it, never the reverse.
  - This is P8's split, and it lets a framed wall look framed before it is finished.
- **LODs and assets serve presentation only.** No gameplay rule may depend on a mesh or LOD level.
- **Era is never a compatibility gate** (ERA-1: power comes from materials; ADR-0012: era is not a tech
  tier). Nothing may forbid combining eras.
- **Terrain stays one height per vertex** (ADR-0035). The HILLSIDE TEST's excavation and retaining
  walls are terrain edits plus structures; caves and undercuts would need a new decision.
- **Claimed or player-owned construction** must be distinguishable in saved state, so regeneration
  can never touch it.

## Known conflicts with the current build (future changes, NOT made in P8)
These are recorded, not resolved. Each needs the future milestone and, where it changes canonical
architecture, the operator.

| Current build | North star | Note |
|---|---|---|
| Inventory has carried weight and overencumbrance: M4, `UGLInventoryComponent` (`OverencumberedSpeedFactor`), `Event.Player.Overencumbered` and its NICE exchanges | no ordinary weight/encumbrance; capacity and stack limits instead | changes inventory rules, a saved-state shape, and dialogue content |
| Building v0 pieces are finished "era + material" pieces unlocked by style knowledge (`buildpiece.modern.timber_wall`, `knowledge.style.*`) | construction from components through STRUCTURE / ELECTRICAL / FINISH; appearance from technique, material and finish | v0 is a prototype set; the part-graph model can carry phases and components |
| Salvage yields are per piece (`salvage.*`), with no intact-vs-scrap quality | careful vs destructive vs collapse recovery quality | needs yield data per path |
| Player-built physical collapse is deferred (ADR-0024; authored structures collapse, ADR-0030) | reckless removal of support may cascade | already a pending operator decision |
| No shared base storage | nearby owned storage feeds workbenches and building | new system |
