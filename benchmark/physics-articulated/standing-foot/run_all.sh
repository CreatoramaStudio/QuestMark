#!/usr/bin/env bash
# standing-foot matrix (correctness and onset, NOT timing). Usage (from the repository root): benchmark/physics-articulated/standing-foot/run_all.sh <build dir> [note]
# Per size_label (h76m, h85m, h96m, h182m), step rate (120, 200 Hz) and sweep (library-default solver iterations; Jolt vel 100 / pos 30 and the same numbers for PhysX):
# cases 2leg-stand, 2leg-impact, 1leg-stand, 1leg-impact, 1leg-swing on jolt, physx (articulation), physx-maximal; 2 runs each for the determinism hash.
set -u
BUILD="${1:?build dir}"
EXE="$BUILD/standing-foot/standing_foot.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/standing-foot/results}"
mkdir -p "$OUT"
NOTE="${2:-NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)}"
for g in "h76m" "h85m" "h96m" "h182m"; do
  gt="${g// /_}"
  for hz in 120 200; do
    for sweep in default iters100-30; do
      extra=""
      [ "$sweep" = "iters100-30" ] && extra="--iters 100,30"
      tag="stand-$gt-${hz}hz-$sweep"
      "$EXE" --size_label "$g" --hz "$hz" --runs 2 $extra --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
    done
  done
done
