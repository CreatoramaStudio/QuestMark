#!/usr/bin/env bash
# Optional setup of the Godot PhysX fork benchmark variant (flavours physx / physxrid): one command from a fresh clone.
#
# STATUS: written by comparing every command line with the scripts that worked on the dev machine (phase2-physx.bat,
# phase2-godot.sh, physx-97bc4de/build.sh, tools/build-godot-ext.sh, tools/export-godot-fork.sh). The FULL FROM-SCRATCH BUILD WAS NOT RUN
# END TO END when this script was written. Verified by running: bash -n, --dry-run, prerequisite checks, and the skip logic against the dev
# machine's existing artifacts. Verified only by comparison with the working scripts: clone/fetch of the pinned commits into empty folders,
# the venv/scons install, the PhysX cmake build via the .bat helper, the scons builds, the API dump, the extension build and the export when run
# from empty folders. Expect to fix small things on the first real run.
#
# Usage: bash benchmark/tools/setup-physx.sh [--dry-run] [--force] [--only=<step>]
#   steps (in order): prereq clone patch physx fork dump ext export env
#   --dry-run    print the plan and the commands, change nothing (no clone, no venv, no build, no env.sh write)
#   --force      redo the selected steps even when their outputs exist (physx/fork rebuild from the existing build trees)
#   --only=STEP  run one step (prereq is always checked first, silently)
# Warnings: the PhysX SDK build takes about 20 min and the two Godot fork builds (editor + template_release, clang, no LTO) about 1 to 2 hours
# on a 16-thread desktop; plan for several hours on a weaker machine. Needs about 60 GB free disk. CPU only (no physx_gpu, no CUDA).
# Environment overrides (relative paths are taken from the project root):
#   DEPS_DIR          default .tmp/deps
#   PHYSX_SDK_DIR     PhysX clone (repo root, contains physx/). Default $DEPS_DIR/PhysX. Reused as is when it exists.
#   FORK_DIR          Godot PhysX fork checkout. Default $DEPS_DIR/godot-physx-fork. Reused as is when it exists.
#   GODOT_CPP_DIR     godot-cpp checkout (default $DEPS_DIR/godot-cpp, shared with setup.sh)
#   PHYSX_BUILD_DIR   PhysX CMake build tree (default $DEPS_DIR/physx-clang-build)
#   API_DIR           folder for the fork's extension_api.json (default $DEPS_DIR/api-fork)
#   EXT_OUT_DIR       extension DLL folder (default $DEPS_DIR/godotext-fork-bin)
#   EXT_BUILD_ROOT    extension build tree (default .tmp/godotext-build-fork)
#   EXPORT_ROOT       export root (default .tmp/export-fork), output $EXPORT_ROOT/physx/QuestBench.exe
#   ENV_FILE          env file that gets PHYSX_BENCH_EXE (default .tmp/env.sh)
#   PYTHON            python interpreter for scons (default: python on PATH if it has SCons, else a venv in $DEPS_DIR/venv)
#   JOBS              parallel jobs (default: nproc; the dev machine used 16)
# Pins: PhysX https://github.com/NVIDIA-Omniverse/PhysX @ 4f2103c3a9052906296defb12166753450ef787c (dev clone, shallow),
#       fork https://github.com/uno1982/godot @ 97bc4dedd7a7bc596a151f375b8372044ae6dbf0, godot-cpp @ b472519c42be09c1acec3fd63acd4010fc586ad4.
# Logs: .tmp/logs/setup-physx-<step>.log. Scratch only under .tmp/.
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
cd "$root"

PHYSX_URL=https://github.com/NVIDIA-Omniverse/PhysX;  PHYSX_PIN=4f2103c3a9052906296defb12166753450ef787c
FORK_URL=https://github.com/uno1982/godot;            FORK_PIN=97bc4dedd7a7bc596a151f375b8372044ae6dbf0
CPP_URL=https://github.com/godotengine/godot-cpp;     CPP_PIN=b472519c42be09c1acec3fd63acd4010fc586ad4
STEPS="prereq clone patch physx fork dump ext export env"

