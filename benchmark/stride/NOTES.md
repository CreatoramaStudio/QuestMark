# Stride benchmark notes

Implements `benchmark/SPEC.md` for Stride 4.4.1 + Stride.BepuPhysics 4.4.1, .NET 10, Direct3D11, scenes built in code.

Run: `StrideBench.exe --scenario=pit-2000|raycast-1000|raycast-10000|chains|mixed --mode=physics|render --run=N --out=<dir>`
Scenarios E and F: `--scenario=unlock-render-mixed|unlock-physics-mixed|unlock-physics-pit-2000|scale [--bodies=N --unpaced=1]`, see "Scenarios E (`unlock`) and F (`scale`)" below.
Scenario G: `--scenario=debris|debris-pooled --rate=R --mode=physics|render` (R events per second, may be fractional; warm-up defaults to 10 s for these two), see "Scenario G (`debris`)" below.
Native AOT variant: `dotnet publish -c Release -r win-x64 -p:BenchAot=true -o <dir>`, see "Native AOT variant" at the end.
Extra options (not in SPEC): `--warmup=<s>`, `--measure=<s>`, `--uncapped` (render only), `--timer1ms=0|1` (default 1 = `timeBeginPeriod(1)`), `--hidden` (physics mode only),
diagnostics (default off): `--diag=1` (writes `<csv>.diag.csv`: per-frame Update start/duration, Draw start/duration, EndDraw (Present) duration), `--nomodels=1`, `--bepu-threads=N`, `--render-threads=N`.
Defaults are the SPEC values (5 s warm-up, 20 s measured).

## How the numbers are taken
- `wall_ms`: time between consecutive `Game.Update` calls (Stopwatch).
- `physics_ms`: Stopwatch between the Bepu plugin's `ISimulationUpdate.SimulationUpdate` and `AfterSimulationUpdate` hooks
  (`PhysicsTimer.cs`), summed per Update. This is the Bepu Timestep plus the plugin's contact flush and transform sync
  between those hooks. It is not the plugin profiler key. Frames in which Bepu ran no step report 0 or several steps summed.
- `query_ms`: Stopwatch around the whole ray batch, a loop of `BepuSimulation.RayCast` (closest hit, length 30) over a
  seeded pre-generated ray set. No batching API is used.
- `gc0..2`, `gc_pause_ms`: `GC.CollectionCount`, `GC.GetTotalPauseDuration` (cumulative).
- `awake_bodies`: `Simulation.Bodies.ActiveSet.Count` minus 1 while the kinematic ceiling body (chains, mixed) is awake, so it counts dynamic bodies not asleep like Godot. Short physics-mode runs: chains 1000/1000 (min/max), mixed 2934/3000.
- Extra summary line `draw_call_ms` (time in `Game.Draw`) is additional to the SPEC.

## Deviations and interpretation choices
- Restitution 0.1 is NOT applied: Bepu (and this plugin) has no restitution. Friction 0.5 is set.
- Solver: Bepu defaults (not tuned); recorded in env.txt. Plugin defaults: sub-steps and iterations are init-only and left alone;
  threading is the plugin default; soft start (1 s at 4x sub-steps) is active, so the first second costs more.
- (superseded, see "200 Hz physics" and "Unlocked render loop") Early runs: plugin `MaxStepPerFrame` 3 and a fixed 60 Hz loop; now 64 and unlocked.
- Every shape is wrapped in a one-child `CompoundCollider` (the plugin needs `ICollider`); this costs extra versus a bare shape.
- Capsule: radius 0.15, Length 0.2 (cylinder part), so total height 0.5 as in SPEC.
- Chains: 100 chains x 10 links = 1000 bodies and 1000 joints (each chain also has a joint to the ceiling bar), not the SPEC's 900.
  Joint type: `BallSocketConstraintComponent`, no limits. Neighbouring links do not collide.
