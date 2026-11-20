#!/usr/bin/env bash
# full-body diagnostic (Jolt only): does the Jolt collapse in the standing and step cases go away with more solver iterations or a higher step rate?
# Usage (from the repository root): benchmark/physics-articulated/full-body/run_jolt_remedy.sh <build dir> [note]
# Variants: 120 Hz with 100 velocity / 30 position iterations (library default 10 / 2), 200 Hz with the same, 480 Hz with the defaults.
set -u
BUILD="${1:?build dir}"
NOTE="${2:-NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)}"
EXE="$BUILD/full-body/full_body.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/full-body/results/jolt-remedy}"
mkdir -p "$OUT"
run() { local tag="$1"; shift; "$EXE" --backend jolt --runs 2 --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" "$@" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"; }
for g in "h76m" "h85m" "h96m" "h182m"; do
  gt="${g// /_}"
  for c in stand step-debris; do
    run "$c-$gt-120hz-iters100_30" --size_label "$g" --case $c --ratio 1000 --hz 120 --iters 100,30
    run "$c-$gt-200hz-iters100_30" --size_label "$g" --case $c --ratio 1000 --hz 200 --iters 100,30
    run "$c-$gt-480hz-defaults" --size_label "$g" --case $c --ratio 1000 --hz 480
  done
done
