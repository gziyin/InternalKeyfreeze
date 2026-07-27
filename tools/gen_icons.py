import os, sys, math
from PIL import Image, ImageEnhance, ImageDraw

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


def frozen(img):
    im = img.copy()
    im = ImageEnhance.Color(im).enhance(0.45)
    im = ImageEnhance.Brightness(im).enhance(0.92)
    ice = Image.new('RGBA', im.size, (110, 190, 250, 60))
    im = Image.alpha_composite(im, ice)
    d = ImageDraw.Draw(im)
    cx = cy = im.size[0] // 2
    R = int(im.size[0] * 0.16)
    white = (255, 255, 255, 210)
    w = max(2, int(im.size[0] * 0.012))
    for i in range(6):
        ang = math.radians(i * 60)
        x2 = cx + R * math.cos(ang)
        y2 = cy + R * math.sin(ang)
        d.line([(cx, cy), (x2, y2)], fill=white, width=w)
        bx = cx + R * 0.62 * math.cos(ang)
        by = cy + R * 0.62 * math.sin(ang)
        for sign in (-1, 1):
            a2 = math.radians(i * 60 + sign * 32)
            ex = bx + R * 0.30 * math.cos(a2)
            ey = by + R * 0.30 * math.sin(a2)
            d.line([(bx, by), (ex, ey)], fill=white, width=w)
    return im


app_sizes = [16, 24, 32, 48, 64, 128, 256]
save_ico(master, 'app-icon.ico', app_sizes)
tray_sizes = [16, 20, 24, 32, 48]
save_ico(master, 'tray-enabled.ico', tray_sizes)
save_ico(frozen(rz(master, 256)), 'tray-frozen.ico', tray_sizes)
rz(master, 256).save(os.path.join(ASSETS, 'icon-256.png'))
rz(master, 512).save(os.path.join(ASSETS, 'icon-512.png'))
print('readme pngs done')
