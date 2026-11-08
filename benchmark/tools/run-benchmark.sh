#!/usr/bin/env bash
# One-command benchmark run: sets up the selected engines (setup.sh), runs the cells, merges the results.
# Usage: bash benchmark/tools/run-benchmark.sh [--quick|--full] [godot] [stride] [ezengine] [--no-setup] [--rid] [--aot] [--runs N] [--yes] [--dry]
#   engines     default: godot stride ezengine
#   --quick     (default) about 25 min for all three engines: per engine the cells unlock-physics-pit-2000, unlock-physics-mixed,
#               scale 1000/2000/4000 (unpaced), unlock-render-mixed, profile stress, 3 runs (6 s warm-up 5 s, measure 20 s / 10 s for scale).
#               A subset for a first impression, NOT the SPEC protocol: do not publish it as the full result.
#   --full      the SPEC protocol: tools/run-all.sh <engine> for each engine (about 1 h per variant).
#   --rid       add a Godot variant that drives bodies through PhysicsServer3D RIDs (BODY_API=rid, flavour rid). Full mode runs stages ABCDEF
#               for it (stage G is unsupported there).
#   --aot       full mode only: also run the Stride Native AOT build (needs setup.sh --aot, done automatically here)
#   --no-setup  do not run setup.sh first (the exe paths come from the environment or ENV_FILE)
#   --summary-only  only merge and print the results already in OUT_DIR
#   --runs N    quick mode: runs per cell (default 3)     --yes  do not wait for a key press     --dry  print the commands, run nothing
# Environment: ENV_FILE (default .tmp/env.sh, written by setup.sh), OUT_DIR (default .tmp/results), the exe variables of run-all.sh
#   (GODOT_BENCH_EXE, STRIDE_BENCH_EXE, EZENGINE_BENCH_EXE; relative paths are taken from the project root), EZENGINE_RENDERER (default DX11).
# The machine must be idle (close other programs) and the benchmark windows must stay visible: timings are only valid then.
# Output: OUT_DIR/*.csv, *.summary.txt, *.env.txt per run, OUT_DIR/SUMMARY.md (tools/merge-results.pl), headline lines on the screen.
set -uo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

MODE=quick; ENGINES=(); SETUP=1; RID=0; AOT=0; RUNS=3; YES=0; DRY=0; SUMONLY=0
args=("$@"); i=0
while [ $i -lt ${#args[@]} ]; do a="${args[$i]}"; case "$a" in
  --quick) MODE=quick ;; --full) MODE=full ;;
  godot|stride|ezengine) ENGINES+=("$a") ;;
  --no-setup) SETUP=0 ;; --rid) RID=1 ;; --aot) AOT=1 ;; --yes) YES=1 ;; --summary-only) SUMONLY=1 ;; --dry) DRY=1 ;;
  --runs) i=$((i+1)); RUNS="${args[$i]:-}" ;; --runs=*) RUNS="${a#--runs=}" ;;
  -h|--help) sed -n '2,/^set -uo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
  *) echo "usage: run-benchmark.sh [--quick|--full] [godot] [stride] [ezengine] [--no-setup] [--rid] [--aot] [--runs N] [--yes] [--dry]" >&2; exit 2 ;;
esac; i=$((i+1)); done
case "$RUNS" in ''|*[!0-9]*|0) echo "--runs needs a positive number" >&2; exit 2 ;; esac
[ ${#ENGINES[@]} -gt 0 ] || ENGINES=(godot stride ezengine)
has() { local e; for e in "${ENGINES[@]}"; do [ "$e" = "$1" ] && return 0; done; return 1; }

ENV_FILE="$(qb_abs "${ENV_FILE:-.tmp/env.sh}")"; export ENV_FILE
# 1. setup (idempotent: finished steps are skipped)
if [ "$SETUP" = 1 ] && [ "$DRY" = 0 ] && [ "$SUMONLY" = 0 ]; then
  sargs=("${ENGINES[@]}"); [ "$AOT" = 1 ] && sargs+=(--aot)
  bash "$QB_TOOLS/setup.sh" "${sargs[@]}" || { echo "setup failed; fix the message above and run again" >&2; exit 1; }
fi
# 2. exe paths: environment first, then ENV_FILE
if [ -f "$ENV_FILE" ]; then
  keep_g="${GODOT_BENCH_EXE:-}"; keep_s="${STRIDE_BENCH_EXE:-}"; keep_e="${EZENGINE_BENCH_EXE:-}"; keep_o="${OUT_DIR:-}"
  . "$ENV_FILE"
  [ -n "$keep_g" ] && GODOT_BENCH_EXE="$keep_g"; [ -n "$keep_s" ] && STRIDE_BENCH_EXE="$keep_s"
  [ -n "$keep_e" ] && EZENGINE_BENCH_EXE="$keep_e"; [ -n "$keep_o" ] && OUT_DIR="$keep_o"
fi
OUT="$(qb_abs "${OUT_DIR:-.tmp/results}")"
exe_of() { # engine -> absolute exe path (may not exist)
  case "$1" in godot) qb_abs "${GODOT_BENCH_EXE:-}" ;; stride) qb_abs "${STRIDE_BENCH_EXE:-}" ;; ezengine) qb_abs "${EZENGINE_BENCH_EXE:-}" ;; esac
}
if [ "$DRY" = 0 ] && [ "$SUMONLY" = 0 ]; then
  for e in "${ENGINES[@]}"; do
    x="$(exe_of "$e")"
    [ -n "$x" ] && [ -f "$x" ] || { echo "No exe for $e (looked for '${x:-<unset>}'). Run: bash benchmark/tools/setup.sh $e   (or set the exe variable)" >&2; exit 1; }
  done
