# Macro Clicker (M5Stack Stick-S3 example)

A pocket Bluetooth macro keyboard built on the ESP32 starter template: two
buttons, three gestures each (tap, double-tap, hold) and up to nine switchable
**profiles**, so up to six macros per profile. A macro is a key combo
(`CMD+SHIFT+4`), typed text, or a timed sequence of both. The screen shows the
active profile, what each button does, Bluetooth status and battery level.

This example is built for the **M5Stack Stick-S3 (ESP32-S3)**: it needs two
buttons and the display. It is the `sticks3-macro` PlatformIO environment; the
default `sticks3` environment builds the starter app from `src/` instead. Both
share the template's `lib/` modules. Unlike the original standalone firmware, a
non-Stick-S3 M5 device is not refused at boot: the board layer restarts up to
three times to retry detection, then logs a warning and continues. Hardware
behavior is **pending physical checks** (see the acceptance checklist);
nothing here has been observed on a device.

This directory holds only the macro-specific parts:

| Path | Role |
| --- | --- |
| `config.yaml` | Profiles, macros and settings (source of truth). |
| `config_schema.py` | The `profiles` section: validates YAML and compiles key combos, text and labels into HID strokes. |
| `include/Macro.h` | Shared data contract (`Stroke`, `Macro`, `Profile`). |
| `include/Controller.h`, `include/MacroPlayer.h` | Pure, header-only logic (gestures to events; stroke pacing), covered by host tests. |
| `include/ProfileStore.h`, `src/ProfileStore.cpp` | Remembers the active profile index in NVS (via `Settings`). |
| `include/Ui.h`, `src/Ui.cpp` | Home, Picker and Transmit screens drawn on the shared `Display`. |
| `src/main.cpp` | Wiring: controller -> player -> keyboard -> ui. |

> **SECURITY: never put secrets in macros.** `config.yaml` is compiled into the
> firmware and embedded in the published firmware manifest (`index.json`).
> Text macros are typed in clear into whatever has keyboard focus. Do not put
> passwords, tokens, API keys or recovery codes in it, in any form.

## Build and flash

Set up the Python environment as in the root README, then from the repository
root:

```sh
.venv/bin/pio run -e sticks3-macro
.venv/bin/pio device list
.venv/bin/pio run -e sticks3-macro -t upload --upload-port <port>
```

Take `<port>` from the device list (it can change after reset or download
mode) and close any serial monitor before uploading. PyYAML must be in the same
Python environment as PlatformIO (`requirements.txt` does this): the pre-build
hook validates this example's `config.yaml`. Setup, recovery and the browser
installer: [../../docs/getting-started.md](../../docs/getting-started.md),
[../../docs/flashing-and-ota.md](../../docs/flashing-and-ota.md).

WiFi and OTA are disabled in this example's `config.yaml` (`wifi.enabled: false`,
`ota.enabled: false`); the macro clicker is Bluetooth only.

## Pair and use

1. Power on and wait for the `SEARCHING` link tag on the display.
2. In the host's Bluetooth settings select **Stick-S3 Macro** (or your
   `ble.name`) and connect. The tag changes to `HANDSHAKE`, then `SECURE`.
3. Focus the application you want to type into and press a button.

The HID report descriptor is unchanged from the previous firmware, so existing
pairings keep working. If you ever change it, forget the device on the host and
pair again. The active profile and pairings survive restarts; the device
advertises again after a disconnect. Bluetooth stays awake; there is no
automatic sleep.

## Controls

Buttons: **A** (`main`) and **B** (`secondary`); the side power button is not one.

| Gesture | What it does |
| --- | --- |
| Tap | Fires on release. |
| Double-tap | Two presses within the double-click window (`double_click_ms`). |
| Hold | Fires **once** when `long_press_ms` is reached, while still held. Releasing afterwards does nothing. |
| A + B together | Opens the profile picker (see below). |

Gesture details:

- A long press with no hold macro assigned behaves like a tap on release.
- With no double-tap macro, two quick taps fire two taps immediately. With one,
  the first tap waits for the double-click window; a recognized double-tap sends
  only the double-tap macro. Leave double-taps unassigned for the most
  responsive buttons.
- Unassigned slots do nothing. Input while Bluetooth is not connected and ready
  is **discarded**, never queued.

### Profile picker

Press **A and B together** to open the picker (list with the active profile
highlighted). **B** highlights the next profile (wraps), **A** selects it,
returns home and saves it across power cycles, **A + B again** or 10 seconds
without a press cancels. The picker works while disconnected. Double-tap and
hold are disabled inside it, and a chord cancels pending single or double
gestures, so opening it never fires a macro.

### Abort a running macro