DRY=0; FORCE=0; ONLY=""
for a in "$@"; do
  case "$a" in
    --dry-run) DRY=1 ;; --force) FORCE=1 ;; --only=*) ONLY="${a#--only=}" ;;
    -h|--help) sed -n '2,36p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "unknown argument: $a" >&2; exit 2 ;;
  esac
done
if [ -n "$ONLY" ]; then case " $STEPS " in *" $ONLY "*) ;; *) echo "unknown step: $ONLY (steps: $STEPS)" >&2; exit 2 ;; esac; fi

abs() { case "$1" in ""|/*|[A-Za-z]:*) echo "$1" ;; *) echo "$root/$1" ;; esac; }
w() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
DEPS_DIR="$(abs "${DEPS_DIR:-.tmp/deps}")"
PHYSX_SDK_DIR="$(abs "${PHYSX_SDK_DIR:-$DEPS_DIR/PhysX}")"
FORK_DIR="$(abs "${FORK_DIR:-$DEPS_DIR/godot-physx-fork}")"
GODOT_CPP_DIR="$(abs "${GODOT_CPP_DIR:-$DEPS_DIR/godot-cpp}")"
PHYSX_BUILD_DIR="$(abs "${PHYSX_BUILD_DIR:-$DEPS_DIR/physx-clang-build}")"
API_DIR="$(abs "${API_DIR:-$DEPS_DIR/api-fork}")"
EXT_OUT_DIR="$(abs "${EXT_OUT_DIR:-$DEPS_DIR/godotext-fork-bin}")"
EXT_BUILD_ROOT="$(abs "${EXT_BUILD_ROOT:-.tmp/godotext-build-fork}")"
EXPORT_ROOT="$(abs "${EXPORT_ROOT:-.tmp/export-fork}")"
ENV_FILE="$(abs "${ENV_FILE:-.tmp/env.sh}")"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 16)}"
LOGDIR="$root/.tmp/logs"
PX_INSTALL="$PHYSX_SDK_DIR/physx/install/vc17clang-godot/PhysX"
PX_LIB="$PX_INSTALL/bin/win.x86_64.vc143.mt/release/PhysX_static_64.lib"
SFX=llvm.physx
EXE_TR="$FORK_DIR/bin/godot.windows.template_release.x86_64.$SFX.exe"
EXE_ED="$FORK_DIR/bin/godot.windows.editor.x86_64.$SFX.console.exe"
API_JSON="$API_DIR/extension_api.json"
EXT_DLL="$EXT_OUT_DIR/libquestbench.windows.template_release.x86_64.dll"
EXPORT_EXE="$EXPORT_ROOT/physx/QuestBench.exe"

rel() { case "$1" in "$root"/*) echo "${1#$root/}" ;; *) echo "$1" ;; esac; }
n=0; total=0
for s in $STEPS; do if [ -z "$ONLY" ] || [ "$s" = "$ONLY" ]; then total=$((total+1)); fi; done
want() { [ -z "$ONLY" ] || [ "$ONLY" = "$1" ]; }
step() { n=$((n+1)); echo "[$n/$total] $1: $2"; }
skip() { echo "    skip: $1"; }
say() { echo "    $*"; }
# run a command, log to .tmp/logs/setup-physx-<step>.log, show the tail on failure
logrun() {
  local s="$1"; shift
  if [ "$DRY" = 1 ]; then say "would run: $*"; return 0; fi
  mkdir -p "$LOGDIR"; local lg="$LOGDIR/setup-physx-$s.log"
  say "running (log $(rel "$lg")): $*"
  if "$@" >> "$lg" 2>&1; then return 0; fi
  echo "FAILED ($s). Last lines of $(rel "$lg"):" >&2; tail -n 25 "$lg" >&2; exit 1
}
die() { echo "ERROR: $*" >&2; exit 1; }

# ---- tool detection (used by prereq and later steps) ----
VSROOT=""; VCVARS=""; NINJADIR=""; LLVMBIN=""; PYBIN=""; COMPILER=""; NINJA_OK=0
detect_tools() {
  local vsw="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
  if [ -x "$vsw" ]; then
    VSROOT="$("$vsw" -version '[17,18)' -products '*' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>/dev/null | tr -d '\r' | head -n 1)"
  fi
  [ -n "$VSROOT" ] && VSROOT="$(cygpath -u "$VSROOT" 2>/dev/null || echo "$VSROOT")"
  [ -n "$VSROOT" ] && [ -f "$VSROOT/VC/Auxiliary/Build/vcvars64.bat" ] && VCVARS="$VSROOT/VC/Auxiliary/Build/vcvars64.bat"
  if command -v ninja >/dev/null 2>&1; then NINJADIR=""; NINJA_OK=1
  elif [ -n "$VSROOT" ] && [ -f "$VSROOT/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" ]; then
    NINJADIR="$VSROOT/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"; NINJA_OK=1
  fi
  if command -v clang-cl >/dev/null 2>&1; then LLVMBIN="$(dirname "$(command -v clang-cl)")"; COMPILER=clang; else COMPILER=msvc; fi
  # python with SCons: PYTHON env, the venv, else python on PATH
  local cand
  for cand in "${PYTHON:-}" "$DEPS_DIR/venv/Scripts/python.exe" python python3; do
    [ -n "$cand" ] || continue
    command -v "$cand" >/dev/null 2>&1 || [ -x "$cand" ] || continue
    if "$cand" -c "import SCons" >/dev/null 2>&1; then PYBIN="$cand"; return; fi
  done
}
detect_tools

# ---- plan banner ----
echo "setup-physx: Godot PhysX fork variant (CPU only). Project root: ."
[ "$DRY" = 1 ] && echo "DRY RUN: nothing is changed."
echo "WARNING: the full build takes hours (PhysX SDK ~20 min, fork editor + template ~1-2 h on 16 threads) and about 60 GB of disk."
echo "paths: deps=$(rel "$DEPS_DIR") physx=$(rel "$PHYSX_SDK_DIR") fork=$(rel "$FORK_DIR") godot-cpp=$(rel "$GODOT_CPP_DIR")"
echo "       log folder=$(rel "$LOGDIR"), jobs=$JOBS, extension compiler=$COMPILER"

# ---- step: prereq ----
do_prereq() {
  local bad=0
  step prereq "checking tools"
  chk() { if [ "$2" = 1 ]; then say "ok      $1"; else say "MISSING $1  -> $3"; bad=1; fi; }
  chk git "$(command -v git >/dev/null && echo 1 || echo 0)" "install Git for Windows"
  chk cmake "$(command -v cmake >/dev/null && echo 1 || echo 0)" "install CMake (https://cmake.org)"
  chk "ninja ($( [ -n "$NINJADIR" ] && echo "VS bundled" || echo PATH))" "$NINJA_OK" "install Ninja or the VS 'C++ CMake tools' component"
  chk "Visual Studio 2022 C++ workload (vcvars64.bat)" "$([ -n "$VCVARS" ] && echo 1 || echo 0)" "install VS 2022 with 'Desktop development with C++' and a Windows SDK"
  chk "clang-cl on PATH (LLVM)" "$([ "$COMPILER" = clang ] && echo 1 || echo 0)" "install LLVM (winget install LLVM.LLVM); PhysX is built with clang-cl"
  case "$root" in *" "*) say "MISSING project path without spaces  -> the cmd.exe helper cannot take a path with spaces"; bad=1 ;; esac
  if [ -n "$PYBIN" ]; then chk "python with SCons ($PYBIN)" 1 ""
  else
    local py="" c; for c in python python3; do command -v $c >/dev/null 2>&1 && { py=$c; break; }; done
    if [ -z "$py" ]; then chk "python 3" 0 "install Python 3"
    else say "no SCons found: will create a venv at $(rel "$DEPS_DIR/venv") and run 'pip install scons' there (step fork)"; fi
  fi
  local free_kb; free_kb="$(df -k "$root" 2>/dev/null | awk 'NR==2{print $4}')"
  if [ -n "$free_kb" ] && [ "$free_kb" -lt $((60*1024*1024)) ]; then say "WARNING: only $((free_kb/1024/1024)) GB free, about 60 GB needed"; else say "ok      free disk >= 60 GB"; fi
  if [ "$bad" = 1 ]; then
    if [ "$DRY" = 1 ]; then say "(dry run: continuing despite missing prerequisites)"; else die "prerequisites missing, see above"; fi
  fi
}

# ---- step: clone ----
do_clone() {
  step clone "PhysX SDK, Godot fork, godot-cpp at pinned commits"
  # PhysX: shallow fetch of the pinned commit (the dev clone is shallow)
  if [ -d "$PHYSX_SDK_DIR/.git" ]; then
    skip "PhysX clone exists ($(rel "$PHYSX_SDK_DIR")), HEAD $(git -C "$PHYSX_SDK_DIR" rev-parse --short HEAD)"
    [ "$(git -C "$PHYSX_SDK_DIR" rev-parse HEAD)" = "$PHYSX_PIN" ] || say "NOTE: HEAD is not the pinned ${PHYSX_PIN:0:8}; left as is"
  else
    mkdir -p "$(dirname "$PHYSX_SDK_DIR")" 2>/dev/null
    logrun clone git init -q "$PHYSX_SDK_DIR"
    logrun clone git -C "$PHYSX_SDK_DIR" remote add origin "$PHYSX_URL"
    logrun clone git -C "$PHYSX_SDK_DIR" fetch --depth 1 origin "$PHYSX_PIN"
    logrun clone git -C "$PHYSX_SDK_DIR" checkout -q FETCH_HEAD
  fi
  if [ -d "$FORK_DIR/.git" ]; then
    skip "fork checkout exists ($(rel "$FORK_DIR")), HEAD $(git -C "$FORK_DIR" rev-parse --short HEAD)"
    [ "$(git -C "$FORK_DIR" rev-parse HEAD)" = "$FORK_PIN" ] || say "NOTE: HEAD is not the pinned ${FORK_PIN:0:8}; left as is"
  else
    mkdir -p "$(dirname "$FORK_DIR")" 2>/dev/null
    logrun clone git clone --filter=blob:none "$FORK_URL" "$FORK_DIR"
    logrun clone git -C "$FORK_DIR" checkout -q "$FORK_PIN"
  fi
  if [ -d "$GODOT_CPP_DIR/.git" ] || [ -f "$GODOT_CPP_DIR/CMakeLists.txt" ]; then
    skip "godot-cpp exists ($(rel "$GODOT_CPP_DIR"))"
  else
    mkdir -p "$(dirname "$GODOT_CPP_DIR")" 2>/dev/null
    logrun clone git clone "$CPP_URL" "$GODOT_CPP_DIR"
    logrun clone git -C "$GODOT_CPP_DIR" checkout -q "$CPP_PIN"
  fi
}

# ---- step: patch ----
do_patch() {
  step patch "apply benchmark/godot/patches/0009-windows-clang-cl-flags.patch to the PhysX clone"
  local p="$root/benchmark/godot/patches/0009-windows-clang-cl-flags.patch"
  [ -f "$p" ] || die "patch missing: $p"
  if [ ! -d "$PHYSX_SDK_DIR/.git" ]; then
    if [ "$DRY" = 1 ]; then say "would apply after the clone (clone step not done yet)"; return; fi
    die "PhysX clone missing: $PHYSX_SDK_DIR"
  fi
  if git -C "$PHYSX_SDK_DIR" apply --check --reverse "$p" >/dev/null 2>&1; then skip "patch already applied"
  elif git -C "$PHYSX_SDK_DIR" apply --check "$p" >/dev/null 2>&1; then
    logrun patch git -C "$PHYSX_SDK_DIR" apply "$p"
  else die "patch neither applies nor is applied in $PHYSX_SDK_DIR (different PhysX version?)"; fi
}

# ---- step: physx ----
do_physx() {
  step physx "PhysX SDK with clang-cl (Ninja Multi-Config, release, static CRT, CPU only), install to $(rel "$PX_INSTALL")"
  if [ -f "$PX_LIB" ] && [ "$FORCE" = 0 ]; then skip "found $(rel "$PX_LIB") (use --force to rebuild)"; return; fi
  say "takes about 20 minutes"
  export PX_VCVARS="$(w "${VCVARS:-vcvars64.bat}")" PX_LLVMBIN="$(w "${LLVMBIN:-.}")" PXR="$(w "$PHYSX_SDK_DIR")/physx" \
         PX_BLD="$(w "$PHYSX_BUILD_DIR")" PX_JOBS="$JOBS"
  if [ -n "$NINJADIR" ]; then export PX_NINJADIR="$(w "$NINJADIR")"; else export PX_NINJADIR=""; fi
  MSYS_NO_PATHCONV=1 logrun physx cmd.exe /d /c "$(cygpath -w "$here/physx/build-physx-sdk.bat" 2>/dev/null || echo "$here/physx/build-physx-sdk.bat")"
  if [ "$DRY" = 0 ] && [ ! -f "$PX_LIB" ]; then die "build finished but $PX_LIB is missing"; fi
}

# ---- python / scons ----
ensure_scons() {
  [ -n "$PYBIN" ] && return 0
  local py="" c; for c in python python3; do command -v $c >/dev/null 2>&1 && { py=$c; break; }; done
  if [ -z "$py" ]; then if [ "$DRY" = 1 ]; then PYBIN="<python>"; return 0; fi; die "python 3 not found"; fi
  say "SCons not found: creating venv $(rel "$DEPS_DIR/venv") and installing scons with pip (network access)"
  logrun fork "$py" -m venv "$DEPS_DIR/venv"
  logrun fork "$DEPS_DIR/venv/Scripts/python.exe" -m pip install scons
  PYBIN="$DEPS_DIR/venv/Scripts/python.exe"
}

# ---- step: fork ----
scons_fork() { # target
  # same arguments as phase2-godot.sh / physx-97bc4de/build.sh; PHYSX_SDK / BLAST_SDK unset, physx_sdk passed explicitly, no physx_gpu
  ( cd "$FORK_DIR" && unset PHYSX_SDK BLAST_SDK &&
    "$PYBIN" -m SCons platform=windows arch=x86_64 use_llvm=yes production=yes optimize=speed debug_symbols=no lto=none accesskit=no d3d12=no \
      silence_msvc=no progress=yes -j"$JOBS" target="$1" extra_suffix=physx physx_sdk="$(w "$PX_INSTALL")" )
}
do_fork() {
  step fork "Godot fork template_release and editor with the PhysX module (scons, clang)"
  if [ ! -d "$FORK_DIR/.git" ] && [ "$DRY" = 0 ]; then die "fork checkout missing: $FORK_DIR"; fi
  local t exe
  for t in template_release editor; do
    exe="$EXE_TR"; [ "$t" = editor ] && exe="$EXE_ED"
    if [ -f "$exe" ] && [ "$FORCE" = 0 ]; then skip "found $(rel "$exe")"; continue; fi
    if [ ! -f "$PX_LIB" ] && [ "$DRY" = 0 ]; then die "PhysX SDK not built ($(rel "$PX_LIB") missing): run the physx step first"; fi
    ensure_scons
    say "$t: takes roughly 30 to 60 minutes"
    say "cmd (in $(rel "$FORK_DIR")): $PYBIN -m SCons platform=windows arch=x86_64 use_llvm=yes production=yes optimize=speed debug_symbols=no lto=none accesskit=no d3d12=no silence_msvc=no progress=yes -j$JOBS target=$t extra_suffix=physx physx_sdk=<install>"
    logrun "fork-$t" scons_fork "$t"
  done
}

# ---- step: dump ----
do_dump() {
  step dump "dump the fork's extension API with the fork editor"
  if [ -f "$API_JSON" ] && [ "$FORCE" = 0 ]; then skip "found $(rel "$API_JSON")"; return; fi
  if [ ! -f "$EXE_ED" ] && [ "$DRY" = 0 ]; then die "fork editor missing: $EXE_ED (run the fork step)"; fi
  [ "$DRY" = 1 ] || mkdir -p "$API_DIR"
  # the editor writes extension_api.json and gdextension_interface.h into its working directory
  logrun dump bash -c 'cd "$1" && "$2" --headless --dump-extension-api --dump-gdextension-interface' _ "$API_DIR" "$EXE_ED"
  if [ "$DRY" = 0 ] && [ ! -f "$API_JSON" ]; then die "dump did not produce $API_JSON"; fi
}

# ---- step: ext ----
do_ext() {
  step ext "QuestBench extension against the fork API (COMPILER=$COMPILER)"
  if [ -f "$EXT_DLL" ] && [ "$FORCE" = 0 ]; then skip "found $(rel "$EXT_DLL")"; return; fi
  if [ "$DRY" = 0 ]; then
    [ -f "$API_JSON" ] || die "API dump missing: $API_JSON (run the dump step)"
    [ -f "$GODOT_CPP_DIR/CMakeLists.txt" ] || die "godot-cpp missing: $GODOT_CPP_DIR (run the clone step)"
  fi
  ensure_scons
  GODOTCPP_DIR="$GODOT_CPP_DIR" GODOT_API_JSON="$API_JSON" BUILD_ROOT="$EXT_BUILD_ROOT" EXT_OUT_DIR="$EXT_OUT_DIR" COMPILER="$COMPILER" PYTHON3="$PYBIN" \
    logrun ext bash "$here/build-godot-ext.sh" template_release
}

# ---- step: export ----
do_export() {
  step export "export QuestBench with the fork template to $(rel "$EXPORT_ROOT")/physx"
  if [ -f "$EXPORT_EXE" ] && [ "$FORCE" = 0 ]; then skip "found $(rel "$EXPORT_EXE")"; return; fi
  if [ "$DRY" = 0 ] && [ ! -f "$EXT_DLL" ]; then die "extension DLL missing: $EXT_DLL (run the ext step)"; fi
  FORK_TEMPLATE="$EXE_TR" FORK_EDITOR="$EXE_ED" GODOT_EXT_DLL="$EXT_DLL" EXPORT_ROOT="$EXPORT_ROOT" \
    logrun export bash "$here/export-godot-fork.sh" physx
  if [ "$DRY" = 0 ] && [ ! -f "$EXPORT_EXE" ]; then die "export did not produce $EXPORT_EXE"; fi
}

# ---- step: env ----
do_env() {
  step env "record PHYSX_BENCH_EXE in $(rel "$ENV_FILE")"
  local line="export PHYSX_BENCH_EXE=$(rel "$EXPORT_EXE")"
  if [ "$DRY" = 1 ]; then say "would ensure line: $line"
  else
    mkdir -p "$(dirname "$ENV_FILE")"; touch "$ENV_FILE"
    if grep -qxF "$line" "$ENV_FILE"; then skip "line already present"
    else
      # replace an older PHYSX_BENCH_EXE line, keep every other line
      local newf="$ENV_FILE.new"; grep -v '^export PHYSX_BENCH_EXE=' "$ENV_FILE" > "$newf" || true
      echo "$line" >> "$newf"; mv "$newf" "$ENV_FILE"; say "wrote: $line"
    fi
  fi
  echo
  echo "Run the PhysX variant (after 'source $(rel "$ENV_FILE")', or give the exe path directly):"
  echo "  BENCH_ENGINE=physx BENCH_FLAVOUR=physx GODOT_BENCH_EXE=$(rel "$EXPORT_EXE") bash benchmark/tools/run-all.sh godot"
  echo "  BODY_API=rid BENCH_ENGINE=physx BENCH_FLAVOUR=physxrid GODOT_BENCH_EXE=$(rel "$EXPORT_EXE") bash benchmark/tools/run-all.sh godot"
  echo "Read PhysX results next to awake_bodies; the engine in use is proved by physics_space_state_class in env.txt."
}

# prereq always runs (silent when --only= another step)
if [ -n "$ONLY" ] && [ "$ONLY" != prereq ]; then
  ( do_prereq ) >/dev/null 2>&1 || { [ "$DRY" = 1 ] || die "prerequisites missing (run with --only=prereq to see which)"; }
else
  do_prereq
fi
for s in clone patch physx fork dump ext export env; do want "$s" && "do_$s"; done
if [ "$DRY" = 1 ]; then echo "dry run finished, nothing changed."; else echo "setup-physx finished."; fi
