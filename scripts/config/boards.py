"""Board table lookup and the compiler defines derived from it."""
import subprocess

from config.core import ConfigError, load_yaml
from config.sections.buttons import button_count


def load_boards(path):
    return load_yaml(path)


def select_board(boards, board_id):
    if board_id not in boards:
        raise ConfigError("custom_board", f"unknown board {board_id!r}; known: {sorted(boards)}")
    return boards[board_id]


def firmware_version(cwd):
    try:
        out = subprocess.run(["git", "describe", "--tags", "--always"], cwd=cwd, capture_output=True,
                             text=True, timeout=10, check=True).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        out = ""
    return out or "0.0.0-dev"


def board_defines(board_id, board, features, version):
    """Return [(name, value)] for CPPDEFINES; string values are pre-quoted by the caller."""
    pins = board.get("button_pins", [])
    led = board.get("led") or {}
    # M5Unified reads a numeric BOARD_ID macro, so ours is APP_BOARD_ID.
    defines = [("APP_BOARD_ID", board_id), ("BOARD_LABEL", board["label"]), ("FIRMWARE_VERSION", version)]
    plain = [("BOARD_DRIVER_M5" if board["driver"] == "m5" else "BOARD_DRIVER_GPIO", 1),
             ("BOARD_BUTTON_COUNT", button_count(board))]
    if pins:
        plain.append(("BOARD_BUTTON_PINS", "{" + ",".join(map(str, pins)) + "}"))
    plain += [("BOARD_LED_PIN", led.get("pin", -1)), ("BOARD_LED_RGB", int(led.get("rgb", False))),
              ("BOARD_HAS_DISPLAY", int(board.get("display", False))),
              ("BOARD_HAS_BATTERY", int(board.get("battery", False)))]
    plain += [(name, int(on)) for name, on in sorted(features.items())]
    return defines, plain
