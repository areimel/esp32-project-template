# Macro Clicker (M5Stack Stick-S3 example)

A pocket Bluetooth macro keyboard built on the ESP32 starter template. Two
buttons, three gestures each (tap, double-tap, hold), and switchable **profiles**
give you up to six macros per profile on the device and up to nine profiles. Each
macro can be a key combo (`CMD+SHIFT+4`), a block of typed text, or a timed
sequence of both. Use it for Claude Code prompts, copy/paste/undo shortcuts,
slide control, or anything else a keyboard can do. The screen shows the active
profile, what each button does, Bluetooth status, and battery level.

This example targets the **M5Stack Stick-S3 (ESP32-S3)** only (it needs two
buttons and the display). It is the `sticks3-macro` PlatformIO environment; the
default `sticks3` environment builds the starter app from `src/` instead. The
two share the template's `lib/` modules (`board`, `ble_hid`, `input`, `store`,
`display`, `core`); this directory holds only the macro-specific parts:

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
> firmware and is also embedded verbatim in the published firmware manifest
> (`manifest.json`) that the web flasher and GitHub Releases serve. Text macros
> are typed in clear text into whatever has keyboard focus. Do not put
> passwords, tokens, API keys, recovery codes, or anything else sensitive in
> `config.yaml`, in any form.

## Build and flash

Set up the template's Python environment and PlatformIO as described in the root
README, then, from the repository root (macOS/Linux/WSL; on Windows use
`.venv\Scripts\pio`):

```sh
.venv/bin/pio run -e sticks3-macro
.venv/bin/pio device list
.venv/bin/pio run -e sticks3-macro -t upload --upload-port /dev/cu.usbmodem11301
.venv/bin/pio device monitor --port /dev/cu.usbmodem11301 --baud 115200
```

Replace the example port with the Stick-S3's current name from the device list;
it can change after unplugging, resetting or entering download mode. Close the
serial monitor before uploading. **PyYAML must be installed in the same Python
environment that runs PlatformIO** (`requirements.txt` does this): the pre-build
hook uses it to validate `examples/macro-clicker/config.yaml` and generate the
firmware's macro tables.

WiFi and OTA are disabled in this example's `config.yaml` (`wifi.enabled: false`,
`ota.enabled: false`); the macro clicker is Bluetooth only.

## Pair and use

1. Power on the Stick-S3 and wait for the `SEARCHING` link tag on the display.
2. On your computer or phone, open Bluetooth settings, select **Stick-S3 Macro**
   (or the `ble.name` you configured), and connect. It presents itself as a
   keyboard. The link tag changes to `HANDSHAKE`, then `SECURE` when ready.
3. Focus the application you want to type into and press a button.

The HID report descriptor is unchanged from the previous firmware, so existing
pairings keep working. If you ever change the descriptor, forget the device on
the host and pair again; clear the device's bonds too if it still misbehaves
(see Bond reset).

Pairing information is retained across restarts, and the active profile is
remembered across power cycles. The device advertises again after a disconnect so
the host can reconnect. Bluetooth stays awake; there is no automatic sleep.

## Controls

The buttons are the two application buttons: **A** (`main`, BtnA) and **B**
(`secondary`, BtnB). The separate reset/power button
is not a macro control: press it once to power on/reset, twice to power off, or
hold it to enter download mode.

| Gesture | What it does |
| --- | --- |
| Tap | Fires on release. |
| Double-tap | Two presses within the double-click window (`double_click_ms`). |
| Hold | Fires **once** when `long_press_ms` is reached, while still held. Releasing afterwards does nothing. |
| A + B together | Opens the profile picker (see below). |

Gesture details:

- A long press with no hold macro assigned behaves like an ordinary tap on
  release.
- If a button has no double-tap macro, two quick taps are two separate taps and
  each fires immediately on release. If it has one, the first tap waits for the
  double-click window so it can tell the difference; a recognized double-tap
  sends only the double-tap macro. Leave double-taps unassigned for the most
  responsive buttons.
- Unassigned slots do nothing.
- Input while Bluetooth is not connected and ready is **discarded**; it is never
  queued for later.

### Profile picker

Press **A and B together** to open the picker. It lists your profiles with the
active one highlighted.

- **B**: highlight the next profile (wraps around).
- **A**: select the highlighted profile and return to the home screen. The choice
  is saved and restored after a power cycle.
- **A + B again**: cancel and keep the current profile.
- 10 seconds without a press cancels.

