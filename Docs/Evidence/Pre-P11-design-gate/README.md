# Pre-P11 design gate: evidence

Operator-approved on 2026-10-03:
- a bounded performance attribution of the inherited reversal-frame spike;
- a docs reconciliation;
- the Building v1 design ([BUILDING-V1-DESIGN](../../BUILDING-V1-DESIGN.md), a proposal).

**No gameplay code changed. The 40 ms budget is unchanged. Nothing was optimized.**

## 1. Runs (strictly sequential, quiet-gated)
Every run started only at 1-minute load < 3 with the GPU idle ([`perf/run.sh.txt`](perf/run.sh.txt), the same gate
as `Tools/perf/quiet-run.sh`).
- **Builds:** a fresh clone of master `e64523b` (the P10 merge), and a fresh clone of the P5 tag
  `p5-seamless-grid-green` (`0a9f0dc`), built from tracked inputs plus the pinned engine.
- **Sharing:** another session and a browser playing video shared the machine, as before. The end-of-run load is
  recorded on each row.

| Run | Build | Worst frame (ms) | p99 (ms) | Load at end |
|---|---|---|---|---|
| towndense reversal, **traced** (heavy channels) | P10 | 36.8 | 5.71 | 5.4 |
| towndense reversal | P10 | **36.0** | 5.44 | 4.4 |
| towndense reversal, traced (light channels) | P10 | 41.2 | — | 6.5 |
| local reversal r1 | P10 | 32.3 | 5.28 | 6.9 |
| local reversal r2 | P10 | 31.9 | 5.09 | 11.0 (a browser joined) |
| local reversal r1 | **P5** | 38.1 (contaminated: a 20.6 ms render-thread blip in its 2nd hitch, load 14.9) | 9.90 | 14.9 |
| local reversal r2 | **P5** | **34.6** | 8.37 | 4.6 |

**P5 on today's machine: 34.6 ms**, against 34.1 ms recorded at P5 on 2026-09-25. P10's local reversal measures
31.9–32.3 ms. **Nothing has drifted in the code.**
- Today's quiet towndense run is within budget (36.0 ms).
- Two GCs in the same run took 18.7 ms and 28.1 ms (below), so a single run varies by about 10 ms.
- Earlier breaches (40.2 / 41.6 / 42.4 ms) sit inside that spread.

## 2. Attribution: the spike is a full garbage collection
The light trace (`-trace=cpu,gpu,frame,bookmark,log`) captured a **41.2 ms** frame, 2 frames after the second lots
unload. Insights' headless export ([`trace/`](trace/), scopes in
[`trace/hitch-41ms-scopes.txt`](trace/hitch-41ms-scopes.txt)) gives:

