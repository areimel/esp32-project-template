# Boards

Pick the env that matches your hardware, or add a row for a new board. Facts
are split on purpose: `boards/boards.yaml` says what the hardware has,
`platformio.ini` says how to build for it. Nothing is stated twice.

## Capability table (`boards/boards.yaml`)

| Env / board id | Label | Chip | Driver | Buttons | Status LED | Display | Battery |
|---|---|---|---|---|---|---|---|
| `sticks3` | M5Stack Stick-S3 | ESP32-S3 | `m5` (M5Unified) | 2 | none | yes | yes |
| `esp32s3` | ESP32-S3 DevKitC-1 | ESP32-S3 | `gpio` | 1 on GPIO 0 (BOOT) | GPIO 48, RGB | no | no |
| `esp32c3` | ESP32-C3 DevKitM-1 | ESP32-C3 | `gpio` | 1 on GPIO 9 (BOOT) | GPIO 8, RGB | no | no |
| `esp32` | ESP32 DevKit (classic) | ESP32 | `gpio` | 1 on GPIO 0 (BOOT) | GPIO 2, plain | no | no |

GPIO buttons are read active-low with the internal pull-up. An RGB LED is
driven with `rgbLedWrite`; a plain LED with `digitalWrite`. Pin assignments
are copied from the boards' standard layouts and are not yet verified against
your hardware; check them against your own board before relying on them.

`sticks3-macro` is a second env on the `sticks3` board that builds the
[macro clicker example](../examples/macro-clicker/README.md); `native` is the
host test env and has no board.

## Build facts (`platformio.ini`)

| Env | PlatformIO board | Partitions and memory | Extra |
|---|---|---|---|
| `sticks3` | `esp32-s3-devkitc-1` | `default_8MB.csv`, `qio_opi`, 8 MB flash | PSRAM, USB CDC on boot, M5Unified, M5GFX |
| `esp32s3` | `esp32-s3-devkitc-1` | board default | none |
| `esp32c3` | `esp32-c3-devkitm-1` | board default | none |
| `esp32` | `esp32dev` | board default | none |

All envs share the pioarduino platform (Arduino-ESP32 3.3.x) and
NimBLE-Arduino from `[env]`. Pins are exact versions.

## Memory

Compile-only measurements, one set: `pio run -e <env>` on commit `b049935`
with no `secrets.yaml` (OTA off), the default `config.yaml` (BLE HID on, WiFi
on, display on where the board has one). These show headroom, not behavior.

| Env | Flash (app partition) | RAM |
|---|---|---|
| `sticks3` | 41.0% of 3,342,336 bytes | 19.2% |
| `esp32s3` | 33.7% of 3,342,336 bytes | 17.3% |
| `esp32c3` | 94.6% of 1,310,720 bytes | 14.7% |
| `esp32` | 90.9% of 1,310,720 bytes | 18.6% |

The C3 and classic ESP32 apps use the default two-app layout and have little
room. The `Flash:` line undercounts the image: the `esp32c3` `firmware.bin` is
1,270,320 bytes (96.9% of the partition) with OTA off. One build with a
temporary `secrets.yaml` (OTA and dev WiFi credentials on) printed
`Flash: 97.7%`, but its `firmware.bin` was 1,317,440 bytes, larger than the
1,310,720-byte app partition: with the default config, OTA does not fit on the
C3 (the packager also rejects an oversized image). `sticks3` with OTA on printed
42.1%. Before adding code, check the `Flash:` line and the `firmware.bin` size;
drop modules in `config.yaml` (for example `ble.enabled: false`) if you run
out, and rebuild with OTA only when you need it. Release and CI builds ship with OTA off. Re-measure after any
dependency, source or config change; these numbers move.

## What is verified

| Env | Status |
|---|---|
| `sticks3` | Compiles. One boot observed over USB serial (see [../TODO.md](../TODO.md#observed)); display, pairing, typing, buttons and everything else pending |
| `esp32s3`, `esp32c3`, `esp32` | Compiles. No hardware behavior has been observed |
| `native` | Unit tests pass on the host |

Per-board physical checks are in [../TODO.md](../TODO.md).

## One-button boards

The default `config.yaml` defines two buttons. On a 1-button board the second
entry is dropped with a build warning, and the first button is the only input.
On 1-button boards the hold-at-boot to clear Bluetooth bonds is 5 s instead
of 3 s ([getting-started.md](getting-started.md#4-use-the-default-app)). `display:` is accepted everywhere and ignored on
boards without a display.

## Add a board

1. Add a row to `boards/boards.yaml`. Required: `label`, `chip` (`ESP32`,
   `ESP32-S3` or `ESP32-C3`; the packager has image parsers for these only),
   `driver` (`m5` or `gpio`), `display`, `battery`. A `gpio` row also needs
   `button_pins` and may have `led: {pin, rgb}`; an `m5` row needs `buttons`.
2. Add `[env:<id>]` to `platformio.ini` with the PlatformIO `board` and any
   partition, memory or flash settings. Use `custom_board = <id>` when the env
   name differs from the board id.
3. If the chip is new, extend `IMAGE_CLASSES` in `scripts/firmware_images.py`,
   and `BOOTLOADER_OFFSETS` if its bootloader is not at `0x0`.
4. Build it: `.venv/bin/pio run -e <id>`. CI and the release workflow pick the
   env up automatically from `scripts/list_envs.py`.
5. Use [../TODO.md](../TODO.md) to record the physical checks you still owe.

The `gpio` driver needs only pins. The `m5` driver is written for the
Stick-S3; other M5Stack boards have not been tried.
