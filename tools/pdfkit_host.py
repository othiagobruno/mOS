#!/usr/bin/env python3
"""PDFKit (pdfkit.c) no host: roda tools/pdfkit_host.c (runtime ObjC mínimo + Core Graphics real) sobre os PDFs do
tools/cgpdf_write_host.py (w.pdf e wenc.pdf) e confere no poppler os PDFs que o PDFKit gravou.
  F="-g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \\
     -Ikernel/include -Iuserland/libsystem"
  for f in coregraphics cgpdf pdf_core cgpdf_draw cg_consumer pdf_write pdfkit; do zig cc $F -c userland/libsystem/$f.c -o /tmp/mariana-pk/$f.o; done
  zig cc $F -DCT_HOST -c userland/libsystem/ct_outline.c -o /tmp/mariana-pk/ct_outline.o
  zig cc -std=gnu2x -g -O1 -w -Iuserland/libsystem tools/pdfkit_host.c /tmp/mariana-pk/*.o userland/libsystem/cg_raster.c \\
     userland/libsystem/cgimage_crop.c -DCS_HOST userland/libsystem/colorsync.c -lm -lz -o /tmp/mariana-pk/pkhost
  python3 tools/pdfkit_host.py /tmp/mariana-pk/pkhost /tmp/mariana-pw"""
import re, subprocess, sys
from PIL import Image
HOST, SRC = sys.argv[1], sys.argv[2]
OUT = "/tmp/mariana-pk"
r = subprocess.run([HOST, SRC + "/w.pdf", SRC + "/wenc.pdf", OUT], capture_output=True, text=True)
print(r.stdout, end="")
fails = 0 if r.returncode == 0 else 1
def check(ok, msg):
    global fails
    print(("ok   " if ok else "FAIL ") + msg); fails += not ok
info = subprocess.run(["pdfinfo", "-f", "1", "-l", "3", OUT + "/edit.pdf"], capture_output=True, text=True).stdout
check("Pages:           3" in info and re.search(r"Page +2 rot: +90", info) is not None, "poppler: edit.pdf com 3 páginas e a 2ª girada")
check("Title:           Título novo" in info, "poppler: Title novo")
txt = subprocess.run(["pdftotext", OUT + "/edit.pdf", "-"], capture_output=True, text=True).stdout
check(txt.index("Segunda pagina") < txt.index("Hamburg"), "poppler: texto na ordem nova")
subprocess.run(["pdftoppm", "-r", "72", "-png", "-singlefile", OUT + "/img.pdf", OUT + "/img"], check=True)
im = Image.open(OUT + "/img.png").convert("RGB")
check(im.size == (64, 32) and im.getpixel((32, 16)) == (0, 0x60, 0xC0), "poppler: página feita de imagem tem a cor certa")
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
