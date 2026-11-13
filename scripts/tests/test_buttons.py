import unittest

from support import make_ctx, default_config
from config.core import ConfigError
from config.sections import keys
from generate_config import render


class ButtonTests(unittest.TestCase):
    def setUp(self):
        self.data = default_config()

    def rows(self, header):
        start = header.index("BUTTON_ACTIONS[BUTTON_SLOTS][3] = {")
        body = header[start:].split("};", 1)[0]
        return [line for line in body.splitlines()[1:] if line.strip()]

    def test_extra_buttons_dropped_with_warning(self):
        ctx = make_ctx("esp32")
        header, _ = render(self.data, ctx)
        self.assertIn("BUTTON_SLOTS = BOARD_BUTTON_COUNT", header)
        self.assertEqual(len(self.rows(header)), 1)
        self.assertIn("buttons[1] ignored: board esp32 has 1 button(s)", ctx.warnings)

    def test_two_button_board_keeps_both(self):
        ctx = make_ctx("sticks3")
        header, _ = render(self.data, ctx)
        self.assertEqual(len(self.rows(header)), 2)
        self.assertEqual(ctx.warnings, ["OTA disabled: no ota_password in secrets.yaml"])

    def test_missing_rows_padded_unassigned(self):
        self.data["buttons"] = [{"single": "A", "double": None, "long": None}]
        header, _ = render(self.data, make_ctx("sticks3"))
        rows = self.rows(header)
        self.assertEqual(len(rows), 2)
        self.assertIn('{"", 0x00, 0x00}', rows[1])

    def test_combo_encoding(self):
        self.data["buttons"] = [{"single": "CMD+SHIFT+4", "double": None,
                                 "long": {"keys": "ctrl+c", "label": "copy"}}]
        header, _ = render(self.data, make_ctx("esp32"))
        self.assertIn('{"", 0x0A, 0x21}', header)
        self.assertIn('{"COPY", 0x01, 0x06}', header)

    def test_invalid_actions(self):
        for bad in ({"single": "NOPE"}, {"single": {"keys": "A", "extra": 1}},
                    {"single": {"keys": "A", "label": ""}}, {"bogus": None},
                    {"single": {"text": "hello"}}):
            self.data["buttons"] = [bad]
            with self.assertRaises(ConfigError, msg=str(bad)):
                render(self.data, make_ctx())

    def test_error_path(self):
        self.data["buttons"] = [{"single": "NOPE"}]
        with self.assertRaises(ConfigError) as caught:
            render(self.data, make_ctx())
        self.assertIn("buttons[0].single", str(caught.exception))

    def test_parse_combo(self):
        self.assertEqual(keys.parse_combo("ctrl+c", "p"), (0x01, 0x06))
        self.assertEqual(keys.parse_combo(" Cmd + Shift + z ", "p"), (0x0A, 0x1D))
        self.assertEqual(keys.parse_combo(5, "p"), (0, 0x22))
        self.assertEqual(keys.parse_combo("SHIFT", "p"), (0x02, 0))
        for bad in ("", "A+B", "CTRL+CTRL+A", "A+CTRL", "FOO", "A+", 12, None):
            with self.assertRaises(ConfigError, msg=repr(bad)):
                keys.parse_combo(bad, "p")

    def test_key_tables(self):
        self.assertEqual(keys.KEYS["F24"], 0x73)
        self.assertEqual(keys.ASCII["A"], (0x02, 0x04))
        self.assertEqual(keys.ASCII["\n"], (0, 0x28))


if __name__ == "__main__":
    unittest.main()
