# Modules

Each directory under `lib/` is a PlatformIO library with its public headers in
`lib/<name>/src/`. `src/main.cpp` wires them together; the headers below are
the authoritative API.

## Feature gating

`config.yaml` drives four flags, emitted as compiler defines (0 or 1) by
`scripts/platformio_config.py`:

| Flag | On when |
|---|---|
| `FEATURE_BLE_HID` | `ble.enabled` |
| `FEATURE_WIFI` | `wifi.enabled` |
| `FEATURE_OTA` | `ota.enabled`, `wifi.enabled` and a password; see [flashing-and-ota.md](flashing-and-ota.md#ota-development-only) |
| `FEATURE_DISPLAY` | `display.enabled` and the board has a display |

Every optional module wraps its whole source in `#if FEATURE_<X>`, so a disabled
module compiles to nothing regardless of PlatformIO's library finder mode. App
code guards each use the same way, as `src/main.cpp` does. Do not call an
optional module from always-on code without a guard.

## Always-on modules

| Module | Header | API |
|---|---|---|
| `lib/core` | `Log.h` | `LOG_E/W/I/D(fmt, ...)` print `[E]`..`[D]` lines to `Serial`; `LOG_LEVEL` 0-4, default 3 |
| `lib/core` | `Every.h` | `Every(periodMs)`; `due(now)` is true on the first call, then once per period; wrap-safe, never bursts |
| `lib/board` | `Board.h` | `board::begin(brightness)`, `update()`, `buttonCount()`, `isPressed(i)`, `batteryPercent()` (-1 if none), `charging()`, `setStatusLed(on)`, and `display()` when `BOARD_HAS_DISPLAY` |
| `lib/board` | `BootHold.h` | `holdAllButtonsAtBoot(holdMs, progress, onHeld)`: true if every button is held for `holdMs`, with the hold starting within `kBootHoldGraceMs` (1000 ms) of the call |
| `lib/input` | `Gesture.h` | `Gesture(debounce, doubleMs, longMs)`; `update(raw, now)` returns `NONE`, `SINGLE`, `DOUBLE` or `HOLD`; `setAssigned`, `reset`, `cancel`, `pressed`, `holdFired` |
| `lib/store` | `Settings.h` | `Settings(ns)`; `getU8(key, def)`; `putU8(key, v)` skips the write when unchanged |
| `lib/improv` | `Improv.h` | Improv Serial codec: `ImprovParser::feed`, `improvFrame`, `improvRpcResult`, `improvWifiSettings` |

Notes:

- `board` selects `BoardM5.cpp` or `BoardGpio.cpp` from `BOARD_DRIVER_M5` or
  `BOARD_DRIVER_GPIO`. Neither waits for USB serial. The GPIO driver reads the
  `BOARD_BUTTON_PINS` list (active-low), the M5 driver uses M5Unified.
- `holdAllButtonsAtBoot` waits up to `kBootHoldGraceMs` (1000 ms) for every
  button to be pressed, so a boot without a hold continues about 1 s later; a
  board with no buttons returns at once. The grace window exists because the
  GPIO boards' only button is a strapping pin (GPIO 0, or GPIO 9 on the C3):
  held during reset it enters ROM download mode, so it is pressed right after
  reset instead. While held it calls `progress(remainingMs)`. When the hold
  completes, `onHeld()` runs first (the maintenance action), then `progress(0)`
  draws the done screen, then it waits for release for at most 5000 ms before
  continuing. The default app's hold times are in
  [getting-started.md](getting-started.md#4-use-the-default-app).
- `Gesture` is header-only with no Arduino includes. A press is always
  classified; whether it maps to an action is the caller's decision. After
  changing assignments or the link state, call `reset()` or `cancel()`.
- `Settings` opens and closes the NVS namespace on every call: read-only for
  `getU8` (a missing namespace returns `def` and is not created), read-write for
  `putU8`. The default app does not use it; the macro example persists the
  active profile with it.
- `ImprovParser` ignores non-Improv bytes, resyncs after a bad checksum or a
  truncated frame, and has no Arduino dependency.

## Optional modules

### `lib/ble_hid`: `BleKeyboard` (`FEATURE_BLE_HID`)

| Call | Behavior |
|---|---|
| `begin(name, keystrokeMs)` | Starts NimBLE and advertising as `name` |
| `update(now)` | Call every loop; advances the press, hold, release, gap phases |
| `send(mods, usage)` | Accepts one stroke when `ready()` and `idle()`; otherwise returns false. Modifier-only chords use `usage 0` |
| `releaseAll()` | Drops an unsent stroke and releases a held key |
| `connected()`, `ready()` | Link up; link up, encrypted and subscribed |
| `linkGeneration()` | Changes on connect, disconnect, resubscribe, bond clear or failed encryption; abort work when it changes |
| `setBatteryLevel(pct)`, `clearBonds()` | Battery service value; remove all bonds |

There is no queue: a stroke accepted before a reconnect is never sent after it.
Release is an explicit all-zero report. The HID report descriptor is fixed;
changing it requires telling users to forget the device and re-pair.

### `lib/net`: WiFi, Improv, OTA (`FEATURE_WIFI`, `FEATURE_OTA`)

Declared in `Net.h`, namespace `net`:

| Call | Behavior |
|---|---|
| `beginWifi(hostname, devSsid, devPass)` | Station mode; stored credentials win, dev credentials are used only when none are stored and are kept in RAM (never written to flash); with neither, no connection is attempted |
| `updateWifi(now)` | Non-blocking; while not connected and credentials are configured, retries every 10 s (the framework also auto-reconnects) |
| `wifiState()`, `ipAddress()` | `NoCredentials` (no SSID in the STA config), `Connecting` or `Connected`; `""` when not connected |
| `beginImprov(firmware, version, chip, deviceName)` | Registers the info reported over Improv and announces state |
| `updateImprov()` | Drains `Serial`, answers Improv RPCs and provisions WiFi; call from `loop()` |
| `beginOta(hostname, password, onStart = nullptr)`, `updateOta()` | ArduinoOTA. `updateOta()` runs a whole transfer inline; `onStart()` runs once as it begins. Success restarts the device; on failure `updateOta()` returns |

Improv shares the log port, so log text and frames can interleave and
non-Improv bytes are ignored. Provisioning is non-blocking: it is advanced only
by `updateImprov()`, allows a connection attempt 15 s, persists the new
credentials only after a successful connection, and restores the previous
credentials on failure. A successful provision answers with an RPC result that
has no redirect URL, because the device serves no web page.

The loop does not run during an OTA transfer. The default app's `onStart` hook
releases any held key and draws the `OTA UPDATING` row once; button presses
during the transfer are never read. If the transfer fails, the app resets its
gesture state and carries on.

### `lib/display`: `Display` (`FEATURE_DISPLAY`)

For the 240x135 panel only, built on M5GFX. It draws; apps decide what to show.

| Call | Behavior |
|---|---|
| `begin(gfx, brightness)` | Creates the sprite; call after `board::begin` |
| `canvas()`, `push()` | Compose on the sprite, then copy it to the panel |
| `header(title, status, battery, charging)` | Shared title bar (battery -1 shows `--`); does not clear or push |
| `splash(name, version)` | About 700 ms, blocking |
| `holdProgress(action, remainingMs, done)` | Boot maintenance screen, `action` truncated to 11 characters |
| `fatal(msg)` | Red halt screen |

`Theme.h` holds the RGB565 palette. `Draw.{h,cpp}` are shared text and shape
helpers (namespace `draw`) used by `Display`, `StatusView` and the macro
example's screens. `src/StatusView.{h,cpp}` is the default
app's screen: a plain `StatusModel` (Bluetooth, WiFi and IP, battery, last
action) and `drawStatus()`. Add screens by writing a model and a draw function;
keep input logic out of them.

## Dependency direction

`main.cpp` depends on every module. `Display`, `StatusView`, `BleKeyboard`
and `net` do not depend on each other except `StatusView -> Display` and
`StatusView -> Net` (for the `WifiState` mapping). `board`, `input`, `store`,
`core` and `improv` know nothing about the optional modules. Keep it that way:
pure logic stays free of Arduino headers so it runs under `pio test -e native`.

## Adding a module

1. Create `lib/<name>/src/<Name>.h` (and `.cpp`).
2. If it is optional, add a section module under `scripts/config/sections/` that
   returns a `FEATURE_<X>` flag from `features()`, and wrap the `.cpp` in
   `#if FEATURE_<X>`.
3. If it is pure logic, add `test/test_<name>/test_main.cpp`; if it touches the
   hardware, add it to `lib_ignore` of `[env:native]` in `platformio.ini`.
4. Keep every file at or under 300 lines.
