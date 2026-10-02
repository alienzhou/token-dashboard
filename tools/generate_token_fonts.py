#!/usr/bin/env python3
"""Generate every fixed UI code point at four sizes using lv_font_conv 1.5.3."""
import argparse
import hashlib
import re
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--converter', required=True)
a = p.parse_args()
converter = str(Path(a.converter).resolve())
assert subprocess.check_output([converter, '--version'], text=True).strip() == '1.5.3'
font = ROOT / 'assets/fonts/NotoSansCJKsc-Regular.otf'
assert hashlib.sha256(font.read_bytes()).hexdigest() == '2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b'
chars = set(range(32, 127))
for name in ('token_ui.c', 'token_model.c'):
    for string in re.findall(r'"([^"\n]*)"', (ROOT / 'main' / name).read_text()):
        chars.update(ord(c) for c in string if ord(c) > 127)
chars = sorted(chars)
(ROOT / 'assets/fonts/token-characters.txt').write_text(''.join(map(chr, chars)) + '\n')
(ROOT / 'assets/fonts/token_characters.h').write_text('#pragma once\n#include <stdint.h>\nstatic const uint32_t TOKEN_CODEPOINTS[] = {' + ','.join(hex(c) for c in chars) + '};\n')
for size in (12, 16, 20, 36):
    subprocess.run([converter, '--font', str(font.relative_to(ROOT)), '--range', ','.join(hex(c) for c in chars),
        '--size', str(size), '--bpp', '4', '--format', 'lvgl', '--no-compress',
        '--lv-include', 'lvgl.h', '--lv-font-name', f'token_font_{size}',
        '--output', f'assets/fonts/token_font_{size}.c'], cwd=ROOT, check=True)
    output = ROOT / f'assets/fonts/token_font_{size}.c'
    output.write_text(output.read_text().rstrip() + '\n')
print(f'Generated {len(chars)} verified inventory characters at 12/16/20/36px')
