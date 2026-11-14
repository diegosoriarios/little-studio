#!/usr/bin/env python3
"""Regenerate Lite Studio raster icons from packaging/lite-studio.svg.

Requires ImageMagick's `magick` command. Writes the hicolor PNG sizes to
packaging/icons/ and the macOS bundle icon packaging/macos/LiteStudio.icns.
"""

from pathlib import Path
import shutil
import struct
import subprocess
import sys

PACKAGING = Path(__file__).resolve().parent
SVG = PACKAGING / "lite-studio.svg"
PNG_DIR = PACKAGING / "icons"
ICNS = PACKAGING / "macos" / "LiteStudio.icns"
SIZES = (16, 32, 48, 64, 128, 256, 512, 1024)

# PNG-based ICNS element types; 48 has no corresponding slot.
ICNS_TYPES = {
    16: b"icp4",
    32: b"icp5",
    64: b"icp6",
    128: b"ic07",
    256: b"ic08",
    512: b"ic09",
    1024: b"ic10",
}


def render_pngs(magick):
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    for size in SIZES:
        target = PNG_DIR / f"lite-studio-{size}.png"
        subprocess.run(
            [magick, "-background", "none", str(SVG),
             "-resize", f"{size}x{size}", str(target)],
            check=True,
        )
        print(f"wrote {target.relative_to(PACKAGING.parent)}")


def write_icns():
    entries = []
    for size, code in ICNS_TYPES.items():
        data = (PNG_DIR / f"lite-studio-{size}.png").read_bytes()
        entries.append(code + struct.pack(">I", len(data) + 8) + data)
    body = b"".join(entries)
    ICNS.write_bytes(b"icns" + struct.pack(">I", len(body) + 8) + body)
    print(f"wrote {ICNS.relative_to(PACKAGING.parent)}")


def main():
    magick = shutil.which("magick") or shutil.which("convert")
    if not magick:
        sys.exit("error: ImageMagick (magick or convert) not found")
    render_pngs(magick)
    write_icns()


if __name__ == "__main__":
    main()
