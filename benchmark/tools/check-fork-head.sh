#!/usr/bin/env bash
# Check whether the PhysX fork has new commits before a rebuild (read-only: fetches the remote refs, never touches the working tree).
# Usage: check-fork-head.sh [--pull]
#   FORK_REPO   fork git repo (default .tmp/godot-physx-fork/godot; relative paths are taken from the project root)
#   --pull      fast-forward the checkout when it is behind (still no build)
# Prints the checked-out commit, the remote head and the new commits with their changed files per top-level module path.
# Exit code: 0 = up to date, 10 = new commits on the remote (read them before building), 2 = error.
set -eu
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
repo="${FORK_REPO:-.tmp/godot-physx-fork/godot}"; case "$repo" in /*|[A-Za-z]:*) ;; *) repo="$root/$repo" ;; esac
[ -d "$repo/.git" ] || { echo "no git repo at FORK_REPO ($repo)" >&2; exit 2; }
cd "$repo"
git fetch -q --all || { echo "git fetch failed" >&2; exit 2; }
head="$(git rev-parse HEAD)"; up="$(git rev-parse '@{u}' 2>/dev/null)" || { echo "branch has no upstream" >&2; exit 2; }
echo "checked out: $(git rev-parse --short HEAD) ($(git rev-parse --abbrev-ref HEAD))   remote: $(git rev-parse --short "$up")"
if [ "$head" = "$up" ]; then echo "up to date"; exit 0; fi
n="$(git rev-list --count "$head..$up")"
echo "$n new commit(s) on the remote:"; git log --format='  %h %ad %s' --date=short "$head..$up"
echo "changed files (module path counts):"; git diff --name-only "$head" "$up" | sed 's|/[^/]*$||' | sort | uniq -c | sort -rn | head -15
if [ "${1:-}" = "--pull" ]; then git merge --ff-only -q "$up" && echo "fast-forwarded to $(git rev-parse --short HEAD) (not built)"; fi
exit 10
