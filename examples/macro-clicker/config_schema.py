"""profiles: macro profiles compiled into HID strokes (the macro clicker's `schema:` module).

Firmware never parses key names, text or labels; this section turns every
profile slot into ready-to-send strokes.
"""
from config.core import ConfigError, cstr, fields, integer, is_printable
from config.sections.keys import ASCII, parse_combo

KEY, REQUIRED, DEFAULT = "profiles", True, None
INCLUDES = ['"Macro.h"']

MAX_PROFILES = 9
MAX_PROFILE_NAME = 12
MAX_LABEL = 10
MAX_STROKES_PER_MACRO = 4096
MAX_STROKES_TOTAL = 32768
STEP_KINDS = ("keys", "text", "delay")

BUTTONS = ("main", "secondary")
GESTURES = ("single", "double", "long")


def _combo(value, path):
    """Return (mods, usage, normalized) for a combo such as "cmd+shift+z"."""
    mods, usage = parse_combo(value, path)
    return mods, usage, "+".join(part.strip().upper() for part in str(value).split("+"))


def _shorten(text):
    return text if len(text) <= MAX_LABEL else text[:MAX_LABEL - 1] + "~"


def _encode_text(text, path):
    if not isinstance(text, str) or not text:
        raise ConfigError(path, "must be a non-empty string")
    strokes = []
    for index, char in enumerate(text):
        if char not in ASCII:
            raise ConfigError(
                path, f"unsupported character {char!r} at index {index}; "
                "use printable US ASCII, newline, or tab")
        mods, usage = ASCII[char]
        strokes.append((mods, usage, 0))
    return strokes


def _parse_label(value, path):
    if (not isinstance(value, str) or not 1 <= len(value) <= MAX_LABEL
            or not is_printable(value) or not value.strip()):
        raise ConfigError(path, f"must be 1-{MAX_LABEL} printable ASCII characters")
    return value.upper()


def _text_label(text):
    line = text.split("\n", 1)[0]
    line = "".join(c if 0x20 <= ord(c) <= 0x7E else " " for c in line).strip()
    return _shorten(line.upper()) if line else "MACRO"


def _step(step, path):
    if isinstance(step, dict):
        kind = [key for key in STEP_KINDS if key in step]
        if len(kind) != 1 or len(step) != 1:
            raise ConfigError(path, "must contain exactly one of keys, text, or delay")
        kind = kind[0]
        if kind == "keys":
            mods, usage, _ = _combo(step["keys"], f"{path}.keys")
            return [(mods, usage, 0)]
        if kind == "text":
            return _encode_text(step["text"], f"{path}.text")
        return [(0, 0, integer(step["delay"], 1, 10000, f"{path}.delay"))]
    mods, usage, _ = _combo(step, path)
    return [(mods, usage, 0)]


def _action(value, path):
    """Return None for an unassigned slot, otherwise (label, strokes)."""
    if value is None:
        return None
    if isinstance(value, dict):
        kind = [key for key in ("keys", "text", "sequence") if key in value]
        unknown = set(value) - {"keys", "text", "sequence", "label"}
        if len(kind) != 1 or unknown:
            raise ConfigError(
                path, "must contain exactly one of keys, text, or sequence, "
                f"plus an optional label (unknown fields {sorted(unknown)})")
        kind = kind[0]
        if kind == "keys":
            mods, usage, normalized = _combo(value["keys"], f"{path}.keys")
            strokes, label = [(mods, usage, 0)], _shorten(normalized)
        elif kind == "text":
            strokes = _encode_text(value["text"], f"{path}.text")
            label = _text_label(value["text"])
        else:
            steps = value["sequence"]
            if not isinstance(steps, list) or not steps:
                raise ConfigError(f"{path}.sequence", "must be a non-empty list")
            strokes = []
            for index, step in enumerate(steps):
                strokes += _step(step, f"{path}.sequence[{index}]")
            label = "MACRO"
        if "label" in value:
            label = _parse_label(value["label"], f"{path}.label")
    else:
        mods, usage, normalized = _combo(value, path)
        strokes, label = [(mods, usage, 0)], _shorten(normalized)
    if len(strokes) > MAX_STROKES_PER_MACRO:
        raise ConfigError(path, f"compiles to {len(strokes)} strokes; the limit is {MAX_STROKES_PER_MACRO}")
    return label, strokes


def validate(value, ctx):
    if not isinstance(value, list) or not 1 <= len(value) <= MAX_PROFILES:
        raise ConfigError(KEY, f"must be a list of 1-{MAX_PROFILES} profiles")
    result, names = [], set()
    for p, profile in enumerate(value):
        where = f"{KEY}[{p}]"
        profile = fields(profile, ["name", "buttons"], where)
        name = profile["name"]
        if (not isinstance(name, str) or not is_printable(name)
                or not 1 <= len(name.strip()) <= MAX_PROFILE_NAME):
            raise ConfigError(f"{where}.name", f"must be 1-{MAX_PROFILE_NAME} printable ASCII characters")
        name = name.strip()
        if name.lower() in names:
            raise ConfigError(f"{where}.name", f"{name!r} duplicates another profile name")
        names.add(name.lower())
        buttons = fields(profile["buttons"], BUTTONS, f"{where}.buttons")
        slots = {}
        for button in BUTTONS:
            mapping = fields(buttons[button], GESTURES, f"{where}.buttons.{button}")
            for gesture in GESTURES:
                slots[(button, gesture)] = _action(
                    mapping[gesture], f"{where}.buttons.{button}.{gesture}")
        result.append((name, slots))
    total = sum(len(s[1]) for _, slots in result for s in slots.values() if s)
    if total > MAX_STROKES_TOTAL:
        raise ConfigError(KEY, f"compile to {total} strokes in total; the limit is {MAX_STROKES_TOTAL}")
    return result


def _stroke_array(name, strokes):
    cells = [f"{{0x{m:02X}, 0x{u:02X}, {d}}}" for m, u, d in strokes]
    rows = [", ".join(cells[i:i + 4]) for i in range(0, len(cells), 4)]
    return f"static const Stroke {name}[] = {{\n  " + ",\n  ".join(rows) + "\n};"


def emit(cfg, ctx):
    lines, entries = [], []
    for p, (profile_name, slots) in enumerate(cfg):
        rows = []
        for button in BUTTONS:
            cells = []
            for gesture in GESTURES:
                slot = slots[(button, gesture)]
                if slot is None:
                    cells.append('{"", nullptr, 0}')
                    continue
                label, strokes = slot
                array = f"P{p}_{button.upper()}_{gesture.upper()}"
                lines += [_stroke_array(array, strokes), ""]
                cells.append(f"{{{cstr(label)}, {array}, {len(strokes)}}}")
            rows.append("{ " + ", ".join(cells) + " }")
        entries.append(f"  {{{cstr(profile_name)}, {{\n    " + ",\n    ".join(rows) + "\n  }}")
    return lines + [
        f"constexpr uint8_t PROFILE_COUNT = {len(cfg)};",
        "static const Profile PROFILES[PROFILE_COUNT] = {",
        ",\n".join(entries),
        "};",
    ]
