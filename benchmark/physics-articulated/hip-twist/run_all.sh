#!/usr/bin/env bash
# hip-twist: correctness and onset, NOT timing. Usage, from the repository root: benchmark/physics-articulated/hip-twist/run_all.sh <build dir> [limb ...]
# Per size_label (4), limb (leg, arm), case (hold swing wall impact limits), step rate (120, 200 Hz): all four variants, 2 runs each (determinism hash).
set -u
BUILD="${1:?build dir}"; shift
LIMBS="${*:-leg arm}"
EXE="$BUILD/hip-twist/hip_twist.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/hip-twist/results}"
mkdir -p "$OUT"
NOTE="NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)"
for limb in $LIMBS; do
  for c in hold swing wall impact limits; do
    for g in "h76m" "h85m" "h96m" "h182m"; do
      gt="${g// /_}"
      for hz in 120 200; do
        tag="$limb-$gt-$c-${hz}hz"
        "$EXE" --size_label "$g" --limb "$limb" --case "$c" --hz "$hz" --runs 2 --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
      done
    done
  done
done
