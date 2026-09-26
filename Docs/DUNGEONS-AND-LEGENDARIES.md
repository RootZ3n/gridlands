# Dungeons and legendaries

Status: **LOCKED DESIGN INTENT** (operator, 2026-09-25; extended 2026-09-26). **NONE OF THIS IS
IMPLEMENTED.**
- The examples set tone and philosophy.
- Exact numbers and mechanics are unbalanced and not final.
- Final dialogue is not locked, except the tooltip lines quoted in §2.1, which are accepted exactly.

Related:
- [SURVIVAL-AND-THREAT](SURVIVAL-AND-THREAT.md): telegraphs, perception, noise.
- [KNOWLEDGE-AND-DISCOVERY §1, §3](KNOWLEDGE-AND-DISCOVERY.md): Chukka, Ofi, Hoponi; rare gear.
- [STORY-AND-DIALOGUE §6a](STORY-AND-DIALOGUE.md): comedy.

## 1. Dungeons
- **One handcrafted dungeon per zone** is the canonical starting rule. No filler dungeons to raise
  the count.
- **Dungeons are voluntary**, concentrated combat and tactical experiences. They never make combat
  mandatory for players who mainly build, explore, cook or sneak (ADR-0009).
- **The encounter philosophy is older MMO group-pull play adapted to single player:**
  - small groups rather than only isolated mobs;
  - priority targets and support mobs;
  - crowd control, and Pehlichi hacks and disruptions (still zero damage, ADR-0017);
  - positioning, and sight/hearing manipulation;
  - readable, **truthful** telegraphs;
  - environmental solutions, and alternative stealth routes where appropriate.
- **Difficulty comes mostly from tactical decisions**, not extreme reflexes or giant health pools.
- **Dungeons use the normal systemic world rules** (noise, perception, structure, terrain), not
  bespoke fake versions, wherever practical.
- **First completion** is mainly discovery, story and legendary progression.
- **Repeat completion** gives worthwhile, non-mandatory rewards:
  - fun gear modifications;
  - high-quality combat gear and materials;
  - cosmetics and decor.
- **No repeated RNG farming for the primary legendary** once its discovery or quest has been
  correctly completed.
- **Structural ground inside dungeons** comes from exported/authored floor-support data, never
  from runtime collision queries ([ADR-0030](ADR/0030-structural-salvage-and-deterministic-collapse.md)).

### 1.1 Historical dungeons (principle)
- **Some dungeons deliberately reinterpret or parody periods of Pehlichi's past.**
  - NICE may distort real elements of his history rather than present it objectively.
  - Pehlichi can react to inaccurate representations of history he actually lived through.
  - The player is allowed to notice this pattern organically. It is never announced.
- **Not every dungeon needs a Pehlichi historical incarnation.**
- **Still without locked legendary or dungeon designs:** 1800s Native America, and possibly World
  War II. Do not invent or lock these just to fill gaps.

## 2. Legendaries
**A legendary is a NAMED, mechanically distinctive discovery**, not a higher colour rarity with
larger numbers. Each should:
- support a playstyle or a broadly useful mechanic;
- have a memorable visual identity, and preferably change interaction or gameplay;
- have an authored discovery story, often with a long-form comedic reveal or punchline;
- support interesting mods later.

**The player is frequently told the literal truth about a legendary while misunderstanding what
that truth means.**

### 2.1 Tooltip convention (LOCKED)
- **The style:** extremely short, deadpan and deliberately unhelpful. It never explains the joke.
- **Chukka** (what Zenny knows) may carry history and lore.
- **Gameplay systems** show the useful mechanics separately.

**Accepted lines (exact):**

| Legendary | Tooltip |
|---|---|
| Cupid's Bow | "Not king of the world." |
| Hammer Toe | "Never fully straight." |
| My Ex's Caliper | "Extreme measures." |
| Ring of Dusty Knee | "One size does not fit all." |
| Invisible Scam | "Still a nobody." |
| Shake's Spear | "Break a leg." |
| Dewey Decimal legendary book | "Flammable." |

### 2.2 The legendaries and their dungeons

| Legendary / dungeon | Pehlichi period | Summary |
|---|---|---|
| Cupid's Bow | 1920s (Pehlichi was a race-car driver) | Carnival; rainbow water-energy waves |
| My Ex's Caliper | 1950s | University; precision and weak points |
| Hammer Toe | HOWA / Roman-era history | Monumental statue; structural destruction |
| Invisible Scam | current era (a first-dungeon candidate) | A losing lottery ticket; stealth |
| Ring of Dusty Knee | none required | Movie studio; extra stamina |
| Shake's Spear | Victorian period | Theatre; knockdown and distraction |
| Ren Faire / Hedge Knight | hedge-knight period | A recipe, not necessarily equipment |
| Ice Age / Museum | Ice Age period | Saber-tooth canine dagger; puncture and bleed |
| Ancient / Municipal Library | Ancient Library period | A Chukka legendary: knowledge inferences |

