# Getting started

Build the default app for your board, read its serial log, then pair Bluetooth
and join WiFi. Hardware results below are what the code is written to do; none
has been observed on a device yet (see [../TODO.md](../TODO.md)).

## 1. Set up tools

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt   # PlatformIO and PyYAML
```

On Windows the same tools are `.venv\Scripts\pio` and `.venv\Scripts\pip`. The
generator runs inside PlatformIO's own Python, so PyYAML must be installed in
the environment that provides `pio`. The first build downloads the pinned
pioarduino platform (Arduino-ESP32 3.3.x), M5Unified, M5GFX and NimBLE.

## 2. Build

```sh
.venv/bin/pio run -e sticks3     # M5Stack Stick-S3
.venv/bin/pio run -e esp32s3     # ESP32-S3 DevKitC-1
.venv/bin/pio run -e esp32c3     # ESP32-C3 DevKitM-1
.venv/bin/pio run -e esp32       # classic ESP32 DevKit
```

Each build validates `config.yaml`, writes
`.pio/build/<env>/generated/generated_config.h` and prints warnings such as
`warning: OTA disabled: no ota_password in secrets.yaml`. A configuration error
stops the build with `Configuration error:` and the path of the bad key. Never
edit or commit the generated header.

## 3. Flash and watch the log

Identify the device first; port names are examples, not stable identifiers.

```sh
.venv/bin/pio device list
.venv/bin/pio run -e sticks3 -t upload
.venv/bin/pio device monitor
```

Close the serial monitor before uploading. To override the port without
editing `platformio.ini`, copy `platformio_override.ini.example` to
`platformio_override.ini` (gitignored) and set `upload_port` and
`monitor_port`. Recovery and the browser installer are in
[flashing-and-ota.md](flashing-and-ota.md).

The firmware never waits for a USB serial host. The log prints at 115200 baud,
for example `[I] ESP32 Starter <version> on <board label> (<n> buttons)`.
Levels are set with `-DLOG_LEVEL=n` (0 off to 4 debug, default 3).

## 4. Use the default app

| Input | Result |
|---|---|
| Single, double or long press on a button | Sends that button's key combo over BLE HID, if configured and the link is ready |
| Hold every button at boot | Clears Bluetooth bonds: 3 s on 2-button boards, 5 s on 1-button boards |

The bond-clear hold is measured after the display splash. After clearing,
forget the device in the host's Bluetooth settings and pair again. Changing
the HID report descriptor requires the same forget-and-re-pair step.

Input is discarded while Bluetooth is not connected and ready; actions are
never queued or replayed. The status LED, where the board has one, blinks
briefly on each sent action. With a display the screen shows Bluetooth, WiFi
(with IP), battery and the last action label.

## 5. Pair Bluetooth

1. Power the board. It advertises as the `ble.name` from `config.yaml`
   (default `ESP32 Starter`).
2. Pair it from the host's Bluetooth settings.
3. Press a button with an assigned action and check the host receives the combo.

Pairing reliability and typing behavior are pending physical checks.

## 6. Join WiFi

With `wifi.enabled: true`, WiFi can be provisioned over USB with Improv Serial.
This works from the web installer right after install, or from any Improv
client on the serial port. The device keeps provisioning non-blocking: the
handler is advanced by `net::updateImprov()` from the main loop, gives a
connection attempt 15 seconds, saves the new credentials only after the
connection succeeds, and restores the previous ones when it fails.
Improv frames share the log port; other bytes are ignored.

For development you can instead set `wifi_ssid` and `wifi_password` in
`secrets.yaml` (copy `secrets.example.yaml`). They are used only when the device
has no stored credentials; stored credentials always win.

## 7. Edit the config

Change `config.yaml` (names, buttons, timing, modules) and rebuild. Keys are
in [configuration.md](configuration.md). To keep several configs, set
`custom_config = my-config.yaml` for an env in `platformio_override.ini`.

## Next

- Add OTA for development: [flashing-and-ota.md](flashing-and-ota.md).
- Turn the template into your own repo: [new-project-checklist.md](new-project-checklist.md).
- Try the original product: [../examples/macro-clicker/README.md](../examples/macro-clicker/README.md).
