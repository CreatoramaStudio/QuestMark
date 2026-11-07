#!/usr/bin/env bash
# Run the full SPEC protocol (benchmark/SPEC.md) for one engine variant at a time, strictly sequential.
# Usage: run-all.sh godot|stride [--dry-run] [--stages=ABCDEFG]
#   Stages: A pit-2000, B raycast-1000/10000, C chains, D mixed (each physics+render, RUNS runs), E unlock (3 variants),
#           F scale ladder, G debris rate ladder (+ debris-pooled).
#   --dry-run (or DRY_RUN=1): print every command and the planned runtime, run nothing.
# Environment:
#   GODOT_BENCH_EXE    exported Godot release exe (QuestBench.exe, .pck and DLL next to it). One exe runs both Jolt profiles: this script passes
#                      --jolt-limits=old explicitly for A-E (10240 / 65536 / 20480) and --jolt-limits=raised for F and G (70000 / 1048576 /
#                      1048576; raised caps cost about +20 percent tick time in A-D, and G needs them: old caps ignored contacts at R=8 and cap
#                      bodies at 10240). Stride has no such limits and gets no flag.
#   BENCH_ENGINE       Godot physics engine under test: jolt (default) | physx | godotphysics. The script writes override.cfg next to the exe
#                      for every run (physics/3d/physics_engine = "Jolt Physics" | PhysX | GodotPhysics3D) and ABORTS when the engine_tag the
#                      run recorded in env.txt (proved from the class of get_direct_space_state()) differs from the requested engine:
#                      a PhysX request on a build without the module silently runs GodotPhysics. The Jolt limit flags are passed for jolt only.
#   BENCH_FLAVOUR      optional label of the build (for example a fork name; letters, digits, underscore). File prefix: godot-<engine>[-<flavour>]-
#                      (jolt without flavour keeps godot-jolt-). The label is also passed to the exe as --flavour.
#   Profiles (SPEC "Profiles"): A (pit-2000), D (mixed), E (unlock) and F (scale) run in both profiles, game and stress (file names get
#                      a "-stress" token after the prefix, game keeps the plain names); B, C and G run in game only.
#   STRIDE_BENCH_EXE   StrideBench.exe (JIT build output or Native AOT publish output)
#   STRIDE_FLAVOR      backend label: bepu (default, JIT) or bepu_aot (AOT). Letters, digits and underscore only (no hyphen).
#   OUT_DIR            output directory (default: <benchmark>/results)
#   WARMUP / MEASURE / RUNS   seconds and repeats for A-E (default 5 / 20 / 3 = SPEC)
#   SCALE_WARMUP (5) SCALE_MEASURE (10)  F base values; warm-up grows with N from LAG_N (see warmup_for)
#   LAG_N              first N that needs the long warm-up (default 4000 godot, 16000 stride, from the NOTES findings)
#   G_WARMUP (10) G_MEASURE (20)  DEBRIS_POOLED (1; 0 skips debris-pooled)
#   DRY_RUN=1, SIM_PASS20 / SIM_PASS33 (F) and SIM_G_PASS20 / SIM_G_PASS33 (G): assumed capacities for the dry-run plan only (defaults for F:
#                      2000 / 4000 at 200 Hz, an assumption from one smoke run: 3.3 ms per step at 2000 awake bodies against a 5 ms budget).
#   Physics runs at a fixed 200 Hz and the render loop is unlocked (no vsync, no frame cap), so a frame carries a varying number of steps. Sanity
#   checks therefore count steps (CSV column steps_per_frame) against 200 x measured seconds; the pass rule is in merge-results.pl (SPEC "Frame rate").
# Each run restarts the process and writes into a staging folder first. A run that crashes or fails the sanity check is kept as
# <name>.failed-1.* and rerun once; the log (OUT_DIR/run-all-<prefix>.log) records both. Scale and debris levels that fail twice
# count as a failed stage (the ladder treats them as the first fail).
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
engine=""; DRY="${DRY_RUN:-0}"; STAGES="${STAGES:-ABCDEFG}"
for a in "$@"; do case "$a" in
  --dry-run) DRY=1 ;;
  --stages=*) STAGES="${a#--stages=}" ;;
  godot|stride) engine="$a" ;;
  *) echo "usage: run-all.sh godot|stride [--dry-run] [--stages=ABCDEFG]" >&2; exit 2 ;;
