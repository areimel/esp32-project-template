# Remaining physical checks

Every checklist item below is pending. The firmware compiles for every env and
the host tests pass, which says nothing about pairing, display, buttons, WiFi,
OTA or battery. The only observation so far is recorded under
[Observed](#observed). Mark an item done only with a date, the board, the
firmware version and who observed it. Never claim battery life.

Before any flashing: run `.venv/bin/pio device list`, confirm the chip and flash
size match the env, and close serial monitors. Port names are examples.

## Observed

- 2026-11-23, M5Stack Stick-S3 (ESP32-S3-PICO-1, 8 MB flash, 8 MB PSRAM), env
  `sticks3`, build `v0.1.0-29-gcfbe729`, observed by Claude over USB serial:
  the default app boots and prints its version, `M5Stack Stick-S3 (2 buttons)`,
  `BLE advertising: ESP32 Starter`, a battery reading, an Improv state frame
  and `wifi: no credentials`. It also printed the boot error
  `STA connect failed! 0x300a: ESP_ERR_WIFI_SSID`. Later firmware no longer
  attempts a connection without credentials; the re-check on hardware is
  pending. Display, pairing, typing, buttons, bond reset, Improv provisioning,
  OTA, the macro example and all other boards remain pending.

## All boards

- [ ] USB upload with `pio run -e <env> -t upload` keeps bonds and WiFi settings.
- [ ] Serial log prints the project name, version and button count at boot and
  never blocks without a host attached.
- [ ] Bluetooth: pairs as `ble.name`, a configured button types its combo on the
  host, and input while disconnected is discarded without replay.
- [ ] Hold-at-boot clears bonds (times in [docs/getting-started.md](docs/getting-started.md)); the host
  must forget and re-pair afterwards. On the GPIO boards, BOOT pressed within
  about 1 s after reset starts the hold, and BOOT held during reset still
  enters download mode. A boot without a hold continues about 1 s later.
- [ ] WiFi provisioning with Improv Serial from the web installer and from a
  serial client: success persists across reboot, a wrong password leaves the
  previous credentials working, and the 15 s timeout reports failure.
- [ ] Dev WiFi credentials from `secrets.yaml` are used only when none are stored,
  and never overwrite Improv-provisioned credentials (they stay in RAM).
- [ ] With no credentials at all, boot no longer logs `ESP_ERR_WIFI_SSID`; with
  stored credentials, the status shows WiFi connecting while disconnected and
  the 10 s retry reconnects after a drop.
- [ ] The STA hostname on the network (DHCP / router list) is `wifi.hostname`.
- [ ] After Improv provisioning the installer offers no dead "Visit device" link.
- [ ] Browser install from a packaged release: normal install keeps bonds and
  WiFi settings; choosing erase removes them; Improv prompt appears when
  `wifi.enabled` is true.
- [ ] OTA (development build with `ota_password`): upload over `espota` works,
  wrong password is rejected, a key held when the transfer starts is released
  on the host, `OTA UPDATING` shows on the display, and the device resumes
  normal input after a failed transfer.
- [ ] Release build confirmed to have OTA off.

## `sticks3` (M5Stack Stick-S3)

- [ ] Both buttons map to the configured combos for single, double and long press.
- [ ] Display: splash, status screen (Bluetooth, WiFi with IP, battery, last
  action), bond-hold screen and brightness setting render correctly.
- [ ] Battery percentage and charging indicator are plausible.
- [ ] Status LED behavior, if the hardware has one, matches `BOARD_LED_PIN`.
- [ ] Browser install works end to end on this board.

## `esp32s3` (DevKitC-1)

- [ ] BOOT button (GPIO 0) is read as button 0 and does not interfere with boot.
- [ ] RGB status LED on GPIO 48 blinks on a sent action.
- [ ] Flash and RAM headroom is acceptable with OTA on.

## `esp32c3` (DevKitM-1)

- [ ] Compile-verified only. BOOT button (GPIO 9) and RGB LED (GPIO 8) work.
- [ ] A build with OTA on boots and completes an OTA update. With the default
  config the OTA image does not fit the app partition (measured, see
  [docs/boards.md](docs/boards.md#memory)); drop a module first.
- [ ] Bluetooth and WiFi coexist stably.

## `esp32` (classic DevKit)

- [ ] Compile-verified only. BOOT button (GPIO 0) and plain LED (GPIO 2) work.
- [ ] Browser install writes the bootloader at 0x1000 and the board boots.
- [ ] Bluetooth and WiFi coexist stably on the nearly full app partition (see [docs/boards.md](docs/boards.md#memory)).

## `sticks3-macro` (macro clicker example)

- [ ] Physical acceptance list in
  [examples/macro-clicker/README.md](examples/macro-clicker/README.md) (pending
  from that example).

## Site

- [ ] Rendered site checked on desktop and phone widths with a real packaged
  release, in light and dark mode.
- [ ] Release workflow run on a real tag: assets attached, Pages deployed.

## Known limitations

- Pin assignments for the GPIO boards come from standard layouts and have not
  been checked against hardware.
- Release and CI builds have no OTA; OTA exists only for development builds.
- No OTA rollback.
- No battery-life measurement exists.
