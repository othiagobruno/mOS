#!/usr/bin/env python3
"""CGPDFContext (coregraphics_pdf.inc) no host. A mesma cena vai para um PDF e para um CGBitmapContext; o PDF é
rasterizado pelo poppler e comparado com o bitmap. Confere também: texto extraível (Type3 + ToUnicode), Info
UTF-16, páginas e tamanhos, tags, link para URL e para destino nomeado, outline, cifra RC4-128 (senha de usuário
e de dono, permissão de cópia) e que o PDF relê no leitor do próprio mOS (cgpdf.c, pelo cg_context_test).
Compilação (da raiz do repo):
  F="-g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \\
     -Ikernel/include -Iuserland/libsystem"
  for f in coregraphics cgpdf pdf_core cg_consumer; do zig cc $F -c userland/libsystem/$f.c -o /tmp/mariana-pw/$f.o; done
  zig cc $F -DCT_HOST -c userland/libsystem/ct_outline.c -o /tmp/mariana-pw/ct_outline.o
  zig cc -std=gnu11 -g -O1 -w -D_DEFAULT_SOURCE -Iuserland/libsystem -Itools tools/cgpdf_write_host.c /tmp/mariana-pw/*.o \\
     userland/libsystem/cg_raster.c userland/libsystem/cgimage_crop.c -DCS_HOST userland/libsystem/colorsync.c -lm -lz \\
     -o /tmp/mariana-pw/pwhost
  python3 tools/cgpdf_write_host.py /tmp/mariana-pw/pwhost
"""
import re, subprocess, sys, zlib
from PIL import Image
HOST = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mariana-pw/pwhost"
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
T = "/tmp/mariana-pw/"
fails = 0
def check(ok, msg):
    global fails
    print(("ok   " if ok else "FAIL ") + msg)
    fails += not ok
def run(*a, **k): return subprocess.run(list(a), capture_output=True, text=True, **k)

run(HOST, "pdf", T + "w.pdf", FONT, check=True)
run(HOST, "png", T + "w.rgba", FONT, check=True)
run(HOST, "pdf", T + "wenc.pdf", FONT, "cifra", check=True)
raw = open(T + "w.pdf", "rb").read()

# 1 pixels: poppler × bitmap da mesma cena
run("pdftoppm", "-r", "72", "-png", "-f", "1", "-l", "1", "-singlefile", T + "w.pdf", T + "wref", check=True)
a = Image.frombytes("RGBA", (400, 450), open(T + "w.rgba", "rb").read()).convert("RGB").tobytes()
b = Image.open(T + "wref.png").convert("RGB").tobytes()
d = [abs(a[i] - b[i]) for i in range(len(a))]
mean = sum(d) / len(d)
bad = sum(1 for i in range(0, len(d), 3) if max(d[i:i + 3]) > 64) / (400 * 450)
check(mean < 2.0 and bad < 0.01, "página 1 renderizada pelo poppler = bitmap (erro médio %.2f, pixels >64: %.2f%%)" % (mean, bad * 100))

# 2 metadados e estrutura
info = run("pdfinfo", T + "w.pdf").stdout
check("Title:           Relatório de teste — mOS" in info, "Info: título UTF-16 com acento e travessão")
check("Author:          Mariana" in info and "Keywords:        pdf, quartz" in info, "Info: autor e palavras-chave")
check("Pages:           2" in info and "Tagged:          yes" in info, "2 páginas, PDF marcado (tags)")
p2 = run("pdfinfo", "-f", "2", "-l", "2", T + "w.pdf").stdout
check(re.search(r"Page +2 size: +300 x 200 pts", p2) is not None, "página 2 com outra MediaBox (300 x 200)")
txt = run("pdftotext", T + "w.pdf", "-").stdout
check("Hamburg Wxyz 123" in txt and "mOS PDFContext" in txt and "Segunda pagina" in txt, "texto extraível (Type3 + ToUnicode)")
# 3 objetos: descomprime os streams e procura o que importa
body = raw
for m in re.finditer(rb"stream\r?\n", raw):
    s = m.end(); e = raw.find(b"endstream", s)
    try: body += zlib.decompress(raw[s:e])
    except Exception: pass
check(b"/S /URI /URI" in raw and b"/Subtype /Link" in raw, "link para URL")
check(b"/Dests" in raw and b"/XYZ 20 150 0" in raw, "destino nomeado (/Dests, /XYZ)")
check(b"/Type /Outlines" in raw and raw.count(b"/Title") >= 2, "outline com 2 capítulos")
check(b"/Type /StructTreeRoot" in raw and b"/S /Figure" in raw and b"/S /P" in raw and b"/MCID" in body, "árvore de estrutura (Document > Figure, P) e MCIDs")
check(b"/Subtype /Type3" in raw and b"/ToUnicode" in raw, "fonte Type3 com ToUnicode")
check(b"/Group << /S /Transparency" in raw, "camada de transparência como grupo")
check(b"/SMask << /Type /Mask /S /Luminosity" in raw, "ClipToMask como SMask de luminosidade")
check(b"/ShadingType 2" in raw and b"/ShadingType 3" in raw, "gradientes linear e radial como shading")
check(b"/BM /Multiply" in raw, "blend multiply no ExtGState")
check(b"\nW* n" in body and b" re W n" in body, "recortes par-ímpar e por retângulo")
# 4 cifra
ienc = run("pdfinfo", "-upw", "usuario", T + "wenc.pdf").stdout
check("Encrypted:       yes (print:yes copy:no" in ienc, "cifra: senha de usuário abre, cópia negada")
check(run("pdfinfo", T + "wenc.pdf").returncode != 0, "cifra: sem senha não abre")
check("Title:           Relatório de teste — mOS" in run("pdfinfo", "-opw", "dono", T + "wenc.pdf").stdout, "cifra: senha de dono abre e decifra o título")
check("Hamburg Wxyz 123" in run("pdftotext", "-upw", "usuario", T + "wenc.pdf", "-").stdout, "cifra: texto decifrado")
run("pdftoppm", "-r", "72", "-png", "-f", "1", "-l", "1", "-singlefile", "-upw", "usuario", T + "wenc.pdf", T + "wencref")
e = Image.open(T + "wencref.png").convert("RGB").tobytes()
check(e == b, "cifra: a página decifrada é igual à sem cifra")
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
