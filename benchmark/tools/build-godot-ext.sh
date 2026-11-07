#!/usr/bin/env bash
# Build the QuestBench GDExtension (benchmark/godot/src) for windows x86_64 with godot-cpp's CMake build.
# Usage: [COMPILER=msvc|clang] build-godot-ext.sh [template_debug|template_release|all]   (default: all, COMPILER=msvc)
# Environment (no machine paths are stored in the repo):
#   GODOTCPP_DIR       godot-cpp checkout (required)
#   GODOT_API_JSON     extension_api.json dumped from the target editor with --dump-extension-api (required)
#   COMPILER           msvc (default): Visual Studio generator, MSVC cl.
#                      clang: Visual Studio generator with the ClangCL toolset (-T ClangCL, LLVM clang-cl + lld-link; needs the
#                             clang-cl on PATH; the VS-bundled LLVM is absent on the dev machine, so LLVMInstallDir and LLVMToolsVersion
#                             are derived from the clang-cl on PATH and exported for MSBuild). Same Release flags (/O2, NDEBUG), same static CRT.
#   BUILD_ROOT         scratch build tree (default: <project root>/.tmp/godotext-build); each compiler gets its own subfolder
#   PYTHON3            python3 interpreter for godot-cpp's binding generator (optional; passed as -DPython3_EXECUTABLE)
#   CMAKE_GENERATOR    default "Visual Studio 17 2022"
#   SELECT_DEFAULT     1 = also point bench.gdextension at this compiler's DLLs (the export then ships those)
# Output: benchmark/godot/bin/<compiler>/libquestbench.windows.<target>.x86_64.dll
# template_release is built with the Release configuration (/O2), template_debug with Debug.
set -eu
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$(cd "$here/../godot" && pwd)"
root="$(cd "$here/../.." && pwd)"
: "${GODOTCPP_DIR:?set GODOTCPP_DIR}"
: "${GODOT_API_JSON:?set GODOT_API_JSON}"
COMPILER="${COMPILER:-msvc}"
BUILD_ROOT="${BUILD_ROOT:-$root/.tmp/godotext-build}"
GEN="${CMAKE_GENERATOR:-Visual Studio 17 2022}"
which_target="${1:-all}"
case "$COMPILER" in msvc) toolset=() ;; clang) toolset=(-T ClangCL) ;; *) echo "COMPILER must be msvc or clang" >&2; exit 2 ;; esac
case "$which_target" in all) targets="template_debug template_release" ;; template_debug|template_release) targets="$which_target" ;; *) echo "bad target" >&2; exit 2 ;; esac
if [ "$COMPILER" = clang ]; then
  # The VS "ClangCL" toolset looks for VS-bundled LLVM (absent here); point MSBuild at the standalone LLVM found on PATH.
  cc="$(command -v clang-cl)" || { echo "clang-cl not on PATH" >&2; exit 2; }
  export LLVMInstallDir="${LLVMInstallDir:-$(cygpath -w "$(dirname "$(dirname "$cc")")" 2>/dev/null || dirname "$(dirname "$cc")")}"
  export LLVMToolsVersion="${LLVMToolsVersion:-$("$cc" --version | sed -n 's/.*version \([0-9]*\)\..*/\1/p' | head -n 1)}"
  echo "clang-cl $("$cc" --version | head -n 1), LLVMToolsVersion=$LLVMToolsVersion"
fi
w() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
outdir="$src/bin/$COMPILER"
mkdir -p "$outdir"
for t in $targets; do
  cfg=Release; [ "$t" = template_debug ] && cfg=Debug
  bdir="$BUILD_ROOT/$COMPILER/$t"
  args=(-S "$(w "$src")" -B "$(w "$bdir")" -G "$GEN" -A x64 "${toolset[@]}"
        -DGODOTCPP_DIR="$(w "$GODOTCPP_DIR")" -DGODOTCPP_CUSTOM_API_FILE="$(w "$GODOT_API_JSON")"
        -DGODOTCPP_TARGET="$t" -DQUESTBENCH_OUTPUT_DIR="$(w "$outdir")")
  [ -n "${PYTHON3:-}" ] && args+=(-DPython3_EXECUTABLE="$(w "$PYTHON3")")
  cmake "${args[@]}"
  cmake --build "$(w "$bdir")" --config "$cfg" --target questbench --parallel
done
if [ "${SELECT_DEFAULT:-0}" = 1 ]; then
  sed -i -E "s#res://bin/(msvc/|clang/)?(libquestbench)#res://bin/$COMPILER/\2#" "$src/bench.gdextension"
  echo "bench.gdextension now selects bin/$COMPILER"
fi
ls -l "$outdir/"*.dll
