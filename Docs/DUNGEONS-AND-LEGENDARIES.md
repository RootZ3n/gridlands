# Dungeons and legendaries

Status: **LOCKED DESIGN INTENT** (operator, 2026-09-25; extended 2026-09-26 and 2026-09-27).
**NONE OF THIS IS IMPLEMENTED.**
- The examples set tone and philosophy.
- Exact numbers and mechanics are unbalanced and not final.
- Final dialogue is not locked, except the tooltip lines quoted in §2.1, which are accepted exactly.

Related:
- [SURVIVAL-AND-THREAT](SURVIVAL-AND-THREAT.md): telegraphs, perception, noise.
- [KNOWLEDGE-AND-DISCOVERY §1, §3](KNOWLEDGE-AND-DISCOVERY.md): Chukka, Ofi, Hoponi; rare gear.
- [STORY-AND-DIALOGUE §6a, §6e](STORY-AND-DIALOGUE.md): comedy; NICE's language rule.
- [ARCHITECTURAL-NORTH-STAR](ARCHITECTURAL-NORTH-STAR.md): the same systemic language builds,
  salvages and weaponizes spaces (§1.7 below).

## 1. Dungeons
- **One handcrafted dungeon per zone** is the canonical starting rule. No filler dungeons to raise
  the count.
- **Dungeons are voluntary**, concentrated tactical experiences. They never make combat mandatory
  for players who mainly build, explore, cook or sneak (ADR-0009). Every major dungeon supports two
  complete routes (§1.2).
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
  War II. Do not invent or lock these just to fill gaps. (The 1800s **Civil War reenactment**, USB's
  Tick, is a separate, locked concept.)
