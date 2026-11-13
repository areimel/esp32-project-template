"""Shared test helpers: import path setup and context builders."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import yaml  # noqa: E402
from config.core import Context, UniqueKeyLoader  # noqa: E402

BOARDS = yaml.load((ROOT / "boards/boards.yaml").read_text(encoding="utf-8"), Loader=UniqueKeyLoader)


def make_ctx(board_id="sticks3", secrets=None):
    return Context(board_id=board_id, board=BOARDS[board_id], secrets=secrets or {}, warnings=[])


def default_config():
    return yaml.load((ROOT / "config.yaml").read_text(encoding="utf-8"), Loader=UniqueKeyLoader)
