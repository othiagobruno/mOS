#!/usr/bin/env python3
"""Contornos vetoriais do Core Text (ct_outline.c) contra o FreeType do PIL: TrueType (DejaVu, Inter, JetBrains Mono)
e CFF/OpenType (Noto CJK). Para cada glifo compara a cobertura recortada à caixa de tinta (IoU). Compilação:
  F="-g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \\
     -Ikernel/include -Iuserland/libsystem"
  zig cc $F -DCT_HOST -c userland/libsystem/ct_outline.c -o /tmp/mariana-ct/ct_outline.o
  zig cc $F -c userland/libsystem/coregraphics.c -o /tmp/mariana-ct/coregraphics.o   (e cgpdf.c, pdf_core.c, cg_consumer.c)
  zig cc -std=gnu11 -O1 -w -D_DEFAULT_SOURCE -Iuserland/libsystem -Itools tools/ct_outline_host.c /tmp/mariana-ct/*.o \\
     userland/libsystem/cg_raster.c userland/libsystem/cgimage_crop.c -DCS_HOST userland/libsystem/colorsync.c -lm -lz \\
     -o /tmp/mariana-ct/cthost
  python3 tools/ct_outline_host.py /tmp/mariana-ct/cthost
"""
import subprocess, sys
from PIL import Image, ImageFont
HOST = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mariana-ct/cthost"
CASES = [("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "agQ&@é8"),
         ("assets/fonts/Inter-Regular.ttf", "Rgaß%"),
         ("root/mOS/Sistema/Biblioteca/Fontes/JetBrainsMono-Regular.ttf", "{w0"),
         ("root/mOS/Sistema/Biblioteca/Fontes/NotoSansJP-Regular.otf", "あ漢g"),
         ("root/mOS/Sistema/Biblioteca/Fontes/NotoSansArabic-Regular.ttf", "ع")]
SIZE = 320      # grande: o hinting do FreeType pesa pouco
fails = 0
def crop(im):
    bb = im.point(lambda v: 255 if v > 127 else 0).getbbox()
    return im.crop(bb) if bb else None
for path, chars in CASES:
    for ch in chars:
        out = "/tmp/mariana-ct/g.pgm"
        r = subprocess.run([HOST, path, str(ord(ch)), str(SIZE), out], capture_output=True, text=True)
        if r.returncode:
            print("FAIL", path.split("/")[-1], ch, r.stderr.strip()); fails += 1; continue
        mine = crop(Image.open(out))
        k = ImageFont.truetype(path, SIZE).getmask(ch)
        ref = crop(Image.frombytes("L", k.size, bytes(k)))
        if mine is None or ref is None:
            ok = mine is None and ref is None; iou = 1.0 if ok else 0.0
        else:
            w, h = max(mine.size[0], ref.size[0]) + 2, max(mine.size[1], ref.size[1]) + 2
            pb = Image.new("L", (w, h)); pb.paste(ref, (1, 1)); B = pb.point(lambda v: v > 127).tobytes()
            iou = 0
            for dx in (0, 1, 2):          # melhor alinhamento a ±1 px (borda de pixel do hinting)
                for dy in (0, 1, 2):
                    pa = Image.new("L", (w, h)); pa.paste(mine, (dx, dy)); A = pa.point(lambda v: v > 127).tobytes()
                    inter = sum(1 for i in range(len(A)) if A[i] and B[i]); uni = sum(1 for i in range(len(A)) if A[i] or B[i])
                    iou = max(iou, inter / uni if uni else 1.0)
            ok = iou > 0.965 and abs(mine.size[0] - ref.size[0]) <= 3 and abs(mine.size[1] - ref.size[1]) <= 3
        print("%s %-32s %s IoU %.3f" % ("ok  " if ok else "FAIL", path.split("/")[-1], ch, iou))
        fails += not ok
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
