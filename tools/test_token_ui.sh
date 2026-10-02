#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
cmake -S tests/token_ui_host -B build/token-ui-host -G Ninja \
    -D "LVGL_PATH=$root/managed_components/lvgl__lvgl" -D CMAKE_BUILD_TYPE=Release
cmake --build build/token-ui-host
mkdir -p build/token-preview
build/token-ui-host/test_token_ui "$root/build/token-preview"
