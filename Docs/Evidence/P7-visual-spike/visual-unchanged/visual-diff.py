# P7 closure: the review frames after ADR-0033 against the approved (committed) frames.
# Mean absolute pixel difference per frame (0-255 scale) at 1280x720, and the share of pixels that differ by > 24.
import sys, glob, os
from PIL import Image, ImageChops
old_dir, new_dir = sys.argv[1], sys.argv[2]
rows = []
for old in sorted(glob.glob(os.path.join(old_dir, '*.jpg'))):
    new = os.path.join(new_dir, os.path.basename(old))
    if not os.path.exists(new):
        rows.append((os.path.basename(old), None, None)); continue
    a = Image.open(old).convert('L'); b = Image.open(new).convert('L').resize(a.size)
    d = ImageChops.difference(a, b)
    hist = d.histogram(); n = sum(hist)
    mean = sum(i * c for i, c in enumerate(hist)) / n
    big = sum(hist[25:]) / n
    rows.append((os.path.basename(old), mean, big))
for name, mean, big in rows:
    print(f"{name}: " + ("MISSING" if mean is None else f"mean abs diff {mean:.2f}, pixels differing > 24: {big*100:.2f}%"))
