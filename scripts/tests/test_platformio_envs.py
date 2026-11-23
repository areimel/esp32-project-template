import configparser
import unittest

from support import ROOT, BOARDS
from list_envs import list_envs
from package_firmware import default_envs


class DefaultEnvsTests(unittest.TestCase):
    def test_default_envs_match_list_envs(self):
        parser = configparser.ConfigParser(interpolation=None, strict=False)
        parser.read(ROOT / "platformio.ini", encoding="utf-8")
        raw = parser.get("platformio", "default_envs", fallback="")
        defaults = raw.replace(",", " ").split()
        self.assertEqual(sorted(defaults), sorted(list_envs(ROOT / "platformio.ini")))
        self.assertNotIn("native", defaults)

    def test_default_envs_exclude_aliases_and_native(self):
        options = {
            "sticks3": {},
            "esp32": {},
            "esp32c3": {},
            "sticks3-macro": {"custom_board": "sticks3"},
            "native": {},
            "esp32s3": {"custom_board": "esp32s3"},
        }
        self.assertEqual(default_envs(options, BOARDS), ["sticks3", "esp32", "esp32c3", "esp32s3"])


if __name__ == "__main__":
    unittest.main()