esac; done
[ -n "$engine" ] || { echo "usage: run-all.sh godot|stride [--dry-run] [--stages=ABCDEFG]" >&2; exit 2; }

OUT_DIR="${OUT_DIR:-$here/../results}"
WARMUP="${WARMUP:-5}"; MEASURE="${MEASURE:-20}"; RUNS="${RUNS:-3}"
SCALE_WARMUP="${SCALE_WARMUP:-5}"; SCALE_MEASURE="${SCALE_MEASURE:-10}"
G_WARMUP="${G_WARMUP:-10}"; G_MEASURE="${G_MEASURE:-20}"; DEBRIS_POOLED="${DEBRIS_POOLED:-1}"
SCALE_LADDER="1000 2000 4000 8000 16000 32000 64000"; G_LADDER="1 2 4 8 16 32"
OVERHEAD_S=2   # process start and exit per run (A-D logs: 25 to 27 s for 5 + 20 s runs); scale adds N/2000 s for building (assumption)
if [ "$DRY" != 1 ]; then mkdir -p "$OUT_DIR"; OUT_DIR="$(cd "$OUT_DIR" && pwd)"; fi
if command -v cygpath >/dev/null 2>&1; then to_win() { cygpath -w "$1"; }; else to_win() { echo "$1"; }; fi
case "$engine" in
  godot)
    BENCH_ENGINE="${BENCH_ENGINE:-jolt}"; export BENCH_ENGINE; export BENCH_FLAVOUR="${BENCH_FLAVOUR:-}"
    case "$BENCH_ENGINE" in jolt|physx|godotphysics) ;; *) echo "BENCH_ENGINE must be jolt|physx|godotphysics: $BENCH_ENGINE" >&2; exit 2 ;; esac
    flavor="$BENCH_ENGINE${BENCH_FLAVOUR:+-$BENCH_FLAVOUR}"; prefix="godot-$flavor"; exe="${GODOT_BENCH_EXE:-}"
    LAG_N="${LAG_N:-4000}"; SIM_PASS20="${SIM_PASS20:-2000}"; SIM_PASS33="${SIM_PASS33:-4000}" ;;
  stride)
    flavor="${STRIDE_FLAVOR:-bepu}"; prefix="stride-$flavor"; exe="${STRIDE_BENCH_EXE:-}"
    LAG_N="${LAG_N:-16000}"; SIM_PASS20="${SIM_PASS20:-2000}"; SIM_PASS33="${SIM_PASS33:-4000}" ;;
esac
SIM_G_PASS20="${SIM_G_PASS20:-8}"; SIM_G_PASS33="${SIM_G_PASS33:-16}"
case "${BENCH_FLAVOUR:-}${STRIDE_FLAVOR:-}" in *[!A-Za-z0-9_]*) echo "flavour label must be [A-Za-z0-9_]+ (no hyphen)" >&2; exit 2 ;; esac
PROF=game; PROF_TOK=""    # current profile (game|stress); set by the stage loops
if [ "$DRY" != 1 ]; then
  [ -n "$exe" ] || { echo "set GODOT_BENCH_EXE / STRIDE_BENCH_EXE" >&2; exit 2; }
  [ -f "$exe" ] || { echo "exe not found: $exe" >&2; exit 2; }
fi
log="$OUT_DIR/run-all-$prefix.log"; stage="$OUT_DIR/.stage-$prefix"
QUIET=0; TOTAL_S=0; NRUNS=0; fails=0
say() { [ "$QUIET" = 1 ] && return; if [ "$DRY" = 1 ]; then echo "$*"; else echo "$(date +%H:%M:%S) $*" | tee -a "$log"; fi; }
has_stage() { case "$STAGES" in *"$1"*) return 0 ;; *) return 1 ;; esac; }

