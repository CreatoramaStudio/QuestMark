#!/bin/bash
# Progress bar for tools/build-godot-ext.sh (CMake + MSBuild). Run it in a second shell or as a background task.
# Usage: ext-build-progress.sh [compiler] [target] [watch]   (defaults: clang template_release)
# Counts the .obj files in .tmp/godotext-build/<compiler>/<target> against the compile units listed in the generated
# vcxproj files (godot-cpp plus questbench). The total is known only after the CMake configure step has run.
# Only .obj files newer than <build dir>/.build-start (touched by build-godot-ext.sh) count, so a rebuild in an old tree starts at 0%.
# Honors BUILD_ROOT and EXT_OUT_DIR like build-godot-ext.sh (relative to the project root).
# "watch" refreshes every 5 s until the extension DLL is newer than every .obj, or nothing changed for 10 min.
comp="${1:-clang}"; tgt="${2:-template_release}"; mode="${3:-}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
abs() { case "$1" in /*|[A-Za-z]:*) echo "$1" ;; *) echo "$root/$1" ;; esac; }
bdir="$(abs "${BUILD_ROOT:-.tmp/godotext-build}")/$comp/$tgt"
dll="$(abs "${EXT_OUT_DIR:-benchmark/godot/bin/$comp}")/libquestbench.windows.$tgt.x86_64.dll"
t0=$(date +%s)
if [ -t 1 ]; then eol='\r'; else eol='\n'; fi
total() { cat "$bdir"/questbench.vcxproj "$bdir"/godot-cpp/godot-cpp.vcxproj 2>/dev/null | grep -c '<ClCompile Include='; }
objs() {
  # with the marker written by build-godot-ext.sh count only fresh objects (an incremental rebuild reuses old .obj files)
  if [ -f "$bdir/.build-start" ]; then find "$bdir" -name '*.obj' -newer "$bdir/.build-start" 2>/dev/null | wc -l
  else find "$bdir" -name '*.obj' 2>/dev/null | wc -l; fi
}
linked() { [ -f "$dll" ] && [ -z "$(find "$bdir" -name '*.obj' -newer "$dll" 2>/dev/null | head -1)" ] && [ "$(objs)" -gt 0 ]; }
draw() {
  local done tot pct n bar el eta
  done=$(objs); tot=$(total); [ "$tot" -gt 0 ] && [ "$done" -gt "$tot" ] && done=$tot; el=$(( $(date +%s) - t0 ))
  if [ "$tot" -gt 0 ]; then pct=$((done * 100 / tot)); [ "$pct" -gt 100 ] && pct=100; else pct=0; fi
  linked && pct=100
  n=$((pct / 2)); bar="$(printf '%*s' "$n" '' | tr ' ' '#')$(printf '%*s' "$((50 - n))" '' | tr ' ' '-')"
  if [ "$tot" -eq 0 ]; then eta="configuring"; elif linked; then eta="linked"; elif [ "$done" -ge "$tot" ]; then eta="linking"; else eta="$comp $tgt"; fi
  printf "[%s] %3d%%  %d / %d units  %s  $eol" "$bar" "$pct" "$done" "$tot" "$eta"
}
draw
if [ "$mode" = watch ]; then
  last=""; idle=0
  while ! linked; do
    sleep 5; draw
    cur="$(objs)"; if [ "$cur" = "$last" ]; then idle=$((idle + 5)); else idle=0; last="$cur"; fi
    [ "$idle" -ge 600 ] && { printf '\nno new .obj for 10 min; build finished with an error or stopped (see its log)'; break; }
  done
fi
echo
