#!/usr/bin/env bash
# full-body run matrix: correctness and onset, NOT timing. Usage (from the repository root):
#   benchmark/physics-articulated/full-body/run_all.sh <build dir> <phase> [size_label] [note]
# phase 1: stand + step-debris (1000:1), 120 Hz (the minimum set)
# phase 2: everything else: step-debris 100:1 and 1000:1 at 200 Hz, step (no debris), impact, wall, stand at 200 Hz, 120 Hz rest
# Per size_label (h76m, h85m, h96m, h182m), both backends, 2 runs each for the determinism hash, CCD on (humanoid links, projectile), 1 thread.
set -u
BUILD="${1:?build dir}"; PHASE="${2:?phase 1|2}"; ONLY="${3:-}"
NOTE="${4:-NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)}"
EXE="$BUILD/full-body/full_body.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/full-body/results}"
mkdir -p "$OUT"
run() {  # tag, args...
  local tag="$1"; shift
  "$EXE" --backend both --runs 2 --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" "$@" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
}
for g in "h76m" "h85m" "h96m" "h182m"; do
  [ -n "$ONLY" ] && [ "$ONLY" != "$g" ] && continue
  gt="${g// /_}"
  if [ "$PHASE" = 1 ]; then
    run "stand-$gt-120hz" --size_label "$g" --case stand --hz 120
    run "stepdebris1000-$gt-120hz" --size_label "$g" --case step-debris --ratio 1000 --hz 120
  else
    run "stepdebris100-$gt-120hz" --size_label "$g" --case step-debris --ratio 100 --hz 120
    run "step-$gt-120hz" --size_label "$g" --case step --hz 120
    run "impact-$gt-120hz" --size_label "$g" --case impact --hz 120
    run "wall-$gt-120hz" --size_label "$g" --case wall --hz 120
    run "stand-$gt-200hz" --size_label "$g" --case stand --hz 200
    run "stepdebris1000-$gt-200hz" --size_label "$g" --case step-debris --ratio 1000 --hz 200
    run "stepdebris100-$gt-200hz" --size_label "$g" --case step-debris --ratio 100 --hz 200
    run "step-$gt-200hz" --size_label "$g" --case step --hz 200
    run "impact-$gt-200hz" --size_label "$g" --case impact --hz 200
    run "wall-$gt-200hz" --size_label "$g" --case wall --hz 200
  fi
done
