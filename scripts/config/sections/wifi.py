"""wifi: station hostname; dev credentials come only from secrets.yaml."""
import re

from config.core import ConfigError, boolean, cstr, fields

KEY, REQUIRED = "wifi", False
DEFAULT = {"enabled": False, "hostname": "esp32-device"}
LABEL = re.compile(r"^[A-Za-z0-9]([A-Za-z0-9-]{0,30}[A-Za-z0-9])?$")  # RFC 1123 label, 1-32


def validate(value, ctx):
    value = fields(value, [], KEY, ["enabled", "hostname"])
    hostname = value.get("hostname", DEFAULT["hostname"])
    if not isinstance(hostname, str) or not LABEL.match(hostname):
        raise ConfigError("wifi.hostname", "must be an RFC 1123 label of 1-32 letters, digits or hyphens")
    return {"enabled": boolean(value.get("enabled", False), "wifi.enabled"), "hostname": hostname}


def features(cfg, ctx):
    return {"FEATURE_WIFI": cfg["enabled"]}


def emit(cfg, ctx):
    return [f"constexpr const char* WIFI_HOSTNAME = {cstr(cfg['hostname'])};",
            f"constexpr const char* DEV_WIFI_SSID = {cstr(ctx.secrets.get('wifi_ssid', ''))};",
            f"constexpr const char* DEV_WIFI_PASSWORD = {cstr(ctx.secrets.get('wifi_password', ''))};"]