The picker works even while Bluetooth is disconnected. Double-tap and hold are
disabled inside the picker so taps are immediate. A chord cancels any pending
single or double gesture, so opening the picker never fires a macro.

### Abort a running macro

Macros run keystroke by keystroke, so a long text macro takes a moment. **Press
any button during playback to abort it.** The current key is released and the
rest of the macro is dropped, not replayed. Playback is also aborted, never
resumed, if Bluetooth disconnects, and a macro cannot start unless the link is
ready. Because any press aborts, tapping a single-only slot twice quickly while
its long text macro is still typing stops that macro rather than queueing a
second run.

### Bond reset

Hold **both application buttons during startup for three seconds** and follow the
on-screen progress to clear saved Bluetooth bonds. Then forget the device in your
computer's Bluetooth settings and pair again. This startup gesture does not use
the separate reset/power button.

## On-screen display

The 240x135 display uses the template's charcoal, ivory, and safety-orange
field-manual look (`lib/display`).

- **Boot:** the shared splash while Bluetooth begins advertising.
- **HOME:** a header with the profile number and battery, the profile name and
  link tag (`SEARCHING`, `HANDSHAKE`, `SECURE`); below it a grid of rows `A`/`B`
  and columns `TAP`/`DBL`/`HOLD` showing each slot's label. An orange square
  marks an assigned slot; an unassigned slot shows a dash.
- **PICKER:** `SELECT PROFILE`, the profile list with the highlight bar,
  `ACTIVE` on the current profile, and the hints for B and A.
- **TRANSMITTING:** the macro label, a progress bar, and "press any button to
  abort".
- **Bond reset** (boot hold) and fault screens, from the shared `Display`.

Rendering was checked at build time only; legibility on the real display is a
pending physical check (see the checklist).

## Customize with YAML

Edit `examples/macro-clicker/config.yaml`, rebuild, and flash. Configuration is
compiled into firmware; there is no runtime configuration upload, no phone app,
and no network service. The build validates the file and generates
`generated_config.h` in the build directory, which must not be edited or
committed. Invalid input fails the build with the path of the problem.

### Top-level sections

```yaml
schema: config_schema.py   # adds the profiles section (path relative to this file)
project:
  name: Stick-S3 Macro     # 1-32 chars; shown on the splash screen
ble:
  enabled: true
  name: Stick-S3 Macro     # 1-29 UTF-8 bytes
  keystroke_ms: 15         # 5-100
wifi:
  enabled: false
ota:
  enabled: false
display:
  enabled: true
  brightness: 64           # 1-255
gestures:
  debounce_ms: 25          # 1-100
  double_click_ms: 250     # 50-1000
  long_press_ms: 650       # 100-10000
profiles:                  # ordered list, 1-9 profiles
  - name: ...
```

`project`, `ble` and `profiles` are required; the others have defaults.
Unknown or missing fields, and duplicate YAML keys, are errors. Gesture timing
must satisfy `debounce_ms < double_click_ms < long_press_ms`. The template's
generic `buttons:` section is not used here; profiles define every button
action. The `ble.enabled` and `display.enabled` settings must stay `true`: the
firmware does not build without them.

**Typing speed (`ble.keystroke_ms`).** Every keystroke is held for this many
milliseconds, released explicitly, then followed by the same gap before the next.
So a character costs about `2 x keystroke_ms`: at the default 15 ms, roughly 33
characters per second, and a 200-character prompt takes about 6 seconds. Lower
is faster but some hosts and Bluetooth stacks drop or reorder characters; higher
is slower but more reliable. If characters go missing, raise it (try 20-30).
Whether 15 ms is reliable for your longest macro on your host is a physical
check; test it.

### Profiles and limits

```yaml
profiles:
  - name: CLAUDE            # 1-12 printable ASCII, unique ignoring case
    buttons:
      main:                 # button A
        single: <action>
        double: <action>
        long: <action>
      secondary:            # button B
        single: <action>
        double: <action>
        long: <action>
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

Each slot takes one of these forms:

```yaml
single: null                                  # unassigned

single: "CMD+SHIFT+4"                         # combo shorthand (a bare string)

single: { keys: "CTRL+C", label: COPY }       # combo with a display label

single:                                       # typed text
  label: REVIEW
  text: "Review the current diff for bugs.\n"   # \n presses ENTER

single:                                       # sequence of steps
  label: SHIP
  sequence:
    - text: "git add -A && git commit -m wip"
    - keys: ENTER
    - { delay: 150 }                          # pause 150 ms
    - text: "git push"
    - keys: ENTER
