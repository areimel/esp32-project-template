# Testing

Run the host checks before every commit that touches logic, then build the
boards you changed. Passing tests and compiles do not prove Bluetooth pairing,
button identity, display rendering, typing reliability, WiFi provisioning, OTA
or battery runtime; those are physical checks tracked in [../TODO.md](../TODO.md).

## Commands

| Check | Command | Needs |
|---|---|---|
| Native C++ tests | `.venv/bin/pio test -e native` | tools only |
| Python tests | `.venv/bin/python -m unittest discover -s scripts/tests -v` | a firmware build of every env first |
| Both | `bash scripts/check.sh` | a firmware build of every env first |
| Firmware build | `.venv/bin/pio run -e <env>` | tools only |
| List envs | `python scripts/list_envs.py` | nothing |
| Site | `cd web && npm ci && npm run check && npm test && npm run build` | Node 22.12 or newer |

`scripts/check.sh` uses `.venv/bin` tools when a virtualenv exists and falls back
to tools on `PATH` (as in CI). The packaging tests read each env's build output
and PlatformIO's esptool, so build every env first:

```sh
for e in sticks3 esp32s3 esp32c3 esp32; do .venv/bin/pio run -e "$e"; done
```

## Native tests (`test/`)

`[env:native]` builds with the host compiler, `-Itest/stubs` and
`-DFEATURE_BLE_HID=1`, and ignores `board`, `net`, `display` and `store`. Only
pure logic and the BLE state machine (against stubs in `test/stubs/`) run there.
`test_boot_hold` includes the header-only `lib/board/src/BootHold.h` directly
and supplies a scripted `board::` and the fake clock.

| Suite | Covers |
|---|---|
| `test_gesture` | `Gesture`: debounce, double window, one-shot long press, release suppression, `cancel`, `reset` |
| `test_every` | `Every`: first call, period, millis wraparound, no burst |
| `test_improv` | Improv codec: round trips, garbage then a frame, bad checksum resync, truncated length, WiFi settings, RPC results (with and without strings) |
| `test_boot_hold` | `holdAllButtonsAtBoot` against a scripted board: grace window, hold through reset, early release, no buttons |
| `test_ble` | `BleKeyboard` stroke pacing, explicit release, abort, generation changes, against NimBLE stubs |
| `test_macro_controller_gestures`, `test_macro_controller_picker`, `test_macro_player` | Macro example logic; added with [examples/macro-clicker](../examples/macro-clicker/README.md) |

Add a suite as `test/test_<name>/test_main.cpp` with Unity. Files that include
Arduino or hardware headers cannot run here; put the logic in a header with no
Arduino includes and test that.

## Python tests (`scripts/tests/`)

| File | Covers |
|---|---|
| `test_config_core.py` | YAML loader (duplicate keys), `fields`, `integer`, `cstr`, secrets loading (errors never quote the file), section loading |
| `test_sections.py` | Each built-in section: limits, defaults, errors with paths, feature flags, OTA gating |
| `test_buttons.py` | Button and key-combo parsing, per-board slot counts and the dropped-button warning |
| `test_packaging.py` | Manifest shape, offsets, NVS-overlap guard, checksums, atomic publish, image validation (fixture in `packaging_support.py`) |
| `test_packaging_secrets.py` | Refusal to package builds with `secrets.yaml` values or OTA, `allow_secrets`, no secret in any packaged `.bin` or `.json` |
| `test_macro_schema.py` | The macro example's `profiles` section; added with the example |

## Site tests (`web/`)

| Script | Does |
|---|---|
| `npm run check` | `astro check`, type and template check |
| `npm test` | `tsx --test tests/*.test.ts`: index validation, the build guard `check-firmware.ts`, paths and hashes |
| `npm run build` | Verifies a packaged `public/firmware/` if present, then `astro build` |

## CI

`.github/workflows/ci.yml` runs on pull requests and pushes to `main`:

| Job | Does |
|---|---|
| `envs` | `scripts/list_envs.py` lists every `[env:*]` except `native` |
| `build` | Matrix: `pio run -e <env>` per env, no `secrets.yaml`, so OTA is off |
| `test` | Builds every env, runs `scripts/check.sh`, then a packaging dry run into a temp directory |
| `web` | `npm ci`, `check`, `test`, `build` in `web/` |

Tooling comes from the composite action `.github/actions/setup` (Python 3.12,
`requirements.txt`, a cache of `~/.platformio`, and optionally Node 22). The
release workflow reuses it; see [flashing-and-ota.md](flashing-and-ota.md).

## Before you claim something works

- State what ran: the command, the env and the result.
- A successful compile is the only claim for `esp32c3` and `esp32` until someone
  runs them on hardware.
- Report physical checks as pending unless you or the user observed them.
- Do not quote battery life; none has been measured.
