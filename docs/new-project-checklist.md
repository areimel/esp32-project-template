# New project checklist

Work top to bottom after cloning or using the repository as a template. Each
step names the file to edit and a command that shows it worked.

## 1. Identity

- [ ] `config.yaml`: set `project.name` (1-32 ASCII characters), `tagline` and
  `description`, `ble.name` (1-29 UTF-8 bytes) and `wifi.hostname`.
- [ ] `web/package.json`: change `"name"` from `esp32-starter-site`.
- [ ] `web/src/content/site.ts`: replace the template copy in `project`,
  `features`, `steps` and `faq`. With a packaged release the name, tagline and
  description come from `index.json` instead.
- [ ] `LICENSE`: set the holder and year.
- [ ] This `README.md` and `docs/`: rewrite the intro for your product; keep the
  parts that describe the template's tooling.
- [ ] Check: `.venv/bin/pio run -e <env>` prints no `Configuration error`.

## 2. Decide which modules to keep

- [ ] In `config.yaml` set `ble.enabled`, `wifi.enabled`, `ota.enabled` and
  `display.enabled`. A disabled module is not compiled in.
- [ ] If you drop WiFi, `ota.enabled` has no effect (the build warns), and the
  installer skips Improv.
- [ ] Delete library folders you will never enable only after checking that
  nothing includes them; `src/main.cpp` guards each module with its flag.
- [ ] Map your buttons in `buttons:` or replace the default app in `src/main.cpp`
  (it is the wiring example, not a framework).
- [ ] Check: build every board you target and read the `Flash:` line; the C3 and
  classic ESP32 are already above 90% ([boards.md](boards.md)).

## 3. Boards

- [ ] Keep only the envs you support: remove unused `[env:*]` sections from
  `platformio.ini` and rows from `boards/boards.yaml`. CI and release build every
  remaining env (`python scripts/list_envs.py`).
- [ ] To add one, follow [boards.md](boards.md#add-a-board).
- [ ] Update the board tables in `README.md` and `docs/boards.md`.

## 4. The macro example

- [ ] If you do not need it: delete `examples/macro-clicker/`, its
  `[env:sticks3-macro]`, the `-Iexamples/macro-clicker/include` flag in
  `[env:native]`, its `test/test_macro_*` suites and
  `scripts/tests/test_macro_schema.py`, then remove it from the docs and
  `AGENTS.md`.
- [ ] If you keep it, read [its README](../examples/macro-clicker/README.md).

## 5. Secrets

- [ ] Keep `secrets.yaml` gitignored. Do not put passwords in `config.yaml`,
  tests, fixtures or macro text; they ship in the firmware and in `index.json`.
- [ ] Only use `ota_password` and dev WiFi credentials for local builds. Do not
  publish binaries built with them.

## 6. Repository, Pages and CI

- [ ] Push to GitHub. In Settings, Pages, choose GitHub Actions as the source.
- [ ] `BASE_PATH` and `SITE_URL` are derived from the repository name and owner
  in `.github/workflows/release.yml`; no edit is needed for a fork or rename.
- [ ] Optionally set `REPO_URL` for the site's source link; on GitHub Actions it
  falls back to `GITHUB_REPOSITORY`.
- [ ] Check: open a pull request and confirm the `CI` jobs pass: `build`
  per env, `test` and `web`.

## 7. Verify before the first release

- [ ] `bash scripts/check.sh` after building every env.
- [ ] `cd web && npm ci && npm run check && npm test && npm run build`.
- [ ] Package locally and open the site:
  `.venv/bin/python scripts/package_firmware.py --output web/public/firmware`
  ([flashing-and-ota.md](flashing-and-ota.md#package-locally)).
- [ ] Flash a real board over USB and work through its section in
  [../TODO.md](../TODO.md). Do not call a board supported until its checks are
  recorded as observed.
- [ ] Decide your pairing story: if you change the HID report descriptor, tell
  users to forget the device and re-pair.

## 8. Cut a release

- [ ] Tag `vX.Y.Z` and push the tag. See
  [flashing-and-ota.md](flashing-and-ota.md#releases). Release builds ship with
  OTA off.
- [ ] Update `AGENTS.md` for the rules your team keeps, and keep its CodeGraph
  section if you use CodeGraph.
