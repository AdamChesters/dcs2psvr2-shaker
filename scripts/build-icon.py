"""Convert the original PNG master into a multi-size Windows icon.

Requires Pillow: python -m pip install Pillow
Run from any directory: python scripts/build-icon.py
"""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent.parent
sizes = [(size, size) for size in (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)]
with Image.open(root / "assets/dcs2psvr2-shaker.png") as master:
    master.convert("RGBA").save(root / "assets/dcs2psvr2-shaker.ico", format="ICO", sizes=sizes)
with Image.open(root / "assets/dcs2psvr2-shaker.ico") as icon:
    assert icon.ico.sizes() == set(sizes)
print("Generated Windows icon sizes: " + ", ".join(str(size[0]) for size in sizes))
