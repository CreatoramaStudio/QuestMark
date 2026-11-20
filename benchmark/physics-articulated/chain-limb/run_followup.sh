#!/usr/bin/env bash
# chain-limb follow-up: onset experiments (correctness, NOT timing). Usage (from the repository root): benchmark/physics-articulated/chain-limb/run_followup.sh <build dir>
# Base case: alternating 50 t / 50 kg chain, tip speed 0.1 m/s (no wall contact), CCD on, 1 thread, 2 episodes (2 x 2 s), 1 run.
# One change at a time against the baseline; every switch defaults to the old behaviour. All timing in these files is meaningless (busy machine).
set -u
BUILD="${1:?build dir}"
EXE="$BUILD/chain-limb/chain_limb.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/chain-limb/results/followup}"
mkdir -p "$OUT"
NOTE="NOT VALID FOR TIMING: machine was busy (onset experiment, no timing claim)"
run() {  # tag, args...
  local tag="$1"; shift
  "$EXE" --profile alternating --tip-speed 0.1 --ccd on --threads 1 --warmup 0 --runs 1 --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" "$@" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
}
for hz in 120 200; do
  steps=$((hz * 4))  # two 2 s episodes
  for axis in z y; do
    S="--hz $hz --steps $steps --axis $axis"
    run "jolt-base-$axis-${hz}hz"            --backend jolt $S
    run "jolt-prio-$axis-${hz}hz"            --backend jolt $S --jolt-prio on
    run "jolt-heavy1-$axis-${hz}hz"          --backend jolt $S --jolt-heavy-body1 on
    run "jolt-imult10-$axis-${hz}hz"         --backend jolt $S --jolt-inertia-mult 10
    run "jolt-imult100-$axis-${hz}hz"        --backend jolt $S --jolt-inertia-mult 100
    run "jolt-thick-$axis-${hz}hz"           --backend jolt $S --light-half-thick 0.5
    run "jolt-maxw1000-$axis-${hz}hz"        --backend jolt $S --jolt-max-angvel 1000
    run "physx-artic-$axis-${hz}hz"          --backend physx $S
    run "physx-maximal-$axis-${hz}hz"        --backend physx-maximal $S
    run "physx-maximal-tgs-$axis-${hz}hz"    --backend physx-maximal $S --physx-tgs on
    run "physx-maximal-thick-$axis-${hz}hz"  --backend physx-maximal $S --light-half-thick 0.5
    run "physx-maximal-it10-4-$axis-${hz}hz" --backend physx-maximal $S --iters 10,4
  done
  run "jolt-base-y-lim3-${hz}hz"         --backend jolt --hz $hz --steps $steps --axis y --limit 3.14
  run "physx-artic-y-lim3-${hz}hz"       --backend physx --hz $hz --steps $steps --axis y --limit 3.14
  run "physx-maximal-y-lim3-${hz}hz"     --backend physx-maximal --hz $hz --steps $steps --axis y --limit 3.14
done
# extra, after the single-change runs: all three Jolt remedies that delayed the failure at once (thicker light box + priority + heavy body 1)
for hz in 120 200; do
  for axis in z y; do
    run "jolt-combo-thick-prio-heavy1-$axis-${hz}hz" --backend jolt --hz $hz --steps $((hz * 4)) --axis $axis --light-half-thick 0.5 --jolt-prio on --jolt-heavy-body1 on
  done
done
# extra: onset in the original launch case (tip speed 30 m/s into the wall, as results/alternating-*-ccdon-t1), to tie the tip-0.1 numbers to it
for hz in 120 200; do
  for b in jolt physx physx-maximal; do
    "$EXE" --profile alternating --tip-speed 30 --ccd on --threads 1 --warmup 0 --runs 1 --hz $hz --steps $((hz * 4)) --backend $b --tag "$b-tip30-z-${hz}hz" --note "$NOTE" --out "$OUT/$b-tip30-z-${hz}hz.json" > "$OUT/$b-tip30-z-${hz}hz.log" 2>&1 || echo "FAILED: $b tip30 $hz"
  done
done
