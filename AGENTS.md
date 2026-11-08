# QuestMark: agent instructions

QuestMark is the engine benchmark and engine study for Quest, a planned physics-heavy 3D game (AI agents write most of the code, target 60 fps on a handheld PC, Windows-only build run via Proton on Linux handhelds). The engine is not final: the study and the benchmark decide it.

## Layout
- `docs/engine-choice.md`: engine study (source-read, 2026-11-06), desktop benchmark results and the verdict (section 12.2). Verdict 2026-11-08, provisional until a Steam Deck run: ezEngine + Jolt, then Stride + Bepu, then Godot (PhysX fork 3a ahead of Jolt 3b on the measured numbers; the final order is the user's decision).
- `benchmark/`: Godot vs Stride (and Godot PhysX fork) benchmark. Start with `benchmark/README.md`; `benchmark/SPEC.md` is the single source of truth for scenarios, settings and pass rules.
- `QuestMark.bat`, `benchmark/tools/setup.sh`, `run-benchmark.sh`, `lib.sh`, `setup-physx.sh`: one-click download, build, run and summary (quick subset or `--full` SPEC protocol). Keep them free of local paths, CRLF-free (`.sh` and `.pl` are LF; `.bat` is CRLF, see `.gitattributes`), and update the Quick start in `README.md` when flags change. The ezEngine setup path, `--full`, `--rid` and `--aot` were not tested end to end.
- `.tmp/`: scratch only (git-ignored, never committed); builds of godot-cpp, the PhysX SDK and the Godot fork live here. Delete your own leftovers when done.

## Rules for this project
- Godot hot-path code is C++ (godot-cpp GDExtension), never GDScript. GDScript only for glue and scene setup.
- Godot body API: `--body-api=nodes|rid` (`BODY_API` in run-all). Node-based and RID results are different variants: never compare them as one; the Godot ranking depends on the API (docs/engine-choice.md 12.3). Debris (G) is not supported in rid mode.
- README charts come from `benchmark/tools/gen-readme-charts.pl` (values copied by hand from the README table).
- Benchmark fairness: engine settings stay as equal as possible (see the parity table in SPEC.md); any new setting goes into the table and `tools/parity_check.pl`.
- Fixed physics step 200 Hz, render unlocked, profiles `game` and `stress`. Timings taken at 60 Hz are not comparable.
- Never take timings while another build or smoke test runs; the full protocol needs an idle machine and the user's go-ahead.
- Read PhysX results next to `awake_bodies` (PhysX sleeps far more than Jolt). Engine in use is proved only by the class of the direct space state, not by the project setting.
- Always read the latest repo heads when checking an engine, not only stable releases.
- No local paths, hardware names or personal data in tracked files; keep them in gitignored local files or env vars.
- Use project-root-relative paths everywhere (scripts, env vars in `.claude/settings.local.json`, commands); the tools resolve them against the project root, not the caller's cwd. The project was moved once: CMake caches under `.tmp/godotext-build/` keep absolute paths and must be rebuilt after a move.

## Build and run (details in benchmark/README.md)
- Godot extension: `benchmark/tools/build-godot-ext.sh` (`COMPILER=clang|msvc`).
- Stride: `dotnet build` (JIT) or `dotnet publish -c Release -r win-x64 -p:BenchAot=true` (Native AOT).
- Runs: `benchmark/tools/run-all.sh <engine> [--dry-run]`, results merged by `merge-results.pl`.

## Model and cost rules (refines the global table for this project)
- Haiku (low): run a given build/smoke command and report; listings and greps.
- Sonnet (medium): benchmark code edits following existing patterns, run-all/merge script changes, docs, screenshot review, render checks.
- Opus (high): root-causing physics divergences between engines (sleeping, solver, restitution), race or threading questions in the engine integrations, deciding pass rules, verifying a result that drives the engine verdict.
- Fable: never. No workflows unless the user asks.
