"""Validate config.yaml and compile it into the firmware's generated header.

Sections live in scripts/config/sections (plus an optional `schema:` module), so
firmware never parses key names, text, or labels.
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from config.boards import load_boards, select_board  # noqa: E402
from config.core import (  # noqa: E402
    ConfigError, Context, fields, load_secrets, load_sections, load_yaml, write_if_changed,
)
import yaml  # noqa: E402

HEADER = "// Generated from config.yaml. Edit YAML, not this file."


def render(data, ctx, base=Path(".")):
    """Return (header text, {FEATURE_*: bool}) for validated `data`."""
    modules = load_sections(data, base)
    fields(data, [], "", [m.KEY for m in modules] + ["schema"])
    features, validated = {}, []
    for module in modules:
        if module.KEY not in data and module.REQUIRED:
            raise ConfigError(module.KEY, "missing required section")
        cfg = module.validate(data.get(module.KEY, module.DEFAULT), ctx)
        validated.append((module, cfg))
        features.update(getattr(module, "features", lambda c, x: {})(cfg, ctx))
    if features.get("FEATURE_OTA") and not features.get("FEATURE_WIFI"):
        features["FEATURE_OTA"] = False
        ctx.warnings.append("OTA disabled: wifi.enabled is false")
    ctx.features = features
    includes = sorted({inc for m, _ in validated for inc in m.INCLUDES})
    lines = [HEADER, "#pragma once", *[f"#include {inc}" for inc in includes], "",
             "namespace AppConfig {"]
    for module, cfg in validated:
        lines += module.emit(cfg, ctx)
    lines += ["} // namespace AppConfig", ""]
    return "\n".join(lines), features


def generate(source, destination, ctx):
    """Render `source` into `destination`; returns the feature flags."""
    source = Path(source)
    try:
        header, features = render(load_yaml(source), ctx, source.parent)
    except (ConfigError, yaml.YAMLError) as exc:
        # load_yaml errors already name the file; print the path once.
        detail = exc.msg if isinstance(exc, ConfigError) and exc.path == str(source) else exc
        raise ConfigError("", f"Invalid {source}: {detail}") from exc
    write_if_changed(destination, header)
    return features


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="config.yaml")
    parser.add_argument("--board", required=True)
    parser.add_argument("--secrets", default="secrets.yaml")
    parser.add_argument("--output", default="build/generated/generated_config.h")
    args = parser.parse_args()
    try:
        ctx = Context(args.board, select_board(load_boards(Path("boards/boards.yaml")), args.board),
                      load_secrets(args.secrets))
        generate(args.config, args.output, ctx)
    except (OSError, ConfigError) as exc:
        parser.exit(1, f"Configuration error: {exc}\n")
    for warning in ctx.warnings:
        print(f"warning: {warning}")
