# ESP32 Starter Template — Design

Status: approved 2026-11-13.

## Context

The repo is currently a working Stick-S3 BLE macro clicker (PlatformIO/Arduino + Astro web flasher on GitHub Pages). Alec wants it reworked into a reusable, general-purpose ESP32 starter that he and other developers can clone as a mostly-complete starting point. It should be smaller, DRY, consistently styled, and easy to update.

Recon found the following:
- About half the firmware is already generic: the `Gesture.h` state machine, BLE HID plumbing, the NVS store, the UiModel render-on-change pattern, and the YAML loader and validators in `generate_config.py`.
- The Stick-S3 coupling sits in `main.cpp`, `Ui.cpp` and `platformio.ini`. It is worst in `scripts/package_firmware.py`, which hardcodes ESP32-S3, 8 MB, the `default_8MB` layout and chip ID 9. The web flasher repeats those literals in about 6 places.
- Cruft:
  - `brag-output/` has 702 tracked files, including an mp4 and a wav.
  - `web/pnpm-lock.yaml` and `pnpm-workspace.yaml` are tracked, but CI uses npm.
  - The C++ test commands are copy-pasted in the README and both workflows.
  - `theme.css` is 923 lines, with duplicated override blocks.
  - Files over 300 lines: README (545), `test_controller.cpp` (406), `test_config.py` (357) and `generate_config.py` (347).
- Prior art:
  - WLED and Tasmota use a shared `[env]` with per-board `extends`.
  - ESPHome compiles YAML to C++.
  - ESP Web Tools is the standard browser flasher. It supports multi-chip `builds` and Improv WiFi provisioning.
  - The PlatformIO `native` env runs Unity tests on the host.
  - pioarduino provides Arduino-ESP32 3.x.

## Decisions (made by the user)

| Topic | Choice |
|---|---|
| Boards | Board-agnostic, with a board layer. Envs: `sticks3` (M5 reference, display and buttons), `esp32s3` devkit, `esp32c3` devkit and `esp32` (classic) devkit. |
| Macro code | Generic parts become optional modules. The macro clicker moves to `examples/macro-clicker/`. A new project starts from a minimal default app. |
| Default connectivity | BLE, WiFi with Improv provisioning, and OTA. |
| Microsite | Flasher plus product page, driven by project config. Neutral, themeable restyle with shared components. |
| Flasher | Replace the custom esptool-js flasher with **ESP Web Tools** (`<esp-web-install-button>`). |
| Platform | **pioarduino** (Arduino-ESP32 3.x), exact version pinned. Bump M5Unified, M5GFX and NimBLE pins to compatible versions. |
| Module layout | `lib/<module>/` PlatformIO libraries. Host tests use **`pio test -e native`** (Unity). |

## Goals, non-goals, anti-goals

**Goals**
- `git clone`, edit `config.yaml`, then `pio run -e <board>` gives a working device with WiFi (Improv), OTA, BLE and a button → action demo.
- One source of truth per fact:
  - Project name and BLE name live in `config.yaml`; the version comes from the git tag.
  - Board build facts live in `platformio.ini`; board capabilities live in `boards/boards.yaml`.
- Files are kept under about 300 lines.
- Anything repeated 3 or more times is centralized: the CI setup, the test command, and the web Panel and Disclosure components.

**Non-goals**
- No captive-portal AP fallback.
- No MQTT, web server UI or Home Assistant integration.
- No ESP-IDF or Kconfig.
- No runtime config UI beyond Improv WiFi credentials.

**Anti-goals**
- Secrets in `config.yaml` or the manifest.
- A web flasher that wipes NVS (BLE bonds and WiFi creds) on a normal reflash.
- Claiming hardware behavior that was only compile-verified.
- A "framework" so abstract that the default app is harder to read than the original.

## Design

### Repo layout (target)

