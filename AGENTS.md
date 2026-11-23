# ESP32 Starter Template

## Working agreements

- This is a template: keep the default app small and readable, prefer clear
  modules over extra features, and do not add network services, runtime
  configuration systems or captive portals without a request. The only runtime
  configuration is WiFi credentials provisioned over Improv Serial.
- Targets are the envs in `platformio.ini`: `sticks3`, `esp32s3`, `esp32c3`,
  `esp32`, `sticks3-macro` and `native`, on the pinned pioarduino platform
  (Arduino-ESP32 3.3.x), NimBLE, M5Unified and M5GFX. Do not bump pins without
  a build of every env.
- Preserve user changes. Do not push commits unless requested. Use a TODO list
  and focused local commits with descriptive titles and bodies. Keep parallel
  workstreams on distinct files and coordinate shared interfaces.

## Sources of truth

| Fact | Lives in | Never also in |
|---|---|---|
| Project name, BLE name, module switches, gestures, buttons | `config.yaml` | firmware, web copy, generated headers (`generated_config.h` is generated, ignored) |
| Version | git tag or `--version`; fallback `git describe --tags --always`, then `0.0.0-dev` | `config.yaml` |
| Build facts: board, partitions, flash, memory type | `platformio.ini` | `boards/boards.yaml` |
| Board capabilities: label, chip, buttons or pins, LED, display, battery, driver | `boards/boards.yaml` | `platformio.ini` |
| Bootloader offset | one map in `scripts/firmware_images.py` (`ESP32` 0x1000, others 0x0) | elsewhere |
| OTA password, dev WiFi credentials | gitignored `secrets.yaml` | `config.yaml`, tests, fixtures, manifests, `index.json` |
| Site project copy | `firmware/index.json` (from `config.yaml`), then `web/src/content/site.ts` | duplicated literals |

## Global constraints

- **No secrets** in `config.yaml`, tests, fixtures, manifests or `index.json`.
  `config.yaml` is embedded in published firmware and text in it is public.
- **OTA scope:** compiled in only when `ota.enabled`, `wifi.enabled` and an
  `ota_password` in `secrets.yaml` all hold. CI and release builds have no
  secrets file, so they ship with OTA off.
- **Gating:** every optional module's `.cpp` is wrapped in `#if FEATURE_<X>`
  (`FEATURE_BLE_HID`, `FEATURE_WIFI`, `FEATURE_OTA`, `FEATURE_DISPLAY`). Do not
  rely on `lib_ldf_mode`. `FEATURE_DISPLAY` also needs a board with a display.
- **Serial:** never block waiting for USB serial. Improv Serial shares the log
  port and must tolerate non-Improv bytes.
- **Reflash safety:** the web installer must never write inside the NVS
  partition. Parts stay separate (bootloader, partitions, boot_app0, firmware);
  no merged image; `new_install_prompt_erase: true`.
- **File size:** every source file stays at or under 300 lines (README at or
  under 250). Anything repeated three or more times becomes a shared helper or
  component.
- **HID descriptor:** unchanged. Changing it requires telling users to forget
  the device and re-pair.
- **Keep Bluetooth awake.** Do not add automatic sleep without considering
  missed input and reconnect delay.
- **Hardware claims:** report hardware behavior as pending unless observed.
  Compile success is the only claim for `esp32c3` and `esp32`. Make no battery
  life claims.

## Module boundaries

- `include/Macro.h` and the `examples/macro-clicker/` sources belong to the
  example only; the default app does not include them.
- `scripts/generate_config.py` plus `scripts/config/core.py` and
  `scripts/config/sections/*.py` validate YAML and compile combos and labels into
  `generated_config.h`; firmware never parses key names or text. Each section
  exports `KEY`, `REQUIRED`, `DEFAULT`, `INCLUDES`, `validate(value, ctx)`,
  `emit(cfg, ctx)` and optionally `features(cfg, ctx)`. Unknown keys are errors
  reported with the key path. `scripts/platformio_config.py` runs it before each
  build and defines the `BOARD_*` and `FEATURE_*` macros.
- `lib/core`: `Log.h` and `Every.h`. Pure, no hardware.
- `lib/board`: `Board.h` and `BootHold.h`, with `BoardM5.cpp` or
  `BoardGpio.cpp` chosen by `BOARD_DRIVER_*`. The only place that touches
  button pins, the status LED, battery or the M5 display handle.
