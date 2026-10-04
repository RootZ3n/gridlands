# ADR-0040: Playable building: build mode, the piece browser, and a UI that says what the commit does

- Status: **Proposed (P12 candidate, awaiting operator review).** Built 2026-10-04 under the approved P12 proposal and
  operator decisions Z1–Z9 of 2026-10-04. Not merged, not tagged.
- Date: 2026-10-04
- Builds on: [ADR-0039](0039-building-v1-canonical-structural-model.md) (Building v1: the canonical structural model,
  PREVIEW == REALITY, the material pool, instanced presentation). Nothing in ADR-0039 is replaced.
- Evidence: [P12 evidence](../Evidence/P12-playable-building/README.md).

## Context
P11 proved the building engine; a script built the WINCHESTER house. P12's question is different: can the operator build,
modify, finish, salvage, save and revisit a WINCHESTER-style house comfortably with the game's own controls? The operator
approved a coherent build mode, code-built UI (no authored UMG assets), and one rule above the others: the UI must say what
the commit does and why, from the commit's own results.

## Decision

### 1. Build mode is a state machine; the UI renders it and sends intents
- `UGLBuildModeComponent` owns build mode. It has one sub-state at a time:
  - **PLACE**, the default;
  - **BROWSE** (Tab);
  - **FINISH** (Y);
  - **REMOVE** (X).

  The primary action means what the sub-state says and nothing else. A state change cancels any pending confirmation.
- **The view.** Every frame the component computes `FGLBuildView` from canonical results only:
  - the placement check;
  - the snap;
  - the cost plan;
  - the removal prediction;
  - the commit's yields;
  - the claim.

  Widgets read the view. Keys, the browser and the proof all go through the same public intents (`Primary`,
  `ToggleBrowser`, `CycleVariant`, `RotateFine`, ...).
- **UI state is never authority.** Every commit re-aims and re-checks at its own moment, so a stale view is never used.
- **UI technology** (Z1):
  - the HUD is Canvas (`AGLHUD`: the build bar, the reason line, the panels, world-space markers);
  - the friction picker is C++ Slate, because it needs text entry;
  - nothing depends on authored UMG assets.

### 2. The piece browser is data: categories, not eras
- **Categories.** A new content kind, `buildcategory.*`: display name, order, `fallbackRoles`.
- **Category per piece.** A piece may name a `category` (author intent). Otherwise it falls into the one category whose
  `fallbackRoles` hold its role.
- **Validator rules.** CAT-1 makes this total and deterministic: every buildable piece resolves to exactly one category,
  and no role falls back into two. CAT-2 keeps category orders unique.
- **The catalogue.** `GLBuildCatalog` (pure) derives it: categories in data order, pieces by display name.
- **Eras and styles are filters and vocabularies, never locks** (operator). A Roman column beside a Victorian porch on a
  log wing is valid whenever the structural rules say so; no hidden style rule exists anywhere.
- **Favorites and recents.** 8 favorites (1–8; Ctrl+digit pins) and 8 recents (deterministic: most recent first, no
  repeats). The browser always returns to placing after a choice.

### 3. WHAT THE UI SAYS == WHY THE COMMIT ACCEPTS OR REFUSES
- **Machine-readable refusals.** The placement check (`FGLBuildCheck`) now carries its refusal detail:
  - `BlockingPieceId`: the piece in the way;
  - `MissingItem` / `MissingNeeded` / `MissingHave`;
  - `Material`: what the LIMIT is about.
- **Text from results.** `GLBuildText::Explain` turns that structured result into words, and widgets never re-derive a
  rule. The state is a word, an icon and a colour (OK `[+]`, LIMIT `[!]`, NO `[x]`), never colour alone.
- **Cost display.** `FGLMaterialPool::PlanConsume` runs the consumption itself on copies.
  `UGLBuildingSubsystem::CostView` shows its plan: what base storage holds and gives, and what Zenny holds and gives, in
  the commit's order. There is no second "UI inventory calculation".
- **Salvage preview.** `RemovalYield` is the commit's own scaled yield, for each path.
- **Snap info.** `FGLSnapInfo` reports what the snap connected (target piece and socket, own socket, yaw from data), and
  the marker draws exactly that.
