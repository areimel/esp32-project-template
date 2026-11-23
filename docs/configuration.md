# Configuration

Edit `config.yaml`, then rebuild. The generator
(`scripts/generate_config.py`, run by `scripts/platformio_config.py` before
every build) validates the file and compiles it into
`.pio/build/<env>/generated/generated_config.h` (namespace `AppConfig`) plus
`FEATURE_*` and `BOARD_*` compiler defines. Firmware never parses key names,
text or labels.

Rules:

- Unknown keys, duplicate keys and wrong types are errors, reported with the
  path, for example `buttons[1].long: unsupported key 'FOO' in 'FOO'`.
- Never put secrets in `config.yaml`. It is embedded in the firmware and in the
  published `firmware/index.json`.
- Keys marked "required" must be present; every other section may be omitted
  and takes its default.
- A top-level `schema: <path.py>` adds project-specific sections; see
  [Custom sections](#custom-sections).

## Keys

Each section is its own module in `scripts/config/sections/`.

| Key | Required | Default | Limits | Effect |
|---|---|---|---|---|
| `project.name` | yes | none | 1-32 printable ASCII characters (trimmed) | `AppConfig::PROJECT_NAME`; shown in the log, display and installer |
| `project.tagline` | no | `""` | text, at most 120 characters | Site only |
| `project.description` | no | `""` | text, at most 500 characters | Site only |
| `ble.enabled` | no | `true` | boolean | Sets `FEATURE_BLE_HID` |
| `ble.name` | yes | none | 1-29 UTF-8 bytes, no control characters | `AppConfig::BLE_NAME`, the pairing name |
| `ble.keystroke_ms` | no | `15` | integer 5-100 | `AppConfig::KEYSTROKE_MS`: time each key is held, and the gap after it |
| `wifi.enabled` | no | `false` | boolean | Sets `FEATURE_WIFI`; also enables Improv in the web installer |
| `wifi.hostname` | no | `esp32-device` | RFC 1123 label, 1-32 letters, digits or hyphens | `AppConfig::WIFI_HOSTNAME`; also the OTA hostname |
| `ota.enabled` | no | `false` | boolean | Requests `FEATURE_OTA`; see [OTA gating](#ota-gating) |
| `display.enabled` | no | `false` | boolean | `FEATURE_DISPLAY`, only on boards with a display |
| `display.brightness` | no | `64` | integer 1-255 | `AppConfig::DISPLAY_BRIGHTNESS` |
| `gestures.debounce_ms` | no | `25` | integer 1-100 | Button debounce |
| `gestures.double_click_ms` | no | `250` | integer 50-1000 | Window for a second press |
| `gestures.long_press_ms` | no | `650` | integer 100-10000 | Hold time for a long press |
| `buttons` | no | `[]` | list, one entry per button | See [Buttons](#buttons) |
| `buttons[i].single` | no | `null` | action | Single press |
| `buttons[i].double` | no | `null` | action | Double press |
| `buttons[i].long` | no | `null` | action | Long press |
| `schema` | no | none | path to a `.py` file relative to the config | Adds sections |

`gestures` must satisfy `debounce_ms < double_click_ms < long_press_ms`.

The shipped `config.yaml` has `ble`, `wifi`, `ota`, `display` all enabled, which
is more than the section defaults. Disabling a module removes its code from the
build.

## Buttons

`buttons` is a list; entry `i` configures physical button `i`. Each entry has
`single`, `double` and `long`. An action is `null` (unassigned), a key combo
string, or a mapping:

```yaml
buttons:
  - single: { keys: "CTRL+C", label: COPY }   # label 1-10 printable ASCII
    double: "CTRL+V"                          # label omitted
    long: null
  - single: ESC
```

- Extra entries beyond the board's button count are dropped with
  `warning: buttons[<i>] ignored: board <id> has <n> button(s)`. Missing
  entries are filled with unassigned slots, so one shared `config.yaml` builds
  for 1-button and 2-button boards.
- The generated table is `AppConfig::BUTTON_ACTIONS[BUTTON_SLOTS][3]`
  (single, double, long) of `{label, mods, usage}`. `0/0` means unassigned.
- Labels are uppercased. `label` is only a display string; it is optional.
- If a button has no double action, a single press fires on release with no
  wait. If it has no long action, a long press is an ordinary tap.
- A combo is one or more names joined by `+`: zero or more modifiers followed
  by at most one key, for example `CMD+SHIFT+4`. A modifier-only combo such as
  `SHIFT` is accepted and sent as a modifier-only chord. An empty combo or
  empty name is an error, as are a second key or a modifier after the key. Names are case-insensitive. A bare YAML digit (`4`)
  is accepted as the key `4`.

Modifiers: `CTRL`/`CONTROL`, `SHIFT`, `ALT`/`OPT`/`OPTION`,
`CMD`/`GUI`/`WIN`/`META`/`SUPER`.

Keys: `A`-`Z`, `0`-`9`, `F1`-`F24`, `ENTER`, `ESC`/`ESCAPE`, `BACKSPACE`, `TAB`,
`SPACE`, `DELETE`, `INSERT`, `HOME`, `END`, `PAGE_UP`, `PAGE_DOWN`, `UP`, `DOWN`,
`LEFT`, `RIGHT`, `CAPS_LOCK`, `PRINT_SCREEN`, `MINUS`, `EQUAL`, `LBRACKET`,
`RBRACKET`, `BACKSLASH`, `SEMICOLON`, `QUOTE`, `GRAVE`, `COMMA`, `PERIOD`,
`SLASH`. The tables live in `scripts/config/sections/keys.py` and use US
keyboard HID usages.

## OTA gating

OTA needs `ota.enabled`, `wifi.enabled` and an `ota_password` in
`secrets.yaml`; the conditions and warnings are in
[flashing-and-ota.md](flashing-and-ota.md#ota-development-only).

## secrets.yaml

Copy `secrets.example.yaml` to `secrets.yaml` (gitignored). Only these keys are
accepted, each a string; anything else is an error.

| Key | Use |
|---|---|
| `ota_password` | ArduinoOTA password; emitted as `AppConfig::OTA_PASSWORD` only when OTA is on |
| `wifi_ssid` | Development WiFi, used only if the device has no stored credentials |
| `wifi_password` | Same |

`DEV_WIFI_SSID` and `DEV_WIFI_PASSWORD` are empty strings when unset. These
values are compiled into your local firmware image, so do not publish a binary
built with them. The packager reads `secrets.yaml` only to render the config for
comparison and writes none of it to a manifest or `index.json`. It refuses to
package when `secrets.yaml` sets any value or OTA is compiled in, because
`firmware.bin` then holds them in plaintext; `--allow-secrets` overrides that
for a deliberate local package
([flashing-and-ota.md](flashing-and-ota.md#package-locally)). A malformed
`secrets.yaml` is reported by line and column only, never with its text.

## Per-env overrides

Set these in `platformio_override.ini` (gitignored; see
`platformio_override.ini.example`) or in an env section:

| Option | Default | Meaning |
|---|---|---|
| `custom_board` | the env name | Row of `boards/boards.yaml` to use |
| `custom_config` | `config.yaml` | Config file for this env |

## Custom sections

A `schema:` module is loaded after the built-in sections and follows the same
contract as `scripts/config/sections/*.py`:

| Name | Meaning |
|---|---|
| `KEY`, `REQUIRED`, `DEFAULT` | Top-level key, whether it must be present, value used when absent |
| `INCLUDES` | C++ includes added to the generated header |
| `validate(value, ctx)` | Return the validated value or raise `ConfigError(path, message)` |
| `emit(cfg, ctx)` | Return lines placed inside `namespace AppConfig` |
| `features(cfg, ctx)` | Optional; return `{FEATURE_X: bool}` |

`ctx` has `board_id`, `board`, `secrets`, `warnings` and `features`. Helpers
(`fields`, `integer`, `boolean`, `cstr`, `ConfigError`) are in
`scripts/config/core.py`. The macro example's schema adds a `profiles` section;
see [../examples/macro-clicker/README.md](../examples/macro-clicker/README.md).

## Generated defines

The pre-build script also defines, per env: `APP_BOARD_ID`, `BOARD_LABEL`,
`FIRMWARE_VERSION`, `BOARD_DRIVER_M5` or `BOARD_DRIVER_GPIO`,
`BOARD_BUTTON_COUNT`, `BOARD_BUTTON_PINS` (GPIO boards), `BOARD_LED_PIN`,
`BOARD_LED_RGB`, `BOARD_HAS_DISPLAY`, `BOARD_HAS_BATTERY` and one
`FEATURE_BLE_HID`/`FEATURE_WIFI`/`FEATURE_OTA`/`FEATURE_DISPLAY` flag as 0 or 1.
`FIRMWARE_VERSION` is `git describe --tags --always`, or `0.0.0-dev` outside a
git checkout; there is no version in `config.yaml`.