fi
[ "$DRY" = 1 ] || mkdir -p "$OUT"

# 3. warning
[ "$SUMONLY" = 1 ] || cat <<EOF

=====================================================================================
 QuestMark benchmark, mode: $MODE, engines: ${ENGINES[*]}$([ "$RID" = 1 ] && echo ", godot rid variant")
 Close every other program first (browser, launchers, updates, builds). The machine must be idle.
 Benchmark windows will open and close by themselves; leave them visible, do not click or move them.
 $( [ "$MODE" = quick ] && echo "Planned: about 8 min per engine variant (quick subset), results in $(qb_rel "$OUT")." || echo "Planned: about 1 h per engine variant (full SPEC protocol), results in $(qb_rel "$OUT").")
=====================================================================================
EOF
if [ "$YES" = 0 ] && [ "$DRY" = 0 ] && [ "$SUMONLY" = 0 ] && [ -t 0 ]; then read -r -t 60 -p "Press Enter to start (Ctrl+C aborts, starts by itself after 60 s) " _ || true; echo; fi

# 4. run
FAILS=0
quick_cells=(
  # name|scenario|mode|warmup|measure|extra args (space separated)
  "unlock-physics-pit-2000|unlock-physics-pit-2000|physics|5|20|"
  "unlock-physics-mixed|unlock-physics-mixed|physics|5|20|"
  "scale-1000|scale|physics|5|10|--bodies=1000 --unpaced=1"
  "scale-2000|scale|physics|5|10|--bodies=2000 --unpaced=1"
  "scale-4000|scale|physics|5|10|--bodies=4000 --unpaced=1"
  "unlock-render-mixed|unlock-render-mixed|render|5|20|"
)
# quick_one <engine> <exe> <run> <cell> <godot extra flags>: one process, one cell (no --headless anywhere: unpaced Godot cells need a window,
# ezEngine has no headless mode, Stride physics runs keep their window as in run-all.sh)
quick_one() {
  local engine="$1" x="$2" run="$3" cell="$4" gextra="$5" name sc mode w m extra
  IFS='|' read -r name sc mode w m extra <<<"$cell"
  local argv=(--scenario="$sc" --mode="$mode" --profile=stress --run="$run" --warmup="$w" --measure="$m")
  # shellcheck disable=SC2206
  [ -n "$extra" ] && argv+=($extra)
  case "$engine" in
    godot)
      argv=(-- "${argv[@]}")
      case "$sc" in scale) argv+=(--jolt-limits=raised) ;; *) argv+=(--jolt-limits=old) ;; esac
      # shellcheck disable=SC2206
      [ -n "$gextra" ] && argv+=($gextra)
      ;;
    ezengine)
      argv+=(-renderer "${EZENGINE_RENDERER:-DX11}" --project="$(qb_win "$QB_ROOT/benchmark/ezengine/Project")") ;;
  esac
  argv+=(--out="$(qb_win "$OUT")")
  local tag="$engine${gextra:+-rid} $name run $run" log="$OUT/quick-$engine${gextra:+-rid}.log"
  if [ "$DRY" = 1 ]; then echo "$tag: \"$(qb_rel "$x")\" ${argv[*]}"; return 0; fi
  echo "$(date +%H:%M:%S) $tag"
  # run-all.sh writes the same override.cfg next to the exe before every Godot run (the engine in use is proved from env.txt afterwards)
  [ "$engine" = godot ] && printf '[physics]\n\n3d/physics_engine="Jolt Physics"\n' > "$(dirname "$x")/override.cfg"
  local rc=0
  { echo "=== $(date +%H:%M:%S) $tag"; timeout 300 "$x" "${argv[@]}"; } >>"$log" 2>&1 || rc=$?
  if [ "$rc" -ne 0 ]; then
    [ "$rc" = 124 ] && taskkill //F //IM "$(basename "$x")" >/dev/null 2>&1
    echo "    FAILED rc=$rc ($tag), see $(qb_rel "$log")"; FAILS=$((FAILS+1))
  fi
  return 0
}
quick_engine() { # engine gextra-flags
  local engine="$1" gextra="${2:-}" x run cell
  x="$(exe_of "$engine")"
  for run in $(seq 1 "$RUNS"); do for cell in "${quick_cells[@]}"; do quick_one "$engine" "$x" "$run" "$cell" "$gextra"; done; done
}
full_engine() { # engine [env assignments...]
  local engine="$1"; shift
  echo "$(date +%H:%M:%S) run-all.sh $engine $*"
  if [ "$DRY" = 1 ]; then OUT_DIR="$OUT" env "$@" bash "$QB_TOOLS/run-all.sh" "$engine" --dry-run | tail -n 5; return 0; fi
  OUT_DIR="$OUT" env "$@" bash "$QB_TOOLS/run-all.sh" "$engine" || { echo "    run-all.sh $engine exited non-zero"; FAILS=$((FAILS+1)); }
}
T0=$(date +%s)
for e in "${ENGINES[@]}"; do
  [ "$SUMONLY" = 1 ] && break
  if [ "$MODE" = quick ]; then
    quick_engine "$e" ""
    [ "$e" = godot ] && [ "$RID" = 1 ] && quick_engine godot "--body-api=rid --flavour=rid"
  else
    export GODOT_BENCH_EXE STRIDE_BENCH_EXE EZENGINE_BENCH_EXE
    case "$e" in
      godot) full_engine godot; [ "$RID" = 1 ] && full_engine godot BENCH_FLAVOUR=rid BODY_API=rid STAGES=ABCDEF ;;
      stride) full_engine stride
              [ "$AOT" = 1 ] && full_engine stride STRIDE_FLAVOR=bepu_aot "STRIDE_BENCH_EXE=$(qb_abs "${STRIDE_AOT_EXE:-}")" ;;
      ezengine) full_engine ezengine ;;
    esac
  fi
