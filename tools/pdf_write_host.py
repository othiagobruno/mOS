#!/usr/bin/env python3
"""pdf_write.c (regravação do PDFKit) no host: PDFs do Ghostscript (3 páginas com texto CFF e imagem), do nosso
CGPDFContext (Type3, links, outline) e um cifrado RC4 destravado com senha. Cada página regravada é rasterizada
pelo poppler e comparada pixel a pixel com a de origem; texto, anotações, outline e Info pelo pdfinfo/pdftotext.
  gcc -O1 -DPDF_HOST -Iuserland/libsystem tools/pdf_write_host.c userland/libsystem/pdf_write.c userland/libsystem/pdf_core.c -lz -o /tmp/pdf_write_host
  python3 tools/pdf_write_host.py /tmp/pdf_write_host [pasta com w.pdf e wenc.pdf do cgpdf_write_host]"""
import os, re, subprocess, sys
from PIL import Image
HOST = sys.argv[1] if len(sys.argv) > 1 else "/tmp/pdf_write_host"
SRC = sys.argv[2] if len(sys.argv) > 2 else "/tmp/mariana-pw"
T = "/tmp/mariana-pwr/"
os.makedirs(T, exist_ok=True)
fails = 0
def check(ok, msg):
    global fails
    print(("ok   " if ok else "FAIL ") + msg); fails += not ok
def run(*a): return subprocess.run(list(a), capture_output=True, text=True)
def page(pdf, n, pw=None):
    out = T + "pg"
    args = ["pdftoppm", "-r", "50", "-png", "-f", str(n), "-l", str(n), "-singlefile"] + (["-upw", pw] if pw else []) + [pdf, out]
    run(*args)
    return Image.open(out + ".png").convert("RGB")
def same(a, b):
    if a.size != b.size: return False
    x, y = a.tobytes(), b.tobytes()
    return sum(abs(x[i] - y[i]) for i in range(len(x))) / len(x) < 0.5
def npages(pdf): return int(re.search(r"Pages:\s+(\d+)", run("pdfinfo", pdf).stdout).group(1))

ps = T + "g.ps"
open(ps, "w").write("""%!PS
/Helvetica findfont 30 scalefont setfont
1 1 3 { /n exch def 50 700 moveto (Pagina ) show n 3 string cvs show
  0.2 n mul 0 0 setrgbcolor 100 400 200 n 50 mul rectfill 0 setgray showpage } for
""")
G = T + "g.pdf"
run("gs", "-q", "-dNOPAUSE", "-dBATCH", "-dSAFER", "-sDEVICE=pdfwrite", "-o", G, ps)
W, WE = SRC + "/w.pdf", SRC + "/wenc.pdf"

r = T + "rev.pdf"; run(HOST, r, "reverse", G)
check(npages(r) == 3 and all(same(page(r, i + 1), page(G, 3 - i)) for i in range(3)), "páginas ao contrário: cada uma igual à de origem")
check("Pagina 3" in run("pdftotext", r, "-").stdout.split("Pagina 2")[0], "texto CFF preservado e na ordem nova")
m = T + "merge.pdf"; run(HOST, m, "merge", G, W)
check(npages(m) == 5 and same(page(m, 4), page(W, 1)) and same(page(m, 2), page(G, 2)), "juntar dois documentos (gs + CGPDFContext)")
check("Hamburg Wxyz 123" in run("pdftotext", m, "-").stdout, "texto Type3 do CGPDFContext preservado")
check(b"/URI" in open(m, "rb").read(), "anotação de link de origem preservada")
rot = T + "rot.pdf"; run(HOST, rot, "rotate", G)
info1 = run("pdfinfo", "-f", "1", "-l", "2", rot).stdout
check(re.search(r"Page +1 rot: +90", info1) is not None and re.search(r"Page +2 size: +200 x 150", info1) is not None, "rotação 90° e MediaBox nova")
an = T + "annot.pdf"; run(HOST, an, "annot", W)
raw = open(an, "rb").read()
check(b"/Subtype /Text" in raw and b"/Subtype /Link" in raw and raw.count(b"/Subtype /Link") >= 3, "anotações novas (link interno, nota) junto com as de origem")
pg2 = re.search(rb"(\d+) 0 obj\n<<[^>]*?/Type /Page /Parent", raw)
check(re.search(rb"/Dest \[\d+ 0 R /XYZ 0 100 0\]", raw) is not None, "destino do link aponta para a página nova")
ol = T + "outline.pdf"; run(HOST, ol, "outline", G)
i = run("pdfinfo", ol).stdout
check("Title:           Documento regravado — mOS" in i and "Author:          PDFKit" in i, "Info nova (UTF-16)")
o = open(ol, "rb").read()
check(b"/Type /Outlines" in o and o.count(b"/Title") == 4 and b"/Count 3" in o, "outline com 3 itens (+ título da Info) e filho aberto (Count 3)")
de = T + "dec.pdf"; run(HOST, de, "reverse", WE, "-", "usuario")
check(npages(de) == 2 and same(page(de, 2), page(WE, 1, "usuario")), "PDF cifrado destravado e regravado sem cifra")
check("Encrypted:       no" in run("pdfinfo", de).stdout, "regravado sai sem cifra")
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