- **Resting snaps: most supports, then away from the viewer, then the aim** (a change to the canonical snap, flagged for
  review). When a piece has several bottom sockets that could rest on the same socket (an upper floor's four edge
  midpoints on a wall top; a porch roof's two post sockets), the choice is, in order:
  1. the way that sits on the most supports (an upper floor over the room rests on both walls, not straddling one);
  2. among equals, the one that extends away from the viewer, along the aim ray (seen from outside a row of walls,
     resting along the row is "supported twice" too; the viewer's direction decides, not a centimetre of aim);
  3. then the one whose centre is nearest the aim point.

  Data order no longer decides. Before P12, an upper floor aimed at a wall top snapped a metre off, outside the room.
  Side links keep their first match, so angled construction is unchanged. The P11 WINCHESTER proof and every P11 test
  pass unchanged with the new rule.
- **Snap tests overlap only.** Snap used to run the whole placement check (including the support solve over the entire
  structure) for every candidate socket, only to ask whether it overlapped. The overlap test is now one function
  (`OverlapsAny`) used by both Snap and the placement check, so they cannot diverge. It was the measured build-mode cost
  (see the evidence's performance section).
- **Finish choices.** `FinishesFor` lists the finishes the install rule itself accepts. The chosen finish is the one
  installed, and the last choice is remembered per role.

### 4. Removal: the prediction decides the confirmation
- **What REMOVE shows:**
  - the target;
  - the canonical collapse set (red overlay; `CollapsesAfterRemoving`, the function the commit's collapse equals);
  - both paths' yields (careful and smash).
- **Confirmation.** A removal that brings nothing else down is one click. One whose prediction is non-empty needs a hold
  (0.4 s), or a second press in the toggle accessibility mode. The collateral collapse triggers it, never the piece's
  category or importance.
- **A confirmation never transfers.** It is bound to its target and its prediction; anything changing cancels it.

### 5. Camera aim == gameplay aim; the build camera is bounded
- **One ray.** The aim is the player's view point (`AController::GetPlayerViewPoint`: the camera, including the build
  camera). The same ray feeds the ghost, the markers, the reason and the commit.
- **The build camera** (Alt, hold by default; a toggle option for accessibility):
  - pulls the arm back to 10 m and raises it;
  - Shift+wheel adjusts the height by ±3 m;
  - spring-arm collision stays on;
  - no free flight.
- **Accessibility settings** (console; saved in the playtest profile, never in a world save):
  - `gl.Build.CameraMode hold|toggle`;
  - `gl.Build.ConfirmMode hold|toggle` (toggle: press twice within 3 s, on the same piece and prediction);
  - `gl.Build.TextScale`.

  There is no remapping screen (out of scope).

### 6. Rotation
- Rotation stays canonical 2.5 degree steps (Z6):
  - Z: +90;
  - Shift+Z: −90;
  - C: +15;
  - Ctrl+wheel: ±2.5.
- The yaw persists while a piece is repeated, unless a snap's data sets it, and the build bar then says "angle from the
  socket".

### 7. Claims are shown, never framed as limits
- **The ring.** Each canonical claim area is drawn as a ring (`FGLBuildView.ClaimAreas`, a list ready for expansion).
- **The label.** "Base area (32 m, provisional)". It never says maximum, limit or base size.
- **Cell boundaries are never drawn.** Long-term direction (operator): claims and single player structures must be able to
  cross cell boundaries. Not built in P12, and nothing in the UI encodes the opposite.

### 8. Primitives (Z2) and the one piece they need
- **Straight stair** (`buildpiece.modern.timber_stair`, category Stairs):
  - an ordinary structural component: support, preview, removal prediction, salvage, ownership, claims, persistence,
    presentation and plans as for any piece;
  - its head is a side socket that snaps to an upper floor's edge; its feet rest on the floors below. Like any piece
    that reaches another, it also holds up what it reaches;
  - 1.4 m wide (navigation's agent radius needs it), a 4 m run and a 2.7 m rise, from a foundation top to an upper floor;
  - 12 solid treads: Zenny climbs them physically and navigation reads them. There is no special case.
  - No families, landings or railings.
- **Window wall** (`buildpiece.modern.window_wall`):
  - a stud wall frame with a real opening (collision is four boxes around it);
  - finished like any stud wall (clapboard; tint-only window boards, so the opening stays open);
  - no glass, no fittings, and not a door (interactive doors are P13, Z3).
- **Upper floor** (`buildpiece.modern.upper_floor`), **an addition the operator did not list**: player building had no
  buildable upper floor (the deck and post are world-only), so a stair led nowhere. It is one 2 m floor that rests on
  stud-wall tops by its edge midpoints and carries the next storey. It is flagged for review, not a floor family.

### 9. The playtest profile and the friction log are evidence, not game state
- **The profile** (`Saved/Profile/build-profile.json`): favorites, recents, the finish remembered per role, the camera
  and confirmation modes, the last piece. Never part of a world save.
- **The friction log** (Z7): F8 opens a Slate picker with a stable category vocabulary (CAMERA … OTHER) and an optional
  note. `gl.Friction <CATEGORY> [note]` writes from the console. Both append one JSON line to
  `Saved/Playtest/friction.jsonl`, with the build context only (state, piece, finish, yaw, snapped, structural word,
  reason, timestamp).
- **The starter kit** (Z9): `gl.Dev.BuildingStarterKit` gives the building knowledge and materials. It is dev only
  (compiled out of shipping builds).

## Consequences
- The operator can judge snapping, structural feedback, finding pieces, finishing, storage and salvage with the real
  controls, and log what grates.
- Every claim the UI makes is checked against the commit: automation, a real-game public-intent proof, and planted
  defects.
- P13 (authoring and export) builds on a usable construction language. Plans remain ordinary pieces.

## Not changed / not built
P13 export and authoring; Ofi; interactive doors; arches; new roof families; electrical; cross-cell structures and claims;
claim expansion; production art; authored-structure instancing; full gamepad support; text search; logistics screens;
key remapping.