# ---- warm-up that covers the fall (godot NOTES: N=4000 paced needs 8 s; fall time sqrt(2h/g), h = layers x 0.6 m + 1) ----
warmup_for() { # N -> seconds
  awk -v n="$1" -v lag="$LAG_N" -v base="$SCALE_WARMUP" 'BEGIN {
    if (n < lag) { print base; exit }
    h = (n/144.0)*0.6 + 1; f = sqrt(2*h/9.81); w = int(2*f + 4 + 0.999); if (w < base) w = base; print w }'
}
num_gt() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a > b) }'; }
num_lt() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a < b) }'; }
limits_for() { case "$1" in scale|debris|debris-pooled) echo raised ;; *) echo old ;; esac; }
g_tries() { case "$1" in 16|32|[2-9][0-9]*|1[6-9]*) echo 3 ;; *) echo 2 ;; esac; }
mid_of() { awk -v a="$1" -v b="$2" -v r="$3" 'BEGIN { m=(a+b)/2; if (r>0) m=int(m/r+0.5)*r; printf "%g", m }'; }

# ---- one process ----
launch() { # exe scenario mode run warm measure extra headless(0|1) outdir
  local x="$1" sc="$2" mode="$3" run="$4" w="$5" m="$6" extra="$7" hl="$8" od="$9" pre=""
  [ "$hl" = 1 ] && pre="--headless -- "
  [ "$engine" = godot ] && [ "$hl" != 1 ] && pre="-- "
  [ "$engine" = stride ] && pre=""
  if [ "$engine" = godot ]; then
    local pe; case "${BENCH_ENGINE:-jolt}" in physx) pe=PhysX ;; godotphysics) pe=GodotPhysics3D ;; *) pe="Jolt Physics" ;; esac
    printf '[physics]\n\n3d/physics_engine="%s"\n' "$pe" > "$(dirname "$x")/override.cfg"
    [ -n "${BENCH_FLAVOUR:-}" ] && extra="$extra --flavour=$BENCH_FLAVOUR"
  fi
  "$x" $pre --scenario="$sc" --mode="$mode" --run="$run" --warmup="$w" --measure="$m" $extra --out="$od" >>"$log" 2>&1
}
cmdline() { # for dry-run display
  local x="$1" sc="$2" mode="$3" run="$4" w="$5" m="$6" extra="$7" hl="$8" pre=""
  [ "$engine" = godot ] && { [ "$hl" = 1 ] && pre="--headless -- " || pre="-- "; }
  [ "$engine" = godot ] && extra="$extra${BENCH_FLAVOUR:+ --flavour=$BENCH_FLAVOUR}"
  echo "\"${x:-<exe>}\" ${pre}--scenario=$sc --mode=$mode --run=$run --warmup=$w --measure=$m${extra:+ $extra} --out=<stage>  [override.cfg engine: ${BENCH_ENGINE:-n/a}]"
}

check() { # kind mode csv measure -> prints problem or nothing. kind: abcd | unlock-render | unlock-phys | paced
  local kind="$1" mode="$2" csv="$3" measure="$4" want=$(( ${4%.*}*200 ))
  [ -s "$csv" ] || { echo "no csv"; return; }
  # steps_per_frame is the second-to-last CSV column (physics_step_ms is last). The render loop is unlocked, so rows vary; steps do not.
  awk -F, -v want="$want" -v kind="$kind" -v mode="$mode" -v eng="$engine" -v sc="$5" '
    NR==1 { if ($(NF-1) != "steps_per_frame") nostep=1; next }
    { n++; for (i=1;i<=9;i++) if ($i ~ /[Nn][Aa][Nn]|[Ii]nf/ || $i == "") bad=1
      st += $(NF-1); if ($9+0 > am) am=$9+0 }
    END {
      if (bad) { print "NaN/inf/empty field"; exit }
      if (nostep) { print "CSV has no steps_per_frame column (60 Hz build?)"; exit }
      if (kind == "abcd") {
        # crash or hang detection only (RTF below 1 is a result, judged by merge-results.pl): 0.5 x .. 1.05 x the expected 200 Hz step count
        if (n < 10) { print "rows " n " < 10"; exit }
        if (st < 0.5*want) { print "steps " st " < " 0.5*want " (half of 200 Hz x measure s)"; exit }
        if (st > 1.05*want + 20) { print "steps " st " > " 1.05*want+20 " (pacing broken)"; exit }
        if (sc !~ /^raycast/ && am == 0) { print "awake_bodies 0 in dynamic scenario"; exit }
      } else if (kind == "unlock-render") {
        if (n < 100) { print "rows " n " < 100"; exit }
        if (am == 0) { print "awake_bodies 0 in dynamic scenario"; exit }
      } else if (kind == "unlock-phys") {
        if (n < 100) { print "rows " n " < 100"; exit }   # piles may be fully asleep in the window, so no awake check
      } else {
        if (n < 5) { print "rows " n " < 5"; exit }
        if (am == 0) { print "awake_bodies 0 in dynamic scenario"; exit }
      }
    }' "$csv"
}