**Press any button during playback to abort it.** The current key is released
and the rest of the macro is dropped. Playback is also aborted if Bluetooth
disconnects, a macro cannot start unless the link is ready, and an aborted macro
is never resumed or replayed. Tapping a slot twice quickly while its long text
is still typing therefore stops it rather than queueing a second run.

### Bond reset

Hold **both application buttons during startup for three seconds** (the timer
starts after the splash; keep them held through it, or press both within about
1 s of the splash ending) and follow the
`BLUETOOTH` progress screen. Then forget the device on the host and pair again.

## On-screen display

The 240x135 display uses the shared `lib/display` look.

- **HOME:** header with profile number and battery, the profile name and link
  tag (`SEARCHING`, `HANDSHAKE`, `SECURE`), and a grid of rows `A`/`B` by
  columns `TAP`/`DBL`/`HOLD` showing each slot's label (a dash if unassigned).
- **PICKER:** `SELECT PROFILE`, the list with a highlight bar and `ACTIVE`.
- **TRANSMITTING:** the macro label, a progress bar and "press any button to abort".
- The splash, bond-reset and fault screens come from the shared `Display`.

Rendering was checked at build time only; legibility on the real display is a
pending physical check.

## Customize with YAML

Edit `examples/macro-clicker/config.yaml`, rebuild and flash. Configuration is
compile-time only; there is no runtime upload, phone app or network service.
The build validates the file and generates `generated_config.h` (never edit or
commit it). Invalid input fails the build with the path of the problem.

### Top-level sections

`examples/macro-clicker/config.yaml` is the reference; it starts with
`schema: config_schema.py` (adds `profiles`). Ranges:

| Key | Rule |
| --- | --- |
| `project.name` | 1-32 printable ASCII, shown on the splash; optional `tagline` (120) and `description` (500) for the installer page |
| `ble.name` | 1-29 UTF-8 bytes; `ble.keystroke_ms` 5-100 (default 15) |
| `display.brightness` | 1-255 (default 64) |
| `gestures` | `debounce_ms` 1-100, `double_click_ms` 50-1000, `long_press_ms` 100-10000, with `debounce < double_click < long_press` |
| `wifi`, `ota` | `enabled: false` in this example; Bluetooth only |

`project`, `ble` and `profiles` are required; unknown or missing fields and
duplicate YAML keys are errors. `ble.enabled` and `display.enabled` must stay
`true` or the firmware does not build. The template's generic `buttons:` section
is not used; profiles define every button action.

**Typing speed (`ble.keystroke_ms`).** Each key is held this long, released
explicitly, then followed by the same gap, so a character costs about
`2 x keystroke_ms` (about 33 characters per second at 15 ms). Raise it (try
20-30) if characters go missing. Whether 15 ms is reliable for your longest
macro on your host is a pending physical check.

### Profiles and limits

```yaml
profiles:
  - name: CLAUDE            # 1-12 printable ASCII, unique ignoring case
    buttons:
      main:      { single: <action>, double: <action>, long: <action> }   # A
      secondary: { single: <action>, double: <action>, long: <action> }   # B
```

- 1 to 9 profiles, each with exactly two buttons (`main`, `secondary`) and
  exactly three gestures (`single`, `double`, `long`): 6 slots per profile. Use
  `null` for a slot you do not want.
- At most 4096 strokes in one macro and 32768 strokes across all profiles. A
  stroke is one key press or one delay step; a typed capital letter or symbol is
  one stroke.
- If you flash a build with fewer profiles than the remembered index, the device
  falls back to the first profile.

### Actions

```yaml
single: null                                  # unassigned
single: "CMD+SHIFT+4"                         # combo shorthand (a bare string)
single: { keys: "CTRL+C", label: COPY }       # combo with a display label
single:                                       # typed text
  label: REVIEW
  text: "Review the current diff for bugs.\n"  # \n presses ENTER
single:                                       # sequence of steps
  label: SHIP
  sequence:
    - text: "git add -A && git commit -m wip"
    - keys: ENTER
    - { delay: 150 }                          # pause 150 ms
    - text: "git push"
```

- **Combo** (`keys`, or a bare string): modifiers joined with `+`, ending in at
  most one key, e.g. `CTRL+SHIFT+T`, `ESC`, `F13`. A modifier-only combo such as
  `CMD` or `CTRL+ALT` is allowed. Names are case-insensitive. Quote combos
  containing `+` to keep YAML happy. A bare digit key (`5`) works too.
- **Text:** typed on a **US keyboard layout**. Allowed: printable ASCII, `\n`
  (ENTER) and `\t` (TAB). Anything else (accents, emoji, curly quotes) is a
  build error naming the character and index. A block scalar's newlines send
  ENTER too.
