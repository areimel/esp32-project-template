"""display: on-board screen brightness; the feature needs board support too."""
from config.core import boolean, fields, integer

KEY, REQUIRED = "display", False
DEFAULT = {"enabled": False, "brightness": 64}


def validate(value, ctx):
    value = fields(value, [], KEY, ["enabled", "brightness"])
    return {"enabled": boolean(value.get("enabled", False), "display.enabled"),
            "brightness": integer(value.get("brightness", 64), 1, 255, "display.brightness")}


def features(cfg, ctx):
    return {"FEATURE_DISPLAY": cfg["enabled"] and bool(ctx.board.get("display"))}


def emit(cfg, ctx):
    return [f"constexpr uint8_t DISPLAY_BRIGHTNESS = {cfg['brightness']};"]
