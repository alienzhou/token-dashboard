[简体中文](README.zh_CN.md) · **English**

<div align="center">

# Token Footprint

### See your AI work add up.

Automatic local collection. A clear dashboard. Your progress, in your pocket.

[Get started](#get-started) · [Supported agents](#supported-agents) · [Verify the numbers](#verify-the-numbers) · [Application guide](docs/token-dashboard.md)

![Python 3.9+](https://img.shields.io/badge/Python-3.9%2B-174D3D?style=flat-square) ![Local data](https://img.shields.io/badge/Data-Local-174D3D?style=flat-square) ![Bluetooth sync](https://img.shields.io/badge/Sync-Bluetooth-174D3D?style=flat-square) ![MIT license](https://img.shields.io/badge/License-MIT-174D3D?style=flat-square)

</div>

<table>
  <tr>
    <td width="50%"><img src="docs/assets/token-dashboard/01-studio.png" alt="AI studio illustration of Token Footprint with synthetic usage data" width="100%"></td>
    <td width="50%"><img src="docs/assets/token-dashboard/02-desktop.png" alt="AI desktop illustration of the Codex usage view with synthetic data" width="100%"></td>
  </tr>
</table>

*AI product illustrations based on the device's appearance. All displayed usage is synthetic.*

| Collect automatically | See the big picture | Keep it local |
| --- | --- | --- |
| Read compatible agent records every five seconds, without manual imports. | Explore cumulative tokens, daily peaks, streaks and half-year activity. | Run a localhost web dashboard and send aggregate statistics to the wearable over encrypted Bluetooth. |

## Get started

The desktop companion has been tested on **macOS**. Install **Python 3.9+** and flash the [merged firmware](#build-and-flash) to your AI Passport first. The device needs no Wi-Fi setup.

**1. Start the companion.**

```bash
git clone --branch feature/token-dashboard https://github.com/alienzhou/token-dashboard.git
cd token-dashboard
./tools/run-token-dashboard.sh
```

The first run installs dependencies. Allow Bluetooth access when prompted. Keep this checkout in place: the macOS launcher depends on it.

**2. Open the local dashboard.**

Visit [127.0.0.1:8964](http://127.0.0.1:8964). To collect and browse without a device, start with `./tools/run-token-dashboard.sh --no-ble`.

**3. Pair the wearable.**

Hold **OK** on the device, then enter its displayed six-digit code in the computer's pairing dialog. The device shows **Connected** when synchronization is established. Hold OK again if the pairing window expires.

**4. Keep creating.**

Leave the companion running and use your AI tools normally. Collection runs every five seconds; press `Ctrl+C` to stop. Collected history remains available on the next launch.

## Your usage, at a glance

<table>
  <tr>
    <td width="50%"><img src="docs/assets/token-dashboard/03-ui-overview.png" alt="Current firmware overview with a cumulative total and activity heatmap" width="100%"></td>
    <td width="50%"><img src="docs/assets/token-dashboard/04-agent-comparison.png" alt="Codex and Claude Code views rendered from current firmware" width="100%"></td>
  </tr>
</table>

*These are source-rendered firmware views with synthetic test data, not on-device screenshots. [Native overview](docs/assets/token-dashboard/screens/all-agents.png) · [Codex](docs/assets/token-dashboard/screens/codex.png) · [Claude Code](docs/assets/token-dashboard/screens/claude-code.png)*

| Button | Action |
| --- | --- |
| Up / Down | Switch between all agents and individual tools |
| Press OK | Switch between usage and synchronization |
| Double-press OK | Switch the half-year activity window |
| Hold OK | Open the physical pairing window and show the six-digit code |

## Supported agents

| Agent | Automatic collection | Validation |
| --- | --- | --- |
| Codex | Compatible local usage records | Reconciled against real local records |
| Claude Code | Compatible local usage records | Schema fixtures |
| OpenCode | Compatible message storage; v2-only storage is unsupported | Schema fixtures |
| Gemini CLI | Compatible local usage records | Schema fixtures |
| Cursor | Currently unsupported | No reliable local adapter; no manual importer |

Availability depends on the records the tool actually writes. See [collection schemas and limitations](docs/token-dashboard.md) before interpreting missing data.

## Verify the numbers

```bash
python3 tools/audit_token_usage.py --url http://127.0.0.1:8964
```

The independent, read-only audit recomputes available Codex log records, reconciles them with the ledger, and checks the dashboard's total and daily values. It reports missing records, mismatches, duplicates, counter resets, retained history and initial cumulative baselines. When using a custom ledger, pass the same `--state` to the companion and audit.

**Logged usage is not provider billing or a subscription limit.** Some initial cumulative baselines predate the first visible event, so their original day cannot be reconstructed. Only Codex has been checked against real local records; the other adapters have fixture validation.

## Build and flash

<details>
<summary><strong>Firmware, validation and flashing details</strong></summary>

This is the complete ESP-IDF project with the reusable FoloToy BSP. Target: **ESP32-C3, 8 MB Flash, no PSRAM**, ESP-IDF **5.5.3**.

```bash
# Activate ESP-IDF 5.5.3 first.
./tools/validate.sh
```

The gate creates `build/FoloToy-AI-Passport-full.bin`, intended for flashing at **0x0**, and a matching immutable ELF/MAP bundle under `build/firmware/<SHA256>/`.

**The merged image clears existing NVS settings and Bluetooth bonds.** A whole-chip erase is unnecessary. Generated firmware is not committed.

Build and host checks passed for the tested firmware. Device flash and bounded startup checks passed; full pairing, display, power-loss and radio acceptance remain listed in the [validation report](docs/token-dashboard-validation.md). A successful build does not establish hardware validation.

</details>

## Explore the project

- [Application guide](docs/token-dashboard.md): collection, BLE synchronization, persistence and limitations.
- [Validation report](docs/token-dashboard-validation.md): exact tested firmware and remaining device checks.
- [Agent instructions](AGENTS.md): read before development. The application branch is `feature/token-dashboard`.

Only numerical metadata is collected. Prompts, code, credentials, personal ledgers, device logs and real-usage screenshots are excluded from Git.

Based on [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport), retaining its [MIT license](LICENSE). Noto CJK fonts use [SIL OFL 1.1](assets/fonts/OFL.txt).
