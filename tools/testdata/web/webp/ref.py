# Referência dos testes WebP: decodifica cada .webp com o libwebp do Pillow e grava <nome>.rgba
# (largura e altura em 4 bytes little-endian cada, depois RGBA não pré-multiplicado; animação: 1º quadro).
# uso: python3 -I ref.py <pasta dos .webp> <pasta de saída>
import os, sys
from PIL import Image
src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
for n in sorted(os.listdir(src)):
    if not n.endswith('.webp'):
        continue
    im = Image.open(os.path.join(src, n))
    im.seek(0)
    im = im.convert('RGBA')
    with open(os.path.join(dst, n[:-5] + '.rgba'), 'wb') as f:
        f.write(im.size[0].to_bytes(4, 'little') + im.size[1].to_bytes(4, 'little') + im.tobytes())
