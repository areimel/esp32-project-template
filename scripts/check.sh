#!/usr/bin/env bash
# Host-side checks: native C++ tests, then the Python script tests.
# Uses .venv/bin tools when a virtualenv exists, otherwise tools on PATH (CI).
# Run a firmware build first: the packaging tests use PlatformIO's esptool.
set -euo pipefail

cd "$(dirname "$0")/.."
bin=""
[ -d .venv/bin ] && bin="$PWD/.venv/bin/"

"${bin}pio" test -e native
"${bin}python" -m unittest discover -s scripts/tests -v
