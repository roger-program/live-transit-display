"""Format owned C++ sources only; exclude local secrets and generated fixtures."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
tool = shutil.which("clang-format")
if not tool:
    raise SystemExit("Install requirements-dev.txt and activate the environment")
paths = sorted(path for folder in ["src", "lib/transit/src", "include", "test", "tools"]
               for path in (ROOT / folder).rglob("*") if path.suffix in {".cpp", ".h"}
               and path.name not in {"secrets.h", "fixture.h"})
flags = ["--dry-run", "--Werror"] if args.check else ["-i"]
subprocess.run([tool, *flags, *map(str, paths)], cwd=ROOT, check=True)
