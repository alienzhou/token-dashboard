[简体中文](token-dashboard.zh_CN.md) · **English**

# Token Dashboard

An original 240×320 AI Passport application and a local desktop companion: a
five-metric strip, a calendar heatmap, and tool-specific usage. BSP drivers and
pins are unchanged. Development starts from local `main` at `0b9e4c8` in the
`feature/token-dashboard` worktree; the original dirty checkout is untouched.

## Automatic collection

| Tool | Automatically read | Counting rule | Verification |
| --- | --- | --- | --- |
| Codex | `CODEX_HOME/sessions` and `archived_sessions` JSONL, default `~/.codex` | Differences of cumulative `total_token_usage.total_tokens`; repeated events and copied logs are deduplicated. Cache and reasoning are already included. | Real local records, incremental reads, fixtures |
| Claude Code | `~/.claude/projects/**/*.jsonl`, including subagents | Input + output + cache read + cache creation, deduplicated by API message ID across content blocks/updates | Fixtures; no local Claude Code records found |
| Cursor | Disabled | No verified automatic source for exact tokens on this installation. No manual import, credentials, private endpoint, or request-count estimates. | Source unavailable |
| OpenCode | `${XDG_DATA_HOME:-~/.local/share}/opencode/opencode.db` in read-only WAL-aware mode, or legacy `storage/message/**/*.json` | v1-compatible assistant input + output + reasoning + cache read/write | SQLite/WAL and JSON fixtures; no local installation found |
| Gemini CLI | `~/.gemini/tmp/*/chats/session-*.json` and `.jsonl` | The message's reported `tokens.total`, deduplicated by message ID | JSON/JSONL fixtures; no local installation found |