- Impulse (2,0,0) is applied once to every 10th link (the chain's 10th link) when the body is awake.
- Physics mode: window 320x180, no meshes, visible (a hidden window gave bursty update pacing in an A/B on one machine; `--hidden` keeps that option).
- Render mode: 1280x720, vsync off, one directional light, procedural primitives with a plain material (superseded: loop is now unlocked, see "Unlocked render loop"). The process requests a 1 ms Windows timer resolution (`timeBeginPeriod(1)`, default on, see "Pacing investigation").

## Shader / asset compile
- The csproj references `Stride.AssetCompiler` 4.4.1 with `IncludeAssets="build;buildTransitive" PrivateAssets="all"` (build targets only).
  Its `build/Stride.AssetCompiler.targets` (in `$USERPROFILE/.nuget/packages/stride.assetcompiler/4.4.1/`) defines target `StrideCompileAsset`
  (AfterTargets=CopyFilesToOutputDirectory, active for OutputType Exe/WinExe, line ~217). It runs `dotnet exec tools/net10.0/Stride.AssetCompiler.dll build ...`
  with a generated runtime config for the project's .NET major (lines ~165-183), so a plain `dotnet build -c Release` produces
  `bin/Release/net10.0-windows/data/db/{index,aliases,bundles/default.*.bundle}` (about 2.3 MB, shader effects included). No project file or sdpkg is needed;
  the build prints one warning "Could not find game settings asset ... Use a Default One", which is harmless here.
- Do NOT reference the package without `IncludeAssets`: it then copies the editor/compiler assemblies into the output and the game dies at start
  with `FileNotFoundException: Stride.NuGetResolver` from `NuGetResolverModuleInitializer` (seen on this machine).
- The earlier workaround (csproj target `CopyStrideShaderSources`, reflection in `BenchGame.SetupShaderSources`) is removed. Render mode loads shaders from the DB
  and printed no "could not be found" error in the short runs listed below.
- Needs the Stride 4.4.1 packages in the NuGet cache (installed with the Stride launcher/editor); the package was not on nuget.org.

## Evidence so far (one machine, desktop class, short runs only)
- `dotnet restore` OK; `dotnet build -c Release`: Build succeeded, 0 errors (NU1902/NU1903 ImageSharp advisories suppressed).
- Short runs only (2 s warm-up + 5 s, pit-2000 render: 330 frames, wall p99 41.6 ms, physics median 3.7 ms), and one `mixed` render run
  (1202 frames, wall median 15.7, p99 47.5 ms, physics median 1.26, query median 0.31 ms, 1000 rays, 417709 hits, awake_last 1221).
  These are not SPEC results: no 5+20 s runs, no 3 repeats.
- After the asset-compiler change (short runs, 2 s warm-up + 5 s, render, one machine, back to back, not SPEC length): pit-2000 wall p99 43.1 and 33.3 ms
  (two runs; p95 24.7 and 16.7), mixed wall p99 53.8 and 51.2 ms (p95 44.8 and 45.5). Same-session runs of the old workaround build: mixed p99 58.9 and 73.0 ms.
  The 40-50 ms stalls remain with compiled shaders, so they were not caused by run-time shader compilation. Run-to-run spread is large.

## Unverified / open
- No full 5 s + 20 s run and no 3 repeats were done. Not run on a Steam Deck / Proton.
- Rendered output: looked at once (2026-11-06, mixed render screenshot, JIT and AOT exe): after adding a 0.5 intensity ambient light (superseded: ambient is now 0.3 with sun 0.55, see "Light and material brightness fit"; `BenchGame.SetupCameraAndLight`, `LightAmbient` white) the shadow side of the walls is dark orange, not black; other scenes and light settings not looked at.
- Wall time p95/p99 in render runs (was 40-60 ms): mostly 15.6 ms timer granularity, fixed by `timeBeginPeriod(1)` (see "Pacing investigation"); the remaining tail in mixed is real draw+update CPU cost.
- `--timer1ms` effect measured (see above); the exact internal wait that is quantised is unknown.
- The effect of one-child compounds, the missing restitution and the 1000 vs 900 joints on cost is unmeasured.
- `Window.Visible = false` (`--hidden`) behaviour is only partly tested.
- The API reading was done against Stride master source vs the 4.4.1 packages; differences were not checked fully.
- Physics-mode runs of every scenario were not exhaustively checked for correctness (e.g. that the pit contains the bodies).

## Pacing investigation (2026-11-06, one machine, Stride 4.4.1, short runs 2 s warm-up + 5 s, `bin/Release/net10.0-windows/StrideBench.exe`, CSVs from the same exe)
Proved by runs (render mode, mixed, 2 runs each unless noted; wall_ms median / p95 / p99 / max):
- Loop settings are already as intended (`BenchGame.cs` ctor: `IsFixedTimeStep=true`, `TargetElapsedTime`=1/60, `SynchronizeWithVerticalRetrace=false`; `wall_ms` is Update-to-Update, `BenchGame.Update`).
  The Present call (`EndDraw`) takes about 0.07 ms per frame (`--diag`), so there is no GPU/vsync wait in Present.
- Physics is not the stall: physics_ms median 1.4-2.0, p99 5-10 ms in every run; physics mode (no draw) holds 16.67 ms (p99 16.68) in both scenarios.
- Draw is the stall: render mode with `--nomodels=1` (same entities and physics, no `ModelComponent`) gave wall 16.666 / 16.677 / 16.68 / 16.74 and draw median 0.27 ms. With models, `Game.Draw` takes 10-55 ms in the bad frames
  (`--diag`: Update 5-8 ms constant, Draw 10-52 ms). A slow Draw delays the next Update, then Stride's fixed-step loop runs the late updates back to back (bursts).
- Windows timer resolution is the dominant factor: with the default 15.6 ms granularity, mixed render gave p95 about 42-47 and p99 53-65 ms in 7 of 7 baseline runs; with `timeBeginPeriod(1)` p95 about 20-22, p99 23-26 ms (3 of 3 runs, plus the table below).
  Draw median fell from 12-19 ms to 6.6-7.9 ms. Mechanism (which wait inside Stride's render/update path is rounded to the timer tick) is NOT identified; it is a hypothesis that a sleep or wait in the loop or render worker threads is quantised.
- Ruled out as the cause (mixed, 2 runs each, baseline timer): `--bepu-threads=1` (p99 52-53 ms), `--render-threads=1` (p99 57 ms), run-time shader compile (earlier), vsync/Present wait. `--render-threads=1` with the 1 ms timer is worse (p99 37 ms) than the default threads with the 1 ms timer.
- Residual after the fix (mixed): Update + Draw + Present exceeds 16.7 ms in 56 of 272 diag frames (max Draw 32 ms, 5-8 ms Update); about 30 frames of 301 are not drawn when this happens (catch-up). This is real CPU cost of drawing about 3000 moving
  models in Stride (draw median 6.5-7.9 ms), not a loop bug. Draw cost drops as bodies sleep (20 s run below).
- Fix in the benchmark: `timeBeginPeriod(1)` is now on by default (`Options.TimerRes1ms`, `Program.cs`); `--timer1ms=0` restores the old behaviour. SPEC unchanged (fixed 1/60, vsync off, 1280x720). Godot side: checked, see "Timer resolution, Godot vs Stride" below (the Godot release export also requests 1 ms).

Before (`--timer1ms=0`) / after (default), single runs, 2 s + 5 s, wall_ms median / p95 / p99 / max, frames:
| scenario, mode | before | after |
|---|---|---|
| pit-2000 render | 16.667 / 19.16 / 33.07 / 50.30, 301 | 16.666 / 16.75 / 18.74 / 22.84, 301 |
| mixed render | 12.558 / 47.06 / 60.92 / 68.47, 303 | 16.667 / 21.41 / 26.57 / 42.02, 301 |
| pit-2000 physics | 16.666 / 16.675 / 16.686 / 16.69, 301 | 16.666 / 16.677 / 16.68 / 17.95, 301 |
| mixed physics | 16.666 / 16.675 / 16.681 / 16.69, 301 | 16.666 / 16.674 / 16.681 / 16.68, 301 |
Mixed render 2 s + 20 s: before (`--timer1ms=0`) 15.90 / 44.15 / 48.96 / 101.6, 1202 frames, awake_last 2518; after 16.667 / 16.68 / 21.96 / 43.08, 1201 frames, awake_last 1292. n=1 each, not SPEC results.

### Frame count and awake-body difference (problem 2)
- Frames per measured window = measured seconds x 60 when the loop is paced (5 s -> 301, 20 s -> 1201). So 300 frames in a 5 s window is a 16.67 ms average (the 2 s warm-up is not recorded), not a 42 ms average.
  wall_ms median is 16.667 now in all "after" runs (and 16.67 in the pit baseline runs); in the mixed baseline runs the median was 12.6-15.8 because bursts of catch-up updates (short wall_ms) follow stalls.
- The earlier "mixed 1202 frames, awake_last 1221" run matches a 20 s measured window (1202 frames at 60 Hz = 20.0 s; this reproduces: 20 s runs gave 1201-1202 frames and awake_last 1292 / 2518). The "2 s + 5 s" label in the earlier note is inconsistent with its frame count;
  the earlier run's arguments were not recorded, so that is inferred from the numbers, not proved. awake_last falls with run time (pit bodies fall asleep: about 2970 at 7 s, 1292 at 22 s), which also explains 1221 vs 2960. Whether the layout differed earlier cannot be checked (no history of the code).

## Damping, summary, layout, timer (2026-11-06, one machine, Stride 4.4.1, Godot 4.7.2 release export, short runs, not results)
- Damping: the plugin default is read at run time from `BepuSimulation.PoseLinearDamping` / `PoseAngularDamping` (`BenchGame.cs`, before they are set) and the env file prints "damping_linear=0.1 damping_angular=0.1 (plugin defaults were 0.05 / 0.05)" (mixed physics run, 2 s + 5 s). Both are now set to 0.1 explicitly to match Godot's default 0.1. The Bepu and Jolt damping formulas are not identical; only the nominal values match.
- Summary: now prints `tick_frames=<n> of <rows>` and `physics_ms (tick frames only)` (a tick frame = at least one Bepu step in that Update, from `PhysicsTimerComponent.Steps`). CSV columns unchanged. In the short physics-mode runs all frames had a tick (301 of 301); render-mode behaviour with this change was not run.
- Mixed layout now follows the SPEC rule (before: inner pit 30 x 30, 24 x 24 column grid, skip below 5.4). See SPEC scenario D for the exact layout and the remaining ceiling and pit-2000 differences.
- Timer resolution, Godot vs Stride:
  - Method: `Sleep(1)` / `OS.delay_msec(1)` duration inside a process, 300 calls. System-wide `NtQueryTimerResolution` is useless here because it read 1.00 ms with no benchmark process running (another process on the machine requests it); on this Windows build the per-process request decides what a process's own sleeps get.
  - Control (C# probe, no `timeBeginPeriod`): Sleep(1) median 16.0 ms, p95 16.07 (2 runs); with `timeBeginPeriod(1)`: median 2.00, p95 2.50.
  - Godot 4.7.2 release export (scratch project exported with the same export template; `QuestBench.exe` itself was not instrumented): `OS.delay_msec(1)` median 2.00, p95 2.50 ms (3 runs), i.e. the same as the 1 ms control, so Godot's release export runs with a 1 ms timer resolution. `QuestBench.exe` also imports `timeBeginPeriod` and `timeEndPeriod` (name found in the binary). The console wrapper exe has no such import (not investigated further).
  - So Stride's default `timeBeginPeriod(1)` is fair on this machine; `--timer1ms=0` would handicap Stride. Not checked: Proton on the Deck.

## Full run (2026-11-06, one desktop with discrete GPU, Windows only, Stride 4.4.1 + Stride.BepuPhysics, `dotnet build -c Release` output)
- Command: `tools/run-all.sh stride` with `STRIDE_BENCH_EXE` = `bin/Release/net10.0-windows/StrideBench.exe`; output `benchmark/results/stride-bepu-<scenario>-<mode>-<run>.csv` (+ `.summary.txt`, `.env.txt`). Merged table: `benchmark/results/SUMMARY.md`. 5 scenarios x 2 modes x 3 runs, 5 s warm-up + 20 s measured, process restarted per run; no run failed the sanity check (rows 1201 in every run).
- Plain pit-2000 layout (12 x 12 columns) is now also the Godot rule (Godot changed from 10 x 10 x 20 layers); smoke check: `bodies_dynamic=2000`.
- Sleeping: the pit-2000 pile fell fully asleep in the 20 s window in all three physics runs and render runs checked (`awake_bodies` reaches 0 near row 1000 to 1170), while `physics_ms` stayed about 1.3 ms in the last 300 rows of physics run 1 (proved for that run; the reason is a hypothesis: per-step cost not proportional to awake bodies, for example the transform sync between the hooks).
- GC: `gc_pause_total_ms` in the summaries was 0 in most runs and 5 to 9 ms in some render runs (pit-2000 render 5.19, 8.63, 8.71; raycast-1000 render 9.32 and 8.87).

## Scenarios E (`unlock`) and F (`scale`) (2026-11-06, Stride 4.4.1 + Stride.BepuPhysics 4.4.1, `dotnet build -c Release`, `bin/Release/net10.0-windows/StrideBench.exe`)
### CLI (existing options unchanged)
- `--scenario=unlock-render-mixed` (render mode implied; `IsFixedTimeStep=false`, no target elapsed time, vsync off, 1280x720, `timeBeginPeriod(1)` default kept). Scene = `mixed`.
- `--scenario=unlock-physics-mixed` and `--scenario=unlock-physics-pit-2000` (physics mode implied, unpaced stepping, see below).
- `--scenario=scale --bodies=N --mode=physics|render [--unpaced=1]` (`--unpaced=1` is physics mode only and is the `scale-unlock-physics` pass).
- `--presize=0|1` (default 0, see "Bepu capacities"). `--warmup`, `--measure`, `--run`, `--out` as before.
- File names (`<out>/stride-bepu-<token>-<mode>-<run>.csv` + `.summary.txt` + `.env.txt`): token = `unlock-render-mixed`, `unlock-physics-mixed`, `unlock-physics-pit-2000`, `scale-<N>` (paced), `scale-unlock-<N>` (unpaced). CSV columns unchanged. The summary header line has `scenario=<token> layout=<scene>`.
- Wrong combinations (for example `unlock-render-mixed --mode=physics`) exit with code 2 and a usage message.

### Methods
(Superseded where it says 1/60 s, `FixedTimeStep` 1/60 s or `MaxStepPerFrame` 3: now 200 Hz and 64, see "200 Hz physics". The measurements below were taken at 60 Hz.)
- Render unlock: the plugin's `PhysicsGameSystem` steps `BepuSimulation` with the frame's `GameTime.Elapsed` through an internal accumulator (`BepuSimulation._remainingUpdateTime`, `FixedTimeStep` 1/60 s, `MaxStepPerFrame` 3, from the member list read by reflection; the method bodies were not read), so with a variable frame time it steps about once per 1/60 s of wall time. Measured, not assumed: summary lines `steps_per_s` (steps counted by the `AfterSimulationUpdate` hook / measured seconds) and `game_time_elapsed_sum_s` (equals `measured_seconds` in 3 of 3 runs). `steps_per_s` was 58.0, 58.2, 59.4, 60.2, 62.8, 63.6, 63.7, 65.4, 66.7 in nine short runs: about 60, not exactly 60 (up to +11 percent, down to -3 percent). Why it overshoots is not explained (hypothesis: catch-up accounting inside the plugin; not checked). Read `steps_per_s`, do not assume 60. The summary prints `wall_ms (tick frames only)` stats (tick frame = at least one step in that Update).
- Physics unlock: `BepuSimulation.Update(TimeSpan)` is internal in 4.4.1; it is bound once by reflection (`UnpacedLoop`) and called with `FixedTimeStep` (166666 ticks) in a loop, one call = one step. It returns without stepping when `BepuSimulation.Enabled` is false (proved: the first attempt with Enabled=false ran 0 steps per call), so `Enabled` is false during the game loop's own `base.Update` (the plugin's `PhysicsGameSystem` must not add wall-time steps) and true only around each call. The loop is `IsFixedTimeStep=false`; each `Game.Update` runs steps for about 20 ms of wall time (`BatchMs`), then returns so the window and the (empty) Draw are serviced.
  One CSV row = one step (`wall_ms` = duration of that step including the ray batch in mixed, `physics_ms` = hook stopwatch as before). Proof that every call was exactly one step: `unpaced_calls_not_one_step=0` in every unpaced run. `steps_per_s` = steps counted by the hook / wall seconds from the first to the last measured row (includes the game loop's work between batches). Extra lines: `steps_per_s_physics_only` (steps / sum of `physics_ms`), `steps_per_s_in_step_loop` (steps / sum of `wall_ms`), `speed_vs_real_time`, `base_update_ms_total`, `draw_ms_total`, `gap_between_batches_ms_total`.
  mixed keeps its 1000 rays per step (cost is in `query_ms` and `wall_ms`, not in `physics_ms`). Limits: the Bepu step is multi-threaded (plugin default), so steps/s depends on the core count and on other load; the window is the 320x180 visible physics window.
  Reading caveat: simulated time runs much faster than wall time, so piles fall asleep within the warm-up (pit-2000: `awake_bodies_last=0`, 4898 steps/s at 0.09 ms median, which is the cost of a fully sleeping world, not of a settling pile). The measured cost depends on how much simulated time has passed.
- Scale: `BuildPit(inner, 12, 0, rng, N, wallH)` follows Scenario A's rule (12 x 12 columns centred, 0.6 spacing, layers upward from y = 1, +-0.05 jitter, box for even index and sphere for odd, mass 1, friction 0.5, damping 0.1). Pit interior side = max(22, 22 x sqrt(N/2000)) rounded up to an even number of metres (22 at N <= 2000, 32 at 4000, 64 at 16000, 88 at 32000, 126 at 64000); wall height = max(6, ceil(half the initial column height)) (6, 9, 35, 68, 134 m at those N). N = 2000 gives exactly Scenario A. These sizes are my choice. The summary lines `bodies_outside_pit` / `bodies_below_floor` count dynamic bodies outside the interior or under the floor at the end of the run (0 in every smoke run below, which only covers 7 s of simulation or less, so spilling during later settling is not covered).
  Summary lines (also printed for the other scenarios): `fps`, `steps_per_s`, `paced_frames` / `expected_at_60hz` / `ratio`, `working_set_mb` (`Process.WorkingSet64` at the end), `gc_heap_mb`, `bepu_pool_mb` (`Simulation.BufferPool.GetTotalAllocatedByteCount()`), `bepu_body_handle_capacity`, `shape_batches`, `awake_bodies_last`, and the windows `window first_5s` / `window last_5s` (rows, awake median, `wall_ms` and `physics_ms` stats; seconds 0-5 and `measure-5` to the end of the measured window, so they overlap when `--measure` is under 10).
- Frame-lag caveat for large N (paced): when a step costs more than 16.7 ms the loop does about one step per frame (frames < 300 in 5 s), so simulated time falls behind wall time; at N >= 16000 the column of bodies is then still falling during the whole short run, and the stage cost is not the settled-pile cost. The SPEC pass rule (frame count within 1 percent) fails those stages anyway; the awake / settled split needs the full warm-up and 10 s window to judge.

### Bepu capacities and compound colliders
- Bepu buffers grow on demand; nothing hard-limits N. Proved by runs: N = 32000 and 64000 ran without error or out-of-memory (list below). `--presize=1` calls `Simulation.EnsureCapacity(SimulationAllocationSizes(...))` before the scene is built (sizes from the layout's body count, same formula for every scenario and N). A/B at N=16000 physics (2 runs each): final capacities identical (`bepu_body_handle_capacity=16384`, shape batches 8192 / 10922 / 16384 in both) and the cost differed by noise only, so the default is 0 (plugin defaults, same as the earlier results).
- Compound-wrapped colliders: `shape_batches` shows a compound shape batch (type id 6, `Compound=true`) with capacity >= N plus the inner box / sphere batches, so every body is a real one-child compound (proved by that summary line). Their memory and time cost is NOT isolated: `BodyComponent.Collider` takes `ICollider`, and `BoxCollider` / `SphereCollider` derive from `ColliderBase`, which is not an `ICollider`, so an unwrapped run is not possible without plugin source or a custom `ICollider`. Totals only: `bepu_pool_mb` 40-41 at N=16000 and 76 at N=64000 (physics mode), about 1.2-2.5 KB per body for the whole simulation.

### Smoke numbers (sanity only: short runs, 2 s warm-up + 5 s measured, run 1, one machine; another agent ran Godot smoke tests at the same time, so timings are noisy)
Exe: `bin/Release/net10.0-windows/StrideBench.exe` (Release build of this folder). Source of each row: `stride-bepu-<token>-<mode>-1.csv.summary.txt` from that run (scratch folder, deleted); CSV row counts equalled `frames=` (+ header) in the runs checked (pit-2000 unpaced, mixed render, scale 1000 / 4000 / 16000 / unpaced).
| token, mode | frames (rows) | wall_ms median / p99 | physics_ms median / p99 | rate | awake last | working set MB |
|---|---|---|---|---|---|---|
| unlock-physics-pit-2000 | 24492 | 0.090 / 3.126 | 0.089 / 3.123 | 4898 steps/s (81.6x), not_one_step=0 | 0 | 169 |
| unlock-physics-mixed (6 runs) | 779-5438 | 1.7 / 6.0 (first run) | 0.15-1.5 median | 156, 207, 462, 489, 950, 1088 steps/s, not_one_step=0 | 2654-2830 | 151 |
| unlock-render-mixed (9 runs) | 408 (first) | 7-13 / 28-75 | 1.9-5.1 median | fps 24-112, steps/s 58-67 | 2804-2972 | 185-198 |
| scale-1000 physics | 301 | 16.666 / 16.792 | 1.54 / 5.69 | 60.00 fps | 987 | 151 |
| scale-1000 render | 301 | 16.667 / 16.696 | 1.62 / 5.31 | 60.00 fps | 993 | 170 |
| scale-4000 physics | 301 | 16.666 / 16.845 | 3.57 / 12.3 | 60.00 fps | 4000 | 157 |
| scale-4000 render | 309 | 16.654 / 41.1 | 4.15 / 21.3 | 61.7 fps | 3998 | 203 |
| scale-16000 physics | 220 | 21.9 / 41.4 | 21.8 / 41.3 | 43.9 fps | 16000 | 210 |
| scale-16000 render | 168 | 27.0 / 110.6 | 26.8 / 47.0 | 33.5 fps | 16000 | 308 |
| scale-32000 physics | 183 | 26.5 / 49.8 | 26.4 / 49.7 | ratio 0.61 | 32000 | 239 |
| scale-32000 render | 118 | 36.1 / 169.4 | 35.2 / 76.7 | ratio 0.39 | 32000 | 521 |
| scale-64000 physics | 163 | 30.3 / 49.6 | 29.9 / 45.3 | ratio 0.54 | 64000 | 306 |
| scale-64000 render | 177 | 24.8 / 109.2 | 24.7 / 49.1 | ratio 0.59 | 64000 | 824 |
| scale-unlock-1000 physics | 35209 | 0.09 | 0.067 / 2.6 | 7042 steps/s (117x) | 0 | 168 |
| scale-unlock-4000 physics | 1142 | - | 3.43 / 14.2 | 228 steps/s (3.8x) | 3300 | 162 |
| scale-unlock-16000 physics | 235 | - | 18.4 / 41.1 | 46.9 steps/s (0.78x) | 16000 | 204 |
- No crash, NaN or out-of-memory up to N=64000 in these runs; `bodies_outside_pit=0` and `bodies_below_floor=0` in every scale run. The 64000 physics median is lower than the 32000 one because the 268 m column of 64000 bodies is still falling and little simulated time has passed (163 steps); that is not a capacity result.
- `unlock-render-mixed` was very unstable between runs here (fps 24 to 112; some runs show `wall_ms` clustered at 66-68 ms, about 4 x 16.7 ms). The bad runs had a Godot process running at the same time and the one run without one gave 112 fps (n=1), so contention is a hypothesis, not proved. One early `unlock-physics-mixed` run had 207 steps/s while its in-loop sum gave 499; later runs show `gap_between_batches_ms_total` of 86-170 ms and no such gap (cause of that run unknown).
- Done 2026-11-07: one full 200 Hz protocol run (JIT and AOT, 3 repeats, SPEC window lengths, midpoint stages), one desktop machine, results not committed. Not done: Steam Deck / Proton (no Deck available), a second run to measure the run-to-run spread of the ladder stops.

## Native AOT variant (2026-11-06, one machine, Stride 4.4.1, .NET SDK 10.0.403 / runtime 10.0.14, short A/B runs; not SPEC runs)
The default build is unchanged (`dotnet build -c Release`, JIT). The AOT build is an extra variant selected by `-p:BenchAot=true` (the csproj sets `PublishAot=true` only then).

### Build
- `dotnet publish -c Release -r win-x64 -p:BenchAot=true -o <dir>` (about 1 min). Output: a native `StrideBench.exe` (about 49.5 MB, no managed Stride/.NET dlls; only native dlls such as `d3dcompiler_47.dll`, `libbulletc.dll`, `SDL2.dll`) plus `data/db` (the asset DB; the same `StrideCompileAsset` target runs, and the 2230298-byte shader bundle is the same size as in the JIT build; the publish copies it). The folder also holds `.pdb`/`.xml` files copied from packages; without them it is about 76 MB (the JIT `bin/Release` build folder is about 698 MB without pdb/xml, 941 MB with; it is a build folder, not a publish).
- Prerequisites: the NuGet packs `microsoft.dotnet.ilcompiler` and `runtime.win-x64.microsoft.dotnet.ilcompiler` 10.0.14 were already in the local NuGet cache (nothing downloaded); Visual Studio 2022 with the C++ desktop workload (MSVC `link.exe`). The ILC link step runs `vswhere.exe` through `cmd`; the first publish failed with "'vswhere.exe' is not recognized" (link exit code 123) from a shell without the Visual Studio Installer folder on `PATH`. Fix: put the folder that holds `vswhere.exe` (`Microsoft Visual Studio\Installer`) on `PATH` for the publish command; no project change needed.
- One source change was needed: `[DynamicDependency(Interfaces, typeof(PhysicsTimerComponent))]` on `Program.Main` (no effect in the JIT build). Proved by bisecting in a scratch copy: Stride's `ProcessorManager.IntroduceComponent` (IL read from the 4.4.1 package with a reflection dump) finds the plugin's `ISimulationUpdate.SimUpdateProcessor` through `component.GetType().GetInterfaces()`, the `IComponent<TProcessor,TThis>` type arguments and `Activator.CreateInstance`. Without a reachable use of the component's interface list ILC drops it: the AOT exe ran and the world stepped (bodies fell and slept), but `PhysicsTimerComponent` was never called (`tick_frames=0`, `physics_ms` 0). With `typeof(PhysicsTimerComponent).GetInterfaces()` reachable, or with the attribute, `tick_frames=121 of 121`. `TrimmerRootAssembly` for `StrideBench` and `Stride.BepuPhysics` did NOT fix it and is not needed; no rd.xml.
- Any custom component that implements a plugin interface of this kind (`IComponent<,>` flexible processing) needs the same attribute under AOT, otherwise it is silently not processed.
- The reflection in `BenchGame.UnpacedLoop` (`typeof(BepuSimulation).GetMethod("Update", ...)` + `CreateDelegate`) works unchanged: `unlock-physics-mixed` ran under AOT (`tick_frames=742 of 742` in the smoke run), no ILC warning for it.
- ILC output: by default only per-assembly lines (IL2104 x5, IL3053 x7: SharpDX, SharpDX.DirectInput, Stride.BepuPhysics, System.ComponentModel.TypeConverter, System.Formats.Nrbf, System.Private.Windows.Core, System.Windows.Forms and its Primitives; IL3000/IL3002 x4 each from Silk.NET.Core and Microsoft.Extensions.DependencyModel). With `-p:TrimmerSingleWarn=false` (scratch copy): IL2026 x44, IL3050 x11, IL3051 x17, IL2067/2070/2072/2057/2090/2094 a few each; nearly all in System.Windows.Forms, SharpDX, System.Private.Windows.Core. The one Stride-owned warning is IL3050 in `BepuSimulation.Elider.AddToHandlers` (`Type.MakeGenericType`, BepuSimulation.cs:927); it works for our class-typed component (reference-type instantiations share canonical code); a struct-typed instantiation was not tested. No warnings are reported for Stride.Core/Engine (not annotated), so their absence is no evidence of AOT safety; only the runs below are.
- Proof that it is native (`Process.Modules` of the running AOT exe, mixed render): none of `coreclr.dll`, `clrjit.dll`, `hostfxr.dll`, `hostpolicy.dll`, `System.Private.CoreLib.dll` is loaded (103 modules); the JIT exe loads all of them (178 modules).
- Rendering under AOT was checked by screenshot (mixed render, shaders from the DB): same picture as the JIT exe. Not run under AOT: the 20 s SPEC windows, chains, raycast-*, scale, unlock-render-mixed, Proton/Deck.
- Stride's own status (second hand: one web search result, not checked in the docs): the 4.4 release notes and docs claim a NativeAOT/trimming-friendly engine (PR 3204). Our runs agree for this benchmark's code paths.

### A/B, AOT vs JIT (ABAB, one process at a time, `--warmup=5 --measure=10`, 3 runs each; tasklist check for other QuestBench/StrideBench processes before every run)
AOT exe = publish of this folder with `-p:BenchAot=true`; JIT exe = `bin/Release/net10.0-windows/StrideBench.exe` (`dotnet build -c Release`). Source: the `*.summary.txt` of each run and the wall time of each process (scratch, deleted after the session). "launch+exit" = process wall time minus 15 s, so it includes startup, scene build and shutdown; it is not window-to-first-frame.
| scenario, mode | variant | launch+exit s | first-120 max wall ms | wall p99 ms | physics median ms | physics p99 ms | GC pause total ms | working set MB |
|---|---|---|---|---|---|---|---|---|
| mixed physics | AOT | 0.97, 0.79, 0.97 | 16.73, 16.68, 16.68 | 16.68 x3 | 1.43, 1.43, 1.44 | 1.97, 1.94, 1.81 | 0 | 97-98 |
| mixed physics | JIT | 1.59, 1.59, 1.61 | 16.68, 16.69, 17.25 | 16.68, 16.68, 16.70 | 1.12, 1.12, 1.55 | 1.86, 1.57, 2.24 | 0 | 144-148 |
| mixed render | AOT | 1.00, 1.01, 0.86 | 16.68 x3 | 16.72, 16.68, 16.68 | 1.97, 1.50, 1.50 | 2.62, 1.87, 2.06 | 0 | 127-132 |
| mixed render | JIT | 1.63, 1.83, 1.84 | 20.15, 19.42, 17.81 | 16.80, 16.68, 16.68 | 1.35, 1.27, 1.21 | 2.11, 3.00, 2.36 | 0 | 179-191 |
| pit-2000 physics | AOT | 0.97, 0.96, 0.97 | 16.68, 16.70, 16.68 | 16.68 x3 | 1.61, 1.60, 1.62 | 2.07, 2.01, 2.21 | 0 | 98-99 |
| pit-2000 physics | JIT | 1.57, 1.57, 1.59 | 16.68, 16.69, 16.72 | 16.68 x3 | 1.26, 1.27, 1.41 | 1.91, 1.95, 1.99 | 0 | 147-148 |
| pit-2000 render | AOT | 1.00, 1.00, 0.81 | 16.68, 16.69, 16.69 | 16.68 x3 | 1.65, 1.67, 1.67 | 2.14, 2.14, 2.23 | 4.86, 5.16, 5.06 | 126 |
| pit-2000 render | JIT | 1.63, 1.66, 1.64 | 16.69, 16.86, 16.68 | 16.69, 16.67, 16.69 | 1.32, 1.37, 1.48 | 2.08, 1.97, 2.21 | 0 | 176-177 |
| unlock-physics-mixed | AOT | 0.98, 0.98, 0.98 | 0.99, 0.57, 0.90 | 0.81, 0.79, 0.76 | 0.100, 0.096, 0.098 | 0.26, 0.29, 0.24 | 4.4, 4.2, 4.3 | 109 |
| unlock-physics-mixed | JIT | 1.80, 1.79, 1.80 | 4.37, 1.06, 5.51 | 0.93, 0.80, 0.90 | 0.092 x3 | 0.57, 0.48, 0.55 | 0 | 167-171 |
unlock-physics-mixed steps per second: AOT 1936, 1991, 1940; JIT 2513, 2476, 2567 (the pile sleeps early in unpaced runs, see the unlock reading caveat above).
Exe sizes: AOT exe 49.5 MB (folder 76 MB without pdb/xml); JIT exe 162 KB plus dlls (folder 698 MB without pdb/xml).

Reading (this machine, n=3 per cell, short runs, these scenarios only):
- Proved: launch+exit is 0.6-0.8 s shorter with AOT in every scenario (ranges do not overlap); working set is about 45-50 MB lower (physics) and 50-55 MB lower (render); the first-120-frames max wall in mixed render is 16.7 ms with AOT versus 17.8-20.1 ms with JIT (no overlap; the only scenario where a JIT first-run hitch shows).
- Proved: paced wall p99 is 16.7 ms for both in all but one run; the differences are inside the run-to-run spread, so the paced loop hides any step-cost difference.
- Per-step physics cost: in this ABAB set AOT was higher (pit-2000 physics median 1.60-1.62 vs 1.26-1.41 ms, render 1.65-1.67 vs 1.32-1.48, mixed render 1.50-1.97 vs 1.21-1.35; mixed physics overlaps, 1.43 vs 1.12-1.55), and unpaced AOT did about 22 percent fewer steps per second (1936-1991 vs 2476-2567). This is NOT a stable result: a later re-run (same machine, minutes later, `.tmp` scratch, pit-2000 physics and unlock-physics-mixed, ABAB 3 runs each, AOT built with `IlcInstructionSet=avx2` in a scratch copy) gave JIT pit-2000 physics medians 2.52, 1.50, 1.48 ms (so JIT drifted above its earlier 1.26-1.41), AOT(avx2) 1.41, 1.48, 1.44, and unpaced steps/s AOT(avx2) 2065, 2136, 2181 vs JIT 2287, 1944, 2670. So between sessions the JIT numbers moved by more than the AOT-JIT gap; AOT being slower per step is a suspected effect of 10-25 percent, not established. Whether AVX2 helps AOT is not decided by n=3 here; since 2026-11-06 the csproj sets `IlcInstructionSet=avx2` for the `BenchAot` variant by user decision (not yet re-run with the committed setting; the AOT binary needs a CPU with AVX2; the default ILC instruction set is baseline x64; `IlcInstructionSet` accepts names like `avx2`; `x86-x64-v3`, `bmi1`, `fma` were rejected by ILC 10.0.14 as unrecognized). The cause of any gap is not investigated (hypothesis: no tiered PGO in AOT).
- GC pause total: 4-5 ms in AOT in pit-2000 render and unlock-physics-mixed, 0 in JIT in the same runs (3 of 3 each); other AOT runs 0. Cause not investigated.
- Not covered: 20 s SPEC windows, other scenarios, Deck/Proton, ReadyToRun/TieredPGO JIT variants (not needed since AOT works).

## Scenario G (`debris`, `debris-pooled`) (2026-11-06, Stride 4.4.1 + Stride.BepuPhysics 4.4.1, JIT `dotnet build -c Release` and Native AOT publish; smoke runs only)
### Implementation (`BenchGame.cs`: `BuildDebrisWorld`, `DebrisTick`, `SpawnEvent`, `Despawn`, `AppendDebrisSummary`; `Options.cs`: `--rate`)
- CLI: `--scenario=debris --rate=R --mode=physics|render` and `--scenario=debris-pooled --rate=R ...`. The SPEC names no CLI args; `--rate` is my choice and must be matched by tools/ and the Godot side. File token: `debris-<R>` / `debris-pooled-<R>` (R printed as `8`, `1.5`), so `stride-bepu-debris-8-physics-1.csv`. `--warmup` defaults to 10 s (SPEC G), `--measure` 20 s. `--unpaced` is rejected.
- World: Scenario A pit via `BuildPit(22, 12, 0, rng, 0)` (floor + 4 walls, no pit bodies) plus 25 static blocks 2 x 2 x 2 (5 x 5 grid, spacing 3, centred, box centre y = 1), each a `StaticComponent` with a one-child `CompoundCollider(BoxCollider)`.
- Event: simulated time = Bepu steps counted by the `PhysicsTimerComponent` / 60 when written, now / 200 (see "200 Hz physics") (events follow simulated time, not wall time; when the loop lags, events lag with it). Event k is due at k / R s (first at t = 0). Slot order: seeded permutation of the 25 blocks (`Random(12345)`, Fisher-Yates, reshuffled after each 25 draws). The event removes the block entity from the scene (`Entities.Remove`, the StaticComponent is detached from Bepu) if that slot is still intact, then creates 64 pieces (4 x 4 x 4 grid of edge 0.5, mass 0.125, friction 0.5, simulation damping 0.1), each with velocity = unit radial direction from the block centre x 1.5 m/s x (1 +- 20 percent, seeded), i.e. impulse 1.5 x mass x factor, and a seeded spin uniform in [-3, 3] rad/s per axis (the SPEC gives no spin magnitude: my choice). Velocity is set with `BodyComponent.LinearVelocity` / `AngularVelocity` right after `Entities.Add`.
- Lifetime: 8 s of simulated time; all 64 pieces of an event are removed in the Update where `simT >= event time + 8` (`Entities.Remove` per piece). Removal runs before the event in the same Update.
- Block rebuild rule (my reading of the SPEC): a slot's block is rebuilt (new static entity) when the last live event of that slot is removed. The SPEC says "a block already used is rebuilt after its debris is removed, so the supply never runs out", which cannot hold literally for R above about 3 (25 blocks, 8 s lifetime). Here an event on a slot whose block is not intact still spawns its 64 pieces at that place (overlapping older debris, Bepu pushes them apart) but removes no block, so at high R `event_ms` is mostly piece creation. Summary line `blocks_removed / blocks_rebuilt / blocks_intact_at_end` shows this per run (R=8 and R=32: 25 removed, 0 rebuilt; R=1: 21 removed, 13 rebuilt). Rebuild time is counted in `despawn_ms`. The Godot side may have chosen differently; compare before reading event_ms across engines.
- `event_ms` = stopwatch around block removal + creation of the 64 pieces (+ velocity set) in the Update, 0 otherwise; `despawn_ms` = stopwatch around the removals of that Update (+ block rebuild), 0 otherwise; `debris_live` = pieces alive. CSV: the 9 standard columns, then `event_ms,despawn_ms,debris_live` at the end (only in debris scenarios). Row = one Update (as everywhere); `wall_ms` of a row is the time since the previous Update started, so an event's cost lands in the NEXT row's `wall_ms`. The summary therefore prints wall_ms for event frames and for the frame after, removal frames and non-event frames.
- Summary lines added: events completed in the measured window (and incl. warm-up), `event_ms` / `despawn_ms` median p95 p99 max, wall_ms stats per frame class, `debris_live` max / mean, `awake_bodies` min / max, blocks counters, `bepu_live_bodies_at_end` (sum of `Bodies.Sets[i].Count`), `bepu_statics_at_end`, `scene_entities_at_end`, pool counters. Working set at the end is the existing `working_set_mb` line.
- Render mode: pieces use the shared 1 m unit box `Model` (same as the blocks and other static boxes) with entity scale 0.5 and one shared `Material`; no per-piece mesh or material creation. The `ModelComponent` is added with the entity, so it is inside `event_ms`.
- `debris-pooled`: `ceil(R x 8) + 2` events' worth of pieces (64 each) are created at start and parked asleep at y <= -1000 (`Awake = false`), still registered in Bepu; an event pops 64, then `Teleport`, `Awake = true`, velocities; removal zeroes velocities, `Teleport`s them back below y = -1000 and puts them to sleep. The pool grows on demand (`pool_grown` in the summary, 0 in the smoke runs). Blocks are not pooled. Caveat: parked bodies stay in the simulation (asleep), so this is reuse of bodies, not removal from Bepu.
- `PresizeSimulation` has a `debris` case (only used with `--presize=1`).

### Proved (smoke runs on one desktop, 10 s warm-up + 10 s measured unless noted; another agent may have run at the same time, so NO timing here is a result)
- Starts and finishes in physics and render, JIT and AOT: debris R=8 physics (JIT and AOT), R=1 physics and R=2 pooled physics (JIT), R=4 render (JIT), pooled R=4 render (AOT), R=32 physics 10 + 20 s (JIT). The CSV header ends `...,awake_bodies,event_ms,despawn_ms,debris_live` (checked with `head`, JIT and AOT).
- `debris_live` is bounded and equals R x 64 x 8 in steady state: R=1 512, R=2 1024, R=4 2048, R=8 4096, R=32 16384 (max = mean in the measured window; R=32: max over all 740 CSV rows 16384).
- Bodies leave the simulation on removal: at the end of the non-pooled runs `bepu_live_bodies_at_end` equals `debris_live` (4096 at R=8, 2048 at R=4, 16384 at R=32) and `bepu_statics_at_end` is 5 (the pit) when no block is intact (R=8, R=4, R=32), 22 at R=1 with 17 intact blocks. This is a count from Bepu's body sets, not a memory measurement. Pooled runs: `bepu_live_bodies_at_end` = pool size (1152 at R=2, 2176 at R=4), parked ones asleep.
- Impulses take effect (temporary debug print, removed): piece 63 of the first event spawned at (6.75, 1.75, 3.75) around the centre (6, 1, 3) with velocity (0.72, 0.72, 0.72) and was at (7.87, 1.75, 4.91) after 120 steps. The pieces slide outward as stacks on the floor (the grid starts as a 4-high stack resting on the floor, so downward components are blocked).
- Working set at the end: R=8 physics JIT 171 MB, AOT 121 MB; R=32 physics JIT 246 MB with 16384 live bodies; R=1 143 MB. Runs are 20 to 30 s of wall time, so a slow leak over minutes is NOT excluded (not run).
- Native AOT: `dotnet publish -c Release -r win-x64 -p:BenchAot=true` (VS Installer folder on PATH for `vswhere.exe`) succeeded; debris R=8 physics and pooled R=4 render ran with `tick_frames=601 of 601` and the same counters as JIT. No new component type implementing a plugin interface was added (only `BodyComponent` / `StaticComponent` created in code), so no new `[DynamicDependency]` was needed.

### Hypotheses / not checked
- Debris stays awake: `awake_bodies` min is within a few of `debris_live` at R >= 2 (R=1: 361 to 487 of 512, some sleep). Probably new events keep overlapping and waking older debris; not investigated.
- Pooled removal looked slower than plain removal in the noisy smoke runs (`despawn_ms` median 2.7 to 3.8 ms vs 0.2 to 0.5); the cost of `Teleport` + sleep per piece is a suspicion only, timings not comparable.
- R=32 lagged in one run (740 of 1200 expected frames in 20 s, ratio 0.62); one run, other load unknown, not a result.
- Piece orientation is identity (SPEC does not ask for random orientation).
- Not run: SPEC 20 s windows per level with 3 repeats, the ladder search, render mode at high R, AOT at other rates, a minutes-long leak check, Steam Deck.

## Render check of every scenario and the camera (2026-11-06, one machine, Stride 4.4.1, JIT `dotnet build -c Release` and Native AOT publish; screenshots in the session scratch folder, not tracked)
Method: render mode, window screenshot from outside the process (screen copy of the client area) about 5 to 45 s into the run, viewed by eye. One screenshot per cell, so "content shown" means: floor, walls, bodies, chains as expected for that scenario, not a blank window. Other windows (the Godot benchmark) partly covered some shots; they did not affect the Stride content.
- Covered, JIT and AOT both: pit-2000, raycast-1000, raycast-10000, chains, mixed, unlock-render-mixed, scale N = 1000, 4000, 16000, 64000 (paced, shot at 6, 8, 20, 45 s), scale N = 1000 and 16000 with `--uncapped=1` (unpaced render; `--unpaced=1` is physics only), debris and debris-pooled R = 1, 8, 32 (shot at 9 s). 36 shots. Not covered: the physics-mode scenarios (no draw by design; the window is 320 x 180 and not looked at), scale other N, other times in a run.
- Result: no gray, black or empty window in any of the 36. The background is the compositor's blue clear colour, the lit surfaces are orange. JIT and AOT pictures match scenario by scenario (same content, same layout).
- Raycast scenes draw only the 2000 static shapes (a cloud, no floor or walls) and no ray lines: rays are not rendered (by design, no line meshes). The camera sits just outside the cloud (z = 38 versus cloud z in -20..20), shapes fill the frame.
- Poor camera, not a rendering bug: the camera is one fixed pose for every scenario and does not follow the pit size. Scale N = 16000: pit inner 64 m, wall height 35 m, north wall at z = 32..33, camera at z = 38, y = 18, so the view is filled by the outside of the wall (full orange screenshot, JIT and AOT, paced and uncapped). N = 64000: pit inner 124 m, wall 134 m high, camera inside the pit; the view shows the floor with bodies and the wall behind it, not the whole pit. N = 4000 shows the pit but the wall is wider than the frame. The physics and the draw submission are unaffected (the entities are still drawn); only what a viewer sees changes. Cameras were NOT changed (a shared definition with Godot is to be decided).
- Chains: small in the frame (ceiling bar slab 11 x 0.2 x 11 at y = 12.1, chains below it), visible.

(Superseded 2026-11-06: the fixed pose (0, 18, 38), FOV 45 and far 200 described above and the poor-camera paragraph are replaced by the shared camera rule below.)

## Shared camera rule, profiles, 200 Hz, unlocked loop, settings parity (2026-11-06, Stride 4.4.1 + Stride.BepuPhysics 4.4.1, one machine, smoke runs only, NO timing claims)

### Camera (`BenchGame.PlaceCamera`, `SetupCameraAndLight`; flag `--camera=content|sky`)
- Perspective, vertical FOV 60, near 0.1, far 2000, 1280 x 720. Content sphere (c, R): c = centre of the content AABB, R = half its diagonal. Camera at c + u x D, u = normalize(0, 1.5, 0.9), D = 1.15 R / sin 30, looking at c, up +Y. `sky` = same position, looking straight up. Stride looks down -Z, so the pitch is `Quaternion.RotationX(-atan2(1.5, 0.9))`.
- AABB definition (same as Godot `quest_bench.cpp`): every static box (centre +- size/2, `AddStaticBox`) plus the analytic spawn volume of the dynamic bodies (pit grid: x, z within +-((cols - 1)/2 x 0.6 + 0.3), y from 0.7 to 1 + (layers - 1) x 0.6 + 0.3), raycast cloud (-20.5, -0.5, -20.5)..(20.5, 20.5, 20.5), chains links (-4.65, 7, -4.65)..(4.65, 12, 4.65). Bounds are computed in physics mode too and printed (no camera is created there).
- Ceiling changed from 10.5 x 0.1 x 10.5 at y 12.05 to 11 x 0.2 x 11 at y 12.1 (bottom face still y = 12, joint anchor y -0.1) to match Godot.
- env.txt keys: `camera_mode`, `camera_position`, `camera_look_at`, `camera_fov_v_deg`, `camera_near`, `camera_far`, `camera_content_radius`, `camera_content_aabb`.
- Proved (read from env.txt, physics smoke): radius pit-2000 17.706, raycast-1000 30.834, chains 8.201, mixed 24.931, scale-1000 17.328, scale-64000 161.994, debris-8 17.328. Godot env.txt files exist for pit-2000 (17.706, same AABB) and raycast-1000 (30.834, same AABB): identical to Stride. Not compared against a Godot env.txt: chains, mixed, scale, debris (the Godot source uses the same box definition; not checked number by number).
- Proved by screenshots (JIT, viewed): pit-2000, raycast-1000, chains, mixed, scale 1000/4000/16000/64000, debris-8, sky pit-2000 show the whole content; the pit fills about half of the frame height. In chains the ceiling slab hides the chains from this angle (only the bottom link ends show below the slab): consequence of the shared rule, not a bug. AOT screenshots were retaken after the final change (2026-11-06, `.tmp/render-fix6/aot-*.png`, all ten viewed: pit-2000, raycast-1000, chains, mixed, scale 1000/4000/16000/64000, debris-8, sky); the JIT set was taken at the same time.

### Profiles and flags (`Options.cs`, `BenchGame.cs`)
- `--profile=game|stress` (default game): game = Bepu default sleeping, directional light with shadows (`LightShadowMapSize.XLarge` hint, 4 cascades, `BenchGame.cs:213-214`), MSAA 4x (`ForwardRenderer.MSAALevel`, line 230), bodies and statics `IsShadowCaster` (line 259); stress = same, sleeping off (`SleepThreshold = -1`, lines 278, 399, 571). Chains (C) and debris (G) keep default sleeping in both profiles. Separate flags win over the profile: `--sleeping=default|off`, `--shadows=on|off`, `--msaa=1|2|4|8`, `--vsync=on|off`, `--physics-hz=N`.
- File names get a `stress` token only for the stress profile (`stride-bepu-stress-pit-2000-physics-1.csv`); merge scripts on the tools side must accept it.
- Proved (windowed physics mode, pit-2000, 5 s warm-up + 30 s measured, one run each, JIT): stress `awake_bodies` is 2000 in every sampled row and at the end; game falls from 1999 at the start to 1687 after about 12 s, then to 38 within the next 2 s of sampling and 0 at the end (CSV column awake_bodies). A 3 s measured run is too short to show it.

### 200 Hz physics (all scenarios and profiles)
- `BepuSimulation.FixedTimeStep` = 50000 ticks = 0.005 s (`BenchGame.cs:124`), `MaxStepPerFrame = 64` (line 125; the plugin default 3 would drop steps at 200 Hz). Steps per 60 Hz frame: 3 or 4 (proved, AOT smoke: min 3, max 4, mean 3.33, `steps_per_s` 200.0 and 200.7).
- Unpaced reflection call passes 0.005 s. Debris simulated time = steps / 200.
- CSV columns appended at the END: `physics_step_ms`, `physics_frame_ms`, `steps_per_frame`; the summary has the same statistics plus `steps_dropped_measured_window` (signed; the AOT smoke gave -0, 0 and 0 in the whole run, and 1 in a second run: the accumulator remainder) and `steps_dropped_whole_run`. A value above a few steps would mean discarded time.
- Physics mode is unlocked too (SPEC "Frame rate"): `IsFixedTimeStep = false` in every mode, no frame cap, so rows are frames with 0 or 1 steps. Proved (raycast-1000, windowed, 6 s): 16537 frames, steps_per_frame 0..1, steps_per_s 199.97, 0 dropped. Do NOT combine this with `--hidden=1`: with a hidden window the same run made only 91 frames in 6 s (about 15 fps, 13-14 steps per frame, still 200.0 steps/s); the cause is not investigated (hypothesis: Stride throttles a hidden window). The run scripts must not pass `--hidden` in physics mode.

### Unlocked render loop
- Render mode: `IsFixedTimeStep = false`, no `TargetElapsedTime`, `InactiveSleepTime = 0`, vsync off in both profiles (`SynchronizeWithVerticalRetrace`, line 84), env `vsync: off`, `max_fps: 0`. Steps per frame vary with the frame time. `--uncapped` is kept as a no-op. Proved (scale-1000 render, 5 s): 566 frames = 113 fps, steps_per_frame 1..7, 0 dropped.

### Rays per physics step
The ray batch (B, D) runs once per Bepu step (`PhysicsTimerComponent.OnStep`, called after the measured step interval, so `physics_ms` is unchanged; `BenchGame.OnStep`). `query_ms` of a row = sum over the steps of that row. Summary: `rays_per_step` and `rays_cast_measured_window` = rays_per_step x steps in the window (proved: raycast-1000 1000 x 1213 = 1213000; mixed 1000 x 1237 = 1237000; unlock-physics-mixed 1000 x 586 = 586000). `ray_hits_total` is the sum over all rays cast.

### Light and material brightness fit
Material albedo is white 1.0 (was orange 0.8/0.45/0.2; Godot uses its white default material). Directional light intensity 0.55 and ambient 0.3 (flags `--sun-intensity`, `--ambient-intensity`; env keys `light_intensity`, `ambient_intensity`). Method: window screenshots of the same scene from both engines, mean luminance (0.2126 R + 0.7152 G + 0.0722 B of the sRGB bytes) over the pixels that differ from the background corner pixel (sum of channel differences above 24), every second pixel. Godot (`.tmp/render-fix4`, 1280 x 720): pit-2000 197.06, mixed 203.34, scale-1000 199.06; content fraction 0.156 / 0.145 / 0.173; std of content luminance 25.1 / 22.4 / 24.5. Stride with the final values (AOT, 1600 x 900 screenshot): pit-2000 197.34, mixed 200.38, scale-1000 197.71; content fraction 0.156 / 0.147 / 0.174; std 27.5 / 28.2 / 27.9; whole-image mean 153.1 / 153.1 / 154.1 against Godot 153.1 / 153.4 / 154.3. Earlier trials (pit-2000, one shot each): sun 3.0 / ambient 0.5 saturated (content 240); sun 0.0 / ambient 0.5 gave 188.7, sun 0.15 / 0.5 gave 199.5 but std only 7.4 (flat); sun 0.55 / ambient 0.3 gave 197.3, std 27.5. Limits: one screenshot per cell, Godot shots were taken by the other agent at a different time in the run (different pile state), the fit matches mean and spread only, not per-face values; the ambient is therefore 0.3 and not the Godot 0.5 (the SPEC row "ambient equal" no longer holds). Tone mapping of both engines not read.

### Settings parity (Stride column; env.txt prints each effective value with the Godot key names)
| setting | Stride value | source | deviation |
|---|---|---|---|
| frame loop | unlocked in render and physics mode (`max_fps: 0`) | `BenchGame.cs` constructor | none (Godot unlocked too) |
| rays | once per physics step | `OnStep` | none |
| physics rate | 200 Hz | `BenchGame.cs:124` | none |
| solver iterations / substeps | `BepuSimulation.SolverIteration` 8, `SolverSubStep` 1 (read at run time, env `solver_iterations`, `solver_substeps`) | plugin defaults, not changed | Bepu solver differs from Jolt's; the numbers are not equivalent (Jolt side values not read here) |
| friction | 0.5 on every body and static | `BenchGame.cs:269-571` | Bepu combines friction per pair, Jolt by its own rule; not compared |
| restitution | none (Bepu has no restitution; Godot bounce 0) | n/a | deviation: no equivalent setting |
| damping | `PoseLinearDamping = PoseAngularDamping = 0.1` (line 122) | | formulas differ (HYPOTHESIS from memory, not read from source in this session: Bepu multiplies velocity by `(1 - d)^dt`, Jolt uses `1 / (1 + d dt)`); same number, possibly not the same decay; not quantified |
| sleeping | profile-dependent, see above | | Bepu sleep rules differ from Jolt's |
| shadows | 4 cascades, `XLarge` hint, shadows on | lines 213-214 | actual texel size not read (hint only); `Shadow.Filter` printed empty (null), meaning unverified unfiltered; Godot filter differs |
| MSAA | 4x | line 230 | none known |
| vsync | off | line 84 | none |
| tone mapping / exposure | Stride default (not changed, not read) | | deviation: Godot uses its own tonemapper; not equalised |
| light / ambient | sun 0.55, ambient 0.3, white material albedo 1.0 (fitted, see "Light and material brightness fit") | `Options.cs` `SunIntensity`, `AmbientIntensity` | Godot 1.0 / 0.5; units differ; matched by mean image luminance (Stride 197.3 vs Godot 197.1 on pit-2000) |
| threads | Bepu default thread count (printed in env.txt) | | not equal to Jolt's job-system threads |

### "Empty scene" report for pit-2000 (2026-11-06, one machine, smoke only)
- PROVED (screenshots, non-background pixel fraction): in `--mode=render`, JIT and AOT, default flags, `--profile=game` and `--profile=stress`, `--camera=content`, pit-2000 draws content (fraction about 0.156, at 2 s and at 12 s). `--camera=sky` draws background only (fraction 0.0000, by design). All other scenarios pass the same check on AOT (see below); the JIT matrix covered pit-2000 variants.
- PROVED: the default `--mode` is `physics`, whose window draws nothing by design (a default run `--scenario=pit-2000` gave an entirely black window, mean 0). Godot's default is physics too.
- HYPOTHESIS (not checkable): the user ran without `--mode=render`, or an intermediate build from during the camera edits. The exact build they saw is unknown.
- Change: in physics mode the window title now says nothing is drawn and a console line says to use `--mode=render`. The default mode is unchanged (run scripts pass `--mode` explicitly).

### `render-smoke.ps1` (render sanity check)
- `powershell -File render-smoke.ps1 -Exe <StrideBench.exe> [-OutDir .tmp/render-smoke] [-Quick] [-Keep]`. Runs render mode on pit-2000, chains, mixed and sky (`-Quick`), plus raycast-1000/10000, unlock-render-mixed, pit-2000-stress, scale 1000/4000/16000/64000, debris-8, debris-pooled-8; screenshots the window and computes the non-background pixel fraction (background = corner pixel, channel-difference sum above 24).
- Pass: content fraction at least 0.02 (sky: at most 0.01, and a black background fails). Exit code 1 on failure.
- PROVED: AOT full run 14 of 14 PASS (fractions 0.045 to 0.179, sky 0.0000; lowest: raycast 0.047, scale-64000 0.059). The negative path failed correctly (a copy with pit-2000 forced to `--camera=sky` gave FAIL, fraction 0.0000). Limits: one machine, one shot per scenario, window must be visible and unobscured.

### Notes added with the follow-up
- Flags `--sun-intensity` (default 0.55) and `--ambient-intensity` (default 0.3).
- Do not combine `--hidden` with physics mode: a hidden window ran about 15 fps (91 frames in 6 s, steps still 200.0/s); windowed ran 16537 frames in 6 s. Cause not investigated (hypothesis: Stride throttles a hidden window).
- 30 s windowed pit-2000, awake_bodies: game 1999 at start, 1687 near 12 s, about 38 near 14 s, 0 at the end; stress 2000 in every sampled row and at the end; `steps_per_s` 200.03 / 199.99.
- Open: chains scene is hidden by the ceiling slab from the camera rule; peak_steps_in_one_Update is 64 at startup in render runs; `parity_check.pl` was not run against a current Godot env.txt.