done
[ "$MODE" = quick ] && [ "$AOT" = 1 ] && echo "note: --aot only applies to --full (quick mode runs the JIT build)"
[ "$DRY" = 1 ] && exit 0

# 5. summary
echo
echo "Finished in $(( ($(date +%s) - T0) / 60 )) min, failed runs/engines: $FAILS"
SUM="$OUT/SUMMARY.md"
if perl "$QB_TOOLS/merge-results.pl" "$OUT" > "$SUM.tmp" 2>"$OUT/merge-results.err" && [ -s "$SUM.tmp" ]; then
  mv "$SUM.tmp" "$SUM"
  echo "Summary: $(qb_rel "$SUM")"
  echo
  echo "Headline: wall ms per physics step (unpaced cells; 1000 / steps_per_s, lower is better)"
  awk '/^## Headline/ {on=1; next} /^## / {on=0} on && /^\|/' "$SUM"
  echo
  echo "Rendered scene (fps = frames per second, wall p99 = 99th percentile frame time in ms):"
  awk -F'|' '/^## Scenario E/ {on=1; next} /^## / {on=0} on && $6 ~ /render/ { gsub(/ /,"",$2); gsub(/ /,"",$3); gsub(/ /,"",$5); gsub(/ /,"",$8); gsub(/ /,"",$12);
              printf "  %-22s %-7s %-22s fps %-8s wall p99 %s ms\n", $2, $3, $5, $8, $12 }' "$SUM"
else
  rm -f "$SUM.tmp"; echo "merge-results.pl failed, see $(qb_rel "$OUT")/merge-results.err (the per-run files are still in $(qb_rel "$OUT"))"
fi
echo
echo "Engine proof (class of the physics space actually used, from env.txt):"
for e in godot stride ezengine; do
  has "$e" || continue
  grep -h -m1 '^physics_space_state_class' "$OUT"/$e-*.env.txt 2>/dev/null | sort | uniq -c | sed "s/^/  $e: /"
done
[ "$FAILS" -eq 0 ] || exit 1
exit 0
