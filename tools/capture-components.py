"""Capture every catalog component at three zoom levels for visual review.

Requires Pillow: python -m pip install pillow
Run after building: python tools/capture-components.py --executable build/build.exe
"""

import argparse
from pathlib import Path
import subprocess

from PIL import Image, ImageStat


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", default="build/build.exe")
    parser.add_argument("--output", default="build/component-review/full")
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    executable = (root / options.executable).resolve()
    output = (root / options.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    listed = subprocess.check_output([str(executable), "--list-components"], cwd=root, text=True)
    components = [line.split("|", 1)[0] for line in listed.splitlines()]
    for zoom in (1, 3, 7):
        for component in components:
            bmp = output / "capture.bmp"
            subprocess.run(
                [str(executable), f"--capture-component={component}@{zoom}", f"--capture-output={bmp}"],
                cwd=root,
                check=True,
                stdout=subprocess.DEVNULL,
            )
            with Image.open(bmp) as source:
                frame = source.convert("RGB")
            if max(ImageStat.Stat(frame).stddev) < 1:
                raise RuntimeError(f"Blank capture: {component} at {zoom}x")
            frame.save(output / f"{component}-{zoom}x.png", optimize=True)
        print(f"Captured {len(components)} components at {zoom}x", flush=True)
    (output / "capture.bmp").unlink(missing_ok=True)


if __name__ == "__main__":
    main()
