#!/usr/bin/env bash
# Runs the chain-limb matrix. Usage (from the repository root): benchmark/physics-articulated/chain-limb/run_all.sh <build dir> <note>
# Example: benchmark/physics-articulated/chain-limb/run_all.sh .build/artic "NOT VALID FOR TIMING: machine was busy"
set -u
BUILD="${1:?build dir}"; NOTE="${2:-}"
EXE="$BUILD/chain-limb/chain_limb.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/chain-limb/results}"
run() {  # tag, args...
  local tag="$1"; shift
  "$EXE" "$@" --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
}
for hz in 120 200; do
  for ccd in on off; do
    run "geometric-${hz}hz-ccd${ccd}-t1" --hz $hz --ccd $ccd --threads 1 --profile geometric
    run "alternating-${hz}hz-ccd${ccd}-t1" --hz $hz --ccd $ccd --threads 1 --profile alternating
  done
  run "geometric-${hz}hz-ccdon-t4" --hz $hz --ccd on --threads 4 --profile geometric
  run "geometric-100kg-${hz}hz-ccdon-t1" --hz $hz --ccd on --threads 1 --profile geometric --mass-min 100
done
# diagnostics for the alternating profile (neighbour mass ratio): is the failure impact-related, gravity-related, ratio-related?
run "diag-alternating-120hz-tip0.1-t1" --hz 120 --threads 1 --profile alternating --tip-speed 0.1
run "diag-alternating-120hz-tip0.1-gravity0-t1" --hz 120 --threads 1 --profile alternating --tip-speed 0.1 --gravity 0
run "diag-alternating-120hz-ratio100-t1" --hz 120 --threads 1 --profile alternating --mass-min 500
run "diag-alternating-120hz-ratio10-t1" --hz 120 --threads 1 --profile alternating --mass-min 5000
run "diag-alternating-120hz-iters100-20-t1" --hz 120 --threads 1 --profile alternating --iters 100,20