# move the staged files of one run to their final names (prefix label, Stride-style scale-unlock-N token)
publish_stage() {
  local f bn nn rx
  # stage files are named by the exe: godot-<recorded engine tag>[-flavour][-stress]-<token>... ; stride-<backend>[-stress]-<token>...
  if [ -n "${BENCH_FLAVOUR:-}" ] && [ "$engine" = godot ]; then rx="^godot-[^-]+-$BENCH_FLAVOUR(-stress)?-"; else rx="^(godot|stride)-[^-]+(-stress)?-"; fi
  for f in "$stage"/*; do [ -e "$f" ] || continue
    bn="$(basename "$f")"
    nn="$(echo "$bn" | sed -E "s/$rx//; s/^scale-([0-9]+)-unlock-physics-/scale-unlock-\1-physics-/")"
    mv -f "$f" "$OUT_DIR/$prefix$PROF_TOK-$nn"
  done
}

# Godot only: the engine proved at run time (env.txt engine_tag) must equal the requested BENCH_ENGINE, else ABORT the whole run.
verify_engine() {
  [ "$engine" = godot ] || return 0
  local ef tag; ef="$(ls "$stage"/*.env.txt 2>/dev/null | head -1)"
  [ -n "$ef" ] || return 0
  tag="$(sed -n 's/^engine_tag:[[:space:]]*//p' "$ef" | head -1 | tr -d '\r')"
  if [ "$tag" != "$BENCH_ENGINE" ]; then
    say "ABORT: requested BENCH_ENGINE=$BENCH_ENGINE but the run recorded engine_tag=${tag:-<none>} (a PhysX request on a build without the module runs GodotPhysics). Files kept in $stage."
    exit 3
  fi
}

CELL_PROB=""; CELL_RC=0
# run_cell kind token scenario mode run warm measure extra headless exe timeoutsecs checksc
# one cell with the one-rerun rule; sets CELL_PROB (empty = fine); csv path is $OUT_DIR/$prefix-$token-$mode-$run.csv
run_cell() {
  local kind="$1" token="$2" sc="$3" mode="$4" run="$5" w="$6" m="$7" extra="$8" hl="$9" x="${10}" to="${11}" chk="${12}"
  local base="$OUT_DIR/$prefix$PROF_TOK-$token-$mode-$run" attempt t0 rc prob f
  extra="$extra --profile=$PROF"
  [ "$engine" = godot ] && [ "$BENCH_ENGINE" = jolt ] && extra="$extra --jolt-limits=$(limits_for "$sc")"
  extra="${extra# }"
  CELL_PROB=""; CELL_RC=0
  if [ "$DRY" = 1 ]; then
    say "run $prefix$PROF_TOK $token $mode $run: $(cmdline "$x" "$sc" "$mode" "$run" "$w" "$m" "$extra" "$hl")"
    TOTAL_S=$(( TOTAL_S + ${w%.*} + ${m%.*} + OVERHEAD_S + ${CELL_N:-0}/2000 ))
    NRUNS=$((NRUNS+1)); return
  fi
  local maxtry="${MAXTRY:-2}"
  for attempt in $(seq 1 "$maxtry"); do
    rm -f "$base".csv "$base".csv.summary.txt "$base".env.txt; rm -rf "$stage"; mkdir -p "$stage"
    say "run $prefix$PROF_TOK $token $mode $run attempt $attempt"
    t0=$(date +%s)
    export BENCH_ENGINE BENCH_FLAVOUR
    timeout "$to" bash -c "$(declare -f launch); exe='$x'; engine='$engine'; log='$log'; launch '$x' '$sc' '$mode' '$run' '$w' '$m' '$extra' '$hl' '$(to_win "$stage")'"
    rc=$?
    verify_engine
    publish_stage; rm -rf "$stage"
    prob="$(check "$kind" "$mode" "$base.csv" "$m" "$chk")"
    [ "$rc" -ne 0 ] && prob="exit code $rc ${prob:+; $prob}"
    say "  rc=$rc secs=$(( $(date +%s) - t0 )) ${prob:+PROBLEM: $prob}"
    CELL_PROB="$prob"; CELL_RC=$rc
    [ -z "$prob" ] && return
    if [ "$attempt" -lt "$maxtry" ]; then
      for f in "$base".*; do [ -e "$f" ] && mv "$f" "${f/$prefix$PROF_TOK-$token-$mode-$run/$prefix$PROF_TOK-$token-$mode-$run.failed-1}"; done
    else
      say "  FAILED $maxtry TIMES (last attempt kept in place, flagged here): $token $mode $run"; fails=$((fails+1))
    fi
  done
}

