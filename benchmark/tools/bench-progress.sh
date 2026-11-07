#!/bin/bash
# Progress bar for a running tools/run-all.sh (same style as build-progress.sh). Run it in a second shell or as a background task.
# Usage: bench-progress.sh <out-dir> [watch] [prefix]   (relative <out-dir> = from the project root, e.g. .tmp/results-200hz)
# Reads <out-dir>/progress-<prefix>.txt (written by run-all.sh after every cell; the newest file when no prefix is given).
# <out-dir> may also be a parent of per-variant folders (shows the newest progress file there, so it follows sequential variants).
# "watch" refreshes every 5 s until the newest file has shown "| done" for 2 min (room for the next variant to start), or nothing changed for 30 min.
dir="${1:?usage: bench-progress.sh <out-dir> [watch] [prefix]}"; mode="${2:-}"; pre="${3:-}"
# a relative <out-dir> is taken from the project root (same rule as run-all.sh OUT_DIR)
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
case "$dir" in /*|[A-Za-z]:*) ;; *) dir="$root/$dir" ;; esac
if [ -t 1 ]; then eol='\r'; else eol='\n'; fi
f() { if [ -n "$pre" ]; then echo "$dir/progress-$pre.txt"; else ls -t "$dir"/progress-*.txt "$dir"/*/progress-*.txt 2>/dev/null | head -1; fi; }
draw() { local p; p="$(f)"; if [ -n "$p" ] && [ -s "$p" ]; then printf "%s  $eol" "$(cat "$p")"; else printf "waiting for run-all.sh (no progress file in %s)  $eol" "$dir"; fi; }
draw
if [ "$mode" = watch ]; then
  idle=0; last=""
  fin=0
  while [ "$fin" -lt 120 ]; do
    sleep 5; draw
    if grep -q '| done$' "$(f)" 2>/dev/null; then fin=$((fin+5)); else fin=0; fi
    cur="$(cat "$(f)" 2>/dev/null)"; if [ "$cur" = "$last" ]; then idle=$((idle+5)); else idle=0; last="$cur"; fi
    [ "$idle" -ge 1800 ] && break
  done
  draw
fi
echo
