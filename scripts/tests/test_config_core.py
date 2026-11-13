import os
import tempfile
import unittest
from pathlib import Path

import yaml

from support import make_ctx, default_config
from config.core import ConfigError, UniqueKeyLoader, cstr, fields, integer, load_secrets, write_if_changed
from generate_config import render


class CoreTests(unittest.TestCase):
    def test_duplicate_keys_rejected(self):
        with self.assertRaises(ConfigError):
            yaml.load("a: 1\na: 2\n", Loader=UniqueKeyLoader)

    def test_non_string_keys_rejected(self):
        with self.assertRaises(ConfigError):
            yaml.load("1: a\n", Loader=UniqueKeyLoader)

    def test_fields_reports_unknown_with_path(self):
        with self.assertRaises(ConfigError) as caught:
            fields({"nmae": "x"}, ["name"], "ble", optional=())
        self.assertIn("ble.nmae", str(caught.exception))

    def test_integer_bounds(self):
        self.assertEqual(integer(5, 5, 100, "x"), 5)
        for bad in (4, 101, True, "5"):
            with self.assertRaises(ConfigError):
                integer(bad, 5, 100, "x")

    def test_cstr_escapes(self):
        self.assertEqual(cstr('a"b'), '"a\\"b"')
        self.assertEqual(cstr("??="), '"\\?\\?="')

    def test_write_if_changed_keeps_mtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sub" / "out.h"
            self.assertTrue(write_if_changed(path, "x"))
            os.utime(path, (1000, 1000))
            self.assertFalse(write_if_changed(path, "x"))
            self.assertEqual(path.stat().st_mtime, 1000)
            self.assertTrue(write_if_changed(path, "y"))

    def test_unknown_top_level_key(self):
        data = default_config()
        data["bluetoth"] = {}
        with self.assertRaises(ConfigError) as caught:
            render(data, make_ctx())
        self.assertIn("bluetoth", str(caught.exception))

    def test_missing_required_section(self):
        data = default_config()
        del data["ble"]
        with self.assertRaises(ConfigError) as caught:
            render(data, make_ctx())
        self.assertIn("ble", str(caught.exception))

    def test_secrets_rejects_unknown_keys(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "secrets.yaml"
            path.write_text("ota_password: x\nother: y\n", encoding="utf-8")
            with self.assertRaises(ConfigError):
                load_secrets(path)
            path.write_text("ota_password: x\nwifi_ssid: s\nwifi_password: p\n", encoding="utf-8")
            self.assertEqual(load_secrets(path)["wifi_ssid"], "s")
            self.assertEqual(load_secrets(Path(tmp) / "missing.yaml"), {})

    def test_schema_hook(self):
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, "extra.py").write_text(
                "KEY='extra'\nREQUIRED=False\nDEFAULT=None\nINCLUDES=[]\n"
                "def validate(v, ctx): return v\n"
                "def emit(cfg, ctx): return ['constexpr int EXTRA = 7;']\n", encoding="utf-8")
            data = default_config()
            data["schema"] = "extra.py"
            data["extra"] = 1
            header, _ = render(data, make_ctx(), base=Path(tmp))
            self.assertIn("EXTRA = 7", header)
            with self.assertRaises(ConfigError):
                render(data, make_ctx(), base=Path(tmp) / "nowhere")


if __name__ == "__main__":
    unittest.main()
