#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
PYTHONDONTWRITEBYTECODE=1 python3 tests/test_token_collectors.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/test_token_audit.py
mkdir -p build/token-tests
PYTHONPATH=companion PYTHONDONTWRITEBYTECODE=1 python3 - <<'PY'
from token_dashboard.collector import Collector
from token_dashboard.protocol import encode
from pathlib import Path
import tempfile
with tempfile.TemporaryDirectory() as d:
    c=Collector(Path(d)/'state.db',Path(d))
    s=c.snapshot(1790910000)
    s['sources'][0].update(total=123456789012,peak=54321,status=1)
    s['sources'][0]['days'][-1]=54321
    s['peak']=54321
    Path('build/token-tests/packet.bin').write_bytes(encode(s,123))
    c.db.close()
PY
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_token_model.c main/token_model.c -o build/token-tests/test_token_model
build/token-tests/test_token_model build/token-tests/packet.bin
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_token_nav.c main/token_nav.c -o build/token-tests/test_token_nav
build/token-tests/test_token_nav
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_token_pairing.c main/token_pairing.c -o build/token-tests/test_token_pairing
build/token-tests/test_token_pairing
