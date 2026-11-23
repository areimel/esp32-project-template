"""Built-in config sections. Each module exposes KEY, REQUIRED, DEFAULT,
validate(value, ctx) and emit(cfg, ctx); it may add INCLUDES (header list) and
features(cfg, ctx) -> {flag: bool}."""
from . import project, ble, wifi, ota, display, gestures, buttons

BUILTIN = [project, ble, wifi, ota, display, gestures, buttons]
