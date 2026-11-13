"""Print a JSON array of every PlatformIO [env:*] name except `native`."""
import configparser
import json
import sys
from pathlib import Path


def list_envs(ini_path):
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.read(ini_path, encoding="utf-8")
    names = [s[4:] for s in parser.sections() if s.startswith("env:")]
    return [n for n in names if n != "native"]


if __name__ == "__main__":
    ini = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "platformio.ini"
    print(json.dumps(list_envs(ini)))
