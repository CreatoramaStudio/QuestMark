#!/usr/bin/env bash
# One-command setup of the QuestMark benchmarks: checks prerequisites, downloads the dependencies, compiles and exports.
# Usage: bash benchmark/tools/setup.sh [godot] [stride] [ezengine] [--all] [--force] [--aot] [--check] [--dry]
#   no engine given = godot stride ezengine (--all means the same). Run it from anywhere; paths are resolved from the project root.
#   --force  redo every selected step (default: a step is skipped when its result exists and is newer than its inputs)
#   --aot    also publish the Stride Native AOT variant (needs the Visual Studio C++ tools; not built by default)
#   --check  only check the prerequisites and print what would be downloaded
#   --dry    print the step plan (and which steps are up to date), run nothing
# Environment (all optional; relative paths are taken from the project root):
#   DEPS_DIR      download and build folder (default .tmp/deps; inside the project, ignored by git)
#   EZENGINE_DIR  ezEngine checkout (default .tmp/ezengine)      ENV_FILE  file written for run-all.sh (default .tmp/env.sh)
#   COMPILER      msvc | clang for the Godot extension (default: clang when clang-cl is on PATH, else msvc)
# What is downloaded (about 1.3 GB for Godot, a few GB of NuGet packages for Stride, 20+ GB with the build for ezEngine):
#   Godot 4.7.2-stable official win64 editor + export templates (github.com/godotengine/godot-builds), godot-cpp (pinned commit, github),
#   ezEngine (pinned commit, github, with 4 submodules), NuGet packages (nuget.org). System software is never installed by this script.
# NOTE (deviation from the dev machine): the dev machine used the MONO editor and templates; the STANDARD (non-mono) ones are used here.
#   The benchmark does not use C#, so the engine code is the same; the exe is not byte-identical to the one behind the published numbers.
# Outputs: Godot  $DEPS_DIR/godot-export/QuestBench.exe     Stride  $DEPS_DIR/stride-jit/StrideBench.exe (AOT: $DEPS_DIR/stride-aot)
#          ezEngine  $EZENGINE_DIR/Workspace/qm-bench-output/Bin/WinVs2022Shipping64/QuestBench.exe
# Logs: .tmp/logs/setup-<step>.log. At the end ENV_FILE (default .tmp/env.sh) holds the exe variables for run-all.sh.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

GODOT_VER="4.7.2-stable"
GODOT_URL="https://github.com/godotengine/godot-builds/releases/download/$GODOT_VER"
GODOT_EDITOR_ZIP="Godot_v${GODOT_VER}_win64.exe.zip"
GODOT_TPL_TPZ="Godot_v${GODOT_VER}_export_templates.tpz"
GODOTCPP_URL="https://github.com/godotengine/godot-cpp.git"
GODOTCPP_SHA="b472519c42be09c1acec3fd63acd4010fc586ad4"
EZ_URL="https://github.com/ezEngine/ezEngine.git"
EZ_SHA="100564a61576cecaa8b39e395689d4c4e9c3f239"
EZ_WORKSPACE="${WORKSPACE:-qm-bench}"
MIN_FREE_GB=30

ENGINES=(); FORCE=0; AOT=0; CHECK=0; DRY=0
for a in "$@"; do case "$a" in
  godot|stride|ezengine) ENGINES+=("$a") ;;
  --all) ENGINES=(godot stride ezengine) ;;
  --force) FORCE=1 ;; --aot) AOT=1 ;; --check) CHECK=1 ;; --dry) DRY=1 ;;
  -h|--help) sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
  *) echo "usage: setup.sh [godot] [stride] [ezengine] [--all] [--force] [--aot] [--check] [--dry]" >&2; exit 2 ;;
