# ESP32 starter template rework

Design: docs/superpowers/specs/2026-11-13-esp32-template-design.md

- [x] 0. Remove demo media and stray pnpm files; add design spec
- [ ] 1. pioarduino platform, multi-board platformio.ini, boards/, lib/core + board layer
- [ ] 2a. Modular config generator (core + sections) and split Python tests
- [ ] 2b. lib/input (Gesture) + lib/store (Settings) with native Unity tests
- [ ] 2c. lib/ble_hid with native stubs
- [ ] 2d. lib/net: WiFi, Improv serial, dev-only OTA
- [ ] 2e. lib/display: Theme + Screen base
- [ ] 3a. Minimal default app (src/main.cpp)
- [ ] 3b. Macro clicker moved to examples/macro-clicker
- [ ] 4a. Board-driven packaging -> ESP Web Tools manifests + index.json
- [ ] 4b. Microsite on ESP Web Tools with shared components
- [ ] 4c. Site CSS split into tokens/base/layout/components
- [ ] 4d. Composite CI setup action, board matrix, release workflow
- [ ] 5. README + docs/ + AGENTS.md for the template
- [ ] 6. Code review, fixes, final verification

## Pending physical checks (user)
- [ ] Flash Stick-S3 from the site; confirm boot and display
- [ ] Improv WiFi provisioning after web install
- [ ] Dev OTA upload with secrets.yaml password
- [ ] BLE pairing and button gestures (default app and macro example)
- [ ] C3 / classic ESP32 boards (compile-verified only)
