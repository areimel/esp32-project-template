import copy
import unittest

from support import make_ctx, default_config
from config.core import ConfigError
from generate_config import render


class SectionTests(unittest.TestCase):
    def setUp(self):
        self.data = default_config()

    def rejected(self, section, key, value, fragment=None):
        data = copy.deepcopy(self.data)
        data[section][key] = value
        with self.assertRaises(ConfigError) as caught:
            render(data, make_ctx())
        self.assertIn(f"{section}.{key}", str(caught.exception))
        if fragment:
            self.assertIn(fragment, str(caught.exception))

    def test_default_renders(self):
        header, features = render(self.data, make_ctx())
        self.assertIn("#pragma once", header)
        self.assertIn("namespace AppConfig {", header)
        self.assertIn('PROJECT_NAME = "ESP32 Starter"', header)
        self.assertIn('BLE_NAME = "ESP32 Starter"', header)
        self.assertIn("KEYSTROKE_MS = 15;", header)
        self.assertIn("DISPLAY_BRIGHTNESS = 64;", header)
        self.assertIn('WIFI_HOSTNAME = "esp32-starter"', header)
        self.assertIn('DEV_WIFI_SSID = ""', header)
        self.assertEqual(features["FEATURE_BLE_HID"], True)
        self.assertEqual(features["FEATURE_DISPLAY"], True)
        self.assertEqual(features["FEATURE_OTA"], False)  # no secrets

    def test_optional_sections_default(self):
        for key in ("wifi", "ota", "display", "gestures", "buttons"):
            del self.data[key]
        header, features = render(self.data, make_ctx())
        self.assertIn("LONG_PRESS_MS = 650;", header)
        self.assertIn("BUTTON_SLOTS", header)
        self.assertEqual(features["FEATURE_WIFI"], False)

    def test_bounds(self):
        self.rejected("project", "name", "")
        self.rejected("project", "name", "x" * 33)
        self.rejected("ble", "name", "é" * 15)  # 30 UTF-8 bytes
        self.rejected("ble", "name", "bad\x07")
        self.rejected("ble", "keystroke_ms", 4)
        self.rejected("ble", "keystroke_ms", 101)
        self.rejected("wifi", "hostname", "-bad")
        self.rejected("wifi", "hostname", "a" * 33)
        self.rejected("wifi", "hostname", "has_underscore")
        self.rejected("display", "brightness", 0)
        self.rejected("display", "brightness", 256)
        self.rejected("gestures", "debounce_ms", 0)
        self.rejected("gestures", "double_click_ms", 49)
        self.rejected("gestures", "long_press_ms", 99)

    def test_gesture_ordering_message_unchanged(self):
        self.data["gestures"] = {"debounce_ms": 100, "double_click_ms": 80, "long_press_ms": 650}
        with self.assertRaises(ConfigError) as caught:
            render(self.data, make_ctx())
        self.assertIn("Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms",
                      str(caught.exception))

    def test_display_feature_needs_board_display(self):
        _, features = render(self.data, make_ctx("esp32"))
        self.assertFalse(features["FEATURE_DISPLAY"])

    def test_disabled_features(self):
        self.data["ble"]["enabled"] = False
        self.data["wifi"]["enabled"] = False
        _, features = render(self.data, make_ctx(secrets={"ota_password": "pw"}))
        self.assertFalse(features["FEATURE_BLE_HID"])
        self.assertFalse(features["FEATURE_WIFI"])
        self.assertFalse(features["FEATURE_OTA"])  # OTA needs WiFi

    def test_ota_with_password(self):
        ctx = make_ctx(secrets={"ota_password": 'p"w'})
        header, features = render(self.data, ctx)
        self.assertTrue(features["FEATURE_OTA"])
        self.assertIn("#if FEATURE_OTA", header)
        self.assertIn('OTA_PASSWORD = "p\\"w"', header)

    def test_ota_without_password(self):
        ctx = make_ctx(secrets={})
        header, features = render(self.data, ctx)
        self.assertFalse(features["FEATURE_OTA"])
        self.assertNotIn("OTA_PASSWORD", header)
        self.assertIn("OTA disabled: no ota_password in secrets.yaml", ctx.warnings)

    def test_ota_forced_off_without_wifi_emits_no_password(self):
        self.data["wifi"]["enabled"] = False
        ctx = make_ctx(secrets={"ota_password": "pw"})
        header, features = render(self.data, ctx)
        self.assertFalse(features["FEATURE_OTA"])
        self.assertNotIn("OTA_PASSWORD", header)
        self.assertIn("OTA disabled: wifi.enabled is false", ctx.warnings)

    def test_dev_wifi_from_secrets(self):
        header, _ = render(self.data, make_ctx(secrets={"wifi_ssid": "net", "wifi_password": "pw"}))
        self.assertIn('DEV_WIFI_SSID = "net"', header)
        self.assertIn('DEV_WIFI_PASSWORD = "pw"', header)


if __name__ == "__main__":
    unittest.main()
