#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
[[ "$(uname -s)" == "Darwin" ]] || { echo "macOS only" >&2; exit 1; }
[[ -x build/companion-venv/bin/python ]] || python3 -m venv build/companion-venv
build/companion-venv/bin/python -m pip install -r companion/requirements.txt 'py2app==0.28.8'
cd companion
../build/companion-venv/bin/python setup_macos.py py2app -A \
    --dist-dir ../build/desktop --bdist-base ../build/desktop-temp
