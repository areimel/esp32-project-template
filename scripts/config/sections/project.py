"""project: name shown in the UI, serial log and installer page."""
from config.core import ConfigError, cstr, fields, is_printable

KEY, REQUIRED, DEFAULT = "project", True, None
MAX_NAME = 32


def _text(value, path, limit):
    if not isinstance(value, str) or len(value) > limit:
        raise ConfigError(path, f"must be text of at most {limit} characters")
    return value


def validate(value, ctx):
    value = fields(value, ["name"], KEY, ["tagline", "description"])
    name = value["name"]
    if not isinstance(name, str) or not 1 <= len(name.strip()) <= MAX_NAME or not is_printable(name):
        raise ConfigError("project.name", f"must be 1-{MAX_NAME} printable ASCII characters")
    return {"name": name.strip(),
            "tagline": _text(value.get("tagline", ""), "project.tagline", 120),
            "description": _text(value.get("description", ""), "project.description", 500)}


def emit(cfg, ctx):
    return [f"constexpr const char* PROJECT_NAME = {cstr(cfg['name'])};"]
