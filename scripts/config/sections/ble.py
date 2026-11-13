"""ble: Bluetooth LE HID keyboard (name shown when pairing, typing speed)."""
from config.core import ConfigError, boolean, cstr, fields, integer

KEY, REQUIRED = "ble", True
DEFAULT = None
INCLUDES = []


def validate(value, ctx):
    value = fields(value, ["name"], KEY, ["enabled", "keystroke_ms"])
    name = value["name"]
    if not isinstance(name, str) or not name.strip() or len(name.encode("utf-8")) > 29:
        raise ConfigError("ble.name", "must contain 1-29 UTF-8 bytes")
    if any(ord(c) < 32 or ord(c) == 127 for c in name):
        raise ConfigError("ble.name", "must not contain control characters")
    return {"enabled": boolean(value.get("enabled", True), "ble.enabled"), "name": name,
            "keystroke_ms": integer(value.get("keystroke_ms", 15), 5, 100, "ble.keystroke_ms")}


def features(cfg, ctx):
    return {"FEATURE_BLE_HID": cfg["enabled"]}


def emit(cfg, ctx):
    return [f"constexpr const char* BLE_NAME = {cstr(cfg['name'])};",
            f"constexpr uint32_t KEYSTROKE_MS = {cfg['keystroke_ms']};"]
