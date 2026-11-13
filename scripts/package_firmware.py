"""Package each board env's build as ESP Web Tools manifests plus firmware/index.json."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))

from config.boards import firmware_version, load_boards  # noqa: E402
from config.core import ConfigError, Context, load_secrets, load_yaml  # noqa: E402
from firmware_images import (  # noqa: E402
    BOOT_APP0_SIZE, PARTITIONS_OFFSET, bootloader_offset, check_parts, flash_layout,
    image_parser, parse_partitions, validate_image,
)
from generate_config import render  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_VERSION = 2
IMPROV_WAIT_SECONDS = 15


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def pio_env_options(project):
    """Return {env: {option: value}} from `pio project config --json-output`."""
    pio = shutil.which("pio") or str(Path(sys.executable).parent / "pio")
    raw = subprocess.check_output([pio, "project", "config", "--json-output"], cwd=project, text=True)
    sections = {name: dict(items) for name, items in json.loads(raw)}
    shared = sections.get("env", {})
    return {name[4:]: {**shared, **opts} for name, opts in sections.items() if name.startswith("env:")}


def default_envs(options, boards):
    """Envs whose board id equals the env name (so aliases like sticks3-macro are skipped)."""
    return [env for env, opts in options.items()
            if env in boards and opts.get("custom_board", env) == env]


def resolve_boot_app0(core):
    """Find boot_app0.bin in whichever framework package (pioarduino or stock) provides it."""
    for found in sorted(Path(core).glob("packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin")):
        return found
    raise ValueError("boot_app0.bin not found in the PlatformIO Arduino framework package")


def board_build(project, env, opts, boards, version, core):
    """Validate one env's build; returns (board record, config, config bytes, {file: bytes})."""
    board_id = opts.get("custom_board", env)
    if board_id not in boards:
        raise ValueError(f"{env}: unknown board {board_id!r}")
    board = boards[board_id]
    chip = board["chip"]
    config_path = project / opts.get("custom_config", "config.yaml")
    config_bytes = config_path.read_bytes()
    config = load_yaml(config_path)
    ctx = Context(board_id, board, load_secrets(project / "secrets.yaml"))
    try:
        expected, features = render(config, ctx, config_path.parent)
    except ConfigError as exc:
        raise ValueError(f"{env}: invalid config: {exc}") from exc
    build = project / ".pio/build" / env
    header = build / "generated/generated_config.h"
    if not header.is_file() or header.read_text(encoding="utf-8") != expected:
        raise ValueError(f"{env}: generated configuration is stale; rebuild firmware")
    if (build / "firmware.bin").stat().st_mtime_ns < header.stat().st_mtime_ns:
        raise ValueError(f"{env}: firmware predates generated configuration; rebuild firmware")
    blobs = {name: (build / name).read_bytes()
             for name in ("bootloader.bin", "partitions.bin", "firmware.bin")}
    blobs["boot_app0.bin"] = resolve_boot_app0(core).read_bytes()
    nvs, otadata, app_offset, app_size = flash_layout(parse_partitions(blobs["partitions.bin"]))
    if len(blobs["boot_app0.bin"]) != BOOT_APP0_SIZE:
        raise ValueError(f"{env}: unexpected boot_app0.bin size")
    if len(blobs["firmware.bin"]) > app_size:
        raise ValueError(f"{env}: firmware.bin exceeds the app partition")
    offsets = {"bootloader.bin": bootloader_offset(chip), "partitions.bin": PARTITIONS_OFFSET,
               "boot_app0.bin": otadata, "firmware.bin": app_offset}
    check_parts([(f"{env}/{n}", offsets[n], len(blobs[n])) for n in offsets], nvs)
    parser = image_parser(core, chip)
    for name in ("bootloader.bin", "firmware.bin"):
        validate_image(blobs[name], f"{env}/{name}", parser, chip)
    wifi = bool(features.get("FEATURE_WIFI"))
    project_name = config["project"]["name"]
    manifest = {"name": f"{project_name} · {board['label']}", "version": version,
                "new_install_prompt_erase": True,
                "new_install_improv_wait_time": IMPROV_WAIT_SECONDS if wifi else 0,
                "builds": [{"chipFamily": chip,
                            "parts": [{"path": n, "offset": offsets[n]} for n in offsets]}]}
    record = {"id": board_id, "label": board["label"], "chip": chip,
              "buttons": board.get("buttons", len(board.get("button_pins", []))),
              "display": bool(board.get("display")), "battery": bool(board.get("battery")),
              "features": sorted(k[8:].lower() for k, on in features.items() if on),
              "manifest": f"{env}/manifest.json",
              "parts": [{"path": f"{env}/{n}", "offset": offsets[n], "size": len(blobs[n]),
                         "sha256": sha256(blobs[n])} for n in offsets]}
    return record, config, config_bytes, blobs, manifest


def package(project, output, version=None, commit=None, envs=None, platformio_dir=None,
            env_options=None):
    project, output = Path(project), Path(output)
    boards = load_boards(project / "boards/boards.yaml")
    options = env_options if env_options is not None else pio_env_options(project)
    envs = envs or default_envs(options, boards)
    if not envs:
        raise ValueError("No board envs to package")
    if version is None:
        version = firmware_version(project)
    elif not re.fullmatch(r"v?\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?", version):
        raise ValueError("Version must be semantic version, e.g. v1.0.0")
    if commit is None:
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=project, text=True).strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Commit must be a full lowercase Git SHA")
    core = Path(platformio_dir or os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    results = [board_build(project, env, options.get(env, {}), boards, version, core) for env in envs]
    config, config_bytes = results[0][1], results[0][2]
    if any(sha256(r[2]) != sha256(config_bytes) for r in results):
        raise ValueError("Boards use different config files; index.json holds one config")
    index = {"schemaVersion": SCHEMA_VERSION, "version": version, "commit": commit,
             "configSha256": sha256(config_bytes),
             "project": {"name": config["project"]["name"],
                         "tagline": config["project"].get("tagline", ""),
                         "description": config["project"].get("description", "")},
             "config": config, "boards": [r[0] for r in results]}
    # Stage next to the output so the final rename is atomic; any failure above wrote nothing.
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".firmware-", dir=output.parent))
    try:
        for env, (_, _, _, blobs, manifest) in zip(envs, results):
            (stage / env).mkdir()
            for name, data in blobs.items():
                (stage / env / name).write_bytes(data)
            (stage / env / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        (stage / "index.json").write_text(json.dumps(index, indent=2) + "\n")
        if output.exists():
            shutil.rmtree(output)
        os.replace(stage, output)
    except BaseException:
        shutil.rmtree(stage, ignore_errors=True)
        raise
    return index


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version")
    parser.add_argument("--commit")
    parser.add_argument("--env", action="append", dest="envs")
    args = parser.parse_args()
    try:
        index = package(args.project, args.output, args.version, args.commit, args.envs)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.exit(1, f"Packaging error: {exc}\n")
    for board in index["boards"]:
        print(f"{board['id']}: {board['chip']}, {len(board['parts'])} parts -> {board['manifest']}")
    print(f"Packaged {index['version']} ({index['commit'][:12]}) to {args.output}")


if __name__ == "__main__":
    main()
