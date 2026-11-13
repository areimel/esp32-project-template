"""ota: over-the-air updates. Compiled in only when a password exists in secrets.yaml."""
from config.core import boolean, cstr, fields

KEY, REQUIRED = "ota", False
DEFAULT = {"enabled": False}
INCLUDES = []


def validate(value, ctx):
    value = fields(value, [], KEY, ["enabled"])
    return {"enabled": boolean(value.get("enabled", False), "ota.enabled")}


def features(cfg, ctx):
    # The render step also requires FEATURE_WIFI before keeping this on.
    if not cfg["enabled"]:
        return {"FEATURE_OTA": False}
    if not ctx.secrets.get("ota_password"):
        ctx.warnings.append("OTA disabled: no ota_password in secrets.yaml")
        return {"FEATURE_OTA": False}
    return {"FEATURE_OTA": True}


def emit(cfg, ctx):
    password = ctx.secrets.get("ota_password")
    if not (cfg["enabled"] and password):
        return []
    return ["#if FEATURE_OTA", f"constexpr const char* OTA_PASSWORD = {cstr(password)};", "#endif"]
