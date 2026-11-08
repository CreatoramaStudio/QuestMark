# Godot PhysX fork: benchmark port and clang-cl build route

Scope of every statement: one machine, Windows x86_64, one Godot fork with a PhysX module (commit `c6f3b659`, 4.7.3 rc; the fork was rebuilt at `97bc4de` later, see "GPU use and fork heads"), the QuestBench GDExtension (`benchmark/godot`), SMOKE runs only (2 s warm-up, 8 s measured, 60 Hz physics, before the 200 Hz change). No timing from these runs is a result. Scenario G (debris) was not in the copy that was tested and was NOT run on PhysX. Sources: the port report and logs kept in the session scratch folder (`.tmp/physx-bench/REPORT.md`, `.tmp/physx-clang/`), not tracked.

Labels: PROVED = read from a file, log or run named in the line. HYPOTHESIS = not checked.

## What the port needed
- PROVED (report): benchmark scenarios A to F run on the PhysX backend with no change to the extension logic: bodies, raycasts, joints and the awake count work.
- Small patches against `benchmark/godot` were needed: result file prefix `godot-<engine>[-<flavour>]-` (`--flavour=<label>`), engine proof from the direct space state class, extra env lines (PhysX settings, Godot sleep thresholds), a `chain_check` summary line, `BENCH_ENGINE` / `BENCH_FLAVOUR` in `run-all.sh`, `tools/export-godot-fork.sh`. PROVED present in the tree: the `BENCH_ENGINE` / `BENCH_FLAVOUR` handling in `tools/run-all.sh`, `tools/export-godot-fork.sh`, and the engine-proof description in SPEC "Versions" and godot NOTES. Not re-checked one by one in `quest_bench.cpp`: the `chain_check` line and the extra env lines.

## Engine proof and silent fallback
- PROVED (report): the engine in use is shown only by the class of `get_world_3d()->get_direct_space_state()` (`GodotPhysXDirectSpaceState3D`, `JoltPhysicsDirectSpaceState3D`, `GodotPhysicsDirectSpaceState3D`). The project setting alone is not proof; `PhysicsServer3D::get_class()` is the same for all.
- PROVED (report, 8 of 8 smoke cells): requesting `PhysX` on a build without the module (the "nophysx" export) runs GodotPhysics with no warning. The same happened in the physx EDITOR exe.
- PROVED (report): the physx editor exe of the earlier build did not contain the module (exe size 167,810,048 bytes against 167,735,808 for nophysx; its API dump had no `PhysXCloth3D`). PhysX ran only from the physx `template_release` exe. HYPOTHESIS: the editor was built without the SDK path (`modules/godot_physx/config.py` returns early when `physx_sdk` / `PHYSX_SDK` is empty; I read that function, but not the build log of the earlier editor). Whether the clang-built physx editor (below) contains the module was NOT checked.
- `tools/run-all.sh` aborts a cell when the recorded `engine_tag` differs from `BENCH_ENGINE`, so a silent fallback cannot enter a result set.
- PROVED (report, `godot_physx_project_settings.cpp:42-46`): the physx template is CPU only (`gpu_dynamics` exists only under `GODOT_PHYSX_GPU`).

## Smoke findings (60 Hz, 2 s warm-up + 8 s, one run per cell)
- PROVED (report): 24 cells per flavour (physx and nophysx exports), all exit 0, CSVs well formed. Covered: A, B (1000 and 10000), C, D, E (`unlock-physics-mixed`, `unlock-physics-pit-2000`), F (scale 3000); windowed render of mixed on PhysX also exits 0.
- PROVED (report): raycast-1000 gives 80,808 hits on PhysX, Jolt and GodotPhysics alike. Chain links stay at y >= 7.2 on all three; the impulse moves the end links 0.11 to 0.16 m.
- PROVED (report), sleeping differs by engine, awake bodies at the end of the run:

