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
- A normal upload keeps NVS, so Bluetooth bonds and WiFi credentials survive.

### Recovery

```sh
.venv/bin/pio run -e esp32c3 -t erase        # erases the whole flash, including NVS
.venv/bin/pio run -e esp32c3 -t upload
```

After an erase, WiFi must be provisioned again and the host must pair again.
If Bluetooth will not connect after an update, forget the device on the host and
pair again; hold every button at boot (3 s on 2-button boards, 5 s on 1-button
boards, counted after the splash) to clear the device's bonds first.

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
normal install keeps bonds and WiFi credentials. The packager checks this per
board and fails if any part overlaps NVS or the app is larger than its
partition. The manifest sets `new_install_prompt_erase: true`, so the installer
offers an erase and you choose. With `wifi.enabled: true` the manifest sets
`new_install_improv_wait_time: 15`, so the installer offers Improv WiFi setup
after the flash.

### Package locally

Packaging reads existing build outputs, so build first.

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

Aliases such as `sticks3-macro` (a different env on the `sticks3` board) are not
packaged by default; use `--env sticks3-macro` to publish one instead of
`sticks3`. Two envs on the same board cannot be packaged in one run. All
packaged boards must share one config file, because `index.json` holds a single
sanitized copy of it. Output: `index.json` (schema version 2) plus a directory
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

OTA is off in CI and release builds. To use it on your own device:

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
  Do not distribute binaries built with a real `secrets.yaml`.
- During an OTA transfer the default app releases keys and ignores buttons.
- No rollback is implemented. A bad OTA image needs a USB reflash.
- OTA needs room for a second app partition and the C3 image is near full (see
  [boards.md](boards.md)); watch the `Flash:` line of the build.
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
Actions. To cut a release, from a clean `main`:

```sh
git tag v0.1.0
git push origin v0.1.0
```

(Documented, not run: this repository has no tags yet and the workflow was not
executed.) CI (`.github/workflows/ci.yml`) runs on pull requests and pushes to
`main`: one build job per env, the host and packaging checks, and the site
checks. Its jobs and local equivalents are in [testing.md](testing.md).
