# ADR-0020: Content ids, tag namespaces, and eras as data

- Status: Accepted
- Date: 2026-09-24
- Decider: operator (E4/E5); grammar proposed by agent

## Decision
- Every content entity has a **stable namespaced id** such as
  `item.material.copper_wire`. The exact grammar and validation rules are in
  [CONTENT-IDS-AND-TAGS.md](../CONTENT-IDS-AND-TAGS.md) and are enforced by
  the M2 validator before content volume grows.
- Categories are **Gameplay Tags** in registered namespaces: `Era`, `Band`,
  `Material`, `Source`, `Yield`, `Event`, `Knowledge`, plus the system
  namespaces already in use (`Interact`, `Tool`, `Station`, `Command`,
  `Capability`, `Requirement`). A new namespace is added **only when a real
  system requires it**, and registering it is a reviewed change.
- **Eras are data**, never a C++ enum or a progression tier. The ten
  established eras are era definitions under `Data/`:
  Ice Age, Ancient Library, Roman, Medieval / Hedge Knight, Feudal Japan,
  Victorian, 1800s Native American, 1920s, 1950s, Modern Day.
  Adding an era is a data change with no core-code change.

## Reversal cost
High once content exists, which is why the grammar is fixed before M2.
