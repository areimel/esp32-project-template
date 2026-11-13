"""HID key tables and combo parsing, shared by every section that takes key combos."""
from config.core import ConfigError

MODIFIERS = {
    "CTRL": 0x01, "CONTROL": 0x01,
    "SHIFT": 0x02,
    "ALT": 0x04, "OPT": 0x04, "OPTION": 0x04,
    "CMD": 0x08, "GUI": 0x08, "WIN": 0x08, "META": 0x08, "SUPER": 0x08,
}
SHIFT = MODIFIERS["SHIFT"]
ENTER, TAB = 0x28, 0x2B

KEYS = {chr(ord("A") + n): 0x04 + n for n in range(26)}
KEYS.update({str(n): 0x1D + n for n in range(1, 10)})
KEYS["0"] = 0x27
KEYS.update({f"F{n}": 0x39 + n for n in range(1, 13)})
KEYS.update({f"F{n}": 0x68 + n - 13 for n in range(13, 25)})
KEYS.update({
    "ENTER": 0x28, "ESC": 0x29, "ESCAPE": 0x29,
    "BACKSPACE": 0x2A, "TAB": 0x2B, "SPACE": 0x2C,
    "CAPS_LOCK": 0x39, "PRINT_SCREEN": 0x46, "INSERT": 0x49, "DELETE": 0x4C,
    "HOME": 0x4A, "PAGE_UP": 0x4B, "END": 0x4D, "PAGE_DOWN": 0x4E,
    "RIGHT": 0x4F, "LEFT": 0x50, "DOWN": 0x51, "UP": 0x52,
    "MINUS": 0x2D, "EQUAL": 0x2E, "LBRACKET": 0x2F, "RBRACKET": 0x30,
    "BACKSLASH": 0x31, "SEMICOLON": 0x33, "QUOTE": 0x34, "GRAVE": 0x35,
    "COMMA": 0x36, "PERIOD": 0x37, "SLASH": 0x38,
})


def _build_ascii():
    """US keyboard layout: character -> (modifiers, usage) for 0x20-0x7E."""
    table = {chr(ord("a") + n): (0, 0x04 + n) for n in range(26)}
    table.update({chr(ord("A") + n): (SHIFT, 0x04 + n) for n in range(26)})
    table.update({str(n): (0, 0x1D + n) for n in range(1, 10)})
    table["0"] = (0, 0x27)
    table.update({c: (SHIFT, 0x1E + n) for n, c in enumerate("!@#$%^&*(")})
    table[")"] = (SHIFT, 0x27)
    table[" "] = (0, 0x2C)
    plain = "-=[]\\;'`,./"
    shifted = '_+{}|:"~<>?'
    usages = [0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38]
    for p, s, u in zip(plain, shifted, usages):
        table[p] = (0, u)
        table[s] = (SHIFT, u)
    return table


ASCII = _build_ascii()
ASCII["\n"] = (0, ENTER)
ASCII["\t"] = (0, TAB)


def parse_combo(value, path):
    """Return (modifiers, usage) for a combo like "CMD+SHIFT+Z"."""
    # YAML naturally parses bare digit keys as integers. Accept these as keys.
    if type(value) is int and 0 <= value <= 9:
        value = str(value)
    if not isinstance(value, str):
        raise ConfigError(path, 'key combo must be a string such as "CTRL+C"')
    parts = [part.strip().upper() for part in value.split("+")]
    mods, usage, seen = 0, 0, set()
    for index, part in enumerate(parts):
        if not part:
            raise ConfigError(path, f"empty key name in {value!r}")
        if part in seen:
            raise ConfigError(path, f"duplicate {part} in {value!r}")
        seen.add(part)
        if part in MODIFIERS:
            if usage:
                raise ConfigError(path, f"modifier {part} must come before the key in {value!r}")
            if mods & MODIFIERS[part]:
                raise ConfigError(path, f"duplicate modifier {part} in {value!r}")
            mods |= MODIFIERS[part]
        elif part in KEYS:
            if index != len(parts) - 1:
                raise ConfigError(
                    path, f"key {part} must be last in {value!r}; only one non-modifier key is allowed")
            usage = KEYS[part]
        else:
            raise ConfigError(path, f"unsupported key {part!r} in {value!r}")
    return mods, usage