References: [Gemini recording implementation](https://github.com/google-gemini/gemini-cli/blob/main/packages/core/src/services/chatRecordingService.ts),
[OpenCode compatibility message table](https://github.com/anomalyco/opencode/blob/dev/packages/core/src/session/sql.ts),
[OpenCode statistics](https://github.com/anomalyco/opencode/blob/dev/packages/opencode/src/cli/cmd/stats.ts),
[Claude Code costs](https://code.claude.com/docs/en/costs).

Adapters only support the documented numeric schemas. New OpenCode v2-only
`session_message` storage is marked unsupported if the compatibility table has
no rows; future renamed fields/formats require adapter updates. These sources
report recorded token throughput, including cache work, rather than billed
credits, subscription utilization, or guaranteed account-wide usage. Deleted
history before first collection and remote-only agents are outside coverage.
Known usage already collected is retained when source files disappear; source
status reports availability separately. The companion never modifies source
logs or authentication files.

Every five seconds, the companion reads appended JSONL bytes, changed JSON
files, and updated database rows. Deduplication and cursors survive restart in
`~/.local/share/token-dashboard/usage.sqlite3` (0600). It stores hashed IDs,
counts, timestamps, file cursors, and completed Codex task durations. It does
not persist prompts, source code, model responses, or credentials. This is a
personal history ledger: it intentionally retains collected usage and cursors
across source-log cleanup; it has no automatic expiration. Use a different
`--state` path for an independent ledger. Only one process should use a ledger.

## Start the companion

From the repository root:

```bash
./tools/run-token-dashboard.sh
# Local live collection and UI, without starting Bluetooth:
./tools/run-token-dashboard.sh --no-ble
```

The script creates a repository-local virtual environment and installs pinned
Bleak 0.22.3 as needed. On macOS it uses the bundled
`build/desktop/Token Dashboard.app`, whose Info.plist declares Bluetooth usage.
`tools/package-token-macos.sh` creates it with py2app 0.28.8 in alias mode.
This local launcher requires the checkout and virtual environment to remain at
their current paths; it is not a portable standalone application. Rebuild it
after moving the checkout. It runs in the background; open the dashboard URL
separately. Python 3.9+ is required. Open
[the local dashboard](http://127.0.0.1:8964). It is bound only to loopback, loads
no external scripts/fonts, and transmits only aggregates over BLE. macOS must
grant the launching terminal/application Bluetooth permission. Linux requires
BlueZ; Windows requires a supported BLE adapter. Those platforms are not
hardware-tested. `--port`, `--interval`, `--device` (BLE UUID/address), and
`--state` are optional. With multiple devices, auto-connect stops and reports
identifiers; explicitly select the intended device with `--device`.

No autostart/login item is installed. Keep the companion running for live
collection and automatic reconnection. First collection can take longer for
large historical logs; subsequent polls process only changes.

macOS alias launchers start inside their application package; the entry point
normalizes the working directory to the checkout root. Relative `--state` paths
therefore refer to the same project location as the shell launcher.

## Accuracy and independent reconciliation

Run `python3 tools/audit_token_usage.py --url http://127.0.0.1:8964`, passing
the same `--state` if using a custom ledger. The audit imports no collector
parser or reducer: it independently rereads Codex numerical metadata through
the saved byte cursors, checks every increment against the read-only ledger,
compares daily sums, and optionally reconciles the panel total/daily array.
The output contains aggregate counters only; keep reports with personal totals
in an ignored local directory, such as `build/`.

`PASS` is limited to arithmetic for currently available log metadata. The report
also identifies historical rows whose source logs are unavailable and initial
cumulative baselines that predate the first visible usage event. Those initial
amounts are assigned to the first visible event's day; their actual original
dates cannot be reconstructed. A changing file/panel can require a fresh audit.
This check cannot prove provider billing, recover deleted pre-collection logs,
or validate an adapter for an uninstalled tool. Real Codex reconciliation and
schema-fixture tests for other tools are reported separately.

## Device UI and controls

- Up/down press: immediately cycle All, Codex, Claude Code, Cursor, OpenCode, Gemini CLI, including from the synchronization page. Releasing or rapidly double-clicking cannot count a press twice; holding up/down does not navigate again.
- OK click: toggle dashboard and synchronization page.
- OK double-click: toggle recent/older half-year calendar windows and return to the dashboard.
- Long OK: open the 60-second physical pairing window and immediately display the six-digit code, before the computer requests it.
- After five idle minutes, the first key press wakes the screen without navigation.
- Navigation is blocked while a pairing passkey is visible so a gesture cannot hide it.

The main page places the selected product in a prominent top selector with
up/down arrows and a position indicator. A 36 px cumulative total is the main
focus, followed by three columns for daily peak/current streak/longest streak
and a Monday-first heatmap with daily tokens. Connection and battery status
use quieter labels. Large values automatically use a smaller font when needed
to fit their field; unavailable products retain an explicit state.
The desktop shows the full rolling 364-day calendar; the wearable shows 182 days
at a time. All calendar calculations use Asia/Shanghai (UTC+8). Current streak
may end yesterday when today has no usage. Peak and streaks use the entire
collected ledger, not only the displayed calendar. Longest task is based solely
on paired Codex `task_started`/`task_complete` events, not session wall time.
Unavailable sources show a distinct state rather than a fabricated zero.

The device uses a fixed licensed Noto Sans CJK subset at 12, 16, 20 and 36 px.
The Chinese product selector uses 16 px with wider letter spacing; ASCII tool
names use the bundled Montserrat 20, separating navigation from the larger total.
`tools/generate_token_fonts.py` pins lv_font_conv 1.5.3 and the source-font hash.
Host tests verify every inventory code point, a known-missing negative glyph,
actual label font selection, text width/screen bounds, navigation-to-visible-product
integration and repeated page reconstruction.
Dynamic text is restricted to fixed labels, ASCII tool names/numbers and the
verified units. These fonts do not promise arbitrary user text/emoji coverage.

After 60 seconds of inactivity the backlight dims; after five minutes it turns
off. Pairing keeps the display awake. BLE remains available to keep the live
sync workflow; deep sleep and measured current optimization are not implemented.

## BLE and persistence

Advertising name: `TokenPassport-XXXX`. Service:
`f2d00001-7a43-4d65-a749-b0249cbb1001`; authenticated write RX ends in `0002`,
authenticated read receipt ends in `0003`. First pairing requires the physical
window, LE Secure Connections, and the device's six-digit display passkey.
An unknown peer is rejected before security starts outside that window; known
bonds use the same resolved identity lookup as NimBLE. Retries reuse the visible
window code; a pending security exchange keeps the code visible until success or
disconnection. Success closes the window and returns to the dashboard. Passkeys
are never logged. Bonded computers can reconnect without opening the window. Key replacement is
accepted only within a physical pairing window, and deletes only that peer's
old bond. At most two bonds are retained. Do not automatically erase NVS on
initialization errors.

A 7,421-byte `TKD1` snapshot contains a LE32 sequence, UTC epoch, Beijing day,
completed task duration, aggregate peak/streaks, five source records, 364 LE32
counts per source, and IEEE CRC32. Each acknowledged ATT write carries LE32
sequence + LE16 offset + payload. The receiver accepts ordered chunks and
identical retransmissions, rejects conflicts/out-of-order/bounds errors, and
applies only a fully decoded, CRC-valid snapshot. Minimum ATT payload 20 is
supported; negotiated larger payloads reduce latency. The receipt is LE32
sequence plus a status byte (0 = accepted into RAM, 1 = invalid snapshot).
Only exact matching receipts count as synchronization success.

Complete snapshots are handed to the application task; Bluetooth and button
callbacks never access LVGL or write application NVS. Changed snapshots are
checkpointed at most once per minute, with the first change saved immediately.
The receipt acknowledges RAM acceptance; the last minute may be lost on abrupt
power loss. A failed save preserves the earlier durable copy and displays a
save error. Identical-data heartbeats do not cause additional Flash writes.
After restart/offline, the cached snapshot is displayed with its sync date on
the synchronization page; offline daily totals are labeled as the sync day.
The computer sends changes automatically and an unchanged-data heartbeat every
60 seconds. Physical pairing behavior, reconnect timing, Flash interruption,
throughput at minimum MTU, and RF range require device validation.

## Build and validation

Activate ESP-IDF **5.5.3**, then run:

```bash
./tools/validate.sh
```

The gate includes upstream static/host checks, collection/transport fixtures,
the independent C receiver tests, an isolated ESP-IDF build, merged-image and
ELF archive verification, and actual pinned-LVGL rendering tests. UI previews
are written to `build/token-preview/*.ppm`. Build outputs and personal usage
ledgers stay ignored. `components/bsp` and the 8 MB default partition table are
unchanged: NVS `0x9000`, PHY `0xF000`, factory app `0x10000`.

Deliver `build/FoloToy-AI-Passport-full.bin` at **0x0**, with the matching archive
under `build/firmware/<SHA256>/`. Never write the app-only image at 0x0. A merged
flash pads gaps and may reset existing NVS/app data and bonds. Flashing and
whole-chip erasure are not authorized by a build request. The delivered pairing/title revision was flashed with explicit approval;
any subsequent artifact requires its own exact-artifact confirmation.

Read [validation results](token-dashboard-validation.md) for the exact delivered
artifact and separate Build/Host tests/Device tests/Unverified results.