```

- **Combo** (`keys`, or a bare string): modifiers joined with `+`, ending in at
  most one key, e.g. `CTRL+SHIFT+T`, `ESC`, `F13`. A modifier-only combo such as
  `CMD` or `CTRL+ALT` is allowed. Names are case-insensitive. Quote combos
  containing `+` to keep YAML happy. A bare digit key (`5`) works too.
- **Text:** typed using a **US keyboard layout**. Allowed characters are printable
  ASCII (space through `~`), `\n` (sent as ENTER), and `\t` (sent as TAB). Anything
  else, including accented letters, emoji, and curly quotes, is a build error
  naming the character and its index. In YAML double quotes, write `\n` and `\t`
  as shown; in a block scalar they become literal newlines, which also send ENTER.
- **Sequence:** an ordered, non-empty list of steps. Each step is exactly one of
  `{keys: ...}` (or a bare combo string), `{text: ...}`, or `{delay: ms}`.
  `delay` is 1-10000 ms and is allowed **only inside a sequence**. Use it to give
  an application time to react, such as opening a prompt before typing into it.
- Text is typed as written; there is no key repeat, no media/consumer keys and no
  mouse action.

### Modifiers and aliases

| Modifier | Aliases |
| --- | --- |
| `CTRL` | `CONTROL` |
| `SHIFT` | |
| `ALT` | `OPT`, `OPTION` |
| `CMD` | `GUI`, `WIN`, `META`, `SUPER` |

These all send the **left-hand** modifier. `CMD` is the Command key on a Mac and
the Windows key on Windows. Modifiers must come before the key, and each may
appear once.

### Key names

- Letters `A`-Z and digits `0`-`9`
- Function keys `F1`-`F24`
- `ENTER`, `ESC` (or `ESCAPE`), `BACKSPACE`, `TAB`, `SPACE`
- `DELETE`, `INSERT`, `HOME`, `END`, `PAGE_UP`, `PAGE_DOWN`
- `UP`, `DOWN`, `LEFT`, `RIGHT`
- `CAPS_LOCK`, `PRINT_SCREEN`
- Punctuation: `MINUS`, `EQUAL`, `LBRACKET`, `RBRACKET`, `BACKSLASH`,
  `SEMICOLON`, `QUOTE`, `GRAVE`, `COMMA`, `PERIOD`, `SLASH`

### Labels

`label` is optional, 1-10 printable ASCII characters, and is shown uppercase in
the grid. When omitted:

- a combo uses its normalized text (`cmd+shift+z` becomes `CMD+SHIFT+Z`);
- a text macro uses the first line of the text, uppercased;
- a sequence uses `MACRO`.

A default label longer than 10 characters is truncated to 9 characters plus `~`
(for example `CMD+SHIFT+4` shows as `CMD+SHIFT~`). Set a short explicit label for
anything the default does not describe well.

### Validation errors

Errors name the setting and its location. Examples:

```text
Configuration error: Invalid examples/macro-clicker/config.yaml: profiles[1].buttons.main.double.sequence[2].delay: must be an integer between 1 and 10000
Configuration error: Invalid examples/macro-clicker/config.yaml: profiles[0].buttons.main.single.text: unsupported character '“' at index 2; use printable US ASCII, newline, or tab
Configuration error: Invalid examples/macro-clicker/config.yaml: profiles[2].buttons.secondary.long.keys: unsupported key 'ENTR' in 'ENTR'
Configuration error: Invalid examples/macro-clicker/config.yaml: profiles[3].name: 'mac edit' duplicates another profile name
Configuration error: Invalid examples/macro-clicker/config.yaml: ble.keystroke_ms: must be an integer between 5 and 100
Configuration error: Invalid examples/macro-clicker/config.yaml: gestures: Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms
```

Fix the YAML and rebuild. Do not edit the generated header to bypass
validation.

### Default profiles

| # | Profile | A tap | A double | A hold | B tap | B double | B hold |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `CLAUDE` | `REVIEW` prompt + Enter | `TESTS` prompt + Enter | `COMMIT` prompt + Enter | `ESC` (INTERRUPT) | `SHIFT+TAB` (MODE) | `/clear` + Enter (CLEAR) |
| 2 | `MAC EDIT` | `CMD+V` PASTE | `CMD+C` COPY | `CMD+X` CUT | `CMD+Z` UNDO | `CMD+SHIFT+Z` REDO | `CMD+A` ALL |
| 3 | `WIN EDIT` | `CTRL+V` PASTE | `CTRL+C` COPY | `CTRL+X` CUT | `CTRL+Z` UNDO | `CTRL+Y` REDO | `CTRL+A` ALL |
| 4 | `DECK` | `RIGHT` NEXT | unassigned | unassigned | `LEFT` BACK | unassigned | `ESC` EXIT |

The `CLAUDE` prompts are typed text followed by Enter. They are ordinary
examples in `config.yaml`; edit them to taste. See that file for the exact
wording and a comment reference for every action form.

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
throwaway container, for example:

```sh
docker run --rm -v "${PWD}:/src" -w /src gcc:14 sh -c "pip install platformio && pio test -e native -f 'test_macro_*'"
```

(`${PWD}` is PowerShell/Bash syntax; use `%cd%` in cmd.exe.)

## Troubleshooting and recovery

- **No serial port:** try another data cable or USB port and rerun the device
  list. Confirm this is a Stick-S3 rather than an older Stick model.
- **Upload cannot connect:** close any serial monitor, reconnect the device, and
  confirm the new port. To enter download mode, connect USB and hold the side
  reset button until the internal green LED flashes, as described in the
  [official Stick-S3 instructions](https://docs.m5stack.com/en/core/StickS3).
  Reidentify the port and rerun the upload command. An application failure can
  usually be recovered by flashing from download mode.
- **Host cannot reconnect, or keys are ignored:** check the display and
  Bluetooth settings. Forget the device on the host and pair again; if
  necessary, clear bonds with the startup gesture first.
- **Connected but nothing happens:** wait for the `SECURE` tag, check which
  application has focus, and make sure the profile has a macro on that slot
  (unassigned slots show a dash). Test in a plain text editor to separate
  keyboard transport from the app's own shortcuts.
- **Text types the wrong characters (for example `@` and `"` swapped, or `y`/`z`
  swapped):** text macros assume a US keyboard layout. Set the host's keyboard
  layout to US while using the device. Layout-dependent shortcuts are affected
  the same way.
