# Story and dialogue

Status: **design; not built.** Decision record:
[ADR-0015](ADR/0015-contextual-dialogue-system.md). All dialogue is authored
content. No runtime language model is involved
([ADR-0006](ADR/0006-companion-isolated-from-lab-agent.md)).

## 1. The cast's dynamic

- **Zenny** is silent. NICE's toy and test subject. He never speaks, and
  NICE notices ("the silent type").
- **Pehlichi** talks for the pair. A former Neurolink scientist in a
  squirrel body: curious, analytical, and **sick of NICE's nonsense**. He
  antagonizes her back. His philosophy: *"Curiosity can't kill me. I'm not a
  cat."*
- **NICE** is the Game Master. Playful, arrogant, funny, cruel, theatrical,
  unpredictable, and increasingly unstable. She does not deliver villain
  exposition; she **plays with Zenny** and makes sure he knows she is
  watching. She taunts Zenny at least as much as she spars with Pehlichi.

**Arc: the more Pehlichi repairs the world, the more NICE comes apart.**
Early NICE is composed and confident. Late NICE is unstable, and the core is
chaotic because she herself is unraveling. Her composure is derived from
world state ([ADR-0013](ADR/0013-derived-world-stability.md)), so her voice
shifts with it.

**Smart-ass, contextual, Zenny-reactive NICE/Pehlichi banter is a core product
feature, not polish.** The early vertical slice must prove the
dialogue-selection architecture with real triggers (about 40 authored test
exchanges), and the architecture must scale to far larger content. The
director is therefore built right after the first salvage/inventory loop
([MILESTONES.md](MILESTONES.md)), not at the end.

## 2. Delivery rules

- **Story accompanies play.** It is told through contextual banter during
  normal gameplay, not by interrupting it. Avoid long monologues and frequent
  cutscenes.
- **Smart-ass humor is core identity.**
- **Silence is important.** NICE and Pehlichi must never become nonstop
  podcast hosts. Quiet stretches are part of low-attention play.

## 3. What triggers dialogue

Lines arise from play:

- exploration and discoveries;
- player behaviour: salvage, building, deaths, overencumbrance;
- combat and avoidance;
- glitch repair;
- past-life/memory discoveries (era fragments);
- Glitch Storms;
- progression milestones;
- unusual player behaviour;
- NICE losing control.

## 4. Voice guide (illustrative; not a script)

These establish tone. **They must not become constantly repeated lines.**

- After Zenny strips wire, NICE jokes about people who strip wire: "Where's your bike?"
- Zenny overencumbered: NICE suggests push-ups.
- A mob kills Zenny: "I bet you won't do that again. ... Yeah, you will."
- Out of nowhere: "Don't you have something better to do?"
- On Zenny being the silent type.
- Mocking, or grudgingly praising, Zenny's building quality.
- Pehlichi: "Curiosity can't kill me. I'm not a cat."
- Eventually NICE offers Pehlichi a dozen nuts to stop repairing glitches.

## 4a. Riddles and hints (ADR-0023)

- NICE poses riddles; Pehlichi and NICE may argue about them; **the dialogue never
  solves them**. Zenny answers by acting: presenting an item, manipulating something,
  or performing an action.
- Pehlichi gives hints on request, escalating from vague to explicit. NICE mocks
  excessive help: "Would you like me to solve it for you too?" / "That's literally
  what I'm here for."
- Riddles are one puzzle family among many; don't let every hidden glitch become a riddle.

## 5. Commentary frequency setting

A player setting controls **optional** NICE commentary:
**Quiet / Normal / Chatty / Unhinged.**

- It scales how often optional lines may fire and the minimum silence between them.
- **Story-critical lines are never suppressed** by this setting.
- Quiet still means NICE exists; she just picks her moments.

## 6. System design (for implementation later)

The system is data-driven and history-aware. Selection logic is pure and
testable, and content is JSON that agents can write.

