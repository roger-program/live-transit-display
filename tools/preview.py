"""Compile and run the real C++ renderer; export PNGs and a local HTML contact sheet."""
import html
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "artifacts" / "preview"
OUT.mkdir(parents=True, exist_ok=True)
compiler = shutil.which(os.environ.get("CXX", "c++"))
if not compiler:
    raise SystemExit("Install a C++17 compiler")
arduinojson = ROOT / ".pio/libdeps/native/ArduinoJson/src"
if not arduinojson.exists():
    raise SystemExit("Run pio test -e native first to install ArduinoJson")
sources = sorted((ROOT / "lib/transit/src").glob("*.cpp"))
binary = OUT / "render-preview"
subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + str(arduinojson),
                "-I" + str(ROOT / "lib/transit/src"), str(ROOT / "tools/render_preview.cpp"),
                *map(str, sources), "-o", str(binary)], check=True)
for fixture in ["feed-v1", "states"]:
    subprocess.run([str(binary), str(ROOT / f"fixtures/{fixture}.json"), str(OUT / fixture)], check=True)

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

entries = []
for ppm in sorted(OUT.glob("*/*.ppm")):
    magic, size, maximum, pixels = ppm.read_bytes().split(b"\n", 3)
    assert magic == b"P6" and maximum == b"255"
    width, height = map(int, size.split())
    scanlines = b"".join(b"\0" + pixels[row * width * 3:(row + 1) * width * 3] for row in range(height))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))
    target = ppm.with_suffix(".png")
    target.write_bytes(png)
    label = f"{ppm.parent.name}/{ppm.stem}"
    entries.append(f'<figure><img src="{target.relative_to(OUT)}" width="{width*4}" height="{height*4}"><figcaption>{html.escape(label)}</figcaption></figure>')
(OUT / "index.html").write_text('<!doctype html><meta charset="utf-8"><title>Firmware raster fixtures</title>'
    '<style>body{background:#202020;color:white;font:14px sans-serif}main{display:flex;flex-wrap:wrap;gap:12px}figure{margin:0}img{image-rendering:pixelated}</style>'
    '<h1>Firmware raster fixtures</h1><p>Historical/synthetic data; fixed clock at generatedAt. Phases 0, 1500 and 3150 ms.</p><main>'
    + "".join(entries) + "</main>")
print(f"Preview: {OUT / 'index.html'}")
