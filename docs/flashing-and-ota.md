# Flashing, OTA and releases

Pick how you get firmware onto a board: USB upload from PlatformIO, the browser
installer built from a release, or OTA for development builds. None of these
has been exercised on hardware yet; see [../TODO.md](../TODO.md).

## USB upload

```sh
.venv/bin/pio device list                    # identify the board
.venv/bin/pio run -e esp32c3 -t upload
.venv/bin/pio device monitor
```

- Port names (`/dev/ttyACM0`, `COM5`) are examples, not stable identifiers.
  Reidentify the board after every reconnect, and confirm the chip and flash
  size match the env before writing anything.
- Close the serial monitor before uploading.
- Pin a port locally in `platformio_override.ini` (gitignored):
  `upload_port = /dev/ttyACM0`.
- If the port does not appear, use a data cable and put the board in download
  mode (commonly: hold BOOT while connecting power or pressing reset; the exact
  sequence is board specific and not verified here).
- A normal upload is expected to keep NVS (bonds and WiFi credentials survive), because it does not rewrite that partition. Not yet observed; pending a physical check.

### Recovery

```sh
.venv/bin/pio run -e esp32c3 -t erase        # erases the whole flash, including NVS
.venv/bin/pio run -e esp32c3 -t upload
```

After an erase, WiFi must be provisioned again and the host must pair again.
If Bluetooth will not connect after an update, forget the device on the host and
pair again; hold every button at boot to clear the device's bonds first (hold
times: [getting-started.md](getting-started.md#4-use-the-default-app)).

## Browser installer

The site in `web/` embeds `esp-web-tools` and installs from the packaged
firmware. It needs a browser with Web Serial (Chrome, Edge, Opera on desktop).

What it writes, per board (`<board>/manifest.json`):

| Part | Offset |
|---|---|
| `bootloader.bin` | `0x1000` on ESP32, `0x0` on ESP32-S3 and ESP32-C3 |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | the `otadata` partition offset from that board's `partitions.bin` |
| `firmware.bin` | the first `app` partition offset |

No merged image is used and nothing is written inside the NVS partition, so a
normal install is expected to keep bonds and WiFi credentials (pending a
physical check). The packager checks the layout per
board and fails if any part overlaps NVS or the app is larger than its
partition. The manifest sets `new_install_prompt_erase: true`, so the installer
offers an erase and you choose. With `wifi.enabled: true` the manifest sets
`new_install_improv_wait_time: 15`, so the installer offers Improv WiFi setup
after the flash.

### Package locally

Packaging reads existing build outputs, so build first. Build without
`secrets.yaml`: the packager refuses an env whose `secrets.yaml` sets any value
or that has OTA compiled in, because those are plaintext in `firmware.bin`.
Move `secrets.yaml` aside and rebuild before packaging for publication.

```sh
for e in $(python scripts/list_envs.py | python -c "import json,sys;print(' '.join(json.load(sys.stdin)))"); do
  .venv/bin/pio run -e "$e"
done
.venv/bin/python scripts/package_firmware.py --version v0.1.0 --output web/public/firmware
cd web && npm ci && npm run build
```

| Option | Meaning |
|---|---|
| `--output DIR` | Required. Packager-owned names are swapped in atomically; other files in `DIR` are untouched |
| `--version` | `vX.Y.Z` or `X.Y.Z` with optional suffix; default `git describe --tags --always`, else `0.0.0-dev` |
| `--commit` | Full lowercase SHA; default `git rev-parse HEAD` |
| `--env ENV` | Package only this env; repeat for several. Default: every env whose board id equals its env name |
| `--project DIR` | Repository root; default is this repository |
| `--allow-secrets` | Package even though `secrets.yaml` sets a value or OTA is compiled in; prints a warning. Local use only, never publish the output |

Aliases such as `sticks3-macro` (a different env on the `sticks3` board) are not
packaged by default; use `--env sticks3-macro` to publish one instead of
`sticks3`. Two envs on the same board cannot be packaged in one run. All
packaged boards must share one config file, because `index.json` holds a copy
of the parsed `config.yaml`, so it must contain no secrets. Output: `index.json` (schema version 2) plus a directory
per board holding the four `.bin` files and `manifest.json`.

If `web/public/firmware/` is absent, `npm run build` still succeeds and the
site shows a "No release published yet" state. If it is present, the build
verifies every part's size and SHA-256 and refuses to build on a mismatch.

### Site environment variables

| Variable | Used for |
|---|---|
| `BASE_PATH` | Astro `base`, for example `/my-repo/` on GitHub Pages; default `/` |
| `SITE_URL` | Astro `site`; default `http://localhost:4321` |
| `REPO_URL` | "Source" link on the page; falls back to `GITHUB_REPOSITORY`, else no link |

Page copy is in `web/src/content/site.ts`; project name, tagline and
description come from `index.json` when a release is packaged.

## OTA (development only)

OTA is compiled in (`FEATURE_OTA`) only when all of these hold; otherwise the
build prints the warning and continues without it. CI and release builds have no
`secrets.yaml`, so they ship with OTA off.

| Condition | Warning when missing |
|---|---|
| `ota.enabled: true` | none (OTA simply off) |
| `ota_password` in `secrets.yaml` | `OTA disabled: no ota_password in secrets.yaml` |
| `wifi.enabled: true` | `OTA disabled: wifi.enabled is false` |

To use it on your own device:

1. `cp secrets.example.yaml secrets.yaml` and set `ota_password` and, if needed,
   `wifi_ssid` and `wifi_password`.
2. Keep `wifi.enabled: true` and `ota.enabled: true` in `config.yaml`.
3. Build and flash once over USB. The build must not print
   `OTA disabled: ...`.
4. Once the board is on the same LAN, upload over WiFi. In
   `platformio_override.ini`:

   ```ini
   [env:esp32c3]
   upload_protocol = espota
   upload_port = esp32-starter.local   ; wifi.hostname, or the device IP
   upload_flags = --auth=<your ota_password>
   ```

   then `.venv/bin/pio run -e esp32c3 -t upload`.

Behavior and limits:

- The OTA hostname is `wifi.hostname`; the password is compiled into the image.
  Do not distribute binaries built with a real `secrets.yaml`; the packager
  refuses them unless given `--allow-secrets`.
- A transfer runs inside `net::updateOta()`, so the app loop does not run
  during it. As it starts, the default app releases any held key and shows
  `OTA UPDATING`; presses during the transfer are never read. Success restarts
  the device. After a failed transfer the app resets its gesture state and
  continues.
- No rollback is implemented. A bad OTA image needs a USB reflash.
- OTA needs room for a second app partition. With the default config the C3
  OTA image is larger than its app partition (measured, see
  [boards.md](boards.md#memory)); drop a module first, and check the
  `firmware.bin` size, not only the `Flash:` line.
- The `espota` upload commands above are documented from PlatformIO's
  conventions and have not been run.

## Releases

A release is a tag `vX.Y.Z` pushed to GitHub (or the `Release` workflow run
manually with an existing tag). The workflow:

1. Rejects tags that are not stable `vX.Y.Z` or that already have a release.
2. Builds every env from `scripts/list_envs.py` with no `secrets.yaml`, so OTA is
   off.
3. Runs `scripts/check.sh` (native tests and Python tests).
4. Runs `scripts/package_firmware.py --version <tag> --output web/public/firmware`.
5. Builds the site with `BASE_PATH=/<repo-name>/` and
   `SITE_URL=https://<owner>.github.io`, so a fork needs no edits.
6. Creates the GitHub Release with `index.json` and one `<board>.zip` per
   packaged board, then deploys the site to Pages.

Prerequisite: in the repository settings, set Pages to deploy from GitHub
Actions. To cut a release, from a clean `main`, choose an unused version:

```sh
git tag vX.Y.Z
git push origin vX.Y.Z
```

(Documented, not run: the release workflow has not been executed.) CI (`.github/workflows/ci.yml`) runs on pull requests and pushes to
`main`: one build job per env, the host and packaging checks, and the site
checks. Its jobs and local equivalents are in [testing.md](testing.md).