# profile helpers: A, D, E, F run in game and stress; B, C, G in game only (SPEC "Profiles")
set_profile() { PROF="$1"; if [ "$1" = stress ]; then PROF_TOK="-stress"; else PROF_TOK=""; fi; }
profiles_for() { case "$1" in pit-2000|mixed|scale) echo "game stress" ;; *) echo game ;; esac; }

# ---- A to D ----
run_abcd() {
  local sc mode run list=""
  has_stage A && list="$list pit-2000"; has_stage B && list="$list raycast-1000 raycast-10000"
  has_stage C && list="$list chains"; has_stage D && list="$list mixed"
  for sc in $list; do for p in $(profiles_for "$sc"); do set_profile "$p"; for mode in physics render; do for run in $(seq 1 "$RUNS"); do
    local hl=0; [ "$mode" = physics ] && hl=1
    CELL_N=0 run_cell abcd "$sc" "$sc" "$mode" "$run" "$WARMUP" "$MEASURE" "" "$hl" "$exe" 300 "$sc"
  done; done; done; done; set_profile game
}

# ---- E ----
run_e() {
  local t mode run hl kind
  for t in "unlock-render-mixed render" "unlock-physics-mixed physics" "unlock-physics-pit-2000 physics"; do
    set -- $t; local sc="$1" mode="$2"
    # Unpaced Godot runs need a real window (no --headless, godot NOTES); the render variant too.
    kind=unlock-phys; [ "$mode" = render ] && kind=unlock-render
    for p in game stress; do set_profile "$p"; for run in $(seq 1 "$RUNS"); do
      CELL_N=0 run_cell "$kind" "$sc" "$sc" "$mode" "$run" "$WARMUP" "$MEASURE" "" 0 "$exe" 300 "$sc"
    done; done
  done
  set_profile game
}

# ---- ladders (F and G share the logic) ----
P20=0; P2=0; STAGE_CRASH=0
sim_pass() { # value pass20max pass33max
  if [ "${SIM_ALL:-0}" = 1 ]; then P20=1; P2=1; return; fi
  case "$1" in *.*) ;; *) P20=$(( $1 <= $2 ? 1 : 0 )); P2=$(( $1 <= $3 ? 1 : 0 )); return ;; esac
  P20=$(awk -v v="$1" -v m="$2" 'BEGIN { print (v <= m) ? 1 : 0 }'); P2=$(awk -v v="$1" -v m="$3" 'BEGIN { print (v <= m) ? 1 : 0 }')
}
eval_cell() { # csv lim2 simvalue sim20 sim33 -> sets P20 P2 STAGE_CRASH
  STAGE_CRASH=0
  if [ "$DRY" = 1 ]; then sim_pass "$3" "$4" "$5"; return; fi
  if [ -n "$CELL_PROB" ]; then P20=0; P2=0; STAGE_CRASH=1; say "  stage counted as FAILED (crash or sanity problem): $CELL_PROB"; return; fi
  local out; out="$(perl "$here/merge-results.pl" --eval "$1" "$2")"
  say "  eval: $out"
  P20="$(echo "$out" | sed -E 's/.*pass20=([01]).*/\1/')"; P2="$(echo "$out" | sed -E 's/.*pass2=([01]).*/\1/')"
}