```
gameplay systems --(tagged events)--> Dialogue Director --(chosen exchange)--> presentation
                                         |   ^
                         candidate pools  |   | facts: history, cooldowns, world state,
                         (Data/dialogue)  v   |        NICE composure, band, setting
                                    selection rule (Core, pure)
```

**Events.** Systems emit **gameplay event tags**
(`Event.Salvage.WireStripped`, `Event.Player.Died`, `Event.Player.Overencumbered`,
`Event.Glitch.Repaired`, `Event.Storm.Started`, ...) with a small payload.
Emitting systems do not know dialogue exists.

**Content unit: an exchange.** One or more lines, alternating speakers (NICE,
Pehlichi), each with text and a stable id. An exchange declares:

| Field | Purpose |
|---|---|
| `trigger` | event tag(s) it responds to |
| `conditions` | facts that must hold (history counters, NICE composure range, band, era, first-time only, ...) |
| `category` | `StoryCritical`, `Contextual`, or `Ambient` |
| `priority` | resolves simultaneous candidates |
| `cooldown` | per exchange, plus the pool's and the global silence gap |
| `maxUses` | repetition cap (often 1) |
| `weight` | random choice among equals |

**Rules the director enforces.**
- One conversation at a time. A story-critical exchange may pre-empt an
  optional one, never the reverse.
- A global minimum silence between optional exchanges, scaled by the setting.
- Repetition protection: per-exchange cooldowns and use caps; recently used
  exchanges are down-weighted.
- History-aware: facts are persisted (what was said, how often an event
  happened), so NICE can say "again?" and mean it.
- Deterministic under a seed, so selection is unit-testable.

**Persistence.** Dialogue history and story flags are saved. NICE's
composure is not stored; it is derived.

**Out of scope for now:** voice acting, lip sync, cinematic cutscenes and
localization tooling. Text ids are stable so these can be added later.

## 6a. Dialogue north star (operator decision, 2026-09-25)
**The primary purpose of ambient and reactive NICE/Pehlichi dialogue is to make the player laugh.**

Story, lore, characterisation and exposition travel *through* that comedic relationship far more
often than they replace it.

**Adult tone is intentional.** Allowed:
- profanity, crude humour and sexual innuendo;
- fart jokes;
- dark comedy and gallows humour;
- insults, and deliberately tasteless jokes where they land.

Do not sanitise to broaden the audience.

**Craft rules:**
- Not every line is a joke. Serious and emotional moments stay serious.
- NICE and Pehlichi have **distinct comedic voices**.
- Timing, restraint, callbacks and relevance matter more than quantity.
- Pehlichi eventually uses "that's what she said", contextually and sparingly.

## 6b. Digestive comedy (writing requirements; mechanics in KNOWLEDGE-AND-DISCOVERY §5–6)
- **Zenny is "silent but deadly"**, a recurring canonical joke.
- **Pehlichi escalates:** a snicker, then "Seriously?", shushing, "You're gonna have to learn how
  to control that thing.", "Dude, put a plug in that thing.", and finally "Bro."
- **NICE reads the context:** the food and its quality, recent repeats, stealth, whether a creature
  heard it, whether it caused detection, and whether Zenny died of it ("Zenny, are you lactose
  intolerant?").
- **Restraint:** silence is a valid outcome. Callbacks and escalation, never a random bucket.
- **Setting:** it can be turned off (removing the events and their dialogue consequences).
- The dialogue system's history requirements (`requires.eventCount`), cooldowns, maxUses and
  subject matching already support escalation and callbacks.

## 7. Invariants

| # | Invariant |
|---|---|
| D-1 | Dialogue is authored content selected by rules. No runtime model generation (ADR-0006). |
| D-2 | Story-critical lines ignore the frequency setting; everything else obeys it. |
| D-3 | Optional dialogue respects a global silence gap and repetition limits. |
| D-4 | Gameplay systems emit events and never call dialogue directly. |
| D-5 | Selection is deterministic for a given seed and state (testable headless). |
