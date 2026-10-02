#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
if [[ ! -x build/companion-venv/bin/python ]]; then
    python3 -m venv build/companion-venv
fi
if ! build/companion-venv/bin/python -c 'import bleak' >/dev/null 2>&1; then
    build/companion-venv/bin/python -m pip install -r companion/requirements.txt
fi
if [[ "$(uname -s)" == "Darwin" ]]; then
    app="$root/build/desktop/Token Dashboard.app/Contents/MacOS/Token Dashboard"
    if [[ ! -x "$app" ]]; then "$root/tools/package-token-macos.sh"; fi
    exec "$app" "$@"
fi
export PYTHONPATH="$root/companion${PYTHONPATH:+:$PYTHONPATH}"
exec build/companion-venv/bin/python -m token_dashboard "$@"
