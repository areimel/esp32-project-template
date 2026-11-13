"""Shared building blocks: YAML loading, validation helpers, section loading."""
import importlib.util
import json
from dataclasses import dataclass, field
from pathlib import Path

try:
    import yaml
except ImportError as exc:
    raise SystemExit(
        "Missing PyYAML. Run: python -m pip install -r requirements.txt "
        "using the Python environment that runs PlatformIO."
    ) from exc

SECRET_KEYS = ("ota_password", "wifi_ssid", "wifi_password")


class ConfigError(ValueError):
    """A configuration problem, reported with the path of the offending key."""

    def __init__(self, path, msg):
        self.path, self.msg = path, msg
        super().__init__(f"{path}: {msg}" if path else msg)


@dataclass
class Context:
    board_id: str
    board: dict
    secrets: dict = field(default_factory=dict)
    warnings: list = field(default_factory=list)
    features: dict = field(default_factory=dict)  # final FEATURE_* flags, set by render


class UniqueKeyLoader(yaml.SafeLoader):
    pass


def _mapping(loader, node, deep=False):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if not isinstance(key, str):
            raise ConfigError("", "YAML mapping keys must be strings")
        if key in result:
            raise ConfigError("", f"Duplicate YAML field: {key}")
        result[key] = loader.construct_object(value_node, deep=deep)
    return result


UniqueKeyLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _mapping)


def load_yaml(path):
    try:
        return yaml.load(Path(path).read_text(encoding="utf-8"), Loader=UniqueKeyLoader)
    except yaml.YAMLError as exc:
        raise ConfigError(str(path), f"malformed YAML: {exc}") from exc


def fields(value, required, path, optional=()):
    """Check a mapping has all `required` keys and only known keys; return it."""
    if not isinstance(value, dict):
        raise ConfigError(path, "must be a mapping")
    for key in sorted(value.keys() - set(required) - set(optional)):
        raise ConfigError(f"{path}.{key}" if path else key, "unknown field")
    missing = sorted(set(required) - value.keys())
    if missing:
        raise ConfigError(path, f"missing fields {missing}")
    return value


def integer(value, low, high, path):
    if type(value) is not int or not low <= value <= high:
        raise ConfigError(path, f"must be an integer between {low} and {high}")
    return value


def boolean(value, path):
    if type(value) is not bool:
        raise ConfigError(path, "must be true or false")
    return value


def is_printable(text):
    return all(0x20 <= ord(c) <= 0x7E for c in text)


def cstr(text):
    # json.dumps gives C-compatible escapes; escape '?' so trigraphs cannot form.
    return json.dumps(text, ensure_ascii=False).replace("?", "\\?")


def write_if_changed(path, text):
    """Write only when content differs, so unchanged builds keep their mtime."""
    path = Path(path)
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return True


def load_secrets(path):
    """Read the optional gitignored secrets file; unknown keys are an error."""
    path = Path(path)
    if not path.exists():
        return {}
    secrets = load_yaml(path) or {}
    fields(secrets, [], "secrets.yaml", SECRET_KEYS)
    for key, value in secrets.items():
        if not isinstance(value, str):
            raise ConfigError(f"secrets.yaml.{key}", "must be a string")
    return secrets


def _load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_sections(data, base=Path(".")):
    """Built-in section modules plus the optional `schema:` module (relative to `base`)."""
    from config import sections

    modules = list(sections.BUILTIN)
    schema = data.get("schema") if isinstance(data, dict) else None
    if schema is not None:
        if not isinstance(schema, str):
            raise ConfigError("schema", "must be a path to a .py file")
        path = Path(base) / schema
        if not path.is_file():
            raise ConfigError("schema", f"{path} not found")
        modules.append(_load_module(path, "config_schema"))
    return modules
