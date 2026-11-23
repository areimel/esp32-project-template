#!/usr/bin/env bash
# Host-side checks: native C++ tests, then the Python script tests.
# Prefers .venv/bin tools, then tools on PATH (CI, no virtualenv).
# Run a firmware build first: the packaging tests use PlatformIO's esptool.
set -euo pipefail

cd "$(dirname "$0")/.."

# First match wins: the venv copy of the tool, then each candidate name on PATH.
resolve() {
  local tool="$1" name
  shift
  if [ -x "$PWD/.venv/bin/$tool" ]; then echo "$PWD/.venv/bin/$tool"; return; fi
  for name in "$@"; do
    if command -v "$name" >/dev/null 2>&1; then echo "$name"; return; fi
  done
  echo "check.sh: none of '$*' found; create .venv or install it" >&2
  return 1
}

pio=$(resolve pio pio platformio)
python=$(resolve python python3 python)

"$pio" test -e native
"$python" -m unittest discover -s scripts/tests -v