| scenario (60 Hz smoke, one run) | PhysX | Jolt | GodotPhysics |
|---|---|---|---|
| pit-2000, of 2000 | 53 | 1936 | 1992 |
| scale-3000 | 113 | 2939 | not listed |
| mixed, of 3000 | 2871 | 2951 | not listed |

- PROVED (report): `unlock-physics-pit-2000` produced 41,708 frames on PhysX (awake 0 at the end), 1,578 on Jolt, 202 on GodotPhysics. The unpaced rate is dominated by the sleeping pile, so it is not a speed comparison.
- Consequence: any PhysX timing must be read next to `awake_bodies`. The `stress` profile (sleeping off) exists for this; a short 200 Hz profile smoke in godot NOTES recorded PhysX pit-2000 game 0 of 2000 awake and stress 2000 (that run is a different, shorter smoke than the 53 above).
- HYPOTHESIS (not checked): the sleep difference comes from sleep thresholds derived from Godot defaults (`godot_physx_space_3d.cpp:236`, `:357-379`). Jolt limit settings in `project.godot` have no effect on the other engines.

## Build route: PhysX SDK with clang-cl
The stock PhysX Windows build uses `generate_projects.bat` and Visual Studio project files with MSVC. The route used here bypasses `generate_projects.bat` and drives CMake directly on `physx/compiler/public` of the PhysX 5.10 clone. Sources: `.tmp/physx-clang/scripts/phase2-physx.bat`, `.tmp/physx-clang/0009-windows-clang-cl-flags.patch`, `.tmp/physx-clang/logs/physx.log`.
1. Apply `patches/0009-windows-clang-cl-flags.patch` in the PhysX clone (copy kept in this repo, 17 lines). It touches `physx/source/compiler/cmake/windows/CMakeLists.txt`: when the compiler id is `Clang` it uses `PHYSX_COMMON_FLAGS = /std:c++14 /W0 /GF /GS- /Gd ${PHYSX_FP_MODE} /Oy`. The MSVC-only flags `/d2Zi+` and `/MP` are dropped (rejected as unused arguments under `/WX`) and `/W4 /WX` is replaced by `/W0` without warnings-as-errors (clang's wider warning set would turn into errors). The MSVC branch is unchanged.
2. Environment: `vcvars64.bat` (VS 2022) for the Windows SDK and MSVC libraries, then LLVM `bin` and the Ninja that ships with the VS CMake folder on `PATH`.
3. Configure: `cmake -S <physx>/compiler/public -B <build> -G "Ninja Multi-Config"` with
   `-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_RC_COMPILER=rc -DCMAKE_NINJA_CMCLDEPS_RC=OFF -DCMAKE_SIZEOF_VOID_P=8 -DTARGET_BUILD_PLATFORM=windows -DPX_OUTPUT_ARCH=x86 -DPHYSX_ROOT_DIR=<physx> -DPX_OUTPUT_LIB_DIR=<physx> -DPX_OUTPUT_BIN_DIR=<physx> -DPX_BUILDSNIPPETS=OFF -DPX_BUILDPVDRUNTIME=OFF -DPX_GENERATE_STATIC_LIBRARIES=ON -DPX_CMAKE_SUPPRESS_REGENERATION=ON -DPX_GENERATE_GPU_PROJECTS=OFF -DNV_USE_STATIC_WINCRT=ON -DNV_USE_DEBUG_WINCRT=OFF -DPX_FLOAT_POINT_PRECISE_MATH=OFF -DCMAKE_INSTALL_PREFIX=<physx>/install/vc17clang-godot/PhysX`.
   Result: static libraries, static CRT, CPU only (no GPU projects), no snippets, no PVD runtime, `PX_FLOAT_POINT_PRECISE_MATH=OFF` (what that does inside PhysX was not read).
4. Build and install: `cmake --build <build> --config release --parallel 16`, then `cmake --install <build> --config release`.
- PROVED (`physx.log`): configure finished ("Configuring done") and the install step ran (the log holds the `Installing:` lines for the headers under `install/vc17clang-godot/PhysX`). The script runs install only after a successful build (`|| exit /b 1` chain). The header comment of the script still says "NOT RUN YET"; that line is stale, the log shows it ran. Whether every PhysX library was produced was not listed from the install tree here.

## Build route: Godot fork against that SDK
Source: `.tmp/physx-clang/scripts/phase2-godot.sh`, run from the fork root.
- Common SCons arguments: `platform=windows arch=x86_64 use_llvm=yes production=yes optimize=speed debug_symbols=no lto=none accesskit=no d3d12=no silence_msvc=no progress=yes -j16`, with `PYTHON` = a Python 3 interpreter (path not recorded here).
- Four builds, each with its own `extra_suffix`: `target=editor` and `target=template_release`, each as `physx` (`physx_sdk=<install>/PhysX`) and `nophysx` (no SDK; `PHYSX_SDK` and `BLAST_SDK` must be unset, the script unsets them). The nophysx pair is the control for the silent-fallback check.
- PROVED (`godot.log`): all four builds ended with "scons: done building targets" and linked `godot.windows.{editor,template_release}.x86_64.llvm.{physx,nophysx}[.console].exe`; the physx logs show `godot_physx` sources compiled (104 matching lines in the log).
- NOT checked: that the clang-built physx editor and template load the module and run PhysX (no run of these new exes was found in the files read). Run the smoke with `BENCH_ENGINE=physx` and read `engine_tag` / `physics_space_state_class` in the env file before using them.

## Export and run
- Export: `tools/export-godot-fork.sh`, with `FORK_TEMPLATE=<fork template_release exe>` and `FORK_EDITOR=<fork editor console exe>` (both required), optional `GODOT_EXT_DLL`, `EXPORT_ROOT`, and a label argument. The editor `--import` step of the fork segfaults harmlessly (godot NOTES); the export works.
- Run: `BENCH_ENGINE=physx BENCH_FLAVOUR=<label> GODOT_BENCH_EXE=<export>/QuestBench.exe bash tools/run-all.sh godot` (see `README.md`).
- The extension DLL must be built against the fork's API dump (`GODOT_API_JSON`); a stock 4.7.2 DLL loads in the fork editor, but the report rebuilt against the fork dump for all results (physx and nophysx dumps were byte-identical).

## Sleep parity (see SPEC "Settings parity", row sleep)
- The Godot `physics/3d/sleep_threshold_*` settings are ignored under Jolt (env.txt line `godot sleep_threshold_linear / angular / time_before_sleep`, `quest_bench.cpp` line 1299). PhysX: mass-normalised energy threshold 0.01475 (about 0.17 m/s equivalent), wake counter 0.5 s, stabilization on (as given by the session that checked the fork; values not re-read here).

## Not done
- The full 200 Hz protocol, Scenario G included, ran once for the PhysX build on 2026-11-07 (one desktop, results not committed). No Steam Deck run (no Deck available).

## GPU use and fork heads (checked 2026-11-07, source reading only, no timings)
- PROVED CPU-only for the exes in use: the scons line (`.tmp/physx-clang/scripts/phase2-godot.sh`) passes no `physx_gpu`; `modules/godot_physx/SCsub:20` (c6f3b659) defaults it to off and defines `GODOT_PHYSX_GPU` only under it; the PhysX SDK was configured with `PX_GENERATE_GPU_PROJECTS=OFF`; neither exe contains the key `simulation/gpu_dynamics`; no `PhysXGpu_64.dll` ships beside the exe. Scope: the exes on disk at this date; an earlier rebuild with other flags was not checked.
- PROVED in the fork source (c6f3b659 and 97bc4de): rigid-body GPU dynamics needs CUDA (`PxCreateCudaContextManager`, README "on NVIDIA", needs an NVIDIA driver). No Vulkan or other non-CUDA path for rigid bodies was found in `modules/godot_physx`. Granular, particle fluid and flow use Godot compute shaders on any GPU; they are separate features and not part of this benchmark.
- Claim by the fork author in a video (transcript read, 2026-11-07): the project is "GPU agnostic" and "runs on a Steam Deck, AMD hardware all the way"; turning `gpu_dynamics` off turns off CUDA but keeps compute-shader acceleration for the features above. It does not state that rigid-body GPU dynamics ran on a Deck. HYPOTHESIS: on a Deck the rigid-body path is the CPU path measured here (follows from the CUDA requirement; not run).
- A GPU variant would be NVIDIA-only (CUDA toolkit, PhysX GPU SDK build, `physx_gpu=yes`, `PhysXGpu_64.dll` beside the exe) and could not run on a Deck. It would need its own flavour label, never replace the CPU variant.
- env.txt now records `physx gpu_build (key ... registered): yes|no` (`quest_bench.cpp`, next to `physx gpu_dynamics`), so a GPU build can be told from a CPU-only one in every result. Added after the 200 Hz run: those results do not have the line; untested until the extension is rebuilt.
- Fork head: the local fork checkout was fast-forwarded from c6f3b659 to 97bc4de (8 commits) on 2026-11-07. The fork was then rebuilt at 97bc4de (local build, CPU-only; smoke run OK; reported by the session that ran it, build log not re-read here). The older sections above and the 200 Hz results from 2026-11-07 are from c6f3b659. New in 97bc4de (commit 0864bdf): `physics/physx_3d/simulation/jointed_position_iterations` (default 8) and `jointed_velocity_iterations` (default 2), equal to the old hard-coded values; they affect only jointed bodies (scenario with chains). Other physx_3d settings are unchanged.
- PROVED (report, 97bc4de smoke): in the game profile all pit-2000 cells end at 0 awake bodies. The 53 awake in the 60 Hz smoke table above came from that 60 Hz smoke (c6f3b659), not from the 200 Hz game profile.
- PROVED (source read plus runtime readback of `getSolverIterationCounts`, chains scenario, `template_release`, game profile; evidence was kept in .tmp/jointed-check, since deleted): in `godot_physx_project_settings.cpp` the keys `jointed_position_iterations` / `jointed_velocity_iterations` are registered only under `#ifdef GODOT_PHYSX_GPU` (lines 43 to 45) but read unconditionally and written into the static members (lines 62 and 63, `CLAMP(..., 1, 32)`). A missing key returns nil, which reads as 0 and clamps to 1, so in our CPU-only 97bc4de build jointed bodies (stage C chains) ran with 1/1 solver iterations instead of 8/2 (readback pos=1 vel=1; with the keys set in override.cfg: pos=8 vel=2). Fix used: `run-all.sh` writes both keys (8 and 2) into override.cfg for PhysX. Not run for stages D and stress (same code path). Stage C and mixed results from a 97bc4de run without the override must not be compared with the c6f3b659 results (hard-coded 8/2). `quest_bench.cpp` prints both values in env.txt (`has_setting` is true once override.cfg sets them, although the keys stay unregistered); a proper fix would be a fork patch registering the keys outside the GPU `#ifdef`.
- Our run keeps the fork defaults for every registered physx_3d setting (solver PGS, enhanced_determinism off, allow_sleep on, stabilization on, cpu_worker_threads 0 = auto, CCD off); the fork author's example project (commit 75b831c) overrides none of them (`project.godot` read).
- Before every fork rebuild run `bash benchmark/tools/check-fork-head.sh` (read-only fetch; exit 10 = new commits on the remote, with the list and changed paths; `--pull` fast-forwards without building). On 2026-11-07 the checkout was at 97bc4de and equal to the remote. Read new commits that touch `objects/`, `spaces/` or `godot_physx_project_settings.*` before deciding which stages to rerun.
