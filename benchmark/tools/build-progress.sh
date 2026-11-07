#!/bin/bash
# Progress bar for a logged scons build (Godot's own bar needs a TTY and an earlier build's node count, so it is off in logs).
# Usage: build-progress.sh <log> [watch]
# Counts "Compiling/Linking/Archiving/Generating" lines in <log> against the step total in <log>.total
# (a rough number, e.g. from `scons -n ... | grep -c -E "^(Compiling|Linking|Archiving|Generating)"` summed over the targets).
# "watch" refreshes every 5 s until the log ends with an *_EXIT= line.
log="${1:?usage: build-progress.sh <log> [watch]}"
draw() {
  local done total pct n bar
  done=$(grep -c -E '^(Compiling|Linking|Archiving|Generating)' "$log")
  total=$(cat "$log.total" 2>/dev/null || echo 0)
  if [ "$total" -gt 0 ]; then pct=$((done * 100 / total)); [ $pct -gt 100 ] && pct=100; else pct=0; fi
  n=$((pct / 2))
  bar=$(printf '#%.0s' $(seq 1 $n))$(printf -- '-%.0s' $(seq 1 $((50 - n))))
  # \r redraws in place on a terminal; elsewhere (captured output) print one line per refresh
  if [ -t 1 ]; then eol='\r'; else eol='\n'; fi
  printf "[%s] %3d%%  %d / ~%d steps  $eol" "$bar" "$pct" "$done" "$total"
}
draw
if [ "$2" = watch ]; then
  while ! tail -1 "$log" | grep -q '_EXIT='; do sleep 5; draw; done
  draw
fi
echo
