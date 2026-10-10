#!/usr/bin/env bash
set -euo pipefail

ROOT="${STUDIOHUB_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

fail() { echo "Doctor FAILED: $*" >&2; exit 1; }

[[ -d "$ROOT" ]] || fail "StudioHub root does not exist: $ROOT"
[[ -f "$ROOT/VERSION" ]] || fail "missing VERSION: $ROOT/VERSION"

require_cmd() { command -v "$1" >/dev/null 2>&1 || fail "missing command: $1"; }
for cmd in cmake ninja g++ python3 git c++ tar; do require_cmd "$cmd"; done

VERSION="$(<"$ROOT/VERSION")"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][A-Za-z0-9.-]+)?$ ]] || fail "unexpected StudioHub version: $VERSION"

echo "=== StudioHub Doctor ==="
command -v cmake
command -v ninja
command -v g++
command -v python3
command -v git
command -v c++
command -v tar

echo
cmake --version | head -1
ninja --version
g++ --version | head -1
python3 --version

echo
printf 'StudioHub root: %s\n' "$ROOT"
printf 'StudioHub version: %s\n' "$VERSION"
echo "Doctor complete."
