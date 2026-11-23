"""Fixture for the packaging tests: a temp project with synthetic builds."""
import hashlib
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from support import ROOT, BOARDS, default_config  # noqa: E402 (also sets sys.path)
import yaml
from config.core import Context, load_secrets
from generate_config import render
from package_firmware import package

CHIP_IDS = {"ESP32": 0, "ESP32-S3": 9, "ESP32-C3": 5}
DEFAULT_PARTS = [("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xE000, 0x2000),
                 ("app0", 0, 0x10, 0x10000, 0x140000), ("spiffs", 1, 0x82, 0x150000, 0x20000)]
SECRET = "hunter2-ota-secret"


def synthetic_image(chip_id, payload=b"test payload"):
    header = bytearray(24)
    header[0], header[1], header[2], header[3], header[23] = 0xE9, 1, 2, 0x3F, 1
    struct.pack_into("<H", header, 12, chip_id)
    checksum = 0xEF
    for byte in payload:
        checksum ^= byte
    image = bytes(header) + struct.pack("<II", 0x3FC88000, len(payload)) + payload
    image += b"\0" * (15 - len(image) % 16) + bytes([checksum])
    return image + hashlib.sha256(image).digest()


def partition_table(parts):
    return b"".join(struct.pack("<HBBII16sI", 0x50AA, k, st, o, s, n.encode(), 0)
                    for n, k, st, o, s in parts)


class PackagingCase(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "project"
        self.output = Path(self.temp.name) / "out/firmware"
        self.core = Path(self.temp.name) / "platformio"
        real = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
        boot = self.core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
        boot.parent.mkdir(parents=True)
        boot.write_bytes(b"\xff" * 0x2000)
        (self.core / "packages/tool-esptoolpy").symlink_to(
            real / "packages/tool-esptoolpy", target_is_directory=True)
        (self.core / "penv").symlink_to(real / "penv", target_is_directory=True)
        (self.root / "boards").mkdir(parents=True)
        (self.root / "boards/boards.yaml").write_bytes((ROOT / "boards/boards.yaml").read_bytes())
        self.write_config(default_config())
        self.options = {"sticks3": {}, "esp32": {}, "esp32c3": {}, "sticks3-macro": {"custom_board": "sticks3"}}
        for env in ("sticks3", "esp32", "esp32c3"):
            self.make_build(env)

    def write_config(self, config):
        (self.root / "config.yaml").write_text(yaml.safe_dump(config))
        self.config = config

    def make_build(self, env, parts=DEFAULT_PARTS):
        board = BOARDS[env]
        build = self.root / ".pio/build" / env
        (build / "generated").mkdir(parents=True, exist_ok=True)
        secrets = load_secrets(self.root / "secrets.yaml")
        header, _ = render(self.config, Context(env, board, secrets))
        (build / "generated/generated_config.h").write_text(header)
        # Like a real build, the app image carries whatever secrets the header compiled in.
        payload = b"test payload" + b"".join(v.encode() for v in secrets.values())
        (build / "bootloader.bin").write_bytes(synthetic_image(CHIP_IDS[board["chip"]]))
        (build / "firmware.bin").write_bytes(synthetic_image(CHIP_IDS[board["chip"]], payload))
        (build / "partitions.bin").write_bytes(partition_table(parts))

    def package(self, **kwargs):
        kwargs.setdefault("envs", ["sticks3", "esp32", "esp32c3"])
        return package(self.root, self.output, "v1.0.0", "a" * 40, platformio_dir=self.core,
                       env_options=self.options, **kwargs)

    def board(self, index, env):
        return next(b for b in index["boards"] if b["id"] == env)

    def offsets(self, board):
        return {p["path"].split("/")[1]: p["offset"] for p in board["parts"]}
