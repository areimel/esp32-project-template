# ESP32 Starter Template Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the Stick-S3 macro clicker into a general ESP32 starter template with a board layer, optional modules (BLE HID, WiFi with Improv, dev OTA, display), a minimal default app, the macro clicker as an example, and an ESP Web Tools microsite.

**Architecture:**
- `platformio.ini` is organized as one shared `[env]` plus thin per-board envs on pioarduino (Arduino-ESP32 3.x).
- A PlatformIO pre-script performs five jobs:
  - validates `config.yaml` (or a per-env `custom_config`);
  - writes `generated_config.h` into the env's build directory;
  - adds `FEATURE_*` and `BOARD_*` CPPDEFINES from the config and `boards/boards.yaml`;
  - injects the dev OTA password from the gitignored `secrets.yaml`;
  - injects the firmware version from git.
- Reusable modules live in `lib/<module>/src/`. Pure logic is tested with `pio test -e native` (Unity).
- A packager writes ESP Web Tools manifests for each board plus a `firmware/index.json`. The Astro site reads both.

**Tech Stack:**
- Firmware: PlatformIO, pioarduino platform (exact release URL pinned), Arduino-ESP32 3.x, NimBLE-Arduino 2.x, M5Unified/M5GFX (Stick-S3 only).
- Tooling: Python 3.12 with PyYAML; Unity on the native env.
- Web: Astro, `esp-web-tools` (pinned), TypeScript, `node:test` via tsx.

**Spec:** `docs/superpowers/specs/2026-11-13-esp32-template-design.md`

## Global Constraints

- **Board envs:** `sticks3`, `esp32s3`, `esp32c3`, `esp32`, `sticks3-macro` and `native`.
- **Source of truth:**
  - `config.yaml` holds the project name and BLE name.
  - The version comes only from the git tag or `--version`. It falls back to `git describe --tags --always`, then `0.0.0-dev`.
  - Build facts (board, partitions, flash and memory type) live only in `platformio.ini`.
  - Board capabilities (label, chip, button pins, LED, display, battery, driver) live only in `boards/boards.yaml`.
  - The bootloader offset comes from one map in the packager: `{"ESP32": 0x1000}`, and 0x0 for all other chips.
- **Secrets:** never put secrets in `config.yaml`, fixtures, manifests or `index.json`. The OTA password and dev WiFi credentials come only from the gitignored `secrets.yaml`.
- **OTA scope:** OTA is compiled in only when `ota.enabled: true` and `secrets.yaml` provides `ota_password`. Otherwise the build prints `OTA disabled: no ota_password in secrets.yaml`. CI and release builds therefore ship with OTA off.
- **Module gating:** every optional module's `.cpp` is wrapped in `#if FEATURE_<X>`. The flags are `FEATURE_BLE_HID`, `FEATURE_WIFI`, `FEATURE_OTA` and `FEATURE_DISPLAY`. `FEATURE_DISPLAY` is set only when `display.enabled` is true and the board has a display. Gating does not rely on `lib_ldf_mode`.
- **Serial:** firmware never blocks waiting for USB serial. Improv Serial shares the log port and tolerates non-Improv bytes.
- **Reflash safety:** the web installer must never write inside the NVS partition. The parts are bootloader, partitions, boot_app0 and firmware, and no merged image is used. The manifest sets `new_install_prompt_erase: true`.
- **Behavior to keep:** the macro example preserves every behavior listed under "Preserve" in the current `AGENTS.md`. The HID report descriptor is unchanged.
- **File size:** every source file stays at or under 300 lines. Anything repeated 3 or more times becomes a shared component or helper.
- **Hardware claims:** report hardware behavior as pending unless observed. Compile success is the only claim made for the C3 and classic ESP32 envs.
- **Original sources:** all pre-rework files can be read with `git show c3e723e:<path>`. Use them when moving or porting code that an earlier task already deleted.
- **Commits:** the controller (not the subagents) makes one focused commit per task, with a descriptive title and body plus the session attribution trailer.

## Interfaces (fixed before fan-out)

**`boards/boards.yaml`** (keys are board ids; an env selects one with `custom_board = <id>`, defaulting to the env name):

```yaml
sticks3: {label: "M5Stack Stick-S3", chip: ESP32-S3, driver: m5, buttons: 2, display: true, battery: true}
esp32s3: {label: "ESP32-S3 DevKitC-1", chip: ESP32-S3, driver: gpio, button_pins: [0], led: {pin: 48, rgb: true}, display: false, battery: false}
esp32c3: {label: "ESP32-C3 DevKitM-1", chip: ESP32-C3, driver: gpio, button_pins: [9], led: {pin: 8, rgb: true}, display: false, battery: false}
esp32:   {label: "ESP32 DevKit (classic)", chip: ESP32, driver: gpio, button_pins: [0], led: {pin: 2, rgb: false}, display: false, battery: false}
```

**CPPDEFINES from the pre-script:**
- `APP_BOARD_ID="<id>"` (not BOARD_ID: M5Unified uses that macro), `BOARD_LABEL="<label>"`
- `BOARD_DRIVER_M5=1` or `BOARD_DRIVER_GPIO=1`
- `BOARD_BUTTON_COUNT=<n>`, `BOARD_BUTTON_PINS={0}` (gpio only)
- `BOARD_LED_PIN=<pin|-1>`, `BOARD_LED_RGB=0|1`
- `BOARD_HAS_DISPLAY=0|1`, `BOARD_HAS_BATTERY=0|1`
- The `FEATURE_*` flags above, and `FIRMWARE_VERSION="<ver>"`