# ladder <family F|G> <mode> : uses LEVELS, fills TESTED (all tested values), sets L20 F20 L2 F2
ladder() {
  local fam="$1" mode="$2" v i
  L20=0; F20=0; L2=0; F2=0; TESTED=""; LCRASH=0
  for v in $LEVELS; do
    stage_once "$fam" "$mode" "$v" 1
    TESTED="$TESTED $v"
    if [ "$P20" = 1 ]; then [ "$F20" = 0 ] && L20="$v"; else [ "$F20" = 0 ] && F20="$v"; fi
    if [ "$P2" = 1 ]; then [ "$F2" = 0 ] && L2="$v"; else [ "$F2" = 0 ] && F2="$v"; fi
    [ "$STAGE_CRASH" = 1 ] && { LCRASH="$v"; break; }
    [ "$F2" != 0 ] && break      # failing the looser limit ends the climb (it also fails the 20 ms rule)
  done
  # one midpoint stage between last pass and first fail, for each limit (the 20 ms midpoint also informs the looser limit)
  local m
  if [ "$L20" != 0 ] && [ "$F20" != 0 ]; then
    m="$(mid_of "$L20" "$F20" "$MIDROUND")"
    if ! echo " $TESTED " | grep -q " $m "; then
      stage_once "$fam" "$mode" "$m" 1; TESTED="$TESTED $m"
      if [ "$P20" = 1 ]; then L20="$m"; else F20="$m"; fi
      if [ "$P2" = 1 ]; then num_gt "$m" "$L2" && L2="$m"; else num_lt "$m" "$F2" && F2="$m"; fi
    fi
  fi
  if [ "$L2" != 0 ] && [ "$F2" != 0 ]; then
    m="$(mid_of "$L2" "$F2" "$MIDROUND")"
    if ! echo " $TESTED " | grep -q " $m "; then
      stage_once "$fam" "$mode" "$m" 1; TESTED="$TESTED $m"
      if [ "$P2" = 1 ]; then L2="$m"; else F2="$m"; fi
    fi
  fi
  # 3 runs of the last passing and the first failing stage (20 ms rule); run 1 exists already
  for v in $L20 $F20; do
    [ "$v" = 0 ] && continue; [ "$v" = "$LCRASH" ] && continue
    for i in $(seq 2 "$RUNS"); do stage_once "$fam" "$mode" "$v" "$i"; done
  done
  say "LADDER $fam $mode profile=$PROF: last pass 20ms=$L20 first fail 20ms=$F20 last pass limit2=$L2 first fail limit2=$F2 tested:$TESTED$([ "$LCRASH" != 0 ] && echo " (crash at $LCRASH)")"
}

stage_once() { # fam mode value run  (paced stage; sets P20 P2)
  local fam="$1" mode="$2" v="$3" run="$4" hl=0 w m
  [ "$engine" = godot ] && [ "$mode" = physics ] && hl=1
  if [ "$fam" = F ]; then
    w="$(warmup_for "$v")"; m="$SCALE_MEASURE"
    local x="$exe" to=$(( w + m + 120 + v/100 ))
    CELL_N="$v" run_cell paced "scale-$v" scale "$mode" "$run" "$w" "$m" "--bodies=$v" "$hl" "$x" "$to" "scale"
    eval_cell "$OUT_DIR/$prefix$PROF_TOK-scale-$v-$mode-$run.csv" 33.3 "$v" "$SIM_PASS20" "$SIM_PASS33"
  else
    CELL_N=0 MAXTRY=$(g_tries "$v") run_cell paced "debris-$v" debris "$mode" "$run" "$G_WARMUP" "$G_MEASURE" "--rate=$v" "$hl" "$exe" 300 "debris"
    eval_cell "$OUT_DIR/$prefix$PROF_TOK-debris-$v-$mode-$run.csv" 36 "$v" "$SIM_G_PASS20" "$SIM_G_PASS33"
  fi
}

run_f() {
  local mode v w p
  LEVELS="$SCALE_LADDER"; MIDROUND=100
  for p in $(profiles_for scale); do
  set_profile "$p"
  local physics_tested=""
  for mode in physics render; do
    ladder F "$mode"
    [ "$mode" = physics ] && physics_tested="$TESTED"
  done
  # unpaced pass (scale-unlock-physics) for every stage tested in paced physics mode, 1 run each
  for v in $physics_tested; do
    w="$(warmup_for "$v")"; local hl=0
    CELL_N="$v" run_cell unlock-phys "scale-unlock-$v" scale physics 1 "$w" "$SCALE_MEASURE" "--bodies=$v --unpaced=1" 0 "$exe" $(( w + SCALE_MEASURE + 120 + v/100 )) "scale"
  done
  done
  set_profile game
}

