#!/usr/bin/env bash
# chain-limb limb scenarios (--scenario limb): correctness and onset, NOT timing. Usage (from the repository root): benchmark/physics-articulated/chain-limb/run_limb.sh <build dir> [note]
# The optional note (default: "NOT VALID FOR TIMING: machine was busy ...") goes into the timing_note field of every result JSON.
# Per size label (h76m, h85m, h96m, h182m), limb (leg, arm), case (hold, swing, wall, impact), step rate (120, 200 Hz):
# both backends (Jolt, PhysX articulation), 2 runs each for the determinism hash, CCD on, 1 thread, one episode, no warm-up.
# Extra: worst-axis hold (every hinge axis parallel to gravity), and the short-ton weight reading for h85m.
set -u
BUILD="${1:?build dir}"
EXE="$BUILD/chain-limb/chain_limb.exe"
OUT="${OUT_DIR:-benchmark/physics-articulated/chain-limb/results/limb}"
mkdir -p "$OUT"
NOTE="${2:-NOT VALID FOR TIMING: machine was busy (correctness and onset only, no timing claim)}"
run() {  # tag, args...
  local tag="$1"; shift
  "$EXE" --scenario limb --backend both --runs 2 --warmup 0 --ccd on --threads 1 --tag "$tag" --note "$NOTE" --out "$OUT/$tag.json" "$@" > "$OUT/$tag.log" 2>&1 || echo "FAILED: $tag"
}
sec_of() { case "$1" in swing) echo 8;; hold) echo 4;; *) echo 2;; esac; }
for g in "h76m" "h85m" "h96m" "h182m"; do
  gt="${g// /_}"
  for limb in leg arm; do
    for c in hold swing wall impact; do
      for hz in 120 200; do
        s=$(sec_of $c)
        run "limb-$limb-$gt-$c-${hz}hz" --size_label "$g" --limb $limb --limb-case $c --hz $hz --episode-sec $s --steps $((s * hz))
      done
    done
    for hz in 120 200; do  # worst axis: hinge axes parallel to gravity, as in the original chain; leg stands, arm in the T-pose (horizontal, like the old chain)
      run "limb-$limb-$gt-hold-worstaxis-${hz}hz" --size_label "$g" --limb $limb --limb-case hold --worst-axes on --hz $hz --episode-sec 4 --steps $((4 * hz))
    done
  done
done
g="h85m"; gt="h85m"
for limb in leg arm; do
  for c in hold swing wall impact; do
    for hz in 120 200; do
      s=$(sec_of $c)
      run "limb-$limb-$gt-$c-${hz}hz-shortton" --size_label "$g" --limb $limb --limb-case $c --hz $hz --episode-sec $s --steps $((s * hz)) --ton-kg 907.185
    done
  done
done
