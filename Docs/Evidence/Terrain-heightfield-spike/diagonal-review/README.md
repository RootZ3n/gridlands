# Render-diagonal visual review (ADR-0035): PASSED (operator, 2026-09-27)

**The review passed** (operator, 2026-09-27).
- On ordinary and natural terrain, the differences are negligible.
- Under aggressive terraforming, they are acceptable.
- **Keep the aggressive-terraforming pair (`05-*`) as regression evidence.** Extreme 1 m edits produce
  harsh wedges, creases and material boundaries: that is future terraforming presentation debt, and
  future work must make that case better, not merely hide or move it.

**The one difference between the two sides:** how each 1 m terrain quad is split into two triangles
for rendering.

| | Split | Where it comes from |
|---|---|---|
| **Before** | (x+1, y) to (x, y+1) | the shipped look, `-GLTerrainCollision=0` |
| **After** | (x, y) to (x+1, y+1) | the diagonal a Chaos heightfield uses, `-GLTerrainCollision=1` |

**What the capture held fixed:**
- the same terrain data and edits (seeded);
- the same cameras (60° FOV, 1920×1080), the day lighting preset and the P7.1 variant A settings;
- the same frame-counted timing (fixed timestep, `-benchmark -fps=30`);
- Zenny and Pehlichi hidden in both runs, so only terrain is judged.

No other visual change was made, and nothing was adjusted to compensate. Reproduce with
`Tools/terrain-diagonal-review.sh` (dev command `gl.Terrain.DiagonalReview`).

**Scenes:** all in the diner lots (6 m rolling relief) around a chunk corner. Each has a medium view
(16 m away, 6.5 m up) and a close view (5.2–5.6 m away).

| Scene | Content |
|---|---|
| 01 | Rolling natural terrain (untouched) |
| 02 | The steepest natural slope in the lots' playable middle: **30.2°**, found by scanning the field |
| 03 | Dig: a pit dug three times, to 4 m |
| 04 | Raise and mound: a 3 m mound with a smaller raise on its flank |
| 05 | Aggressive irregular terraforming: 40 seeded random digs and raises in an 18 m square |
| 06 | Chunk seam: a raise and a dig straddling a 64 m chunk seam |

**Control:** "before" was captured twice. Identical runs differ by a mean of 0.00–0.22 (0–255 scale),
so the capture is deterministic. `pixel-differences.json` has, per shot, the control and the
before/after difference:

| Shot | before/after mean abs | pixels changed > 8 |
|---|---|---|
| rolling (medium / close) | 2.3 / 2.1 | 3.8% / 4.0% |
| steep slope | 2.4 / 3.5 | 4.0% / 8.8% |
| dig | 2.9 / 26.0 | 5.9% / 59.4% |
| raise and mound | 2.5 / 7.7 | 5.7% / 27.6% |
| aggressive terraforming | 10.4 / 14.2 | 30.4% / 43.2% |
| chunk seam | 4.0 / 9.5 | 10.1% / 28.6% |

**What changes, observed without judgement:**
- **Facet and silhouette edges** of steep, edited ground follow the other diagonal. Pit rims, mound
  ridges and the spikes of aggressive edits have differently shaped crease lines.
- **The grass-to-exposed-earth boundary changes shape.** The exposed-earth mask is stored per vertex
  and blended across each triangle, so the diagonal decides the blend's shape. On the close dig in
  particular, V-shaped earth wedges appear along the new diagonal.
- **Per-vertex shading normals shift slightly,** because they are averaged from the triangles around
  each vertex.
- **Smooth untouched ground** changes least: rolling and steep natural terrain have means of 2–3.5.

`side-by-side/*.jpg`: full resolution, before on the left, after on the right.
