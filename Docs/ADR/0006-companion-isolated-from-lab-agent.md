# ADR-0006: The in-game Pehlichi is isolated from the real Pehlichi lab agent

- Status: Accepted
- Date: 2026-09-24
- Decider: operator

## Decision
The Unreal companion never connects to a cloud model, agent API, lab service
or live Pehverse runtime. No network calls, no credentials, no lab endpoints
in the game. Classes are `GL`-prefixed (`AGLPehlichi`), so lab tooling that
greps for the agent does not match game code by accident.

## Reversal cost
Any future integration is a new ADR with its own isolation boundary.

## Amendment (2026-09-24, design reconciliation)
This applies equally to **NICE** and to all dialogue: every line is authored
content selected by rules (ADR-0015). No runtime language-model generation in
the game.
