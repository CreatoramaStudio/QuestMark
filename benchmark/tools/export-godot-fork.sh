#!/usr/bin/env bash
# Export the benchmark with a custom (fork) Godot export template, e.g. the PhysX fork. Nothing machine-specific is committed:
# the template and editor paths come from the environment, the project copy and the export presets are generated under <project root>/.tmp.
#
# Usage: FORK_TEMPLATE=<fork template_release exe> FORK_EDITOR=<fork editor console exe> bash tools/export-godot-fork.sh [label]
#   FORK_TEMPLATE  (required) the fork's windows template_release exe, used as custom_template/release.
#   FORK_EDITOR    (required) a fork editor exe (console build) that runs the export. Any editor of the same fork version works.
#   GODOT_EXT_DLL  optional: the release DLL of the QuestBench extension (default: benchmark/godot/bin/clang/libquestbench.windows.template_release.x86_64.dll,
#                  built by tools/build-godot-ext.sh template_release).
#   EXPORT_ROOT    optional output root (default <project root>/.tmp/export-fork). Output: $EXPORT_ROOT/<label>/QuestBench.exe (+ .pck, DLL).
#   Relative paths in FORK_TEMPLATE, FORK_EDITOR, GODOT_EXT_DLL and EXPORT_ROOT are taken from the project root.
#   label          default "fork" (folder name only; the file-name label for results is BENCH_FLAVOUR in run-all.sh).
# The scratch copy of the project points the extension's debug entry at the RELEASE DLL (the fork editor loads the extension while
# exporting and no debug DLL is built). The committed bench.gdextension is not changed.
# Then run:  BENCH_ENGINE=physx BENCH_FLAVOUR=<label> GODOT_BENCH_EXE=$EXPORT_ROOT/<label>/QuestBench.exe bash tools/run-all.sh godot
set -eu
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
proj="$(cd "$here/../godot" && pwd)"
root="$(cd "$here/../.." && pwd)"
: "${FORK_TEMPLATE:?set FORK_TEMPLATE to the fork template_release exe}"
: "${FORK_EDITOR:?set FORK_EDITOR to a fork editor console exe}"
# relative paths in the env vars are taken from the project root (not from the caller's cwd)
abs() { case "$1" in ""|/*|[A-Za-z]:*) echo "$1" ;; *) echo "$root/$1" ;; esac; }
FORK_TEMPLATE="$(abs "$FORK_TEMPLATE")"; FORK_EDITOR="$(abs "$FORK_EDITOR")"
[ -n "${GODOT_EXT_DLL:-}" ] && GODOT_EXT_DLL="$(abs "$GODOT_EXT_DLL")"
[ -n "${EXPORT_ROOT:-}" ] && EXPORT_ROOT="$(abs "$EXPORT_ROOT")"
[ -f "$FORK_TEMPLATE" ] || { echo "FORK_TEMPLATE not found: $FORK_TEMPLATE" >&2; exit 2; }
[ -f "$FORK_EDITOR" ] || { echo "FORK_EDITOR not found: $FORK_EDITOR" >&2; exit 2; }
label="${1:-fork}"
case "$label" in *[!A-Za-z0-9_]*) echo "label must be [A-Za-z0-9_]+" >&2; exit 2 ;; esac
dll="${GODOT_EXT_DLL:-$proj/bin/clang/libquestbench.windows.template_release.x86_64.dll}"
[ -f "$dll" ] || { echo "extension DLL not found: $dll (build it: tools/build-godot-ext.sh template_release)" >&2; exit 2; }
exp_root="${EXPORT_ROOT:-$root/.tmp/export-fork}"
work="$exp_root/proj-$label"; out="$exp_root/$label"
to_tool() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }

rm -rf "$work" "$out"; mkdir -p "$work/bin" "$out"
cp "$proj/project.godot" "$proj/main.tscn" "$work/"
cp "$dll" "$work/bin/"
cat > "$work/bench.gdextension" <<'EXT'
[configuration]
entry_symbol = "questbench_init"
compatibility_minimum = "4.5"

[libraries]
windows.debug.x86_64 = "res://bin/libquestbench.windows.template_release.x86_64.dll"
windows.release.x86_64 = "res://bin/libquestbench.windows.template_release.x86_64.dll"
EXT
cat > "$work/export_presets.cfg" <<EOF
[preset.0]

name="Windows Desktop"
platform="Windows Desktop"
runnable=true
advanced_options=false
dedicated_server=false
custom_features=""
export_filter="all_resources"
include_filter=""
exclude_filter="legacy/*"
export_path="$(to_tool "$out")/QuestBench.exe"
encryption_include_filters=""
encryption_exclude_filters=""
encrypt_pck=false
encrypt_directory=false

[preset.0.options]

custom_template/debug=""
custom_template/release="$(to_tool "$FORK_TEMPLATE")"
debug/export_console_wrapper=2
binary_format/embed_pck=false
texture_format/s3tc_bptc=true
texture_format/etc2_astc=false
binary_format/architecture="x86_64"
codesign/enable=false
application/modify_resources=false
application/product_name="QuestBench"
application/file_description="Quest engine benchmark"
application/company_name=""
application/copyright=""
EOF
cd "$work"
"$FORK_EDITOR" --headless --path . --import >/dev/null 2>&1 || true
"$FORK_EDITOR" --headless --path . --export-release "Windows Desktop" 2>&1 | grep -i "error" || true
[ -f "$out/QuestBench.exe" ] || { echo "export failed: $out/QuestBench.exe missing" >&2; exit 1; }
ls -l "$out"
echo "exported: $out/QuestBench.exe"
