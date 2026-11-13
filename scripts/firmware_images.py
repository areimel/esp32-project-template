"""Image, partition and flash-layout validation shared by the packager."""

import io
from pathlib import Path
import struct
import sys

SECTOR = 0x1000
PARTITIONS_OFFSET = 0x8000
BOOT_APP0_SIZE = 0x2000
# Bootloader offset by chip: classic ESP32 is the exception, all others use 0x0.
BOOTLOADER_OFFSETS = {"ESP32": 0x1000}
# One table maps a board's `chip` to the vendored esptool image class name.
IMAGE_CLASSES = {"ESP32": "ESP32FirmwareImage", "ESP32-S3": "ESP32S3FirmwareImage",
                 "ESP32-C3": "ESP32C3FirmwareImage"}


def bootloader_offset(chip):
    return BOOTLOADER_OFFSETS.get(chip, 0)


def image_parser(core, chip):
    """Return the esptool image class for `chip` from PlatformIO's vendored esptool."""
    if chip not in IMAGE_CLASSES:
        raise ValueError(f"Unsupported chip {chip!r}; known: {sorted(IMAGE_CLASSES)}")
    tool = Path(core) / "packages/tool-esptoolpy"
    if not (tool / "esptool/bin_image.py").is_file():
        raise ValueError("PlatformIO esptool is missing; build firmware first")
    # esptool v4 vendored dependencies in _contrib; v5 relies on PlatformIO's own
    # penv. Put the tool first and the dependency locations last so they never
    # shadow the project's packages.
    sys.path.insert(0, str(tool))
    for extra in [tool / "_contrib", *Path(core).glob("penv/lib/python*/site-packages"),
                  Path(core) / "penv/Lib/site-packages"]:
        if extra.is_dir() and str(extra) not in sys.path:
            sys.path.append(str(extra))
    try:
        from esptool import bin_image
        return getattr(bin_image, IMAGE_CLASSES[chip])
    except (ImportError, AttributeError) as exc:
        raise ValueError("Cannot load PlatformIO esptool; rebuild/install firmware tools") from exc


def validate_image(data, name, parser, chip):
    if len(data) < 24 or data[0] != 0xE9:
        raise ValueError(f"{name}: not an ESP image")
    if not 1 <= data[1] <= 16 or data[23] != 1:
        raise ValueError(f"{name}: expected segments and appended SHA-256 digest")
    try:
        image = parser(io.BytesIO(data))
    except Exception as exc:
        raise ValueError(f"{name}: malformed or truncated ESP image for {chip}: {exc}") from exc
    expected_id = parser.ROM_LOADER.IMAGE_CHIP_ID
    if struct.unpack_from("<H", data, 12)[0] != expected_id:
        raise ValueError(f"{name}: image chip ID does not match {chip}")
    # esptool parses these fields without rejecting checksum/digest mismatches.
    if image.checksum != image.calculate_checksum():
        raise ValueError(f"{name}: ESP segment checksum mismatch")
    if image.stored_digest != image.calc_digest or len(image.stored_digest) != 32:
        raise ValueError(f"{name}: ESP image SHA-256 mismatch")
    if len(data) != image.data_length + 32:
        raise ValueError(f"{name}: unexpected bytes after ESP image digest")


def parse_partitions(data):
    """Return {label: (type, subtype, offset, size)}, in table order."""
    table = {}
    for position in range(0, len(data) - 31, 32):
        magic, kind, subtype, offset, size, label, _flags = struct.unpack_from(
            "<HBBII16sI", data, position)
        if magic != 0x50AA:
            break
        name = label.rstrip(b"\0").decode("ascii")
        if name in table:
            raise ValueError(f"Duplicate partition label {name}")
        table[name] = (kind, subtype, offset, size)
    return table


def flash_layout(table):
    """Return (nvs, otadata offset, first app offset, first app size) from a partition table."""
    nvs = next(((o, s) for k, st, o, s in table.values() if k == 1 and st == 2), None)
    otadata = next((o for k, st, o, s in table.values() if k == 1 and st == 0), None)
    app = next(((o, s) for k, st, o, s in table.values() if k == 0), None)
    if nvs is None or otadata is None or app is None:
        raise ValueError("Partition table needs nvs, otadata and an app partition")
    return nvs, otadata, app[0], app[1]


def check_parts(parts, nvs):
    """`parts` is [(name, offset, size)]. No sector-rounded range may touch NVS or a neighbour."""
    nvs_start, nvs_end = nvs[0], nvs[0] + nvs[1]
    spans = []
    for name, offset, size in parts:
        if size <= 0:
            raise ValueError(f"{name}: empty")
        # Erase operations round to 4 KiB sectors.
        spans.append((name, offset, (offset + size + SECTOR - 1) & ~(SECTOR - 1)))
    for name, start, end in spans:
        if start < nvs_end and end > nvs_start:
            raise ValueError(f"{name}: overlaps NVS partition")
    ordered = sorted(spans, key=lambda s: s[1])
    for (a, _, a_end), (b, b_start, _) in zip(ordered, ordered[1:]):
        if a_end > b_start:
            raise ValueError(f"{a}: overlaps {b}")
