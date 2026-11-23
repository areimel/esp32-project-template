import configparser
import unittest

from support import ROOT
from list_envs import list_envs


class DefaultEnvsTests(unittest.TestCase):
    def test_default_envs_match_list_envs(self):
        parser = configparser.ConfigParser(interpolation=None, strict=False)
        parser.read(ROOT / "platformio.ini", encoding="utf-8")
        raw = parser.get("platformio", "default_envs", fallback="")
        defaults = raw.replace(",", " ").split()
        self.assertEqual(sorted(defaults), sorted(list_envs(ROOT / "platformio.ini")))
        self.assertNotIn("native", defaults)


if __name__ == "__main__":
    unittest.main()