run_g() {
  local mode v best
  LEVELS="$G_LADDER"; MIDROUND=0
  declare -A bestR
  for mode in physics render; do
    ladder G "$mode"; bestR[$mode]="$L20"
  done
  if [ "$DEBRIS_POOLED" = 1 ]; then
    for mode in physics render; do
      best="${bestR[$mode]}"; [ "$best" = 0 ] && { say "no passing R in $mode, debris-pooled skipped"; continue; }
      local hl=0; [ "$engine" = godot ] && [ "$mode" = physics ] && hl=1
      CELL_N=0 MAXTRY=$(g_tries "$best") run_cell paced "debris-pooled-$best" debris-pooled "$mode" 1 "$G_WARMUP" "$G_MEASURE" "--rate=$best" "$hl" "$exe" 300 "debris"
    done
  fi
}

run_protocol() {
  say "== $prefix exe=$(basename "${exe:-<exe>}") stages=$STAGES warmup=$WARMUP measure=$MEASURE runs=$RUNS out=$OUT_DIR"
  if [ "$DRY" != 1 ]; then
    { echo "$(date +%Y-%m-%dT%H:%M:%S) prefix=$prefix engine=$engine flavor=$flavor bench_engine=${BENCH_ENGINE:-n/a} bench_flavour=${BENCH_FLAVOUR:-} exe=$(basename "$exe") stages=$STAGES profiles=A,D,E,F:game+stress B,C,G:game"; } >> "$OUT_DIR/variant-$prefix.txt"
  fi
  has_stage A || has_stage B || has_stage C || has_stage D && run_abcd
  has_stage E && run_e
  has_stage F && run_f
  has_stage G && run_g
}

fmt_time() { awk -v s="$1" 'BEGIN { printf "%.0f min (%.2f h)", s/60, s/3600 }'; }

if [ "$DRY" = 1 ]; then
  run_protocol
  exp_s="$TOTAL_S"; exp_n="$NRUNS"; exp_p20="$SIM_PASS20"; exp_p33="$SIM_PASS33"; exp_g20="$SIM_G_PASS20"; exp_g33="$SIM_G_PASS33"
  # range of the plan over every possible capacity (ladder fails first at level k): the plan depends on where the ladders stop
  QUIET=1; lo_s=""; hi_s=""; lo_n=0; hi_n=0
  read -r -a SL <<< "0 $SCALE_LADDER 1000000"; read -r -a GL <<< "0 $G_LADDER 1000000"
  for k in 0 1 2 3 4 5 6 7; do
    TOTAL_S=0; NRUNS=0
    SIM_PASS20="${SL[$k]}"; SIM_PASS33="${SL[$((k+1))]}"; gk=$k; [ "$gk" -gt 6 ] && gk=6
    SIM_G_PASS20="${GL[$gk]}"; SIM_G_PASS33="${GL[$((gk+1))]}"
    run_protocol
    if [ -z "$lo_s" ] || awk -v a="$TOTAL_S" -v b="$lo_s" 'BEGIN { exit !(a < b) }'; then lo_s="$TOTAL_S"; lo_n="$NRUNS"; fi
    if [ -z "$hi_s" ] || awk -v a="$TOTAL_S" -v b="$hi_s" 'BEGIN { exit !(a > b) }'; then hi_s="$TOTAL_S"; hi_n="$NRUNS"; fi
  done
  QUIET=0
  echo "ESTIMATE $prefix stages=$STAGES: expected $(fmt_time "$exp_s") in $exp_n runs (assumed capacities: scale pass20<=$exp_p20 pass33<=$exp_p33, debris pass20<=$exp_g20 pass33<=$exp_g33; process overhead ${OVERHEAD_S}s + N/2000 s per scale run, all assumptions)"
  echo "ESTIMATE $prefix stages=$STAGES: range over all possible ladder stops: $(fmt_time "$lo_s") ($lo_n runs) to $(fmt_time "$hi_s") ($hi_n runs)"
  exit 0
fi

# real run: print the plan estimate first (same code path as --dry-run)
"$0" "$engine" --dry-run --stages="$STAGES" 2>/dev/null | grep '^ESTIMATE' | tee -a "$log"
run_protocol
say "== $prefix done, failed cells: $fails"