- **Sequence:** a non-empty list of steps, each exactly one of `{keys: ...}`,
  `{text: ...}` or `{delay: ms}` (1-10000, allowed **only inside a sequence**).

### Modifiers and key names

Modifiers are `CTRL`, `SHIFT`, `ALT` (`OPT`, `OPTION`) and `CMD` (`GUI`, `WIN`,
`META`, `SUPER`); all send the left-hand modifier, must come before the key, and
may appear once. Key names are letters, digits, `F1`-`F24`, navigation, editing
and punctuation names such as `ENTER`, `ESC`, `PAGE_UP`, `GRAVE`. The complete
list is in the comment at the top of `examples/macro-clicker/config.yaml` and in
`scripts/config/sections/keys.py`.

### Labels

`label` is optional, 1-10 printable ASCII characters, shown uppercase. When
omitted a combo uses its normalized text, a text macro the first line of the
text, and a sequence `MACRO`. A default label over 10 characters is cut to 9
plus `~` (`CMD+SHIFT~`); set a short explicit label where that reads badly.

### Validation errors

Errors name the setting and location, for example `Invalid
examples/macro-clicker/config.yaml: profiles[3].name: 'mac edit' duplicates
another profile name`. Fix the YAML and rebuild; do not edit the generated header.

### Default profiles

Four profiles ship in `config.yaml`: `CLAUDE` (prompts typed with Enter, `ESC`
interrupt, `SHIFT+TAB`, `/clear`), `MAC EDIT`, `WIN EDIT` and `DECK` (slide
next/back/exit). They are ordinary examples; edit them to taste.

## Tests

From the repository root (Windows: `.venv\Scripts\python` and `.venv\Scripts\pio`):

```sh
.venv/bin/python -m unittest discover -s scripts/tests -v   # includes test_macro_schema.py
.venv/bin/pio test -e native -f "test_macro_*"              # controller (gestures, picker) and player
.venv/bin/pio test -e native                                # every host suite
```

`test_macro_schema.py` covers the `profiles` section (combos, text, sequences,
labels, limits, error paths). The controller and player run as PlatformIO Unity
suites in `test/test_macro_controller_gestures`, `test/test_macro_controller_picker`
and `test/test_macro_player`, sharing the fixtures in `test/macro_controller_sim.h`.
The native environment needs a host C++ compiler; on Windows use WSL or a
container (see [../../AGENTS.md](../../AGENTS.md#build-and-validation)).

## Troubleshooting

Port, download-mode and recovery steps are in
[../../docs/flashing-and-ota.md](../../docs/flashing-and-ota.md).

- **Host cannot reconnect, or keys are ignored:** forget the device on the host
  and pair again; if needed, clear bonds with the startup gesture first.
- **Connected but nothing happens:** wait for `SECURE`, check which application
  has focus and that the slot is assigned. Test in a plain text editor.
- **Wrong characters (`@`/`"` or `y`/`z` swapped):** text assumes a US layout;
  set the host layout to US.
- **Dropped or repeated characters:** raise `ble.keystroke_ms` and reflash.
- **A macro stops partway:** any button press or a disconnect aborts it; it is
  never resumed or replayed.
- **Build fails with a configuration error:** read the message and fix `config.yaml`.

## Known limitations

- Text macros assume a US layout and are limited to printable ASCII, newline
  and tab. No media or consumer keys, no mouse input.
- Configuration is compile-time only (limits above).
- Long text takes seconds, bounded by `ble.keystroke_ms`; macros can be aborted
  but not paused, resumed or replayed.
- No battery-life measurement exists; none is claimed.

## Acceptance checklist

Software verification runs the Python configuration tests, the native C++
suites and the `sticks3-macro` firmware build. A passing build does not prove
Bluetooth pairing, button identity, display rendering, typing reliability, or
battery runtime. These physical checks require the device and are **pending**
until you have personally confirmed them:

- [ ] Flash succeeds, the board boots, and the display shows the splash and HOME
      screen with the correct profile and labels.
- [ ] The device pairs as a keyboard, reaches `SECURE`, and reconnects after a
      power cycle and after an idle period.
- [ ] Each gesture on both buttons fires the right macro exactly once; a hold
      does not also fire on release.
- [ ] A+B opens the picker; B cycles, A selects, A+B and the 10 s timeout
      cancel; the selected profile survives a power cycle.
- [ ] A button press during a macro aborts it with no stuck keys, nothing is
      replayed after reconnecting, and input while disconnected is discarded.
- [ ] The longest text macro types completely at the configured `keystroke_ms`.
- [ ] Display text is legible at the configured brightness.
- [ ] The startup A+B hold clears bonds and the device pairs again.
- [ ] Battery runtime: not measured.

Keep a regular keyboard available while you validate a new profile.
