import os, sys, math
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
SITE = os.environ.get('IKF_SITE') or os.path.join(ROOT, '.workbuddy', 'iconbuild', 'site')
ASSETS = os.environ.get('IKF_ASSETS') or os.path.join(ROOT, 'assets')
sys.path.insert(0, SITE)

master = Image.open(os.path.join(ASSETS, 'icon-master.png')).convert('RGBA')


def rz(img, s):
    return img.resize((s, s), Image.LANCZOS)


def save_ico(src_large, name, sizes):
    path = os.path.join(ASSETS, name)
    src_large.save(path, format='ICO', sizes=[(s, s) for s in sizes])
    print(name, sizes)


def prohibition(size):
    """Standard 'no entry' / prohibition sign: a bold red ring with a red
    diagonal slash, on a transparent background. Drawn 4x supersampled then
    downsampled with LANCZOS so the edges stay crisp at 16px tray size."""
    import math
    S = size * 4
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = cy = S / 2
    red = (214, 38, 38, 255)
    ring_r = S * 0.42
    thickness = max(4, int(S * 0.12))
    # Red ring
    d.ellipse([cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r],
              outline=red, width=thickness)
    # Diagonal slash (bottom-left -> top-right), endpoints just inside the ring
    r = ring_r * 0.72
    p = r / math.sqrt(2)
    d.line([(cx - p, cy + p), (cx + p, cy - p)], fill=red, width=thickness)
    return img.resize((size, size), Image.LANCZOS)


app_sizes = [16, 24, 32, 48, 64, 128, 256]
save_ico(master, 'app-icon.ico', app_sizes)
tray_sizes = [16, 20, 24, 32, 48]
save_ico(master, 'tray-enabled.ico', tray_sizes)
save_ico(prohibition(256), 'tray-frozen.ico', tray_sizes)
rz(master, 256).save(os.path.join(ASSETS, 'icon-256.png'))
rz(master, 512).save(os.path.join(ASSETS, 'icon-512.png'))
print('readme pngs done')
