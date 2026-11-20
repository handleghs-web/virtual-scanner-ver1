# Creates 3 simple A4 test pages (200 dpi) in C:\VirtualScan\in to test the virtual scanner
# with any TWAIN program (e.g. NAPS2). Requires: pip install pillow
import os
from PIL import Image, ImageDraw

OUT = r"C:\VirtualScan\in"
os.makedirs(OUT, exist_ok=True)
W, H = 1654, 2339   # A4 at 200 dpi
for n in (1, 2, 3):
    im = Image.new("L", (W, H), 255)
    d = ImageDraw.Draw(im)
    d.rectangle([60, 60, 200, 80], fill=0); d.rectangle([60, 60, 80, 200], fill=0)
    d.rectangle([W - 200, 60, W - 60, 80], fill=0); d.rectangle([W - 80, 60, W - 60, 200], fill=0)
    d.text((W // 2 - 60, H // 2), "TEST PAGE %d" % n, fill=0)
    tmp = os.path.join(OUT, "%04d_test.tmp" % n)
    im.save(tmp, format="TIFF", dpi=(200, 200), compression="tiff_lzw")
    os.replace(tmp, os.path.join(OUT, "%04d_test.tif" % n))   # write as .tmp, then rename
print("done:", OUT)