```
config.yaml                 project/ble/wifi/ota/display/buttons sections (no secrets)
secrets.example.yaml        OTA password + optional dev WiFi creds; secrets.yaml gitignored
platformio.ini              [env] shared + [chip_*] bases + thin [env:<board>] + [env:native]
boards/boards.yaml          env -> label, chipFamily, buttons, display, battery (site + validator facts)
boards/<board>.h            pins/capability macros (BOARD_HAS_DISPLAY, BUTTON pins, LED pin, battery)
lib/core/                   Log.h (leveled macros, no-op on native), Every.h (periodic timer), Board.h interface
lib/board_m5/  lib/board_gpio/   Board implementations (M5Unified vs plain GPIO buttons/LED)
lib/input/                  Gesture.h (from include/), Buttons
lib/store/                  Settings: typed NVS wrapper (generalized ProfileStore)
lib/ble_hid/                BleKeyboard (from BleKeyboard.h/.cpp), one keystroke at a time
lib/net/                    Wifi (connect from stored creds), Improv serial (pure packet codec + handler), Ota (ArduinoOTA)
lib/display/                Theme.h + Screen base (UiModel diff/render-on-change); only built on display boards
src/main.cpp                minimal default app: buttons -> configured key combo over BLE HID, status on display/serial
examples/macro-clicker/     Controller.h, MacroPlayer.h, Macro.h, screens, config.yaml, own env + tests
scripts/config/             core.py (loader, fields/integer validators, cstr, write-if-changed) + sections/*.py
scripts/generate_config.py  thin entry; scripts/platformio_config.py unchanged role
scripts/package_firmware.py per-board packaging driven by platformio.ini + boards/boards.yaml -> ESP Web Tools manifests
test/                       pio native Unity tests (gesture, improv codec, ble stubs in test/stubs/)
scripts/tests/              Python tests (config core, sections, packaging), split <300 lines
web/                        Astro + esp-web-tools; content from manifest + web/src/content/site.ts
docs/                       getting-started, boards, config, modules, flashing, testing, new-project checklist
.github/actions/setup/      composite action (python+pio cache, node) reused by ci + release
```

### Firmware

- **Board layer.**
  - `Board.h` defines `begin()`, `update()`, `buttonCount()`, `isPressed(i)`, `batteryPercent()` (-1 when there is no battery), `setStatusLed()` and `hasDisplay()`.
  - `board_m5` wraps M5Unified for `sticks3`. `board_gpio` uses the pins from `boards/<board>.h` (for example the BOOT button and the onboard LED).
  - The env selects the board with `-DBOARD_<NAME>` and `-include boards/<board>.h`.
  - The hard `board_M5StickS3` halt in `main.cpp` goes away.
- **Modules.**
  - Each module is enabled by `config.yaml`. The generator emits `#define FEATURE_WIFI 1` and so on into `generated_config.h`.
  - Module sources are wrapped in `#if FEATURE_X` guards (see resolutions).
  - Pure logic (Gesture, the Improv codec, Every) has no Arduino includes and is tested on native.
- **WiFi and Improv.**
  - The device stores credentials through the Arduino WiFi persistence or `store`.
  - It answers Improv Serial (identify, get state, device info, send WiFi settings) so that ESP Web Tools can provision right after install.
  - Write a small Improv implementation (about 150 lines, with codec tests) rather than adding a dependency. The builder may swap in a pinned library if it is smaller and tested.
- **OTA.**
  - ArduinoOTA runs on the LAN when WiFi is up. Its password is the `OTA_PASSWORD` build flag, injected by `platformio_config.py` from the gitignored `secrets.yaml`.
  - If the password is missing, OTA is disabled and a build warning is printed.
  - `platformio_override.example.ini` shows `upload_protocol = espota`.
  - Use the framework's default OTA partitions for each flash size.
  - No rollback is claimed.
- **Default app (`src/main.cpp`, about 150 lines).**
  - It shows the wiring pattern from the current `main.cpp`: an anonymous-namespace instance, an event → `handle()` switch, a `UiModel` diff redraw, and `Every` periodic tasks.
  - `config.yaml` `buttons:` maps each button's single, double and long gesture to a key combo or `null`. Combo parsing stays in the generator.
  - BLE HID types the combo, WiFi and OTA run, and status shows on the display when there is one and on serial always.
  - Holding A+B at boot clears bonds on 2-button boards.
- **Macro clicker example.**
  - It keeps all the current behaviors listed in AGENTS.md: one-shot long press, release suppression, explicit HID release, discarding input while disconnected, abort on press, the A+B chord, and the picker.
  - It builds as `env:sticks3-macro` using `build_src_filter` to the example's `src/`, and it has its own config and schema section.
  - `test_controller` and `test_player` move into `examples/macro-clicker/test/` or a filtered native test. `test_controller` is split below 300 lines.