**Generated header:**
- Path: `$BUILD_DIR/generated/generated_config.h`, added to CPPPATH.
- Namespace: `AppConfig`.
- Contents:
  - `PROJECT_NAME`, `BLE_NAME`, `KEYSTROKE_MS`, `DEBOUNCE_MS`, `DOUBLE_CLICK_MS`, `LONG_PRESS_MS`, `DISPLAY_BRIGHTNESS` and `WIFI_HOSTNAME`.
  - `OTA_PASSWORD`, only `#if FEATURE_OTA`.
  - `DEV_WIFI_SSID` and `DEV_WIFI_PASSWORD`. These are empty strings unless `secrets.yaml` sets them.
  - `struct ButtonAction { const char* label; uint8_t mods; uint8_t usage; };` (with `usage == 0 && mods == 0` meaning unassigned), `constexpr uint8_t BUTTON_SLOTS = BOARD_BUTTON_COUNT;` and `static const ButtonAction BUTTON_ACTIONS[BUTTON_SLOTS][3]` (single, double, long).
  - Schema sections may add their own includes and declarations. The macro example's `profiles` section emits `PROFILE_COUNT` and `PROFILES[]`, as today, and adds `#include "Macro.h"`.

**`config.yaml` sections:** `project` (required) and `ble` (required), plus `wifi`, `ota`, `display`, `gestures` and `buttons` (each optional with defaults). There is an optional `schema: <path to .py>` key. Unknown keys are an error, reported with the path to the key.

```yaml
project: {name: "ESP32 Starter", tagline: "...", description: "..."}   # name 1-32 chars
ble: {enabled: true, name: "ESP32 Starter", keystroke_ms: 15}           # name 1-29 UTF-8 bytes, keystroke 5-100
wifi: {enabled: true, hostname: esp32-starter}                          # RFC1123 label, 1-32
ota: {enabled: true}
display: {enabled: true, brightness: 64}                                # 1-255
gestures: {debounce_ms: 25, double_click_ms: 250, long_press_ms: 650}   # debounce < double < long
buttons:                                                                # list; index = button
  - {single: {keys: "CTRL+C", label: COPY}, double: null, long: "CMD+SHIFT+4"}
  - {single: ESC, double: null, long: null}
```

Entries in `buttons:` beyond the board's button count are dropped, with the build warning `buttons[<i>] ignored: board <id> has <n> button(s)`.

