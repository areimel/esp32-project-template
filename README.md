# ESP32 Starter

A PlatformIO and Arduino template for ESP32 firmware. Clone it, edit
`config.yaml`, and build for your board. Optional modules (Bluetooth LE HID,
WiFi with Improv provisioning, development OTA, display) are compiled in from
that one file, and a static site installs the firmware from the browser with
ESP Web Tools.

Hardware behavior is not yet verified on any board. Compile success is the only
claim made, and the remaining checks are listed in [TODO.md](TODO.md).

## Quick start

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/pio run -e sticks3        # or esp32s3, esp32c3, esp32
.venv/bin/pio device list           # identify the board before flashing
.venv/bin/pio run -e sticks3 -t upload
```

On Windows use `.venv\Scripts\pio` and `.venv\Scripts\pip`. The Python
environment that runs PlatformIO must contain PyYAML (`requirements.txt`).

The default app maps each button's single, double and long press to a key combo
sent over Bluetooth, shows status on the display when the board has one, and
logs to serial. [docs/getting-started.md](docs/getting-started.md) walks through
the first build, serial output, pairing and WiFi.

## Boards

| Env | Board | Chip | Buttons | Display | Battery |
|---|---|---|---|---|---|
| `sticks3` | M5Stack Stick-S3 | ESP32-S3 | 2 | yes | yes |
| `esp32s3` | ESP32-S3 DevKitC-1 | ESP32-S3 | 1 (BOOT) | no | no |
| `esp32c3` | ESP32-C3 DevKitM-1 | ESP32-C3 | 1 (BOOT) | no | no |
| `esp32` | ESP32 DevKit (classic) | ESP32 | 1 (BOOT) | no | no |
| `sticks3-macro` | Stick-S3 running the macro clicker example | ESP32-S3 | 2 | yes | yes |
| `native` | Host unit tests only | - | - | - | - |

Pins, memory, flash facts and add-a-board steps are in
[docs/boards.md](docs/boards.md). `sticks3-macro` builds
[examples/macro-clicker](examples/macro-clicker/README.md), the original
profile and macro device, on top of the same modules.

## Modules

| Module | Purpose | Gated by |
|---|---|---|
| `lib/core` | `LOG_*` macros and the `Every` periodic gate | always |
| `lib/board` | Buttons, battery, status LED, boot-hold helper | always |
| `lib/input` | `Gesture`: debounced single, double and long press | always |
| `lib/store` | `Settings`: NVS-backed `uint8_t` store | always |
| `lib/ble_hid` | `BleKeyboard`: one-keystroke HID transport | `FEATURE_BLE_HID` |
| `lib/improv` | Improv Serial packet codec (pure) | always |
| `lib/net` | WiFi, Improv Serial handler, ArduinoOTA | `FEATURE_WIFI`, `FEATURE_OTA` |
| `lib/display` | Screen chrome and theme for a 240x135 panel | `FEATURE_DISPLAY` |

APIs and boundaries are in [docs/modules.md](docs/modules.md).

## Commands

| Task | Command |
|---|---|
| Build one board | `.venv/bin/pio run -e esp32c3` |
| List board envs | `python scripts/list_envs.py` |
| Host C++ tests | `.venv/bin/pio test -e native` |
| Host and Python tests | `bash scripts/check.sh` (build every board first) |
| Package firmware | `.venv/bin/python scripts/package_firmware.py --output web/public/firmware` |
| Site | `cd web && npm ci && npm run check && npm test && npm run build` |

Testing details are in [docs/testing.md](docs/testing.md); flashing, recovery,
OTA and releases are in [docs/flashing-and-ota.md](docs/flashing-and-ota.md).

## Configuration

`config.yaml` is the source of truth for the project name, Bluetooth name,
module switches, gesture timing and button actions. It is validated during every
build, and a bad value fails the build with the path of the key. Never put
secrets in it: it is embedded in the firmware and in the published
`index.json`. OTA and development WiFi credentials go in the gitignored
`secrets.yaml`. Keys, defaults and limits: [docs/configuration.md](docs/configuration.md).

## Release builds

Release and CI builds have no `secrets.yaml`, so they ship with OTA off. Flash
size is tight on the ESP32-C3 and classic ESP32; measured figures are in
[docs/boards.md](docs/boards.md#memory).

## Starting a new project

Follow [docs/new-project-checklist.md](docs/new-project-checklist.md): rename
the project, set the repository URL and Pages base path, decide which modules
to keep, remove the macro example if unused, then cut the first tag.

## Repository layout

```
config.yaml            project, ble, wifi, ota, display, gestures, buttons
secrets.example.yaml   template for the gitignored secrets.yaml
platformio.ini         shared [env] and one thin env per board
boards/boards.yaml     board capability table
lib/                   core, board, input, store, ble_hid, improv, net, display
src/                   default app: main.cpp, StatusView
examples/              macro-clicker example (own config, schema, env, tests)
scripts/               config generator, packager, checks, Python tests
test/                  native Unity tests and stubs
web/                   Astro site with ESP Web Tools
docs/                  guides; docs/superpowers holds design history
.github/               CI, release workflow, shared setup action
```

## Docs

| Guide | Contents |
|---|---|
| [Getting started](docs/getting-started.md) | First build, serial, pairing, WiFi |
| [Boards](docs/boards.md) | Capability table, memory, adding a board |
| [Configuration](docs/configuration.md) | Every `config.yaml` and `secrets.yaml` key |
| [Modules](docs/modules.md) | Module APIs, gating, boundaries |
| [Flashing and OTA](docs/flashing-and-ota.md) | Upload, web install, OTA, releases, recovery |
| [Testing](docs/testing.md) | Host tests, Python tests, site tests, CI |
| [New project checklist](docs/new-project-checklist.md) | Turning the template into your project |
| [AGENTS.md](AGENTS.md) | Rules for coding agents |
| [TODO.md](TODO.md) | Remaining physical checks |

License: see [LICENSE](LICENSE).