- **HID descriptor.** Unchanged. If the builder has to change it, document a forget-and-re-pair step.

### Config generator
- `scripts/config/core.py` is the shared toolkit: `UniqueKeyLoader`, `fields()`, `integer()`, `cstr()`, path-qualified errors, and write-if-changed.
- Each section module, for example `sections/wifi.py`, exports `validate(data) -> dict` and `emit(cfg) -> list[str]`.
- The HID key tables move to `sections/keys.py`, which both the default buttons and the macro example use.
- An app can register extra sections from `<app>/config_schema.py` through a `schema:` path in its config.

### Packaging and web
- **Board facts.** See the builder-check resolutions: build facts from `platformio.ini`, capabilities from `boards/boards.yaml`, bootloader offset from one chip-family map.
- **Packaging output.**
  - Each board gets `web/public/firmware/<board>/manifest.json` in ESP Web Tools format: `name`, `version`, `new_install_prompt_erase: true`, and `builds[{chipFamily, parts[{path, offset}]}]`.
  - The parts are bootloader, partitions, boot_app0 and app as separate entries. A merged image is avoided because it would overwrite NVS.
  - A top-level `firmware/index.json` lists the boards, with their labels and capabilities, plus the project identity and the sanitized config.
  - The existing image validation, the checksums and the NVS-overlap guard are kept and generalized.
- **Web.**
  - Delete `session.ts`, `serial-adapter.ts`, `flasher.ts` and `firmware.ts`, and replace their tests.
  - Add a pinned `esp-web-tools` dependency.
  - Pages: Hero, a board picker with an install button per board, getting started (flash, Improv WiFi, pair BLE, OTA), features, downloads and troubleshooting.
  - Shared components: `Panel`, `Disclosure`, `Button` and `KeyValue`/`ReleaseStrip`.
  - The project identity comes from `index.json`. Page copy lives in `web/src/content/site.ts`.
  - `DevicePlate` and `FieldGuide` are removed. The macro-specific copy moves into the example's README.
- **CSS.** Split into `tokens.css` (color, space, type and tracking scales, with light and dark), `base.css`, `layout.css` and `components/*.css`, each under 300 lines. The late override blocks are folded back into their original rules.
- **Build fallback.** `check-firmware.ts` lets the site build with a "no release yet" state when the firmware is absent.

### CI and release
- The composite `.github/actions/setup` is used by both workflows.
- `ci.yml` runs a matrix `pio run -e` over all board envs plus the macro example, then `pio test -e native`, the Python tests and the web check, test and build.
- `release.yml` runs on a tag or a dispatch, packages every board, attaches the files to the GitHub Release, and deploys Pages.
- `BASE_PATH` is `/${{ github.event.repository.name }}/` and the `site` URL comes from the repository owner, so a fork works with no edits.

## Builder-check resolutions

- **OTA (user decision):** OTA is for development builds only. It is compiled in only when the gitignored `secrets.yaml` sets `ota_password`. CI and release binaries ship with OTA off, and the site says so.
- **Board facts (my call):**
  - `platformio.ini` owns the build facts: board, flash size, partitions and memory type.
  - `package_firmware.py` reads them back with `pio project config --json-output`.
  - `boards/boards.yaml` holds only the facts the site and the validator need: label, button count, display and battery.
  - The bootloader offset comes from a single chip-family map in the packager: 0x1000 on ESP32, 0x0 on the others.
  - The manifest uses `chipFamily` strings.
- **Macro example layout (my call):**
  - `env:sticks3-macro` uses `build_src_filter = -<*> +<../examples/macro-clicker/src/>` plus `-Iexamples/macro-clicker/include`.
  - Its tests are `test/test_macro_*`, so there is one `test_dir`.
  - It is built in CI, but no web manifest is generated for it by default. The packager takes `--env`, so a project can publish it.
- **Pins (my call):** use the latest stable pioarduino release, referenced by an exact zip URL, plus M5Unified, M5GFX and NimBLE versions that compile with it. Compile success is the only verification claimed.
- **One-button boards (my call):**
  - The generator validates `buttons:` against the board's button count. The env is passed in by `platformio_config.py`.
  - On 1-button boards, holding the button for 5 s at boot clears the BLE bonds.
  - `display:` is accepted everywhere and ignored when the board has no display.
