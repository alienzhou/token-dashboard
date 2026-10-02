[简体中文](token-dashboard-validation.zh_CN.md) · **English**

# Token Dashboard validation

Validated on 2026-10-02, macOS ARM64, ESP-IDF 5.5.3. The exact pairing/title revision below was flashed at 0x0 with explicit user approval and passed the bounded startup checks.

| Field | Result | Evidence |
| --- | --- | --- |
| Build | PASS | Complete `./tools/validate.sh`, isolated ESP32-C3 build, 8 MB partition bounds/MD5, merged-image byte matching, ELF-bound debug archive verification |
| Host tests | PASS | Upstream checks, 10 collection/transport tests and 3 independent reconciliation tests, independent C packet/receiver, navigation and pairing-state tests, actual LVGL rendering: 208 glyphs at four sizes, all pages/states and 600 UI reconstructions including product navigation and large-number field bounds |
| Device tests | PASS (flash/startup only) | Native USB Serial/JTAG flash hash verified; boot ELF identity matched, LCD/LVGL, buttons, battery gauge and BLE initialized; no panic/watchdog in bounded startup observation. Actual pairing and visual checks remain pending |
| Unverified | Pending | Actual BLE pairing, OS Bluetooth permission, reconnect and MTU throughput; visual LCD/Chinese text and physical key behavior; NVS power-loss recovery; runtime free/largest heap with an encrypted connection, battery readings, current and RF range; real Claude Code/OpenCode/Gemini CLI records and other desktop platforms |

The local companion ran against real Codex metadata and was observed updating
in the browser. Tool filtering and unsupported Cursor placeholders were
checked. The macOS Info.plist launcher was packaged and smoke-tested with
live local collection/HTTP and Bluetooth disabled. Other adapter tests use
synthetic data from the documented schemas, not claims of installed-tool
validation. OpenCode v2-only storage is unsupported; Cursor has no importer.

## Exact artifact

- Source checkout: `feature/token-dashboard`, baseline `0b9e4c8`, original project changes remain untouched; application changes are sanitized for the user-authorized commit/push, excluding personal ledgers and real-usage screenshots.
- Target: ESP32-C3, 8 MB Flash, no PSRAM. Existing BSP/dependency lock/partition table unchanged.
- Flash at **0x0**: `build/FoloToy-AI-Passport-full.bin` (1,200,384 bytes).
- Full-image SHA256: `522dde4e12cef2ccbd81a55d676082279e84d40cf05e0ade97f65f46e4d41a54`.
- Matching ELF SHA256: `18dfaf0919ba3479c920d6c4eb10f1f77a566e0c77631b331c30f92fde8e3b55`.
- App descriptor: `0b9e4c8-dirty`, IDF `v5.5.3`.
- Immutable matching bundle: `build/firmware/522dde4e12cef2ccbd81a55d676082279e84d40cf05e0ade97f65f46e4d41a54/`, including the full/app images, ELF, MAP, bootloader, partition table, flash arguments and manifest.
- Application image: 1,134,848 bytes, fitting the 8,323,072-byte factory partition at `0x10000`.
- Linker-reported DIRAM use: 187,974/321,296 bytes; static data/BSS 93,500 bytes. Remaining link-time space 133,322 bytes is not measured runtime free heap.
- Host LVGL pool: 14,352/20,720 bytes used in the final measured state. Boot free heap was 90,676 bytes and largest block 81,920 bytes; encrypted-sync heap and board current remain unverified.

The current merged bytes cover NVS with FF padding: `true`.
Writing this whole image replaces the old application and clears old NVS,
including records/settings/bonds. Whole-chip erase is unnecessary and was not
performed. A compatible segmented flash is a separate choice if old NVS needs
preservation; no original firmware backup is required.

## Remaining on-device acceptance

1. Flash/startup checks passed for the exact revision. Retain the immutable matching debug archive. Raw serial logs remain local and ignored.
2. Any later flash requires confirmation of its exact artifact/data impact; never write the app-only image at 0x0.
3. Inspect all tools, both calendar windows, empty/unavailable states, Chinese
   text, battery fallback, key navigation and dim/off/wake behavior.
4. Cancel any stale OS dialog. Long OK must display the passkey immediately; enter it in the new computer dialog. Verify
   exact receipt matching, live log updates, reconnection, and refusal of new
   pairing outside the physical window. Old OS-side bonds may need forgetting
   before re-pairing after a merged flash.
5. Disconnect and reboot after a checkpoint: verify cached counts/date and
   heatmap. Accept at most one minute of unsaved changes, and test interrupted
   saves preserve the earlier good blob. Measure runtime heap/current/RF on
   the board; do not infer those from this build.

See [the application guide](token-dashboard.md) for controls and source limitations.

## Independent reconciliation and sanitization

Available local Codex records were independently recomputed through ledger byte cursors: missing records,
per-record differences and daily differences were zero. The panel total and its 364-day array also matched
the ledger with zero difference. No collector parser/reducer was reused. Initial cumulative baseline
warnings remain because their original earlier dates cannot be reconstructed; log agreement does not prove
provider billing. Personal totals, raw logs, MAC/serial identifiers, passkeys and personal paths are excluded
from the public commit. Repository previews use synthetic test data only.
