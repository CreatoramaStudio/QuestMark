# QuestMark engine benchmark

Purpose: measure Godot (Jolt, optionally a PhysX fork) and Stride (Bepu) on the same physics-heavy scenes (A to G), so the engine choice in `docs/engine-choice.md` can rest on numbers. The rules are in [SPEC.md](SPEC.md); this file only says where things are and how to run them. Status: the full protocol (A to G, both profiles) ran once at 200 Hz on 2026-11-07 for all four variants (Godot Jolt, Godot PhysX fork, Stride Bepu JIT and AOT), on one desktop machine; the results are not committed and no verdict is drawn from them yet. Nothing has run on a Steam Deck, and none is available (godot and stride NOTES, "not done" lists).

## Layout
- `godot/`: Godot project; the measured logic is a C++ GDExtension in `src/` (godot-cpp), `legacy/` holds the old GDScript. Notes: [godot/NOTES.md](godot/NOTES.md). PhysX fork port: [godot/PHYSX.md](godot/PHYSX.md), patch in `godot/patches/`.
- `stride/`: Stride 4.4.1 + Bepu solution (C#, .NET 10, scenes in code). Notes: [stride/NOTES.md](stride/NOTES.md). `render-smoke.ps1` is the render check.
- `tools/`: `build-godot-ext.sh`, `export-godot-fork.sh`, `run-all.sh`, `merge-results.pl`, `parity_check.pl`, `build-progress.sh` (scons fork builds), `ext-build-progress.sh` (extension build), `bench-progress.sh` (benchmark run). Paths given to the tools may be relative to the project root.
- `results/`: output (CSV, `.summary.txt`, `.env.txt`, `SUMMARY.md`); `results/gdscript-old/` is the old GDScript run. Not ignored by git yet (see SPEC "Layout").
- Scratch files go in `.tmp/` at the project root only.

## Build
Only release builds give valid results; editor and debug runs do not.
- Godot extension: `GODOTCPP_DIR=<godot-cpp> GODOT_API_JSON=<extension_api.json> [PYTHON3=<python3>] COMPILER=clang|msvc bash tools/build-godot-ext.sh [template_debug|template_release|all]`. `GODOT_API_JSON` comes from `<godot exe> --headless --dump-extension-api`. Output: `godot/bin/<compiler>/`. Script default is msvc; the committed `bench.gdextension` selects `bin/clang/` (`SELECT_DEFAULT=1` rewrites it). Then export the "Windows Desktop" preset with the editor and matching export templates (godot NOTES, "C++ port", step 5).
  Progress: `bash tools/ext-build-progress.sh clang template_release watch` (compiled units / units in the vcxproj files, then "linked"). After the project folder moved, the script removes the stale CMake tree and rebuilds (clang about 14 min on the dev machine, 1092 units).
- Stride JIT: `dotnet build -c Release` in `stride/` (SPEC label `bepu`).
- Stride Native AOT: `dotnet publish -c Release -r win-x64 -p:BenchAot=true -o <dir>` (label `bepu_aot`). `IlcInstructionSet` is `avx2` in `stride/StrideBench.csproj` for this variant, so the exe needs a CPU with AVX2. The ILC link step needs `vswhere.exe` on `PATH` (the Visual Studio Installer folder; stride NOTES, "Native AOT variant").
- Godot PhysX fork: build the fork and the PhysX SDK as in [godot/PHYSX.md](godot/PHYSX.md), then `FORK_TEMPLATE=<fork template_release exe> FORK_EDITOR=<fork editor console exe> bash tools/export-godot-fork.sh [label]`. Optional `GODOT_EXT_DLL`, `EXPORT_ROOT` (default `.tmp/export-fork`). Output: `<EXPORT_ROOT>/<label>/QuestBench.exe`.

## Run
- Variants (one `run-all.sh` invocation each, strictly sequential): Godot Jolt (`BENCH_ENGINE=jolt`), Godot PhysX fork (`BENCH_ENGINE=physx` with `BENCH_FLAVOUR`), Stride Bepu JIT (`STRIDE_FLAVOR=bepu`), Stride Bepu Native AOT (`STRIDE_FLAVOR=bepu_aot`). GodotPhysics3D is not benchmarked.
- Planned runtime from `--dry-run` (stages ABCDEFG): about 57 to 58 min per variant, 138 runs (range 40 to 80 min); about 3 h 50 min for all four. The ladder capacities in the estimate are assumptions; setup and retries are not included.
- Set the exe paths and `OUT_DIR` in a gitignored local file (for example the `env` block of `.claude/settings.local.json`), never in tracked files. Re-export the Godot exe after any change to `benchmark/godot/src` or `project.godot`, and rebuild the Stride exes after source changes: stale exes were found once and would have produced 60 Hz data.
- Full protocol, one engine variant at a time, strictly sequential: `bash tools/run-all.sh godot|stride [--dry-run] [--stages=ABCDEFG]`. `--dry-run` (or `DRY_RUN=1`) prints every command and the planned runtime and runs nothing; do this first.
- Godot: `GODOT_BENCH_EXE=<QuestBench.exe>`, `BENCH_ENGINE=jolt|physx` (default jolt), optional `BENCH_FLAVOUR=<label>` (letters, digits, underscore). Files are named `godot-<engine>[-<flavour>][-stress]-<scenario>-<mode>-<run>`.
- Stride: `STRIDE_BENCH_EXE=<StrideBench.exe>`, `STRIDE_FLAVOR=bepu` (default, JIT) or `bepu_aot`.
- Other variables: `OUT_DIR`, `WARMUP`, `MEASURE`, `RUNS`; full list in the header of `tools/run-all.sh`.
- Progress: `run-all.sh` writes `OUT_DIR/progress-<prefix>.txt` after every cell (cells done / expected cells from the dry-run plan, elapsed, rough ETA, failures). Show it like the build bar with `bash tools/bench-progress.sh <OUT_DIR> watch` in a second shell or as a background task (refresh every 5 s, ends 2 min after "done"; a parent folder such as `.tmp/results-200hz` follows the newest variant). `BENCH_PROGRESS=1` additionally pins the bar on the last line of a real terminal (escape sequences checked with fake cells on a forced 24x80 terminal; not yet watched in a real one). The expected cell count moves with the ladders, so the bar stays below 100% until the run ends.
- Merge: `perl tools/merge-results.pl <results-dir> > SUMMARY.md`.
- Compare two env files key by key: `perl tools/parity_check.pl <a.env.txt> <b.env.txt>`.
- Render sanity check (Stride): `powershell -File stride/render-smoke.ps1 -Exe <StrideBench.exe> [-OutDir .tmp/render-smoke] [-Quick] [-Keep]`. The window must be visible and unobscured.

## Fixed settings (SPEC is the source of truth)
- Physics 200 Hz (dt 0.005 s), max 64 steps per frame, in every scenario, mode and profile. The render loop is unlocked (vsync off, no frame cap), so a frame carries a varying number of steps.
- Profiles: `--profile=game` (default sleeping) and `--profile=stress` (sleeping off); A, D, E, F run in both, B in game only, C and G always use default sleeping.
- Camera: one shared rule from the content bounding sphere (vertical FOV 60, near 0.1, far 2000), see SPEC "Camera".
- Results measured at 60 Hz (older runs, including `results/gdscript-old/`) are NOT comparable with 200 Hz results and must never be merged into one table.

## Gotchas
Each item is stated in the NOTES or SPEC named; PROVED there means measured on one machine.
- The default `--mode` is `physics` and its window draws nothing (PROVED, stride NOTES: black window). Use `--mode=render` to see a scene.
- CLI args need `=`: `--arg=value`. A space-separated form is silently ignored (PROVED, godot NOTES: `--warmup 2` got the defaults).
- Do not combine `--hidden` with Stride physics mode: a hidden window ran about 15 fps (PROVED, 6 s run, cause not investigated). The run scripts do not pass it.
- Godot unpaced runs need a real window (no `--headless`); paced physics runs use `--headless` and ran at about 144 fps there in one smoke (godot NOTES; cause not investigated).
- A PhysX request on a build without the module silently runs GodotPhysics (PROVED, 8 of 8 smoke cells). The engine proof is the class of the direct space state in env.txt (`physics_space_state_class`, `engine_tag`); `run-all.sh` aborts on a mismatch.
- The physx EDITOR exe of the earlier fork build lacked the module; PhysX ran only from the physx template_release exe (PROVED, [godot/PHYSX.md](godot/PHYSX.md)). The clang-built editor was not checked.
- Run on an idle machine with the benchmark windows visible. Other load changed results in several smoke runs (HYPOTHESIS for the cause in each case, stride and godot NOTES).
- One process at a time. Do not run two benchmark runs or builds of the same tree at once.
