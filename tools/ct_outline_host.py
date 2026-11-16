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

# métricas da face (cx_font_metrics → CTFontGetAscent/CapHeight/…) contra o FreeType (libfreetype por ctypes)
import ctypes
ft = ctypes.CDLL("libfreetype.so.6")
lib = ctypes.c_void_p(); ft.FT_Init_FreeType(ctypes.byref(lib))
ft.FT_Get_Sfnt_Table.restype = ctypes.c_void_p
S16, U16, UL = ctypes.c_short, ctypes.c_ushort, ctypes.c_ulong
class OS2(ctypes.Structure):
    _fields_ = [("version", U16), ("xAvg", S16), ("wght", U16), ("wdth", U16), ("fsType", U16)] + \
               [("y%d" % i, S16) for i in range(10)] + [("fam", S16), ("panose", ctypes.c_ubyte * 10)] + \
               [("ur%d" % i, UL) for i in range(4)] + [("vend", ctypes.c_char * 4), ("fsSel", U16), ("first", U16), ("last", U16),
               ("tA", S16), ("tD", S16), ("tG", S16), ("wA", U16), ("wD", U16), ("cp1", UL), ("cp2", UL), ("xh", S16), ("cap", S16)]
class POST(ctypes.Structure):
    _fields_ = [("fmt", ctypes.c_long), ("italic", ctypes.c_long), ("ulPos", S16), ("ulTh", S16)]
def s16(b, o): return int.from_bytes(b[o:o + 2], "big", signed=True)
for path, _ in CASES:
    r = subprocess.run([HOST, path, "metricas"], capture_output=True, text=True)
    name = path.split("/")[-1]
    if r.returncode: print("FAIL", name, "métricas"); fails += 1; continue
    m = [float(v) for v in r.stdout.split()]
    face = ctypes.c_void_p()
    ft.FT_New_Face(lib, path.encode(), 0, ctypes.byref(face))
    raw = ctypes.string_at(face, 152)
    upem = int.from_bytes(raw[136:138], "little")
    bbox = [ctypes.c_long.from_buffer_copy(raw, 104 + 8 * i).value / upem for i in range(4)]
    # ascendente/descendente/lineGap do hhea, como o Core Text (o FreeType expõe ascender/descender/height)
    asc, desc, hgt = [int.from_bytes(raw[o:o + 2], "little", signed=True) / upem for o in (138, 140, 142)]
    os2 = OS2.from_address(ft.FT_Get_Sfnt_Table(face, 2)) if ft.FT_Get_Sfnt_Table(face, 2) else None
    post = POST.from_address(ft.FT_Get_Sfnt_Table(face, 5))
    pil = ImageFont.truetype(path, upem)
    capref = os2.cap / upem if os2 and os2.version >= 2 else -pil.getbbox("H", anchor="ls")[1] / upem
    xref = os2.xh / upem if os2 and os2.version >= 2 else -pil.getbbox("x", anchor="ls")[1] / upem
    want = [upem, asc, -desc, hgt - asc + desc, capref, xref, post.italic / 65536.0, post.ulPos / upem, post.ulTh / upem] + bbox
    bad = [i for i in range(13) if abs(m[i] - want[i]) > (0.5 if i == 0 else 0.004)]
    print("%s %-32s métricas %s" % ("ok  " if not bad else "FAIL", name, "" if not bad else
          ", ".join("%d: %.4f × %.4f" % (i, m[i], want[i]) for i in bad)))
    fails += bool(bad)
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