#### Cupid's Bow
- **Period:** tied to Pehlichi's 1920s era, when he was a race-car driver.
- **Dungeon:** a colourful corrupted carnival, with evil clowns and carnival mechanics.
  - The boss/guardian is the *Jealous Boyfriend*, guarding the Tunnel of Love.
- **Reveal:** *Cupid* is the ride boat, and its **bow** (front section) becomes the legendary.
- **Identity:**
  - fires travelling rainbow water-energy waves;
  - crowd-control and group utility;
  - repeat-run mods alter the wave.
- **Tooltip:** "Not king of the world."

#### My Ex's Caliper
*(Earlier docs said "My Ex's Caliber". The operator's name is **Caliper**.)*
- **Period:** tied to Pehlichi's 1950s era.
- **Story:**
  - a college sweetheart leaves the legend's protagonist to study, because he distracts her;
  - her new study partner gets far too friendly with her calipers;
  - he resolves to "rescue" them.
- **Dungeon:**
  - a corrupted university/college with frat-brother enemies and tactical group pulls;
  - beer-pong balls are real, avoidable, telegraphed projectiles;
  - the boss is *Your Ex's New Study Partner*.
- **Reveal:** an oversized caliper, adapted by Ofi for combat.
- **Identity:** precision, weak points, and synergy with Pehlichi's analysis.
- **Tooltip:** "Extreme measures."

#### Hammer Toe
- **Period:** tied to HOWA / Roman-era history.
- **Story:**
  - a legendary stone carver's hammer could "break anything";
  - finishing a monumental statue, he accidentally broke off its pinky toe;
  - in a rage, he made the hammer head from the broken toe.
- **Identity:**
  - structural, armour and guard destruction;
  - environmental destruction and stagger.
  - It must interact with the structural system ([ADR-0030](ADR/0030-structural-salvage-and-deterministic-collapse.md)),
    not just carry a large DPS number.
- **Tooltip:** "Never fully straight."

#### Invisible Scam
- **Period:** the current era; a candidate for the first dungeon.
- **Concept:** a supposedly extraordinary stealth legendary turns out to be (or be represented by)
  a **losing lottery ticket**.
- **Pehlichi's reaction:** "What. That's it?" (or equivalent).
- **Identity:** stealth and perception manipulation, not generic damage.
- **Tooltip:** "Still a nobody."

#### The Legendary Ring of Dusty Knee
- **Period:** no required Pehlichi historical era at present.
- **Dungeon:**
  - a movie studio and soundstage, heavily implied to have produced adult films;
  - **the game never says so explicitly**; characters use euphemism and denial;
  - production mechanics are real gameplay: sets, cameras, lights, stage machinery, fake walls.
- **The running gag:** the legend keeps being misheard as the "Ring of Destiny", and Pehlichi keeps
  insisting on *Destiny*.
- **Dusty Knee** is an old performer's stage name.
- **Reveal:**
  - the artifact is clearly labelled *the Ring of Dusty Knee*;
  - it is physically far too large for Zenny's finger or wrist;
  - Pehlichi asks where he will put it;
  - Zenny answers nonverbally.
- **Identity:** grants **extra stamina**. The game never explains why.
- **Tooltip:** "One size does not fit all."

#### Shake's Spear
- **Period:** a Victorian-period theatre.
- **Dungeon:** pompous actor enemies, and theatre and stage mechanics.
- **Reveal:** the legendary is a **mannequin head wearing an old dusty wig, mounted on a pole**.
- **Identity:** may emphasise knockdown and distraction.
- **Tooltip:** "Break a leg."

#### Ren Faire / Hedge Knight
- **Period:** tied to Pehlichi's hedge-knight period. NICE uses it as parody and confrontation
  with his past.
- **Enemies:** LARPers and cosplayers with foam weapons and armour.
  - Fake spells are mundane physical props: someone throws a coloured sandbag while yelling
    "FIREBOLT".
- **Boss:** literally the *Hedge Knight*, wearing a bush/hedge as armour.
- **Reward:** not necessarily equipment. The concept is a **legendary Hoponi turkey-leg recipe**.
  Repeat runs may give rare-quality recipe, ingredient or preparation rewards.

#### Ice Age / Museum
- **Period:** tied to Pehlichi's Ice Age period.
- **Dungeon:** NICE presents the era as a modern museum exhibit.
  - Security guards and museum systems form the opposition.
  - The *Tour Guide* is the boss/signature antagonist.
  - Pehlichi can react to inaccurate exhibits of history he actually experienced.
- **Legendary:** a dagger that is actually a **saber-toothed cat canine**, removed from the skeleton
  exhibit.
- **Identity:** likely puncture and bleed.

#### Ancient Library / Municipal Library
- **Period:** tied to the Ancient Library period. NICE reduces and reinterprets it as a municipal
  library.
- **Enemies and boss:** *Quiet Zone Monitors*; the *Librarian* is the boss/signature antagonist.
- **Legendary:** a book about the **Dewey Decimal system**.
  - **It is primarily a CHUKKA legendary.** It grants or permits knowledge connections and
    inferences the player could not otherwise make.
  - The inference mechanics are unimplemented and unbalanced.
- **Tooltip (exact):** "Flammable."
