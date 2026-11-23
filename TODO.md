# Remaining physical checks

Every item below is pending. Nothing here has been observed on a device: the
firmware compiles for four boards and the host tests pass, which says nothing
about pairing, display, buttons, WiFi, OTA or battery. Mark an item done only
with a date, the board, the firmware version and who observed it. Never
claim battery life.

Before any flashing: run `.venv/bin/pio device list`, confirm the chip and flash
size match the env, and close serial monitors. Port names are examples.

## All boards

- [ ] USB upload with `pio run -e <env> -t upload` keeps bonds and WiFi settings.
- [ ] Serial log prints the project name, version and button count at boot and
  never blocks without a host attached.
- [ ] Bluetooth: pairs as `ble.name`, a configured button types its combo on the
  host, and input while disconnected is discarded without replay.
- [ ] Hold-at-boot clears bonds (times in [docs/getting-started.md](docs/getting-started.md)); the host
  must forget and re-pair afterwards.
- [ ] WiFi provisioning with Improv Serial from the web installer and from a
  serial client: success persists across reboot, a wrong password leaves the
  previous credentials working, and the 15 s timeout reports failure.
- [ ] Dev WiFi credentials from `secrets.yaml` are used only when none are stored.
- [ ] Browser install from a packaged release: normal install keeps bonds and
  WiFi settings; choosing erase removes them; Improv prompt appears when
  `wifi.enabled` is true.
- [ ] OTA (development build with `ota_password`): upload over `espota` works,
  wrong password is rejected, buttons are ignored during transfer, the device
  recovers after a failed transfer.
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
- [ ] A build with OTA on (see the measured size in [docs/boards.md](docs/boards.md#memory)) boots
  and completes an OTA update; the app partition is nearly full.
- [ ] Bluetooth and WiFi coexist stably.

## `esp32` (classic DevKit)

- [ ] Compile-verified only. BOOT button (GPIO 0) and plain LED (GPIO 2) work.
- [ ] Browser install writes the bootloader at 0x1000 and the board boots.
- [ ] Bluetooth and WiFi coexist stably on the 90% full app partition.

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