esac; done
[ ${#ENGINES[@]} -gt 0 ] || ENGINES=(godot stride ezengine)
want() { local e; for e in "${ENGINES[@]}"; do [ "$e" = "$1" ] && return 0; done; return 1; }

DEPS="$(qb_abs "${DEPS_DIR:-.tmp/deps}")"
EZ="$(qb_abs "${EZENGINE_DIR:-.tmp/ezengine}")"
ENV_FILE="$(qb_abs "${ENV_FILE:-.tmp/env.sh}")"
LOGS="$QB_ROOT/.tmp/logs"
DL="$DEPS/downloads"
ED_DIR="$DEPS/godot-editor"
ED_EXE="$ED_DIR/Godot_v${GODOT_VER}_win64.exe"
ED_CON="$ED_DIR/Godot_v${GODOT_VER}_win64_console.exe"
TPL_DIR="$ED_DIR/editor_data/export_templates/${GODOT_VER%-stable}.stable"
CPP_DIR="$DEPS/godot-cpp"
API_DIR="$DEPS/api-dump"; API_JSON="$API_DIR/extension_api.json"
EXT_BIN="$DEPS/godot-ext-bin"; EXT_DLL="$EXT_BIN/libquestbench.windows.template_release.x86_64.dll"
EXP_PROJ="$DEPS/godot-export-proj"; EXP_OUT="$DEPS/godot-export"; GODOT_EXE="$EXP_OUT/QuestBench.exe"
STRIDE_OUT="$DEPS/stride-jit"; STRIDE_EXE="$STRIDE_OUT/StrideBench.exe"
STRIDE_AOT_OUT="$DEPS/stride-aot"; STRIDE_AOT_EXE="$STRIDE_AOT_OUT/StrideBench.exe"
EZ_BIN="$EZ/Workspace/$EZ_WORKSPACE-output/Bin/WinVs2022Shipping64"; EZ_EXE="$EZ_BIN/QuestBench.exe"
EZ_STAMP="$EZ/Workspace/.qm-setup-$EZ_WORKSPACE"

# ------------------------------------------------------------------ prerequisites
MISSING=(); WARNINGS=()
miss() { MISSING+=("$1"); }
check_prereqs() {
  local os arch
  os="$(uname -s 2>/dev/null || echo unknown)"; arch="${PROCESSOR_ARCHITEW6432:-${PROCESSOR_ARCHITECTURE:-}}"
  case "$os" in MINGW*|MSYS*|CYGWIN*) ;; *) miss "Windows 10/11 x64 with Git for Windows' bash is required (this shell reports '$os'). The benchmark builds Windows executables only." ;; esac
  case "$arch" in AMD64|x86_64|"") ;; *) miss "Windows x64 is required (this machine reports architecture '$arch')." ;; esac
  qb_have git || miss "git is missing. Install: winget install --id Git.Git -e   (or https://git-scm.com/download/win)"
  qb_have curl || miss "curl is missing (ships with Windows 10+ and Git for Windows). Install Git for Windows: winget install --id Git.Git -e"
  if ! qb_have unzip && ! qb_have powershell; then miss "neither unzip nor powershell found (needed to extract archives). Install Git for Windows: winget install --id Git.Git -e"; fi
  local needvs=0 needcmake=0
  want godot && { needvs=1; needcmake=1; }
  want ezengine && needvs=1
  [ "$AOT" = 1 ] && want stride && needvs=1
  if [ "$needcmake" = 1 ]; then
    if ! qb_have cmake; then miss "cmake is missing. Install: winget install --id Kitware.CMake -e   (https://cmake.org/download/)"
    else
      local v; v="$(cmake --version | head -n 1 | sed -n 's/.*version \([0-9]*\)\.\([0-9]*\).*/\1 \2/p')"
      set -- $v; { [ "${1:-0}" -gt 3 ] || { [ "${1:-0}" -eq 3 ] && [ "${2:-0}" -ge 21 ]; }; } || miss "cmake 3.21 or newer is required for the Visual Studio 2022 generator (found $(cmake --version | head -n 1)). Update: winget upgrade --id Kitware.CMake -e"
    fi
  fi
  if [ "$needvs" = 1 ]; then
    if [ -z "$(qb_vswhere || true)" ]; then
      miss "Visual Studio 2022 is missing (vswhere not found). Install Build Tools with the C++ workload: winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override \"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive\""
    elif [ -z "$(qb_vs_path)" ]; then
      miss "Visual Studio 2022 is installed but without the 'Desktop development with C++' workload (component VC.Tools.x86.x64). Open the Visual Studio Installer, Modify, tick 'Desktop development with C++'; or: winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override \"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive\""
    fi
    [ -d "$(qb_pf86)/Windows Kits/10/Include" ] || miss "Windows 10/11 SDK not found. In the Visual Studio Installer tick 'Windows 11 SDK' (part of the C++ workload with --includeRecommended)."
  fi
  if want godot; then
    PY="$(qb_python)"
    [ -n "$PY" ] || miss "python 3 is missing or only the Microsoft Store stub is on PATH (it does not run). godot-cpp needs it. Install: winget install --id Python.Python.3.12 -e   (then open a new shell; or turn off the Store alias in Settings > Apps > Advanced app settings > App execution aliases)"
    if [ -z "${COMPILER:-}" ]; then if qb_have clang-cl; then COMPILER=clang; else COMPILER=msvc; fi; fi
    case "$COMPILER" in msvc|clang) ;; *) miss "COMPILER must be msvc or clang (got '$COMPILER')." ;; esac
    if [ "$COMPILER" = clang ] && ! qb_have clang-cl; then miss "COMPILER=clang but clang-cl is not on PATH. Install: winget install --id LLVM.LLVM -e   (or unset COMPILER to use MSVC)"; fi
  fi
  if want stride; then
    qb_dotnet10 || miss ".NET SDK 10 is missing (dotnet --list-sdks shows no 10.x). Install: winget install --id Microsoft.DotNet.SDK.10 -e   (https://dotnet.microsoft.com/download/dotnet/10.0)"
  fi
  if want ezengine && ! qb_have powershell; then miss "powershell not found (ezEngine's RunCMake.ps1 needs it)."; fi
  local free; mkdir -p "$QB_ROOT/.tmp"; free="$(qb_free_gb "$QB_ROOT/.tmp")"
  if [ -n "$free" ] && [ "$free" -lt "$MIN_FREE_GB" ]; then WARNINGS+=("only ${free} GB free on the project drive; $MIN_FREE_GB GB or more are recommended (ezEngine alone needs about 20 GB with its build)."); fi
  return 0
}
PY=""; COMPILER="${COMPILER:-}"
check_prereqs
if [ ${#MISSING[@]} -gt 0 ]; then
  echo "Prerequisites missing (${#MISSING[@]}). Nothing was changed. Install these, open a NEW shell, and run again:" >&2
  i=0; for m in "${MISSING[@]}"; do i=$((i+1)); echo "  $i. $m" >&2; done
  exit 1
fi
for w in "${WARNINGS[@]+"${WARNINGS[@]}"}"; do echo "WARNING: $w"; done
echo "Prerequisites OK for: ${ENGINES[*]}${COMPILER:+  (Godot extension compiler: $COMPILER)}${PY:+  (python: $(basename "$PY"))}"
echo "Downloads go to: $(qb_rel "$DEPS") (Godot editor + templates, godot-cpp), $(qb_rel "$EZ") (ezEngine); NuGet packages go to the NuGet cache."
[ "$CHECK" = 1 ] && exit 0

# ------------------------------------------------------------------ steps
fetch_sha() { # <dir> <url> <sha>: checkout of one commit (shallow, falls back to a full clone)
  local dir="$1" url="$2" sha="$3"
  if [ -d "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" = "$sha" ]; then echo "$dir already at $sha"; return 0; fi
  mkdir -p "$dir"
  [ -d "$dir/.git" ] || git -C "$dir" init -q
  git -C "$dir" remote get-url origin >/dev/null 2>&1 || git -C "$dir" remote add origin "$url"
  if git -C "$dir" fetch --depth 1 origin "$sha" && git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD; then :; else
    echo "shallow fetch of $sha failed, falling back to a full fetch"
    git -C "$dir" fetch origin && git -C "$dir" -c advice.detachedHead=false checkout -q "$sha"
  fi
  [ "$(git -C "$dir" rev-parse HEAD)" = "$sha" ] || { echo "checkout is not at $sha" >&2; return 1; }
}

# --- Godot editor (official, standard build; self-contained through ._sc_)
skip_godot_editor() { [ -f "$ED_EXE" ] && [ -f "$ED_CON" ] && [ -f "$ED_DIR/._sc_" ]; }
do_godot_editor() {
  local zip="$DL/$GODOT_EDITOR_ZIP"
  qb_zip_ok "$zip" || { rm -f "$zip"; qb_download "$GODOT_URL/$GODOT_EDITOR_ZIP" "$zip"; }
  qb_zip_ok "$zip" || { echo "archive does not open: $zip" >&2; return 1; }
  rm -rf "$ED_DIR.partial" "$ED_DIR"; mkdir -p "$ED_DIR.partial"          # own empty folder; the archive is untrusted
  qb_unzip "$zip" "$ED_DIR.partial"
  [ -f "$ED_DIR.partial/$(basename "$ED_EXE")" ] || { echo "editor exe missing in the archive" >&2; ls "$ED_DIR.partial" >&2; return 1; }
  mv "$ED_DIR.partial" "$ED_DIR"
  : > "$ED_DIR/._sc_"     # self-contained mode: editor data (export templates) stays next to the exe, nothing touches the user profile
}
# --- export templates
skip_godot_templates() { [ -f "$TPL_DIR/windows_release_x86_64.exe" ] && [ -f "$TPL_DIR/windows_release_x86_64_console.exe" ] && [ -f "$TPL_DIR/version.txt" ]; }
do_godot_templates() {
  local tpz="$DL/$GODOT_TPL_TPZ" src="$DEPS/godot-templates-src"
  qb_zip_ok "$tpz" || { rm -f "$tpz"; qb_download "$GODOT_URL/$GODOT_TPL_TPZ" "$tpz"; }
  qb_zip_ok "$tpz" || { echo "archive does not open: $tpz" >&2; return 1; }
  rm -rf "$src"; mkdir -p "$src"                                           # own empty folder; the archive is untrusted
  qb_unzip "$tpz" "$src"
  [ -f "$src/templates/windows_release_x86_64.exe" ] || { echo "windows_release_x86_64.exe missing in the templates archive" >&2; return 1; }
  rm -rf "$TPL_DIR"; mkdir -p "$TPL_DIR"
  cp "$src"/templates/windows_* "$src/templates/version.txt" "$TPL_DIR/"
  echo "templates installed: $(tr -d '\r\n' < "$TPL_DIR/version.txt") in $TPL_DIR (editor expects folder ${GODOT_VER%-stable}.stable)"
  rm -rf "$src"
}
# --- godot-cpp
skip_godot_cpp() { [ "$(git -C "$CPP_DIR" rev-parse HEAD 2>/dev/null)" = "$GODOTCPP_SHA" ]; }
do_godot_cpp() { fetch_sha "$CPP_DIR" "$GODOTCPP_URL" "$GODOTCPP_SHA"; }
# --- extension_api.json from the downloaded editor (a scratch folder: the editor writes into its cwd)
skip_godot_api() { qb_up_to_date "$API_JSON" "$ED_EXE"; }
do_godot_api() {
  rm -rf "$API_DIR"; mkdir -p "$API_DIR"
  ( cd "$API_DIR" && timeout 300 "$ED_CON" --headless --dump-extension-api ) || true
  [ -s "$API_JSON" ] || { echo "extension_api.json was not written to $API_DIR" >&2; return 1; }
  ls -l "$API_JSON"
}
# --- the GDExtension (template_release only: that is the one the export ships)
skip_godot_ext() {
  [ "$(cat "$EXT_BIN/.compiler" 2>/dev/null)" = "$COMPILER" ] || return 1
  qb_up_to_date "$EXT_DLL" "$QB_ROOT/benchmark/godot/src" "$QB_ROOT/benchmark/godot/CMakeLists.txt" "$API_JSON" "$QB_TOOLS/build-godot-ext.sh"
}
do_godot_ext() {
  rm -f "$EXT_BIN/.compiler"
  GODOTCPP_DIR="$CPP_DIR" GODOT_API_JSON="$API_JSON" PYTHON3="$PY" COMPILER="$COMPILER" \
    BUILD_ROOT="$DEPS/godotext-build" EXT_OUT_DIR="$EXT_BIN" bash "$QB_TOOLS/build-godot-ext.sh" template_release
  [ -f "$EXT_DLL" ] || { echo "extension DLL missing: $EXT_DLL" >&2; return 1; }
  echo "$COMPILER" > "$EXT_BIN/.compiler"
}
# --- export with the downloaded editor. A scratch copy of the project is exported (nothing is written into benchmark/godot): its
#     bench.gdextension points the debug entry at the RELEASE DLL too, because the editor loads the extension while exporting
#     and only template_release is built (same trick as export-godot-fork.sh).
skip_godot_export() {
  [ "$(cat "$EXT_BIN/.compiler" 2>/dev/null)" = "$COMPILER" ] || return 1
  qb_up_to_date "$GODOT_EXE" "$EXT_DLL" "$QB_ROOT/benchmark/godot/project.godot" "$QB_ROOT/benchmark/godot/main.tscn" "$QB_ROOT/benchmark/godot/export_presets.cfg" "$ED_EXE" "$TPL_DIR/windows_release_x86_64.exe" \
    && [ -f "$EXP_OUT/QuestBench.pck" ]
}
do_godot_export() {
  local src="$QB_ROOT/benchmark/godot"
  rm -rf "$EXP_PROJ" "$EXP_OUT"; mkdir -p "$EXP_PROJ/bin" "$EXP_OUT"
  cp "$src/project.godot" "$src/main.tscn" "$src/export_presets.cfg" "$EXP_PROJ/"
  cp "$EXT_DLL" "$EXP_PROJ/bin/"
  cat > "$EXP_PROJ/bench.gdextension" <<'EXT'
[configuration]
entry_symbol = "questbench_init"
compatibility_minimum = "4.5"

[libraries]
windows.debug.x86_64 = "res://bin/libquestbench.windows.template_release.x86_64.dll"
windows.release.x86_64 = "res://bin/libquestbench.windows.template_release.x86_64.dll"
EXT
  ( cd "$EXP_PROJ" && timeout 600 "$ED_CON" --headless --path "$(qb_win "$EXP_PROJ")" --import ) || echo "(--import exit $?, continuing)"
  ( cd "$EXP_PROJ" && timeout 600 "$ED_CON" --headless --path "$(qb_win "$EXP_PROJ")" --export-release "Windows Desktop" "$(qb_win "$GODOT_EXE")" )
  [ -f "$GODOT_EXE" ] && [ -f "$EXP_OUT/QuestBench.pck" ] || { echo "export failed: $GODOT_EXE or .pck missing" >&2; ls -l "$EXP_OUT" >&2; return 1; }
  ls -l "$EXP_OUT"
}
# --- Stride (JIT build, optional Native AOT publish)
skip_stride_build() { qb_up_to_date "$STRIDE_EXE" "$QB_ROOT/benchmark/stride"; }
do_stride_build() {
  rm -f "$STRIDE_EXE"
  dotnet build "$(qb_win "$QB_ROOT/benchmark/stride/StrideBench.csproj")" -c Release -o "$(qb_win "$STRIDE_OUT")" -nologo
  [ -f "$STRIDE_EXE" ] || { echo "missing: $STRIDE_EXE" >&2; return 1; }
}
skip_stride_aot() { qb_up_to_date "$STRIDE_AOT_EXE" "$QB_ROOT/benchmark/stride"; }
do_stride_aot() {
  rm -f "$STRIDE_AOT_EXE"
  dotnet publish "$(qb_win "$QB_ROOT/benchmark/stride/StrideBench.csproj")" -c Release -r win-x64 -p:BenchAot=true -o "$(qb_win "$STRIDE_AOT_OUT")" -nologo
  [ -f "$STRIDE_AOT_EXE" ] || { echo "missing: $STRIDE_AOT_EXE" >&2; return 1; }
}
# --- ezEngine: pinned checkout with submodules, then Shipping build + asset transform (render mode needs the transformed stock assets)
skip_ezengine_clone() { [ "$(git -C "$EZ" rev-parse HEAD 2>/dev/null)" = "$EZ_SHA" ] && [ -f "$EZ/Data/Tools/Precompiled/cmake/bin/cmake.exe" ]; }
do_ezengine_clone() {
  fetch_sha "$EZ" "$EZ_URL" "$EZ_SHA"
  git -C "$EZ" submodule update --init --depth 1 || { echo "shallow submodule fetch failed, retrying without --depth"; git -C "$EZ" submodule update --init; }
  [ -f "$EZ/Data/Tools/Precompiled/cmake/bin/cmake.exe" ] || { echo "submodule Data/Tools/Precompiled is incomplete" >&2; return 1; }
}
skip_ezengine_build() { [ -f "$EZ_STAMP" ] && qb_up_to_date "$EZ_STAMP" "$QB_ROOT/benchmark/ezengine" && [ -f "$EZ_EXE" ]; }
do_ezengine_build() {
  rm -f "$EZ_STAMP"
  # as documented in benchmark/ezengine/NOTES.md: Shipping + EditorProcessor + asset transform (Shipping needs the /wd4100 the script sets)
  if ! EZENGINE_DIR="$EZ" WORKSPACE="$EZ_WORKSPACE" CONFIG=Shipping EZENGINE_TOOLS=1 bash "$QB_TOOLS/build-ezengine.sh"; then
    echo "Shipping build with tools failed; retrying the tools part (EditorProcessor + asset transform) with CONFIG=Dev"
    EZENGINE_DIR="$EZ" WORKSPACE="$EZ_WORKSPACE" CONFIG=Dev EZENGINE_TOOLS=1 bash "$QB_TOOLS/build-ezengine.sh"
    [ -f "$EZ_EXE" ] || EZENGINE_DIR="$EZ" WORKSPACE="$EZ_WORKSPACE" CONFIG=Shipping bash "$QB_TOOLS/build-ezengine.sh"
  fi
  [ -f "$EZ_EXE" ] || { echo "missing: $EZ_EXE" >&2; return 1; }
  : > "$EZ_STAMP"
}

# ------------------------------------------------------------------ step runner
PLAN=()   # name|label
if want godot; then
  PLAN+=("godot_editor|downloading Godot $GODOT_VER editor (about 70 MB)"
         "godot_templates|downloading Godot export templates (about 1.3 GB, standard build)"
         "godot_cpp|fetching godot-cpp at ${GODOTCPP_SHA:0:8}"
         "godot_api|dumping extension_api.json with the downloaded editor"
         "godot_ext|building the Godot extension with $COMPILER (about 10 min with clang, 3 min with msvc)"
         "godot_export|exporting QuestBench.exe (Windows Desktop, release)")
fi
if want stride; then
  PLAN+=("stride_build|building Stride (dotnet build -c Release, JIT; first run restores NuGet packages, about 5 min)")
  [ "$AOT" = 1 ] && PLAN+=("stride_aot|publishing Stride Native AOT (about 10 min)")
fi
if want ezengine; then
  PLAN+=("ezengine_clone|fetching ezEngine at ${EZ_SHA:0:8} with submodules (a few GB)"
         "ezengine_build|building ezEngine QuestBench, Shipping + asset transform (about 30 to 60 min)")
fi
TOTAL=${#PLAN[@]}; IDX=0
mkdir -p "$LOGS" "$DEPS"
for entry in "${PLAN[@]}"; do
  name="${entry%%|*}"; label="${entry#*|}"; IDX=$((IDX+1)); log="$LOGS/setup-${name//_/-}.log"
  if [ "$FORCE" = 0 ] && "skip_$name"; then echo "[$IDX/$TOTAL] $label: up to date, skipped"; continue; fi
  if [ "$DRY" = 1 ]; then echo "[$IDX/$TOTAL] $label: WOULD RUN (log $(qb_rel "$log"))"; continue; fi
  echo "[$IDX/$TOTAL] $label  (log: $(qb_rel "$log"))"
  : > "$log"; t0=$(date +%s)
  set +e; ( set -e; "do_$name" ) >>"$log" 2>&1; rc=$?; set -e
  if [ "$rc" -ne 0 ]; then
    echo "FAILED: step '$name' exited with $rc. Last lines of $(qb_rel "$log"):" >&2
    tail -n 25 "$log" >&2
    echo "Fix the cause and run again; finished steps are skipped. Full log: $(qb_rel "$log")" >&2
    exit 1
  fi
  echo "    done in $(( $(date +%s) - t0 )) s"
done
[ "$DRY" = 1 ] && { echo "dry run: nothing was changed"; exit 0; }

# ------------------------------------------------------------------ env file for run-all.sh (repo-relative paths, regenerated from what exists)
mkdir -p "$(dirname "$ENV_FILE")"
{
  echo "# written by benchmark/tools/setup.sh; paths are relative to the project root (run-all.sh and run-benchmark.sh resolve them there)"
  [ -f "$GODOT_EXE" ] && echo "export GODOT_BENCH_EXE=$(qb_rel "$GODOT_EXE")"
  [ -f "$STRIDE_EXE" ] && echo "export STRIDE_BENCH_EXE=$(qb_rel "$STRIDE_EXE")"
  [ -f "$STRIDE_AOT_EXE" ] && echo "export STRIDE_AOT_EXE=$(qb_rel "$STRIDE_AOT_EXE")"
  [ -f "$EZ_EXE" ] && echo "export EZENGINE_BENCH_EXE=$(qb_rel "$EZ_EXE")"
  echo "export OUT_DIR=\${OUT_DIR:-.tmp/results}"
} > "$ENV_FILE"
echo
echo "Setup finished. Wrote $(qb_rel "$ENV_FILE"):"; sed 's/^/    /' "$ENV_FILE"
want godot && echo "Note: Godot editor and templates are the standard (non-mono) 4.7.2-stable build; the dev machine's published numbers used the mono build."
echo "Next: bash benchmark/tools/run-benchmark.sh --quick        (about 25 min; close other programs first)"
