# Gera os WebP de teste (codificados pelo libwebp do Pillow). As referências saem do ref.py na hora do teste.
# uso: python3 -I tools/testdata/web/webp/gerar.py   (só para regenerar; os arquivos já estão no repositório)
import os, math, random
from PIL import Image
D = os.path.dirname(os.path.abspath(__file__))
random.seed(7)

def foto(w, h, alpha=False):
    im = Image.new('RGBA', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            r = int(127 + 120 * math.sin(x / 7.0 + y / 13.0))
            g = int(127 + 120 * math.cos(x / 11.0 - y / 5.0))
            b = (x * 3 + y * 5 + random.randint(0, 40)) & 255
            a = 255 if not alpha else int(255 * (x / max(1, w - 1)))
            px[x, y] = (r, g, b, a)
    return im

def paleta(w, h, n):
    cores = [(random.randint(0, 255), random.randint(0, 255), random.randint(0, 255), 255) for _ in range(n)]
    im = Image.new('RGBA', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = cores[((x // 3) + (y // 2)) % n]
    return im

casos = {
    'll_foto': (foto(67, 41), dict(lossless=True)),
    'll_alfa': (foto(33, 29, alpha=True), dict(lossless=True, exact=True)),
    'll_pal2': (paleta(50, 20, 2), dict(lossless=True)),
    'll_pal4': (paleta(37, 19, 4), dict(lossless=True)),
    'll_pal16': (paleta(41, 23, 13), dict(lossless=True)),
    'll_pal200': (paleta(64, 64, 200), dict(lossless=True)),
    'll_lento': (foto(130, 90), dict(lossless=True, method=6, quality=100)),
    'ly_q90': (foto(67, 41), dict(quality=90)),
    'ly_q50': (foto(160, 120), dict(quality=50)),
    'ly_q5': (foto(45, 37), dict(quality=5)),
    'ly_alfa': (foto(50, 34, alpha=True), dict(quality=75)),
    'ly_m6': (foto(96, 80), dict(quality=70, method=6)),
}
for nome, (im, kw) in casos.items():
    if 'ly_alfa' != nome and not kw.get('lossless') :
        im = im.convert('RGB')
    caminho = os.path.join(D, nome + '.webp')
    im.save(caminho, 'WEBP', **kw)
# animação: dois quadros
q = [foto(40, 30), paleta(40, 30, 5)]
q[0].save(os.path.join(D, 'anim.webp'), 'WEBP', save_all=True, append_images=[q[1]], duration=100, lossless=True)
print('ok', len(casos) + 1)
