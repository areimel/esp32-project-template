"""gestures: button timing for single / double / long press detection."""
from config.core import ConfigError, fields, integer

KEY, REQUIRED = "gestures", False
DEFAULT = {"debounce_ms": 25, "double_click_ms": 250, "long_press_ms": 650}


def validate(value, ctx):
    value = fields(value, [], KEY, DEFAULT)
    value = {**DEFAULT, **value}
    debounce = integer(value["debounce_ms"], 1, 100, "gestures.debounce_ms")
    double = integer(value["double_click_ms"], 50, 1000, "gestures.double_click_ms")
    hold = integer(value["long_press_ms"], 100, 10000, "gestures.long_press_ms")
    if not debounce < double < hold:
        raise ConfigError("gestures", "Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms")
    return {"debounce_ms": debounce, "double_click_ms": double, "long_press_ms": hold}


def emit(cfg, ctx):
    return [f"constexpr uint32_t DEBOUNCE_MS = {cfg['debounce_ms']};",
            f"constexpr uint32_t DOUBLE_CLICK_MS = {cfg['double_click_ms']};",
            f"constexpr uint32_t LONG_PRESS_MS = {cfg['long_press_ms']};"]
