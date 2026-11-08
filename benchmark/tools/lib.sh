#!/usr/bin/env bash
# Shared helpers for setup.sh and run-benchmark.sh (sourced, not run). No machine-specific paths are stored here.
# Everything is resolved from the project root; relative paths in env vars are taken from the root, not from the caller's cwd.

QB_TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QB_ROOT="$(cd "$QB_TOOLS/../.." && pwd)"

# abs <path>: keep absolute (/x, C:\x, C:/x) paths, prefix the project root to relative ones
qb_abs() { case "$1" in ""|/*|[A-Za-z]:*) echo "$1" ;; *) echo "$QB_ROOT/$1" ;; esac; }
# rel <abs path>: path relative to the project root when it lies inside it (for env.sh and messages)
qb_rel() { case "$1" in "$QB_ROOT"/*) echo "${1#"$QB_ROOT"/}" ;; *) echo "$1" ;; esac; }
# win <path>: path in the form Windows tools understand (C:/dir/file)
if command -v cygpath >/dev/null 2>&1; then qb_win() { cygpath -m "$1"; }; else qb_win() { echo "$1"; }; fi

qb_have() { command -v "$1" >/dev/null 2>&1; }
qb_say() { echo "$*"; }
qb_die() { echo "ERROR: $*" >&2; exit 1; }

# newer_ok <target> <input>...: 0 when <target> exists and no input file (directories are searched) is newer than it
qb_up_to_date() {
  local t="$1"; shift
  [ -e "$t" ] || return 1
  local i
  for i in "$@"; do
    [ -e "$i" ] || continue
    [ -z "$(find "$i" \( -name bin -o -name obj -o -name .godot \) -prune -o -type f -newer "$t" -print -quit 2>/dev/null)" ] || return 1
  done
  return 0
}

# ---- archives ----
# qb_zip_ok <zip>: the archive opens and lists
qb_zip_ok() {
  [ -s "$1" ] || return 1
  if qb_have unzip; then unzip -tqq "$1" >/dev/null 2>&1; return $?; fi
  powershell -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; \$z=[IO.Compression.ZipFile]::OpenRead('$(qb_win "$1")'); \$z.Dispose()" >/dev/null 2>&1
}
# qb_unzip <zip> <empty dest dir>
qb_unzip() {
  if qb_have unzip; then unzip -qq -o "$1" -d "$2"; return $?; fi
  powershell -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [IO.Compression.ZipFile]::ExtractToDirectory('$(qb_win "$1")', '$(qb_win "$2")')"
}
# qb_download <url> <file>: curl with retries and resume; the file appears only when complete (written as <file>.part first)
qb_download() {
  local url="$1" out="$2" rc=0
  mkdir -p "$(dirname "$out")"
  qb_have curl || { echo "curl not found" >&2; return 1; }
  curl -L --fail --retry 6 --retry-delay 4 --retry-all-errors -C - -o "$out.part" "$url" || rc=$?
  # 33 = range not satisfiable: the .part file already holds the whole file
  if [ "$rc" -ne 0 ] && [ "$rc" -ne 33 ]; then echo "download failed (curl exit $rc): $url" >&2; return 1; fi
  mv -f "$out.part" "$out"
}

# ---- toolchain detection (each prints a value or nothing; no side effects) ----
qb_pf86() { local v; v="$(printenv 'ProgramFiles(x86)' 2>/dev/null)"; [ -n "$v" ] && cygpath -u "$v" 2>/dev/null || echo "/c/Program Files (x86)"; }
qb_vswhere() { local v; v="$(qb_pf86)/Microsoft Visual Studio/Installer/vswhere.exe"; [ -x "$v" ] && echo "$v"; }
# qb_vs_path: install path of a VS 2022 (17.x) with the C++ x64 tools; empty when missing
qb_vs_path() {
  local vw p; vw="$(qb_vswhere)" || return 0; [ -n "$vw" ] || return 0
  p="$("$vw" -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>/dev/null | tr -d '\r' | head -n 1)"
  echo "$p"
}
# qb_python: prints the real python 3 interpreter (Windows path, from sys.executable); the Microsoft Store stub does not run and is skipped
qb_python() {
  local c out
  for c in "python3" "python" "py -3"; do
    out="$($c -c 'import sys; print(sys.executable if sys.version_info[0] == 3 else "")' 2>/dev/null | tr -d '\r' | head -n 1)" || out=""
    [ -n "$out" ] && [ -e "$(cygpath -u "$out" 2>/dev/null || echo "$out")" ] && { echo "$out"; return 0; }
  done
  return 0
}
qb_dotnet10() { qb_have dotnet && dotnet --list-sdks 2>/dev/null | tr -d '\r' | grep -q '^10\.'; }
qb_free_gb() { df -Pk "$1" 2>/dev/null | awk 'NR==2 { printf "%d", $4 / 1048576 }'; }