- **Characters are dropped or repeated in long text:** raise
  `ble.keystroke_ms` (try 20-30) and reflash. Slower hosts or busy Bluetooth
  links may need more.
- **A macro stops partway:** pressing any button aborts playback, and a Bluetooth
  disconnect aborts it as well. Aborted macros are never resumed or replayed.
- **Configuration fails to build:** read the validation error, fix
  `config.yaml`, and rebuild. Do not edit the generated header.

## Known limitations

- US keyboard layout only for text macros; set the host layout to US.
- Typed text is limited to printable ASCII, newline and tab; no Unicode input.
- No media or consumer keys (volume, play/pause, brightness), and no mouse input.
- Configuration is compile-time only: change `config.yaml`, rebuild and reflash.
  There is no runtime editing, app, or network service.
- At most 9 profiles and 6 slots per profile (2 buttons x tap/double/hold).
- Typing speed is bounded by `ble.keystroke_ms`; long text takes seconds, and
  reliability at low settings depends on the host.
- Macros cannot be paused, resumed, or replayed; only aborted.
- Battery runtime has not been measured and is not claimed.

## Acceptance checklist

Software verification runs the Python configuration tests, the native C++
suites and the `sticks3-macro` firmware build. A passing build does not prove
Bluetooth pairing, button identity, display rendering, typing reliability, or
battery runtime. These physical checks require the device and are **pending**
until you have personally confirmed them:

- [ ] Flash succeeds, the board boots, and the display shows the splash and HOME
      screen with the correct profile and labels.
- [ ] After forgetting any old entry, the device pairs as a keyboard and reaches
      `SECURE`; it reconnects after a device power cycle and after an idle period.
- [ ] Each gesture (tap, double-tap, hold on both buttons) fires the right macro
      exactly once; a hold does not also fire on release.
- [ ] A+B opens the picker; B cycles, A selects, A+B and the 10 s timeout cancel;
      the selected profile survives a power cycle.
- [ ] Pressing a button during a macro aborts it immediately with no stuck keys,
      and nothing is replayed after reconnecting.
- [ ] Input while disconnected is discarded.
- [ ] The longest text macro (such as the `CLAUDE` prompts) types completely and
      correctly at the configured `keystroke_ms` on your host.
- [ ] Display text is legible at the configured brightness, indoors and outdoors.
- [ ] Startup A+B hold clears bonds, and the device pairs again afterward.
- [ ] Battery runtime: **not measured.** Verify for yourself before depending on
      it unplugged.

Keep a regular keyboard available while you validate a new profile.
