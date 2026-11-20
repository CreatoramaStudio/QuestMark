#!/usr/bin/env bash
# standing-foot Jolt remedy ladder (correctness only): which solver-iteration counts (velocity,position) or gain change cure the sag seen with Jolt's default iterations.
# Usage (from the repository root): benchmark/physics-articulated/standing-foot/run_ladder.sh <build dir> [note]. Cases 2leg-stand and 1leg-swing, Jolt only, 2 runs each.
set -u
BUILD="${1:?build dir}"
EXE="$BUILD/standing-foot/standing_foot.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/standing-foot/results}"
mkdir -p "$OUT"
NOTE="${2:-NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)}"
for g in "h76m" "h85m" "h96m" "h182m"; do
  gt="${g// /_}"
  for hz in 120 200; do
    for v in "it20-4:--iters 20,4" "it30-10:--iters 30,10" "it50-15:--iters 50,15" "kp4:--kp-mult 4" "kp4-it50-15:--kp-mult 4 --iters 50,15"; do
      name="${v%%:*}"; args="${v#*:}"
      tag="ladder-$gt-${hz}hz-$name"
      "$EXE" --size_label "$g" --hz "$hz" --runs 2 --backend jolt --cases 2leg-stand,1leg-swing $args --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
    done
  done
done
