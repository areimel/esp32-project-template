import hashlib
import json
import os
import struct
import tempfile
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from support import ROOT, BOARDS, default_config  # noqa: F401 (sets sys.path)
import yaml
from config.core import Context, load_secrets
from generate_config import render
from package_firmware import package

CHIP_IDS = {"ESP32": 0, "ESP32-S3": 9, "ESP32-C3": 5}
DEFAULT_PARTS = [("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xE000, 0x2000),
                 ("app0", 0, 0x10, 0x10000, 0x140000), ("spiffs", 1, 0x82, 0x150000, 0x20000)]
SECRET = "hunter2-ota-secret"


def synthetic_image(chip_id):
    header = bytearray(24)
    header[0], header[1], header[2], header[3], header[23] = 0xE9, 1, 2, 0x3F, 1
    struct.pack_into("<H", header, 12, chip_id)
    payload = b"test payload"
    checksum = 0xEF
    for byte in payload:
        checksum ^= byte
    image = bytes(header) + struct.pack("<II", 0x3FC88000, len(payload)) + payload
    image += b"\0" * (15 - len(image) % 16) + bytes([checksum])
    return image + hashlib.sha256(image).digest()


def partition_table(parts):
    return b"".join(struct.pack("<HBBII16sI", 0x50AA, k, st, o, s, n.encode(), 0)
                    for n, k, st, o, s in parts)


class PackagingTests(unittest.TestCase):
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
        header, _ = render(self.config, Context(env, board, load_secrets(self.root / "secrets.yaml")))
        (build / "generated/generated_config.h").write_text(header)
        image = synthetic_image(CHIP_IDS[board["chip"]])
        (build / "bootloader.bin").write_bytes(image)
        (build / "firmware.bin").write_bytes(image)
        (build / "partitions.bin").write_bytes(partition_table(parts))

    def package(self, **kwargs):
        kwargs.setdefault("envs", ["sticks3", "esp32", "esp32c3"])
        return package(self.root, self.output, "v1.0.0", "a" * 40, platformio_dir=self.core,
                       env_options=self.options, **kwargs)

    def board(self, index, env):
        return next(b for b in index["boards"] if b["id"] == env)

    def offsets(self, board):
        return {p["path"].split("/")[1]: p["offset"] for p in board["parts"]}

    def test_bootloader_offsets_by_chip(self):
        index = self.package()
        self.assertEqual(self.offsets(self.board(index, "esp32"))["bootloader.bin"], 0x1000)
        self.assertEqual(self.offsets(self.board(index, "sticks3"))["bootloader.bin"], 0)
        self.assertEqual(self.offsets(self.board(index, "esp32c3"))["bootloader.bin"], 0)

    def test_offsets_come_from_partition_table(self):
        parts = [("nvs", 1, 2, 0x9000, 0x4000), ("otadata", 1, 0, 0xD000, 0x2000),
                 ("app0", 0, 0x10, 0x20000, 0x100000)]
        self.make_build("esp32", parts)
        offsets = self.offsets(self.board(self.package(), "esp32"))
        self.assertEqual(offsets["boot_app0.bin"], 0xD000)
        self.assertEqual(offsets["firmware.bin"], 0x20000)
        self.assertEqual(offsets["partitions.bin"], 0x8000)

    def test_part_overlapping_nvs_rejected_and_nothing_written(self):
        path = self.root / ".pio/build/esp32c3/bootloader.bin"
        path.write_bytes(path.read_bytes() + b"\xff" * 0x9000)
        with self.assertRaisesRegex(ValueError, "overlaps NVS"):
            self.package()
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.output.parent.glob("*")) if self.output.parent.exists() else [], [])

    def test_wrong_chip_image_rejected(self):
        path = self.root / ".pio/build/esp32c3/firmware.bin"
        path.write_bytes(synthetic_image(CHIP_IDS["ESP32-S3"]))
        with self.assertRaisesRegex(ValueError, "chip"):
            self.package()
        self.assertFalse(self.output.exists())

    def test_manifest_shape_and_index_shape(self):
        index = self.package()
        manifest = json.loads((self.output / "sticks3/manifest.json").read_text())
        self.assertEqual(list(manifest), ["name", "version", "new_install_prompt_erase",
                                          "new_install_improv_wait_time", "builds"])
        self.assertEqual(manifest["builds"][0]["chipFamily"], BOARDS["sticks3"]["chip"])
        self.assertTrue(manifest["new_install_prompt_erase"])
        self.assertEqual(manifest["new_install_improv_wait_time"], 15)
        self.assertEqual(manifest["builds"][0]["parts"][0], {"path": "bootloader.bin", "offset": 0})
        self.assertEqual(index, json.loads((self.output / "index.json").read_text()))
        self.assertEqual(index["schemaVersion"], 2)
        sticks = self.board(index, "sticks3")
        self.assertEqual(sticks["manifest"], "sticks3/manifest.json")
        self.assertEqual(sticks["buttons"], 2)
        self.assertEqual(self.board(index, "esp32")["buttons"], 1)
        self.assertEqual(len(index["configSha256"]), 64)
        self.assertEqual(index["project"]["name"], self.config["project"]["name"])
        for part in sticks["parts"]:
            data = (self.output / part["path"]).read_bytes()
            self.assertEqual(part["size"], len(data))
            self.assertEqual(part["sha256"], hashlib.sha256(data).hexdigest())

    def test_improv_wait_zero_when_wifi_disabled(self):
        config = dict(self.config, wifi={"enabled": False})
        self.write_config(config)
        for env in ("sticks3", "esp32", "esp32c3"):
            self.make_build(env)
        self.package()
        manifest = json.loads((self.output / "esp32/manifest.json").read_text())
        self.assertEqual(manifest["new_install_improv_wait_time"], 0)

    def test_secrets_never_reach_output(self):
        (self.root / "secrets.yaml").write_text(
            f"ota_password: {SECRET}\nwifi_ssid: homenet-{SECRET}\nwifi_password: pw-{SECRET}\n")
        for env in ("sticks3", "esp32", "esp32c3"):
            self.make_build(env)
        self.package()
        for path in self.output.rglob("*.json"):
            self.assertNotIn(SECRET, path.read_text())

    def test_atomic_output_when_any_board_fails(self):
        (self.root / ".pio/build/esp32c3/firmware.bin").unlink()
        with self.assertRaises(OSError):
            self.package()
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.output.parent.glob("*")) if self.output.parent.exists() else [], [])

    def test_failure_keeps_previous_output(self):
        self.package()
        before = (self.output / "index.json").read_bytes()
        (self.root / ".pio/build/esp32/firmware.bin").write_bytes(b"junk")
        with self.assertRaises(ValueError):
            self.package()
        self.assertEqual(before, (self.output / "index.json").read_bytes())

    def test_stale_configuration_rejected(self):
        (self.root / ".pio/build/esp32/generated/generated_config.h").write_text("stale")
        with self.assertRaisesRegex(ValueError, "stale"):
            self.package()

    def test_firmware_older_than_header_rejected(self):
        build = self.root / ".pio/build/esp32"
        os.utime(build / "firmware.bin", ns=(1_000_000_000, 1_000_000_000))
        os.utime(build / "generated/generated_config.h", ns=(2_000_000_000, 2_000_000_000))
        with self.assertRaisesRegex(ValueError, "predates"):
            self.package()

    def test_corrupt_images_rejected(self):
        path = self.root / ".pio/build/sticks3/firmware.bin"
        original = path.read_bytes()
        cases = {"segment checksum": 32, "SHA-256": len(original) - 1}
        for message, index in cases.items():
            with self.subTest(message):
                data = bytearray(original)
                data[index] ^= 1
                path.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    self.package()
        path.write_bytes(original + b"unexpected")
        with self.assertRaisesRegex(ValueError, "unexpected bytes"):
            self.package()
        for length in (24, 35, 47, len(original) - 1):
            with self.subTest(length=length):
                path.write_bytes(original[:length])
                with self.assertRaisesRegex(ValueError, "malformed|SHA-256"):
                    self.package()

    def test_missing_esptool_explains_build_requirement(self):
        (self.core / "packages/tool-esptoolpy").unlink()
        with self.assertRaisesRegex(ValueError, "build firmware first"):
            self.package()

    def test_swap_keeps_unrelated_files(self):
        self.package()
        (self.output / "notes.txt").write_text("keep me")
        (self.output / "other").mkdir()
        self.package()
        self.assertEqual((self.output / "notes.txt").read_text(), "keep me")
        self.assertTrue((self.output / "other").is_dir())
        self.assertTrue((self.output / "index.json").is_file())
        self.assertEqual([p.name for p in self.output.parent.iterdir()], ["firmware"])

    def test_boards_dropped_from_the_index_are_removed(self):
        self.package()
        (self.output / "other").mkdir()  # not a packager board: stays
        self.package(envs=["sticks3", "esp32"])
        self.assertFalse((self.output / "esp32c3").exists())
        self.assertTrue((self.output / "esp32/manifest.json").is_file())
        self.assertTrue((self.output / "other").is_dir())
        self.assertEqual([b["id"] for b in json.loads((self.output / "index.json").read_text())["boards"]],
                         ["sticks3", "esp32"])

    def test_failure_partway_through_swap_restores_previous_output(self):
        import package_firmware
        self.package()
        before = (self.output / "index.json").read_bytes()
        sticks = (self.output / "sticks3/manifest.json").read_bytes()
        real, calls = os.replace, []

        def flaky(src, dst):
            calls.append(dst)
            if len(calls) == 6:
                raise OSError("disk full")
            real(src, dst)

        package_firmware.move = flaky
        self.addCleanup(setattr, package_firmware, "move", real)
        with self.assertRaisesRegex(OSError, "disk full"):
            self.package()
        self.assertEqual(before, (self.output / "index.json").read_bytes())
        self.assertEqual(sticks, (self.output / "sticks3/manifest.json").read_bytes())
        self.assertEqual([p.name for p in self.output.parent.iterdir()], ["firmware"])

    def test_esptool_import_failure_reports_cause_and_search_path(self):
        (self.core / "packages/tool-esptoolpy").unlink()
        tool = self.core / "packages/tool-esptoolpy/esptool"
        tool.mkdir(parents=True)
        (tool / "bin_image.py").write_text("")
        (tool / "__init__.py").write_text("import not_a_real_dependency_xyz\n")
        for name in [m for m in sys.modules if m == "esptool" or m.startswith("esptool.")]:
            del sys.modules[name]
        self.addCleanup(lambda: [sys.modules.pop(m) for m in list(sys.modules)
                                 if m == "esptool" or m.startswith("esptool.")])
        with self.assertRaises(ValueError) as caught:
            self.package()
        message = str(caught.exception)
        self.assertIn("not_a_real_dependency_xyz", message)
        self.assertIn("site-packages", message)

    def test_duplicate_boards_rejected(self):
        with self.assertRaisesRegex(ValueError, "Duplicate board"):
            self.package(envs=["sticks3", "sticks3"])
        with self.assertRaisesRegex(ValueError, "Duplicate board"):
            self.package(envs=["sticks3", "sticks3-macro"])
        self.assertFalse(self.output.exists())

    def test_classic_bootloader_fits_below_partition_table(self):
        index = self.package()
        boot = next(p for p in self.board(index, "esp32")["parts"] if p["path"].endswith("bootloader.bin"))
        self.assertEqual(boot["offset"], 0x1000)
        self.assertLessEqual(boot["offset"] + boot["size"], 0x8000)

    def test_bootloader_reaching_nvs_rejected_on_classic_esp32(self):
        # 0x1000 + 0x8100 crosses the partition table and runs into NVS at 0x9000.
        path = self.root / ".pio/build/esp32/bootloader.bin"
        path.write_bytes(path.read_bytes() + b"\xff" * 0x8100)
        with self.assertRaisesRegex(ValueError, "overlaps NVS"):
            self.package()


if __name__ == "__main__":
    unittest.main()