- **Version (my call):** the git tag and `--version` are the only sources, and `config.yaml` has no version field.
- **Module gating (my call):**
  - Each module's sources are wrapped in `#if FEATURE_X`, driven by the generated header. `lib_ldf_mode` is not relied on.
  - Improv frames are parsed tolerantly next to the log output.
  - The firmware never blocks waiting for USB serial.

## Execution plan (subagent-driven, TODO-tracked)

The first execution step writes this design to `docs/superpowers/specs/2026-11-13-esp32-template-design.md`. The next step is `superpowers:writing-plans`, which expands it into a task plan, and the plan is then run with `superpowers:subagent-driven-development`. Each workstream owns distinct files. I coordinate the shared interfaces (`Board.h`, the `generated_config.h` macros, the `index.json` and manifest schema) and write them before fan-out. The target is about 12–16 subagents in total, well under the cap of 30.

| # | Workstream | Model | Depends | Commit |
|---|---|---|---|---|
| 0 | Remove `brag-output/` and the pnpm files, update `.gitignore`, add the spec and TODO.md | Haiku | — | 1. "Remove demo media and stray lockfiles; add template design spec" |
| 1 | pioarduino platform, restructured `platformio.ini` with 4 board envs plus native, `boards/`, `lib/core` with `Board.h`, `board_m5`, `board_gpio`. Builds green on every env with a placeholder app. | Sonnet | 0 | 2. "Multi-board PlatformIO layout on Arduino-ESP32 3.x" |
| 2a | Split the config generator into `core` and `sections`, add the new schema sections and secrets injection, split the Python tests | Sonnet | 1 | 3. "Modular config generator with per-section schemas" |
| 2b | `lib/input` (Gesture), `lib/store` (Settings), Unity native tests | Sonnet | 1 | 4. "Input and settings modules with native tests" |
| 2c | `lib/ble_hid` and the native test stubs | Sonnet | 1 | (with 4) |
| 2d | `lib/net`: WiFi, Improv codec and handler, OTA, codec tests | Sonnet | 1 | 5. "WiFi with Improv provisioning and LAN OTA" |
| 2e | `lib/display`: Theme and Screen base | Haiku | 1 | (with 6) |
| 3a | Default app in `src/main.cpp` | Sonnet | 2* | 6. "Minimal default app wiring all modules" |
| 3b | `examples/macro-clicker` migration with its env and tests | Sonnet | 2* | 7. "Move macro clicker into examples" |
| 4a | Board-table-driven packaging producing ESP Web Tools manifests and `index.json`, plus tests | Sonnet | 1, 2a | 8. "Board-driven firmware packaging for ESP Web Tools" |
| 4b | Web: ESP Web Tools installer, board picker, shared components, pages | Sonnet | 4a schema | 9. "Rebuild microsite on ESP Web Tools with shared components" |
| 4c | Web CSS split into tokens, base, layout and components | Sonnet | parallel with 4b; each owns distinct files | 10. "Split site styles into tokens and components" |
| 4d | Composite setup action, CI matrix, release workflow | Haiku | 3, 4a | 11. "Reusable CI setup and per-board matrix" |
| 5 | README trimmed below 300 lines, `docs/*.md`, AGENTS.md rewritten for the template, new-project checklist | Haiku drafts, Sonnet reviews | all | 12. "Template docs and agent guide" |
| 6 | Code review on the full diff (feature-dev code-reviewer, Sonnet), fixes, final verification | Sonnet + me | all | 13. "Review fixes" |

## Verification
- `.venv/bin/pio run -e sticks3 -e esp32s3 -e esp32c3 -e esp32 -e sticks3-macro` all succeed.
- `.venv/bin/pio test -e native` passes the gesture, Improv codec, BLE stub, controller and player tests.
- `python -m unittest discover -s scripts/tests -v` passes. Run it after the builds, because packaging uses esptool.
- `package_firmware.py` produces a manifest for each board. ESP Web Tools manifest shape is checked in the tests.
- In `web/`, `npm ci`, `npm run check`, `npm test` and `npm run build` pass, and a Playwright screenshot of the built site is checked on desktop and phone widths.
- Remaining hardware checks are reported to Alec as pending, never as passed: flashing on the Stick-S3, Improv WiFi provisioning, OTA upload, BLE pairing (forget and re-pair if needed), and gestures. The C3 and classic ESP32 envs are compile-verified only unless he has those boards.
