#!/usr/bin/env bash
# Build the QuestBench ezEngine benchmark (benchmark/ezengine) with an ezEngine checkout's CMake build.
# Usage: [WORKSPACE=qm-bench] [CONFIG=Dev|Shipping] build-ezengine.sh [--configure-only]
# Environment (no machine paths are stored in the repo):
#   EZENGINE_DIR   ezEngine checkout (default: .tmp/ezengine). Relative paths are taken from the project root, not from the caller's cwd.
#   WORKSPACE      build folder name below <EZENGINE_DIR>/Workspace (default: qm-bench); the output goes to <WORKSPACE>-output next to it.
#                  Use a name of your own: never reuse the workspace of another build while it is in use.
#   CONFIG         Dev (default) | Shipping. Shipping failed to build at engine head 100564a (warning-as-error C4100 in Foundation
#                  RttiConverter.h:73, an upstream bug), so Dev is the tested configuration. The NOTES list what that means for results.
#   EZENGINE_D3D11 1 = also configure the D3D11 renderer (-DEZ_BUILD_D3D11=ON, off by default in the engine); the app then takes
#                  -renderer DX11|Vulkan. Default build: Vulkan only.
#   EZENGINE_TOOLS 1 = also build the headless editor (EditorProcessor), needed for the one-time asset transform (see NOTES).
# Steps: RunCMake.ps1 (first run only: creates the workspace), then a second CMake configure that points the engine at this project
# (-DEZ_EXTERNAL_PROJECT_1=<benchmark/ezengine>, re-run on every call so new source files are picked up), then a build of the QuestBench
# target only (its dependencies are built with it), with the engine's recommended "-- -m -p:CL_MPCount=16".
# Output: <EZENGINE_DIR>/Workspace/<WORKSPACE>-output/Bin/WinVs2022<CONFIG>64/QuestBench.exe (set EZENGINE_BENCH_EXE to it for run-all.sh).
set -eu
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
abs() { case "$1" in ""|/*|[A-Za-z]:*) echo "$1" ;; *) echo "$root/$1" ;; esac; }
EZENGINE_DIR="$(abs "${EZENGINE_DIR:-.tmp/ezengine}")"
WORKSPACE="${WORKSPACE:-qm-bench}"; CONFIG="${CONFIG:-Dev}"
case "$CONFIG" in Dev|Shipping) ;; *) echo "CONFIG must be Dev or Shipping: $CONFIG" >&2; exit 2 ;; esac
[ -f "$EZENGINE_DIR/RunCMake.ps1" ] || { echo "no ezEngine checkout at EZENGINE_DIR=$EZENGINE_DIR" >&2; exit 2; }
if command -v cygpath >/dev/null 2>&1; then to_win() { cygpath -w "$1"; }; else to_win() { echo "$1"; }; fi
cmake="$EZENGINE_DIR/Data/Tools/Precompiled/cmake/bin/cmake.exe"
ws="$EZENGINE_DIR/Workspace/$WORKSPACE"
cd "$EZENGINE_DIR"
if [ ! -f "$ws/CMakeCache.txt" ]; then
  powershell -NoProfile -ExecutionPolicy ByPass -File RunCMake.ps1 -Target vs2022x64 -SolutionName "QMBench" -WorkspaceDir "$WORKSPACE" -NoSubmoduleUpdate
fi
args=("-DEZ_EXTERNAL_PROJECT_1:PATH=$(to_win "$here/../ezengine")")
[ "${EZENGINE_D3D11:-0}" = 1 ] && args+=("-DEZ_BUILD_D3D11:BOOL=ON")
"$cmake" "${args[@]}" "$(to_win "$ws")"
[ "${1:-}" = "--configure-only" ] && exit 0
targets=(--target QuestBench)
[ "${EZENGINE_TOOLS:-0}" = 1 ] && targets+=(--target EditorProcessor)
"$cmake" --build "$(to_win "$ws")" --config "$CONFIG" "${targets[@]}" -- -m -p:CL_MPCount=16
echo "built: $EZENGINE_DIR/Workspace/$WORKSPACE-output/Bin/WinVs2022${CONFIG}64/QuestBench.exe"