- `lib/input/src/Gesture.h`: pure, header-only, host-tested; no Arduino or
  hardware includes.
- `lib/store`: `Settings` wraps NVS (Preferences) and skips unchanged writes.
- `lib/ble_hid`: HID transport only, one keystroke at a time, paced by
  `keystroke_ms`; no queue, no macro logic. `FEATURE_BLE_HID`.
- `lib/improv`: pure Improv Serial codec, host-tested.
- `lib/net`: WiFi, Improv Serial handler and ArduinoOTA. Improv is non-blocking
  and advanced by `net::updateImprov()`: 15 s connection timeout, credentials
  persisted only after success, previous credentials restored on failure.
  `FEATURE_WIFI`, `FEATURE_OTA`.
- `lib/display` (`Display.h`, `Theme.h`) and `src/StatusView.{h,cpp}`: render a
  plain model; no input logic. `FEATURE_DISPLAY`.
- `src/main.cpp`: wiring only: board -> gestures -> keyboard -> status view.
- `scripts/package_firmware.py` and `scripts/firmware_images.py`: read build
  outputs, validate images and the NVS-overlap guard, write ESP Web Tools
  manifests and `index.json`.
- `web/` is the independent Astro site. It reads everything from the packaged
  `index.json` and `src/content/site.ts`; it does not duplicate `config.yaml`.

## examples/macro-clicker

Built by env `sticks3-macro`; it has its own `config.yaml`, `config_schema.py`
(`profiles` section), sources and native tests (`test_macro_*`,
`scripts/tests/test_macro_schema.py`). Profiles stay compile-time; the only
runtime state is the remembered active profile index. When changing it,
preserve: one-shot long presses, release suppression after a long press,
explicit HID key release, discarding input during disconnection, aborting
playback on any new press, the A+B chord (which cancels pending gestures so
releases fire nothing), and never replaying or resuming a macro after a
disconnect or abort. The picker must keep working while disconnected; macros
play only when the link is ready. Never put secrets in macro text: it is typed
in clear and embedded in the manifest.

## Build and validation

Create a Python virtual environment and install `requirements.txt` into it.
Use `.venv/bin/pio run -e <env>` for a firmware build and
`.venv/bin/pio device list` to identify serial devices. On Windows the same
tools are `.venv\Scripts\pio` and `.venv\Scripts\python`. The PlatformIO Python
environment must contain PyYAML for the pre-build generator.

- Logic changes: `.venv/bin/pio test -e native` and
  `.venv/bin/python -m unittest discover -s scripts/tests -v`, or both with
  `bash scripts/check.sh`. Build every env first; the packaging tests use
  PlatformIO's esptool.
- Build every env you could have affected (`python scripts/list_envs.py` lists
  them) and verify a build after integration.
- Web changes: `npm ci`, `npm run check`, `npm test` and `npm run build` in `web/`.
- Windows has no native compiler by default: use WSL or
  `docker run --rm -v "${PWD}:/src" -w /src gcc:14 sh -c "..."`.
- Keep the README, `docs/`, `config.yaml` comments and the validation limits in
  `scripts/config/sections/*.py` aligned. Do not copy a limit into docs without
  checking the code.
- Do not claim a successful compile proves Bluetooth pairing, button identity,
  display rendering, typing reliability, WiFi provisioning, OTA or battery
  runtime.

## Flashing safety

Before flashing, reidentify the intended USB device and verify its chip and
flash identity. Flash only within the user's authorization. Port names are
examples, not stable device identifiers. Close serial monitors before uploading.
Never run erase, upload or `espota` against a device the user has not named.
Report physical checks as pending unless observed or confirmed by the user, and
record them in `TODO.md`.

## CodeGraph

In repositories indexed by CodeGraph (a `.codegraph/` directory exists at the repo root), reach for it BEFORE grep/find or reading files when you need to understand or locate code:

- **MCP tool** (when available): `codegraph_explore` answers most code questions in one call — the relevant symbols' verbatim source plus the call paths between them, including dynamic-dispatch hops grep can't follow. Name a file or symbol in the query to read its current line-numbered source. If it's listed but deferred, load it by name via tool search.
- **Shell** (always works): `codegraph explore "<symbol names or question>"` prints the same output.

If there is no `.codegraph/` directory, skip CodeGraph entirely — indexing is the user's decision.
<!-- CODEGRAPH_END -->