- **Cultural-comedy rule (LOCKED):** THE CULTURE/HISTORY IS NOT THE JOKE. NICE'S WORDPLAY,
  MISINTERPRETATION, ANACHRONISMS AND MANIPULATION ARE THE JOKE. A historical environment is
  legitimate world material, treated with respect (for example, Sue Shi's feudal Japan).

### 1.2 Two complete routes through the same dungeon (LOCKED, 2026-09-27)
Every major dungeon should ultimately support **two viable ways through the same authored
environment:**
- **A. Direct / combat.** Fight patrols and enemies normally, and defeat the boss through direct
  combat.
- **B. Non-attack / infiltration.** Never directly attack an enemy. Instead use:
  - spatial reasoning, traversal and platforming;
  - stealth, noise management and hiding (line-of-sight breaks);
  - patrol manipulation and distractions;
  - environmental manipulation;
  - Pehlichi's cooperation.

**Route B is not an easier bypass.** It is a substantial challenge of its own.
- *Example:* in the 1920s carnival (Cupid's Bow), Zenny can fight through the evil clowns, or
  attempt the **fun house** to bypass them. The fun house is a serious stealth, traversal and
  puzzle challenge, not a shortcut.

**Each dungeon's alternate route reflects its environment:** the fun house, Sue Shi's air ducts
(§2.2). Never a repeated generic "stealth corridor".

### 1.3 Detection changes the state; it does not end the run
**Detection never invalidates a non-attack run.** The state flows:

```
STEALTH -> DETECTED -> ESCAPE / SEARCH -> HIDDEN -> STEALTH
```

A discovered Zenny can flee, break line of sight, hide and let the patrols search, then continue
without being forced into direct combat. The seed of this already exists: creatures keep a last
known position and search it ([ADR-0031](ADR/0031-authoritative-world-noise.md)).

**A missed traversal should create a situation, not a reload.**
- *Example:* Zenny misses a beam, lands loudly, alerts the patrols, and has to escape.

**Precision platforming is not the primary difficulty.** Route choice, timing, reading the
environment and managing noise matter more.

### 1.4 Every major boss: direct defeat or environmental resolution
- **A. Direct defeat.** Zenny attacks and defeats (or kills) the boss in normal combat.
- **B. Environmental resolution.** Zenny stays under **full threat** but never needs to damage the
  boss directly.
  - This is **not "skip the boss"**: Zenny must survive the encounter while creating
    opportunities for Pehlichi to investigate the arena.

**The core loop:**

```
Zenny creates an opportunity
-> Pehlichi investigates
-> Pehlichi discovers an environmental possibility
-> Zenny helps create the required conditions
-> Pehlichi / Zenny activates or completes it
-> the environment neutralizes the boss
```

**Possible solutions:**
- trapdoors, pits and containment;
- machinery and sprinklers;
- structural collapse;
- power shutdown, doors and security systems;
- environmental traps;
- arena-specific mechanisms and other systemic interactions.

**Pehlichi investigates the ENVIRONMENT, not necessarily the boss.** He may:
- scan, inspect and hack;
- crawl into spaces Zenny cannot reach;
- investigate machinery and locate controls;
- identify structural opportunities and find hidden buttons.

**He sometimes has no idea what a control does before he activates it.** Canonical tone:

> Peh: "Found a button."
> NICE: "Don't touch that."
> Peh: "What's this do?"
> *CLICK.*

The result may solve the encounter or radically change it.

**The player stays involved.** Pehlichi discovers possibilities. He never solves the fight while
Zenny waits.

**Both resolutions are full victories (reward parity).** Direct defeat and environmental
resolution both satisfy progression and give the major dungeon reward. Different dialogue and
achievements are fine; environmental resolution is never a lesser victory.

**Open question (for the operator, not decided):** [ADR-0017](ADR/0017-pehlichi-deals-zero-damage.md)
says Pehlichi deals zero **direct** damage, and its enforcement test asserts that no Pehlichi action
produces a damage event. When Pehlichi activates a mechanism that harms or neutralizes a boss,
attribution needs a rule. For example: the environment is the instigator, or neutralization is a
distinct non-damage outcome (contained, trapped, disabled). ADR-0017 is unchanged until the operator
decides.

### 1.5 Designed solutions exist; systemic solutions are legitimate
**DESIGNED SOLUTIONS EXIST. SYSTEMIC SOLUTIONS ARE LEGITIMATE WHEN NORMAL GAME RULES PRODUCE AN
EQUIVALENT NEUTRALIZATION.**

*Example:*
- A boss arena contains a structurally valid balcony, and the authored solution is a trapdoor.
- A player sees that the balcony stands on removable structural members.
- The player lures the boss underneath, removes the support, and collapses the balcony onto the
  boss.
- If the normal structural rules produce that outcome ([ADR-0030](ADR/0030-structural-salvage-and-deterministic-collapse.md)),
  it can count as a valid environmental resolution.

**Never special-case every imaginable solution.** A neutralization is recognised by its outcome
under the normal rules, not by a list of approved tricks.

### 1.6 Legendary design principle
**Whenever practical, a legendary grants an unusual gameplay verb or capability, not larger
statistics.**
- *USB's Tick:* time control and crowd control that doesn't cause aggro.
- *5th Amendment:* stealth preparation, by suppressing fart-noise events.
- *Drew Id's Fang* is a deliberate exception: an extremely straightforward, excellent stabbing
  implement is part of its joke.

Legendaries are memorable because they change what is possible.

### 1.7 Relation to architecture-first Gridlands
These dungeon decisions **strengthen** the [architectural north star](ARCHITECTURAL-NORTH-STAR.md);
they do not replace it. One systemic language should connect:

**BUILD** the environment → **UNDERSTAND** it → **SALVAGE** it → **TRAVERSE** it → **MANIPULATE**
it → **WEAPONIZE** it.

- Building teaches players how spaces work.
- Dungeon infiltration tests whether they can read spaces.
- Environmental boss resolution tests whether they can manipulate spaces under pressure.

**Dungeon systems must not bypass or duplicate the canonical structural, terrain, noise and
environmental systems.** Reuse them wherever appropriate.

## 2. Legendaries
**A legendary is a NAMED, mechanically distinctive discovery**, not a higher colour rarity with
larger numbers. Each should:
- support a playstyle or a broadly useful mechanic;
- have a memorable visual identity, and preferably change interaction or gameplay;
- have an authored discovery story, often with a long-form comedic reveal or punchline;
- support interesting mods later.

**The player is frequently told the literal truth about a legendary while misunderstanding what
that truth means.** This is NICE's language rule ([STORY-AND-DIALOGUE §6e](STORY-AND-DIALOGUE.md)):
part of her voice, not a mandatory template for every legendary. §1.6 gives the design principle.

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
| Drew Id's Fang (Ice Age / Museum) | Ice Age period | The tour guide's saber-tooth fang necklace; a plain, excellent stabbing implement |
| Ancient / Municipal Library | Ancient Library period | A Chukka legendary: knowledge inferences |
| 5th Amendment (Sue Shi, feudal Japan) | none recorded | A legendary **brisket** recipe; suppresses fart-noise events |
| USB's Tick (1800s Civil War reenactment) | none recorded | Ulysses S. Brant's watch; freezes ordinary mobs without aggro |
| Your USB Stick (Your Nemesis; the last dungeon before NICE) | none recorded | A literal stick with a USB drive; completes the USB joke |

#### Cupid's Bow
- **Period:** tied to Pehlichi's 1920s era, when he was a race-car driver.
- **Dungeon:** a colourful corrupted carnival, with evil clowns and carnival mechanics.
  - The boss/guardian is the *Jealous Boyfriend*, guarding the Tunnel of Love.
- **Two routes (§1.2):** fight through the evil clowns, or attempt the **fun house**: a serious
  stealth, traversal and puzzle route, not a shortcut.
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

#### Drew Id's Fang (Ice Age / Museum)
- **Period:** tied to Pehlichi's Ice Age period.
- **Dungeon:** NICE's "Ice Age" is a modern museum exhibit.
  - Museum security, tour-guide behaviour and museum systems form the opposition, not literal
    prehistoric humans.
  - Pehlichi can react to inaccurate exhibits of history he actually experienced.
- **The rumour** is heard and interpreted as **"Druid's Fang"**. Zenny and Pehlichi expect an
  ancient dagger or artifact.
- **The tour guide is Drew Id** (the boss/signature antagonist). He wears a saber-tooth tiger fang
  on a necklace.
- **The legendary is DREW ID'S FANG:** the saber-tooth fang from his necklace.
  - *Changed 2026-09-27 (operator):* it previously came from the skeleton exhibit. The necklace
    replaces that.
  - NICE didn't necessarily lie; Zenny and Pehlichi supplied the expected spelling and meaning.
- **Identity:** a simple stabbing weapon; no elaborate magic needed. (Puncture and bleed stay
  possible.) It is the deliberate exception to §1.6.
- **Locked comedic beat** (the final dialogue may still be polished):

  > Peh: "What are we supposed to do with this?"
  > NICE: "Stick them with the pointy end, Sweetie."
  > Peh: "I'd stab you with the pointy end if I could."

  Zenny silently holds the fang upright toward NICE, like a middle finger.

#### Ancient Library / Municipal Library
- **Period:** tied to the Ancient Library period. NICE reduces and reinterprets it as a municipal
  library.
- **Enemies and boss:** *Quiet Zone Monitors*; the *Librarian* is the boss/signature antagonist.
- **Legendary:** a book about the **Dewey Decimal system**.
  - **It is primarily a CHUKKA legendary.** It grants or permits knowledge connections and
    inferences the player could not otherwise make.
  - The inference mechanics are unimplemented and unbalanced.
- **Tooltip (exact):** "Flammable."

#### Sue Shi (feudal Japan): the 5th Amendment
- **Setting:** feudal Japan. Its architecture and culture are **legitimate world material**, never
  the target of the joke (cultural-comedy rule, §1.1).
- **The misunderstanding:**
  - Zenny and Pehlichi hear about "Sue Shi" and reasonably assume a legendary **sushi** recipe;
  - they are wrong: **Sue Shi is a person**;
  - that assumption is the only sushi/Japan wordplay.
- **Boss:** **SUE SHI**, *Ambulance Chaser*.
- **Contamination:** NICE fills the dungeon with deliberately absurd, anachronistic legal/corporate
  contamination.
- **Mobs:** **Interns.**
- **Non-attack route (§1.2):** Zenny traverses the **air ducts** above and around the interns.
  - The ducts are deliberately **noisy**: a stealth, noise and traversal challenge.
  - Environmental noise masking matters here in particular ([SURVIVAL-AND-THREAT §9](SURVIVAL-AND-THREAT.md)).
- **Legendary:** the recipe **5th Amendment**.
  - It is **brisket**. Never explain why.
  - **Description (canonical):** "You have a right to remain silent."
  - **Effect:** suppresses Zenny's fart events for a substantial duration, so a player can
    deliberately prepare for stealth-heavy content ([KNOWLEDGE-AND-DISCOVERY §5](KNOWLEDGE-AND-DISCOVERY.md)).
  - It is **renewable** (cooked again), never a permanent character toggle.
  - Duration, cost and ingredients are future balance work.

#### USB's Tick (1800s Civil War reenactment)
- **Scenario:** a Civil War **reenactment**.
- **The legendary** is **USB's Tick**: a watch that belonged to **Ulysses S. Brant**. Not the
  president.
- **Boss title:** **NOT THE PRESIDENT.**
- **Ability:** stops or freezes time for **ordinary mobs** without itself starting combat or aggro.
  - Its value is control and avoidance, not damage: another tool for the non-attack playstyle.
  - Duration, radius, cooldown and boss immunity or resistance are balance decisions.

#### Your Nemesis (the last major dungeon before NICE): Your USB Stick
- **Your Nemesis** is the man's actual name. He stole Zenny's USB stick and swapped it for NICE's,
  which caused the original incident.
- **The dungeon returns YOUR USB STICK:** literally a wooden stick or branch with a USB drive or
  connector built in. This completes the long-running USB language joke.
- **It is a culmination of the player's learned systems,** not merely a conventional high-health
  boss.
