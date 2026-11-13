"""buttons: one entry per physical button, each with single / double / long actions."""
from config.core import ConfigError, cstr, fields, is_printable
from config.sections.keys import parse_combo

KEY, REQUIRED, DEFAULT = "buttons", False, []
INCLUDES = ["<stdint.h>"]
GESTURES = ("single", "double", "long")
MAX_LABEL = 10


def button_count(board):
    return board.get("buttons") or len(board.get("button_pins", []))


def _action(value, path):
    """Return (label, mods, usage); an unassigned slot is ("", 0, 0)."""
    if value is None:
        return "", 0, 0
    label = ""
    if isinstance(value, dict):
        value = fields(value, ["keys"], path, ["label"])
        if "label" in value:
            label = value["label"]
            if (not isinstance(label, str) or not 1 <= len(label) <= MAX_LABEL
                    or not is_printable(label) or not label.strip()):
                raise ConfigError(f"{path}.label", f"must be 1-{MAX_LABEL} printable ASCII characters")
            label = label.upper()
        value = value["keys"]
        path = f"{path}.keys"
    mods, usage = parse_combo(value, path)
    return label, mods, usage


def validate(value, ctx):
    if not isinstance(value, list):
        raise ConfigError(KEY, "must be a list with one entry per button")
    rows = []
    for index, entry in enumerate(value):
        path = f"{KEY}[{index}]"
        entry = fields(entry, [], path, GESTURES)
        rows.append([_action(entry.get(g), f"{path}.{g}") for g in GESTURES])
    count = button_count(ctx.board)
    for index in range(count, len(rows)):
        ctx.warnings.append(f"buttons[{index}] ignored: board {ctx.board_id} has {count} button(s)")
    rows = rows[:count] + [[("", 0, 0)] * 3 for _ in range(count - len(rows))]
    return rows


def emit(cfg, ctx):
    lines = ["struct ButtonAction { const char* label; uint8_t mods; uint8_t usage; };",
             "constexpr uint8_t BUTTON_SLOTS = BOARD_BUTTON_COUNT;",
             "static const ButtonAction BUTTON_ACTIONS[BUTTON_SLOTS][3] = {"]
    for row in cfg:
        cells = ", ".join(f"{{{cstr(l)}, 0x{m:02X}, 0x{u:02X}}}" for l, m, u in row)
        lines.append(f"  {{ {cells} }},")
    return lines + ["};"]