| Thread | In the 41.2 ms frame |
|---|---|
| **Game thread** | busy all 41.2 ms: normal work ~6 ms, `TickCompletionEvents` 2.4 ms, and **`ConditionalCollectGarbage` 28.1 ms** |
| ↳ `BroadcastPreGarbageCollect` | 5.9 ms, of which **`PyUtil::CollectGarbage` 5.7 ms** (the editor's Python plugin; it exists only because perf runs use `UnrealEditor -game`) |
| ↳ `FRealtimeGC_PerformReachabilityAnalysis` | **11.9 ms** (parallel; workers run `GC.MarkRootObjectsAsReachable`) |
| ↳ `UnhashUnreachableObjects` / `ConditionalBeginDestroy` | 4.8 ms |
| ↳ `IncrementalPurgeGarbage` | 3.1 ms |
| **Render thread** | its `Frame/WorldTick` scope spans the frame, waiting on the game thread; `SceneRender` 5.0 + 2.9 ms on either side |
| **RHI thread** | mostly `WaitForTasks` (18.8 ms during the GC); `DeleteRHIResources` 1.0 + 0.9 ms |
| **GPU** (graphics queue) | idle for ~21 ms in the middle of the frame. It is starved, not slow |
| **Level removal / our streaming** | in the unload frame itself, 2 frames earlier: "load/unload" 9.5–10.6 ms, within the 12 ms streaming budget (unchanged since P5) |
| **Synchronization** | nothing beyond the render and RHI threads waiting on the game thread |

The 31.2 ms hitch at the first unload has the same shape (GC 18.7 ms: pre-GC 5.9, reachability 9.4).

**Why GC runs there.** UE forces a full GC after a streamed level is removed (`s.ForceGCAfterLevelStreamedOut`,
engine default on; not overridden by the project). Each lots unload therefore triggers one GC, two frames later.
- Its cost scales with live UObjects (reachability) and with the unloaded cell's objects (unhash and purge). That
  is why local < dense < town < towndense.

**This corrects the earlier reading.** The P5 notes, and the post-P10 roadmap review, said the hitch was "engine
level removal" with "~30 ms outside the game's timers". That was a measurement artifact:
- The harness records `FApp::GetDeltaTime()` (the previous frame's duration) together with this frame's
  `GGameThreadTime`, `GRenderThreadTime` and GC flag.
- So the hitch record showed ~5–6 ms of game thread and `garbageCollected: false` for a frame that spent 28 ms in
  GC (`GLPerfCommands.cpp:385-400`).

## 3. Conclusions
- **The 40 ms threshold is reproducible only stochastically.** The quiet median is ~36 ms, and GC duration
  variance alone spans ~10 ms.
- **The cause is a deterministic GC on every lots unload,** with nondeterministic duration.
- **No milestone introduced it.** The P5 build gives the same numbers today (34.6 ms local), so git bisection has
  nothing to find.
- **Two parts are editor-binary artifacts:** `PyUtil::CollectGarbage` (5.7 ms), and an object graph enlarged by
  editor objects and plugins (PCG and Python reference collectors appear in the trace). A cooked game build would
  not pay them. **The budget is not changed on that basis.**
- **It does not block P11.** It is an engine-owned, well-understood cost, not an architectural flaw in the Grid or
  the structural model.

## 4. Does Building v1 risk making it worse?
**Moderately, and measurably.**
- Every player piece is an actor with components, so it is a set of UObjects.
- A 300-piece base adds objects to GC reachability everywhere, and to the unload GC of its own cell.
- Expect roughly +1–3 ms per few hundred pieces, by analogy with town (442 parts) against local. This is an
  estimate, not a measurement.
- **P11 must measure it:** the dense player-built fixture (≥ 300 pieces) runs in the reversal suite under the
  unchanged budget.

## 5. Small, evidence-backed fixes (proposed, NOT done; operator choice)
1. **Fix the harness's hitch attribution:** record GC, game-thread and render-thread values for the same frame as
   the delta.
   - It is a measurement fix of a few lines, and it changes no game behaviour.
   - **Recommended before P11,** because P11's perf evidence depends on it.
2. **Measure the reversal suite once in a cooked development build.** It removes the editor-only GC work and shows
   the real game's margin. The cost is a packaging step.
3. **Spread the GC (an experiment, not a fix):**
   - UE's incremental reachability (`gc.AllowIncrementalReachability` with a time limit) and incremental
     BeginDestroy;
   - or decoupling the forced GC from level removal (`s.ForceGCAfterLevelStreamedOut=0`, which relies on the 61 s
     periodic GC and therefore moves the cost rather than removing it).

   Engine-level settings with gameplay-wide effects: **operator decision, and not before measurement (2).**

## 6. Raw evidence
- `perf/`: every run's JSON and log excerpt, the runner, and its load log.
- `trace/`: the Insights command files, the thread list, and the 41.2 ms frame's scopes.
- The traces themselves (3.4 GB heavy, 0.9 GB light) stay out of git, under `Saved/pre-p11/`. The heavy trace
  (`-statnamedevents`, 339 M scopes) OOM-killed Insights' UI at 20 GB, hence the light re-capture.
- **Harness notes:** Insights ran `-ExecOnAnalysisCompleteCmd` only with its UI (`-NoUI` skipped the commands, and
  `-RenderOffscreen` crashed). `-threads=` did not match "RenderThread 0" by name or id, so the window was exported
  for all threads.
