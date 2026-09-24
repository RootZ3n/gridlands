# ADR-0004: Valheim-style snap-socket building, not a strict grid

- Status: Accepted
- Date: 2026-09-24
- Decider: operator

## Decision
Build pieces declare named sockets. Placement snaps socket-to-socket, with free
placement on terrain. The world may *look* grid-like as simulation aesthetic,
but construction should feel organic.

Structural support (Valheim-style integrity propagation) is designed for, not
built: `UGLBuildPieceDefinition` reserves support fields and the piece graph
is kept, but no integrity simulation runs during bootstrap.

Repair of a building means restoring a `Damaged` piece to `Intact`. This is
a player action with materials, distinct from glitch repair (ADR-0005).