**Section module contract (`scripts/config/sections/<name>.py`, plus the example's `config_schema.py`):**
- `KEY: str`
- `REQUIRED: bool`
- `DEFAULT: dict | list | None`
- `INCLUDES: list[str]`
- `def validate(value, ctx: Context) -> object` raises `ConfigError(path, message)`.
- `def emit(cfg: object, ctx: Context) -> list[str]`
- `Context` is a dataclass with these fields: `board_id: str`, `board: dict`, `secrets: dict` and `warnings: list[str]`.

**C++ module APIs:**

```cpp
// lib/core/src/Every.h   — pure
class Every { public: explicit Every(uint32_t periodMs); bool due(uint32_t now); };  // true on first call, then once per period (wrap-safe)
// lib/core/src/Log.h     — LOG_E/W/I/D(fmt, ...) -> Serial.printf("[I] " fmt "\n"), level via LOG_LEVEL (default 3)

// lib/board/src/Board.h
namespace board {
void begin(uint8_t brightness);   // M5.begin or pinMode; never waits for serial
void update();                    // poll (M5.update on m5)
uint8_t buttonCount();
bool isPressed(uint8_t i);
int batteryPercent();             // -1 when none
bool charging();
void setStatusLed(bool on);       // no-op when BOARD_LED_PIN < 0; rgbLedWrite when BOARD_LED_RGB
#if BOARD_HAS_DISPLAY
M5GFX& display();
#endif
}
// lib/board/src/BootHold.h
// Returns true if every button stays held for holdMs at boot; calls progress(remainingMs) while held.
template <class F> bool holdAllButtonsAtBoot(uint32_t holdMs, F progress);

// lib/input/src/Gesture.h   — moved unchanged from include/Gesture.h
// lib/store/src/Settings.h
class Settings { public: explicit Settings(const char* ns); uint8_t getU8(const char* key, uint8_t def);
                 bool putU8(const char* key, uint8_t value); /* skips write when unchanged */ };

// lib/ble_hid/src/BleKeyboard.h — class renamed MacroKeyboard -> BleKeyboard, API otherwise unchanged

// lib/improv/src/Improv.h    — pure, no Arduino
enum class ImprovType : uint8_t { CurrentState = 1, ErrorState = 2, Rpc = 3, RpcResult = 4 };
enum class ImprovState : uint8_t { Authorized = 2, Provisioning = 3, Provisioned = 4 };
enum class ImprovError : uint8_t { None = 0, InvalidRpc = 1, UnknownRpc = 2, UnableToConnect = 3, Unknown = 0xFF };
enum class ImprovCommand : uint8_t { WifiSettings = 1, GetState = 2, GetInfo = 3, Scan = 4 };
class ImprovParser { public: bool feed(uint8_t b);  // true when a valid frame completed; bad checksum/garbage resyncs
                     ImprovType type() const; const uint8_t* data() const; uint8_t length() const; };
size_t improvFrame(ImprovType t, const uint8_t* data, uint8_t len, uint8_t* out, size_t cap);
size_t improvRpcResult(ImprovCommand c, const char* const* strings, uint8_t count, uint8_t* out, size_t cap);
bool improvWifiSettings(const uint8_t* rpc, uint8_t len, char* ssid, size_t ssidCap, char* pass, size_t passCap);

// lib/net/src/Net.h  (#if FEATURE_WIFI)
enum class WifiState : uint8_t { NoCredentials, Connecting, Connected };
namespace net {
void beginWifi(const char* hostname, const char* devSsid, const char* devPass);  // stored creds win; dev creds only if none stored
void updateWifi(uint32_t now);  WifiState wifiState();  const char* ipAddress();  // "" when not connected
void beginImprov(const char* firmware, const char* version, const char* chip, const char* deviceName);
void updateImprov();            // drains Serial, answers RPCs, provisions via WiFi.begin + persist
#if FEATURE_OTA
void beginOta(const char* hostname, const char* password); void updateOta(); bool otaActive();
#endif
}

// lib/display/src/Display.h  (#if FEATURE_DISPLAY) — Theme.h moved here unchanged
class Display { public: void begin(M5GFX& gfx, uint8_t brightness);  // 240x135 sprite, rotated like today
  M5Canvas& canvas(); void push();
  void header(const char* title, const char* status, int battery, bool charging);  // shared chrome
  void splash(const char* name, const char* version);  // ~700 ms
  void holdProgress(const char* action, uint32_t remainingMs, bool done);  // boot maintenance screen
  void fatal(const char* msg); };
```

**`web/public/firmware/index.json`** (written by the packager and read by the site):

```json
{ "schemaVersion": 2, "version": "v1.2.3", "commit": "<40 hex>", "configSha256": "<hex>",
  "project": {"name": "...", "tagline": "...", "description": "..."},
  "config": { "...": "parsed config.yaml" },
  "boards": [ {"id": "sticks3", "label": "M5Stack Stick-S3", "chip": "ESP32-S3",
               "buttons": 2, "display": true, "battery": true, "features": ["ble_hid","wifi"],
               "manifest": "sticks3/manifest.json",
               "parts": [{"path": "sticks3/bootloader.bin", "offset": 0, "size": 0, "sha256": "..."}] } ] }
```

Each board's `<id>/manifest.json` (ESP Web Tools format):

```json
{"name": "<project.name> · <label>", "version": "...", "new_install_prompt_erase": true,
 "new_install_improv_wait_time": 15 /* 0 when wifi disabled */,
 "builds": [{"chipFamily": "ESP32-S3", "parts": [{"path": "bootloader.bin", "offset": 0}, ...]}]}
```

## Review Focus

1. **A shared `config.yaml` with 2 buttons built for a 1-button board.** The build succeeds, prints a warning, and emits 1 slot. Test is in Task 1.
2. **`secrets.yaml` missing, or present without `ota_password`.** The build succeeds with no `FEATURE_OTA` and no password in any output. A manifest or `index.json` never contains secrets. Tests are in Tasks 1 and 8.
3. **Improv bytes mixed with log text, a truncated frame, or a bad checksum.** These are ignored, and the next valid frame parses. Test is in Task 4.
4. **A non-erase web reflash.** No packaged part's sector-rounded range intersects the NVS partition read from that board's `partitions.bin`. This is checked for each board, including the classic ESP32 bootloader at 0x1000. Test is in Task 8.
5. **A site build with no packaged firmware.** The site builds and shows a "No release published yet" state instead of failing. Test is in Task 9.

---

### Task 1: Config pipeline, board table and multi-board platformio.ini

**Files:**
- Create:
  - `boards/boards.yaml`
  - `scripts/config/__init__.py`
  - `scripts/config/core.py`
  - `scripts/config/sections/{__init__,project,ble,wifi,ota,display,gestures,buttons,keys}.py`
  - `secrets.example.yaml`
  - `platformio_override.ini.example`
  - `scripts/list_envs.py`
  - `scripts/tests/{test_config_core,test_sections,test_buttons}.py`
- Modify:
  - `scripts/generate_config.py`: a thin CLI with `render(data, ctx)` and `generate(source, destination, ctx)`.
  - `scripts/platformio_config.py`
  - `platformio.ini`
  - `config.yaml`: the new default template config.
- Delete: `tests/test_config.py`. Its generic cases move to the new tests, and its profile cases move in Task 7.

**Interfaces:**
- Produces: the board table, the CPPDEFINES, the generated header, and the section contract, all as given in **Interfaces**.
- Produces: `keys.parse_combo(value, path) -> (mods, usage)` and `keys.MODIFIERS`, `keys.KEYS` and `keys.ASCII`, moved from `generate_config.py`.
- Produces: `scripts/list_envs.py`, which prints a JSON array of every `[env:*]` name except `native`.

- [x] **Step 1: Write failing tests**
  - In `test_config_core`:
    - duplicate YAML keys are rejected;
    - non-string keys are rejected;
    - `fields()` reports `ble.nmae` as unknown;
    - `cstr` escapes trigraphs and quotes;
    - write-if-changed keeps the mtime;
    - an unknown top-level key raises `ConfigError` with its path.
  - In `test_sections`: every section accepts its default, the bounds above are enforced, and the gesture ordering error message is unchanged from today.
  - In `test_buttons`:
    - With `ctx.board.buttons == 1` and two entries, the header has `BUTTON_SLOTS = BOARD_BUTTON_COUNT`, there is 1 row, and the warnings include `buttons[1] ignored: board esp32 has 1 button(s)`.
    - `"CMD+SHIFT+4"` emits `{"", 0x0A, 0x21}`, with an empty label when none is given.
  - Secrets:
    - A `ctx.secrets` of `{}` with `ota.enabled: true` yields no `OTA_PASSWORD` in the header, and the warnings include `OTA disabled: no ota_password in secrets.yaml`.
    - A secrets file with keys other than `ota_password`, `wifi_ssid` and `wifi_password` is rejected.
- [x] **Step 2: Run the tests and see them fail.** Run `python -m unittest discover -s scripts/tests -v`. The tests fail with `ModuleNotFoundError: config`.
- [x] **Step 3: Implement `scripts/config/core.py`.** It defines `UniqueKeyLoader`, `ConfigError(path, msg)`, `fields`, `integer`, `cstr`, `write_if_changed`, `Context` and `load_sections(data) -> list[module]`. The last loads the built-ins plus `data["schema"]`, resolved relative to the config file. Then implement the section modules and `render(data, ctx) -> (header: str, features: dict[str, bool])`.
- [x] **Step 4: Rewrite `scripts/platformio_config.py`.** Its job:
  - Skip entirely when `env["PIOPLATFORM"] == "native"`.
  - Resolve `custom_config` (default `config.yaml`) and `custom_board` (default `env["PIOENV"]`), and read `boards/boards.yaml` and the optional `secrets.yaml`.
  - Generate the header into `$BUILD_DIR/generated/`.
  - Append the CPPPATH and the CPPDEFINES, and print the warnings.
- [x] **Step 5: Restructure `platformio.ini`.**
  - `[platformio] extra_configs = platformio_override.ini`. Confirm that a missing file is tolerated. If it is not, document copying the `.example` file instead.
  - `[env]`:
    - `platform = <pioarduino stable release zip URL, exact tag>`
    - `framework = arduino`, `monitor_speed = 115200`
    - `extra_scripts = pre:scripts/platformio_config.py`
    - `lib_deps = h2zero/NimBLE-Arduino@<2.x that builds on 3.x>`
  - Board envs:
    - `sticks3` keeps today's flags. It adds M5Unified and M5GFX at versions that compile on 3.x.
    - `esp32s3` uses `esp32-s3-devkitc-1`.
    - `esp32c3` uses `esp32-c3-devkitm-1`.
    - `esp32` uses `esp32dev`.
  - `[env:native]`: `platform = native`, `test_framework = unity`, empty `lib_deps`, `lib_ignore = board, net, display, store`, `build_flags = -std=gnu++17 -Itest/stubs -Iexamples/macro-clicker/include`.
- [x] **Step 6: Run the tests and see them pass.** Run `python -m unittest discover -s scripts/tests -v`. All tests pass.
- [x] **Step 7: Confirm that each board env reaches compilation.**
  - Use a temporary `src/main.cpp` that only includes `generated_config.h`, logs `BOARD_LABEL`, and stays in `loop` doing `delay(1)`.
  - Run `.venv/bin/pio run -e sticks3 -e esp32s3 -e esp32c3 -e esp32`. All four report SUCCESS.
  - `include/` keeps the old headers until Tasks 2 to 7 move them. Old `src/*.cpp` files that no longer compile go behind `build_src_filter = +<main.cpp>` in `[env]` for now.

### Task 2: lib/core and lib/board

**Files:**
- Create:
  - `lib/core/src/{Every.h,Log.h}`
  - `lib/board/src/{Board.h,BoardM5.cpp,BoardGpio.cpp,BootHold.h}`
  - `test/test_every/test_main.cpp`
- Modify: the temporary `src/main.cpp`, which now blinks the LED and logs button edges.

**Interfaces:** Consumes the Task 1 CPPDEFINES. Produces `Every`, `LOG_*`, `board::*` and `holdAllButtonsAtBoot`, as given in **Interfaces**.

- [x] **Step 1: Write the failing test.** In `test_every`:
  - `Every e(100)`: `due(0)` is true, `due(50)` is false, `due(100)` is true, and `due(150)` is false.
  - The wrap case: `Every w(10)` with `due(0xFFFFFFF8)` true, then `due(2)` true.
- [x] **Step 2: Run the test and see it fail.** Run `.venv/bin/pio test -e native -f test_every`. It fails because `Every.h` is not found.
- [x] **Step 3: Implement `Every.h`, `Log.h`, `Board.h` and its two drivers, and `BootHold.h`.**
  - `BoardM5.cpp` (`#if BOARD_DRIVER_M5`) moves the M5 config, button and power code out of today's `main.cpp`. If `M5.getBoard() != board_M5StickS3` it logs a warning but does not halt.
  - `BoardGpio.cpp` (`#if BOARD_DRIVER_GPIO`) treats buttons as active-low with `INPUT_PULLUP`.
- [x] **Step 4: Run the test and see it pass.** Run `.venv/bin/pio test -e native -f test_every`. It passes.
- [x] **Step 5: Build every board env.** Run `.venv/bin/pio run -e sticks3 -e esp32s3 -e esp32c3 -e esp32`. All four report SUCCESS. Tasks 1 and 2 share commit 2: "Multi-board PlatformIO layout on Arduino-ESP32 3.x".

### Task 3: lib/input, lib/store and lib/ble_hid with native tests

**Files:**
- Move: `include/Gesture.h` to `lib/input/src/Gesture.h`.
- Move: `include/BleKeyboard.h` and `src/BleKeyboard.cpp` to `lib/ble_hid/src/`, and rename the class to `BleKeyboard`.
- Move: `tests/ble/*` to `test/stubs/`.
- Create: `lib/store/src/{Settings.h,Settings.cpp}`.
- Convert: `tests/test_gesture.cpp` to `test/test_gesture/test_main.cpp`, and `tests/test_ble.cpp` to `test/test_ble/test_main.cpp`.
- Delete: `include/ProfileStore.h` and `src/ProfileStore.cpp`. Their clamp logic moves to the example in Task 7.

**Interfaces:**
- Consumes: the `FEATURE_BLE_HID` guard.
- Produces: `Gesture` (unchanged), `BleKeyboard` and `Settings`.

- [ ] **Step 1: Convert the tests.**
  - Each top-level `{ ... }` block in `main()` becomes a named Unity test: `test_tap_without_double`, `test_double_window` and so on, named after what each block asserts.
  - `assert(x)` becomes `TEST_ASSERT_TRUE(x)`.
  - Add `setUp`, `tearDown` and `main` with `UNITY_BEGIN`, `RUN_TEST` and `UNITY_END`.
  - Every existing assertion stays.
- [ ] **Step 2: Run the tests.** Run `.venv/bin/pio test -e native -f test_gesture -f test_ble`. All tests pass, with the same case count as the old asserts. `test_ble` compiles `BleKeyboard.cpp` against `test/stubs`, with `FEATURE_BLE_HID=1` defined in `[env:native]` build_flags.
- [ ] **Step 3: Implement `Settings`.** Use `Preferences` with namespace `ns`, opened read-write for each call. `putU8` reads first and returns true without writing when the value is unchanged.
- [ ] **Step 4: Build `sticks3` and `esp32c3`.** Run `.venv/bin/pio run -e sticks3 -e esp32c3`. Both report SUCCESS. Commit 3: "Input, settings and BLE HID modules with native tests".

### Task 4: lib/improv and lib/net (WiFi, Improv Serial, dev OTA)

**Files:**
- Create:
  - `lib/improv/src/{Improv.h,Improv.cpp}`
  - `lib/net/src/{Net.h,Wifi.cpp,ImprovSerial.cpp,Ota.cpp}`
  - `test/test_improv/test_main.cpp`

**Interfaces:** Consumes `FEATURE_WIFI` and `FEATURE_OTA`, and the `AppConfig::WIFI_HOSTNAME`, `DEV_WIFI_*` and `OTA_PASSWORD` values. Produces the `Improv*` and `net::*` APIs given in **Interfaces**.

- [ ] **Step 1: Write the failing tests.**
  - First, re-read https://www.improv-wifi.com/serial/ to confirm the header (`IMPROV`, version `1`, type, length, data, checksum = low byte of the sum of all preceding bytes), the RPC layout (command, length, payload) and the WiFi payload (ssid_len, ssid, pass_len, pass).
  - The test cases:
    - **`test_roundtrip_state`:** `improvFrame(CurrentState, {2})` is parsed back by `ImprovParser` with type 1 and data `{2}`.
    - **`test_garbage_then_frame`:** bytes `"[I] boot\nIMPRO"` followed by a full valid frame yields exactly one frame.
    - **`test_bad_checksum_resyncs`:** a frame with a wrong checksum returns false, and the next valid frame returns true.
    - **`test_truncated_length`:** a frame whose declared length runs past the data, followed by a valid frame, parses only the valid one.
    - **`test_wifi_settings`:** an RPC payload for SSID `"home"` and password `"p@ss"` parses to those strings. An SSID longer than the cap returns false.
    - **`test_rpc_result`:** `improvRpcResult(GetInfo, {"ESP32 Starter","v1.0.0","ESP32-S3","Stick"}, 4)` encodes a total length followed by each string's length and bytes.
- [ ] **Step 2: Run the tests and see them fail.** Run `.venv/bin/pio test -e native -f test_improv`. The tests fail.
- [ ] **Step 3: Implement `Improv.cpp`.** It is a pure state machine that matches the header byte by byte and restarts on a mismatch. Its total size must stay at or under 200 lines.
- [ ] **Step 4: Run the tests and see them pass.** Run `.venv/bin/pio test -e native -f test_improv`. All tests pass.
- [ ] **Step 5: Implement `Wifi.cpp`, `ImprovSerial.cpp` and `Ota.cpp`.**
  - `Wifi.cpp` is non-blocking. It sets `WiFi.persistent(true)` and `setHostname`, and reconnects every 10 s while disconnected.
  - `ImprovSerial` answers `GetState`, `GetInfo` and `WifiSettings`. For `WifiSettings` it connects with a 15 s timeout, replies with `http://<ip>`, and sends `UnableToConnect` on timeout.
  - `Ota.cpp` uses ArduinoOTA with the hostname and password. It logs progress, and `otaActive()` is true from onStart until onEnd or onError.
- [ ] **Step 6: Build every board env.** Run `.venv/bin/pio run -e sticks3 -e esp32s3 -e esp32c3 -e esp32`. All report SUCCESS.
  - Also build once with a local `secrets.yaml` containing `ota_password: test` and confirm the build log shows no "OTA disabled" warning.
  - Delete that `secrets.yaml` afterwards.
  - Commit 4: "WiFi with Improv provisioning and dev-only LAN OTA".

### Task 5: lib/display

**Files:**
- Move: `include/Theme.h` to `lib/display/src/Theme.h`.
- Create: `lib/display/src/{Display.h,Display.cpp}`.

**Interfaces:**
- Consumes: `board::display()` and `FEATURE_DISPLAY`.
- Produces: the `Display` class. Its `header`, `splash`, `holdProgress` and `fatal` drawing is extracted from today's `src/Ui.cpp`, which is the header and battery chrome, the boot screen and the bond-reset screen.

- [ ] **Step 1: Implement `Display`.** Lift the drawing code verbatim where possible, without changing colors or layout. `Display.cpp` stays at or under 200 lines.
- [ ] **Step 2: Build `sticks3`.** Run `.venv/bin/pio run -e sticks3`. It reports SUCCESS.

### Task 6: Minimal default app

**Files:**
- Replace: `src/main.cpp`, at or under 200 lines.
- Create: `src/StatusView.{h,cpp}` (`StatusModel` with `operator==`, and `void drawStatus(Display&, const StatusModel&)`).
- Delete: `src/Ui.cpp`, `include/Ui.h` and `include/Macro.h`. Task 7 reads the originals from `c3e723e`.

**Interfaces:** Consumes everything from Tasks 2 to 5 and `AppConfig::BUTTON_ACTIONS`.

- [ ] **Step 1: Write `main.cpp`.**
  - Keep the patterns from today's `main.cpp`: module instances in an anonymous namespace, `handle(event)`, the `model()` diff redraw, and `Every` for the battery (30 s) and the idle redraw.
  - Startup order:
    1. Serial.
    2. `board::begin`.
    3. `Display` and its splash, only `#if FEATURE_DISPLAY`.
    4. The boot hold, which clears bonds after 3000 ms on 2-button boards or 5000 ms on 1-button boards.
    5. BLE.
    6. WiFi and Improv.
    7. OTA.
  - Loop behavior:
    - Each button has its own `Gesture`, whose assignment comes from `BUTTON_ACTIONS`.
    - A gesture whose action is assigned, while `keyboard.ready()`, calls `keyboard.send(mods, usage)` and blinks the LED.
    - Input received while not ready is discarded, as today.
    - While `otaActive()`, the loop skips input and calls `keyboard.releaseAll()`.
- [ ] **Step 2: Build every env.** Run `.venv/bin/pio run -e sticks3 -e esp32s3 -e esp32c3 -e esp32`. All report SUCCESS. Commit 5: "Display module and minimal default app wiring all modules".

### Task 7: Macro clicker example

**Files:**
- Create:
  - `examples/macro-clicker/{config.yaml,config_schema.py,README.md}`
  - `examples/macro-clicker/include/{Macro.h,Controller.h,MacroPlayer.h,ProfileStore.h,Ui.h}`
  - `examples/macro-clicker/src/{main.cpp,Ui.cpp,ProfileStore.cpp}`
- Create: `test/test_macro_controller_{gestures,picker}/test_main.cpp`, which is `tests/test_controller.cpp` split along its picker and chord groups, each at or under 300 lines.
- Create: `test/test_macro_player/test_main.cpp`.
- Create: `scripts/tests/test_macro_schema.py`, which contains the profile cases from the old `tests/test_config.py`.
- Modify: `platformio.ini`, adding `[env:sticks3-macro]`:
  - `extends = env:sticks3`
  - `custom_board = sticks3`
  - `custom_config = examples/macro-clicker/config.yaml`
  - `build_src_filter = -<*> +<../examples/macro-clicker/src/>`
  - `build_flags = ${env:sticks3.build_flags} -Iexamples/macro-clicker/include`
- Delete: the remaining `include/*` and `tests/*`.

**Interfaces:**
- Consumes: `keys.parse_combo` and `keys.ASCII` from Task 1, the section contract, `Settings`, `BleKeyboard`, `Display` and `board::*`.
- Produces: the `profiles` section (`KEY = "profiles"`, `INCLUDES = ["Macro.h"]`), whose header output is byte-identical to today's profile arrays.

- [ ] **Step 1: Port the tests.** Port the controller, player and profile-schema tests with every existing assertion. Run `.venv/bin/pio test -e native -f "test_macro_*"` and `python -m unittest scripts.tests.test_macro_schema`. Both fail until the files move.
- [ ] **Step 2: Move the code.**
  - Move the code and rename it to `BleKeyboard`. `ProfileStore` wraps `Settings("macro")` with today's clamp.
  - `Ui.cpp` uses the `Display` chrome and keeps its Home, Picker and Transmit screens.
  - `examples/macro-clicker/config.yaml` is today's config.yaml in the new section format, with `wifi` and `ota` disabled and `schema: config_schema.py`.
- [ ] **Step 3: Run the tests and see them pass.** Rerun both commands from Step 1. All tests pass.
- [ ] **Step 4: Build the macro env.** Run `.venv/bin/pio run -e sticks3-macro`. It reports SUCCESS. Commit 6: "Move macro clicker into examples/macro-clicker".

### Task 8: Board-driven packaging

**Files:**
- Modify: `scripts/package_firmware.py`, at or under 250 lines. Split out `scripts/firmware_images.py` with the image and partition parsing if needed.
- Replace: `tests/test_packaging.py` with `scripts/tests/test_packaging.py`.

**Interfaces:**
- Consumes:
  - `pio project config --json-output`, for the env list and options;
  - `boards/boards.yaml`;
  - `render(data, ctx)`, for the staleness check against `$BUILD_DIR/generated/generated_config.h`.
- Produces:
  - The CLI `package_firmware.py --output DIR --version V [--commit SHA] [--env E ...]`. It defaults to every env whose `custom_board` id equals its env name, which excludes `sticks3-macro`.
  - `firmware/index.json` and `firmware/<id>/{manifest.json,*.bin}`, as given in **Interfaces**.

- [x] **Step 1: Write the failing tests.** Use synthetic images and partition tables, as today.
  - **Offsets:** a classic ESP32 bootloader gets offset `0x1000`, and the S3 and C3 bootloaders get `0x0`.
  - **Partition-derived offsets:** the `boot_app0` offset equals the otadata offset, and the firmware offset equals the first app partition, both read from `partitions.bin`.
  - **NVS overlap:** a part whose sector-rounded range overlaps NVS raises `overlaps NVS`.
  - **Chip check:** an image whose chip id does not match `chip` is rejected.
  - **Manifest shape:** keys are `name`, `version`, `new_install_prompt_erase`, `new_install_improv_wait_time` and `builds`, and `builds[0].chipFamily` equals the board's `chip`.
  - **Improv wait:** `new_install_improv_wait_time == 0` when the config has wifi disabled.
  - **No secrets:** when a `secrets.yaml` exists, the dumped `index.json` contains none of its values.
  - **Atomic output:** if any board fails, nothing is written to the output.
- [x] **Step 2: Run the tests and see them fail.** Run `python -m unittest scripts.tests.test_packaging -v`. The tests fail.
- [x] **Step 3: Implement the packager.** Parameterize today's validations by chip. The parser for each chip comes from PlatformIO's vendored esptool (`ESP32FirmwareImage`, `ESP32S3FirmwareImage` and `ESP32C3FirmwareImage`), and the checksum and SHA checks are kept. Stage the output in a temporary directory and rename it into place.
- [ ] **Step 4: Run the tests and package the real builds.**
  - Run `python -m unittest scripts.tests.test_packaging -v` and the real packaging run, `python scripts/package_firmware.py --output web/public/firmware --version 0.0.0-dev`.
  - The tests pass.
  - The run prints one line per board, and 4 manifests exist.
  - Commit 7: "Board-driven firmware packaging for ESP Web Tools".

### Task 9: Microsite on ESP Web Tools

This task can run in parallel with Tasks 1 to 8, because it consumes only the `index.json` schema. It owns `web/` except for `web/src/styles/`.

**Files:**
- Delete:
  - `web/src/lib/{session,serial-adapter,flasher,firmware}.ts`
  - `web/src/release.ts`, `web/src/product.ts`
  - `web/src/components/{DevicePlate,FieldGuide,Installer}.astro`
  - `web/tests/flasher.test.ts`
- Create:
  - `web/src/lib/project.ts` (types, `readIndex(dir): FirmwareIndex | null`, `boardSummary(board): string`)
  - `web/src/content/site.ts` (generic template copy: hero, features, steps, faq)
  - `web/src/components/{Panel,Disclosure,KeyValue,Hero,BoardPicker,InstallCard,Steps,FeatureGrid,Downloads,Troubleshooting}.astro`
  - `web/tests/project.test.ts`
  - `web/tests/fixtures/index.json`
- Modify:
  - `web/scripts/check-firmware.ts`: it validates `index.json` and the hashes of all parts when the file is present, and exits 0 with the log line "No packaged firmware; building without a release" when it is absent.
  - `web/src/pages/index.astro`
  - `web/src/layouts/SiteLayout.astro`
  - `web/astro.config.mjs`: `site: process.env.SITE_URL ?? "http://localhost:4321"`, `base: process.env.BASE_PATH ?? "/"`.
  - `web/package.json`: name `esp32-starter-site`, add `esp-web-tools` at an exact current version, and remove `esptool-js` and `@noble/hashes`.

**Interfaces:**
- Consumes: `index.json` and each `<id>/manifest.json`, as given in **Interfaces**.
- Produces: the CSS class vocabulary listed in a comment at the top of each component. Task 10 styles it, so Tasks 9 and 10 must agree on it.

**Class vocabulary (fixed):**
- Layout: `.shell`, `.site-header`, `.site-footer`, `.section`, `.section-title`
- Panel: `.panel`, `.panel-head`, `.panel-tag`
- Disclosure: `.disclosure` (on `<details>`)
- Key-value: `.kv`, `.kv-label`, `.kv-value`
- Buttons: `.btn`, `.btn-primary`, `.btn-ghost`
- Boards: `.board-grid`, `.board-card`, `.board-meta`
- Content: `.feature-grid`, `.steps`, `.hero`, `.eyebrow`, `.muted`, `.note`

- [x] **Step 1: Write the failing tests.**
  - `readIndex` returns null for a missing directory.
  - With the fixture, it returns boards in file order.
  - `boardSummary` gives `"ESP32-S3 · 2 buttons · display · battery"` for the sticks3 board in the fixture, and `"ESP32 · 1 button"` for esp32.
  - `check-firmware` exits 0 with the "no release" log line on an empty directory and exits 1 on a hash mismatch.
- [x] **Step 2: Run the tests and see them fail.** Run `cd web && npm install && npm test`. The tests fail.
- [x] **Step 3: Implement `project.ts` and the components.**
  - `InstallCard` renders `<esp-web-install-button manifest={base + board.manifest}>` with an `activate` slot `.btn.btn-primary` and `unsupported` and `not-allowed` slot notes. It imports the element in a client `<script>`.
  - When the index is null, the page shows the Hero and a `.note` reading "No release published yet. Build locally with `pio run`, or publish a release to enable browser install."
  - The page copy mentions that release builds have OTA disabled.
- [ ] **Step 4: Run the checks.** Run `npm test && npm run check && npm run build`, first with `web/public/firmware` absent and then with the Task 8 output present. Both runs succeed. Commit 8: "Rebuild microsite on ESP Web Tools with shared components".

### Task 10: Site styles split

This task runs in parallel with Task 9 and owns `web/src/styles/`.

**Files:**
- Replace: `web/src/styles/theme.css` with `web/src/styles/{tokens,base,layout,components}.css` and `web/src/styles/index.css` (imports only). Each file stays at or under 300 lines.

- [x] **Step 1: Write the tokens.**
  - `tokens.css` defines the color tokens on `:root`, in today's palette as the dark theme plus a light theme under `prefers-color-scheme: light`.
  - It adds the scales `--space-1..6`, `--text-xs..xl`, `--track-tight|wide` and `--radius`.
  - No literal colors appear outside `tokens.css`.
- [x] **Step 2: Style the class vocabulary from Task 9.**
  - Fold today's late override blocks and their four separate `details` treatments into one `.disclosure` rule.
  - Put each media query next to the rule it modifies.
  - Responsive down to 360 px, with no horizontal scroll.
- [x] **Step 3: Check the result.**
  - Run `grep -c '#[0-9a-fA-F]\{3,6\}' web/src/styles/{base,layout,components}.css`, which gives 0 per file, and `wc -l web/src/styles/*.css`, where every file is at or under 300.
  - Take Playwright screenshots of `npm run preview` at 1280 px and 390 px, then review them.
  - Commit 9: "Split site styles into tokens, base, layout and components".

### Task 11: CI and release

**Files:**
- Create:
  - `.github/actions/setup/action.yml`: a composite action with input `node: boolean`. It sets up Python 3.12 with the pip cache, runs `pip install -r requirements.txt`, caches `~/.platformio`, and when `node` is set, sets up Node 22 with the npm cache on `web/package-lock.json`.
  - `scripts/check.sh`: runs `pio test -e native` and then `python -m unittest discover -s scripts/tests -v`.
- Modify:
  - `.github/workflows/ci.yml`. Jobs:
    1. `envs` outputs `python scripts/list_envs.py`.
    2. `build` runs a matrix over `fromJson(envs)` with `pio run -e ${{ matrix.env }}`.
    3. `test` runs `scripts/check.sh`, then the packaging dry-run.
    4. `web` runs `npm ci`, `check`, `test` and `build`.
  - `.github/workflows/release.yml`. It triggers on a `v*.*.*` tag push and on `workflow_dispatch`, and keeps today's guard against an existing release. It:
    - builds all envs;
    - runs `scripts/check.sh`;
    - packages;
    - builds the site with `BASE_PATH: /${{ github.event.repository.name }}/` and `SITE_URL: https://${{ github.repository_owner }}.github.io`;
    - attaches `index.json` and the per-board folders, zipped per board, with the title `${{ github.event.repository.name }} $VERSION`;
    - deploys Pages.
  - Neither workflow contains a project name or env name literal.

- [ ] **Step 1: Validate the workflows.** Run `python -c "import yaml,sys;[yaml.safe_load(open(f)) for f in sys.argv[1:]]" .github/workflows/*.yml .github/actions/setup/action.yml` and `bash scripts/check.sh`. Both exit 0. If `actionlint` is available, run it and expect 0 findings. Commit 10: "Reusable CI setup action and per-board build matrix".

### Task 12: Docs and agent guide

**Files:**
- Rewrite: `README.md`, at or under 250 lines: what it is, the quick start, the board table, the module table, the commands and links to the docs.
- Create:
  - `docs/{getting-started,boards,configuration,modules,flashing-and-ota,testing,new-project-checklist}.md`, each at or under 300 lines.
- Rewrite:
  - `AGENTS.md`, for the template: module boundaries per lib, the global constraints above, the build and validation commands, and the flash-safety rules kept from today. Keep the CodeGraph block.
  - `TODO.md`, as the remaining physical checks.
- Move: the macro-specific README sections into `examples/macro-clicker/README.md`.

- [ ] **Step 1: Check the docs against the code.**
  - Every config key documented in `configuration.md` exists in `scripts/config/sections/*.py`.
  - The validation limits match the code.
  - Every command in the docs has been run once in this session.
  - Run `wc -l README.md docs/*.md AGENTS.md`, where all files are within their limits.
  - Commit 11: "Template docs, new-project checklist and agent guide".

### Task 13: Whole-branch review and verification

- [ ] **Step 1: Review.** Run the feature-dev code-reviewer on `git diff c3e723e..HEAD`, and fix the confirmed findings.
- [ ] **Step 2: Verify everything.**
  - Run `.venv/bin/pio run` for every env from `scripts/list_envs.py`.
  - Run `bash scripts/check.sh`.
  - Package the builds.
  - In `web/`, run `npm ci`, `npm run check`, `npm test` and `npm run build`.
  - Take the Playwright screenshots.
  - Run `find . -path ./web/node_modules -prune -o -path ./.pio -prune -o \( -name '*.cpp' -o -name '*.h' -o -name '*.py' -o -name '*.ts' -o -name '*.astro' -o -name '*.css' \) -print | xargs wc -l | sort -n | tail`, which shows nothing over 300.
  - Commit 12: "Review fixes and final verification".
