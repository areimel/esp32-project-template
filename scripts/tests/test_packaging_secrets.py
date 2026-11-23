"""Packaging must never publish an image that embeds secrets.yaml values or OTA."""
import contextlib
import io
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from packaging_support import SECRET, PackagingCase  # noqa: E402


class PackagingSecretTests(PackagingCase):
    def write_secrets(self, text):
        (self.root / "secrets.yaml").write_text(text)
        for env in ("sticks3", "esp32", "esp32c3"):
            self.make_build(env)

    def assert_secret_absent(self, pattern="*"):
        for path in self.output.parent.rglob(pattern):
            if path.is_file():
                self.assertNotIn(SECRET.encode(), path.read_bytes(), path)

    def test_secrets_never_reach_output(self):
        self.package()  # a clean earlier package must survive the refusal untouched
        before = (self.output / "index.json").read_bytes()
        self.write_secrets(f"ota_password: {SECRET}\nwifi_ssid: homenet-{SECRET}\nwifi_password: pw-{SECRET}\n")
        self.assertIn(SECRET.encode(), (self.root / ".pio/build/sticks3/firmware.bin").read_bytes())
        with self.assertRaises(ValueError) as caught:
            self.package()
        message = str(caught.exception)
        self.assertNotIn(SECRET, message)
        for expected in ("sticks3", "secrets.yaml", "ota_password", "wifi_ssid", "wifi_password",
                         "without secrets.yaml", "--allow-secrets"):
            self.assertIn(expected, message)
        self.assertEqual(before, (self.output / "index.json").read_bytes())
        self.assert_secret_absent()  # every packaged .bin and .json

    def test_ota_build_refused_with_reason(self):
        self.write_secrets(f"ota_password: {SECRET}\n")
        with self.assertRaisesRegex(ValueError, r"OTA is compiled in") as caught:
            self.package()
        self.assertIn("ota_password", str(caught.exception))
        self.assertFalse(self.output.exists())

    def test_wifi_credentials_alone_refused(self):
        config = dict(self.config, ota={"enabled": False})
        self.write_config(config)
        self.write_secrets(f"wifi_ssid: homenet-{SECRET}\n")
        with self.assertRaisesRegex(ValueError, "wifi_ssid") as caught:
            self.package()
        self.assertNotIn("OTA", str(caught.exception))
        self.assertFalse(self.output.exists())

    def test_empty_secret_values_do_not_refuse(self):
        self.write_secrets('wifi_ssid: ""\nwifi_password: ""\n')
        self.package()
        self.assertTrue((self.output / "index.json").is_file())

    def test_allow_secrets_packages_with_loud_warning(self):
        self.write_secrets(f"ota_password: {SECRET}\nwifi_ssid: homenet-{SECRET}\n")
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.package(allow_secrets=True)
        warning = err.getvalue()
        self.assertIn("WARNING", warning)
        self.assertIn("sticks3", warning)
        self.assertIn("do not publish", warning)
        self.assertNotIn(SECRET, warning)
        self.assert_secret_absent("*.json")  # manifests and index.json never carry secrets


if __name__ == "__main__":
    unittest.main()
