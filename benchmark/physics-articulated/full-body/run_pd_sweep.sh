#!/usr/bin/env bash
# full-body diagnostic: is the Jolt standing collapse a PD-gain artefact? h76m, stand case, 120 Hz, both backends, 1 run per combination.
# Usage (from the repository root): benchmark/physics-articulated/full-body/run_pd_sweep.sh <build dir> [note]
# stand_mult = effective inertia of the leg joints as a multiple of the inertia of the rest of the humanoid (0 = reduced inertia only); f = PD natural frequency in Hz.
set -u
BUILD="${1:?build dir}"
NOTE="${2:-NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)}"
EXE="$BUILD/full-body/full_body.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/full-body/results/pd-sweep}"
mkdir -p "$OUT"
for sm in 0 0.02 0.25; do
  for f in 1 2 4; do
    tag="stand-h76m-120hz-sm${sm}-f${f}"
    "$EXE" --backend both --runs 1 --case stand --size_label "h76m" --hz 120 --stand-mult $sm --pd-freq $f --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
  done
done
