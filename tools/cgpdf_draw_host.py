#!/usr/bin/env python3
"""CGContextDrawPDFPage contra o poppler (pdftoppm -r 72): gera PDFs à mão (paths, recortes, cores, imagens,
máscaras, imagem inline, CCITT, shadings 2/3/4/6, ladrilho, form com grupo de transparência, ExtGState, Type3,
conteúdo opcional, anotação) e pelo Ghostscript (texto com fonte embutida CFF, TrueType e Type1), renderiza
com o mOS e compara pixel a pixel (o ladrilho contra o Ghostscript). Compilação (da raiz do repo), dentro da trava do time:
  F="-g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \
     -Ikernel/include -Iuserland/libsystem"
  for f in coregraphics cgpdf pdf_core cgpdf_draw cg_consumer; do zig cc $F -c userland/libsystem/$f.c -o /tmp/mariana-pd/$f.o; done
  zig cc $F -DCT_HOST -c userland/libsystem/ct_outline.c -o /tmp/mariana-pd/ct_outline.o
  zig cc -std=gnu11 -g -O1 -w -D_DEFAULT_SOURCE -Iuserland/libsystem -Itools tools/cgpdf_draw_host.c /tmp/mariana-pd/*.o \
     userland/libsystem/cg_raster.c userland/libsystem/cgimage_crop.c -DCS_HOST userland/libsystem/colorsync.c -lm -lz \
     -o /tmp/mariana-pd/drawhost
  python3 tools/cgpdf_draw_host.py /tmp/mariana-pd/drawhost
"""
import io, os, subprocess, sys, zlib
from PIL import Image

HOST = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mariana-pd/drawhost"
TMP = "/tmp/mariana-pd"
W = H = 200


def pdf(objs, page_extra=b"", content=b"", res=b"<<>>", W=W, H=H, extra_catalog=b""):
    """objs: lista de bytes; 1 = catálogo, 2 = páginas, 3 = página, 4 = conteúdo, 5.. = os dados."""
    body = [b"<< /Type /Catalog /Pages 2 0 R " + extra_catalog + b">>",
            b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
            b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] /Resources " % (W, H) + res + b" /Contents 4 0 R " + page_extra + b">>",
            stream(b"", content)] + objs
    out = io.BytesIO()
    out.write(b"%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")
    offs = []
    for i, o in enumerate(body):
        offs.append(out.tell())
        out.write(b"%d 0 obj\n" % (i + 1) + o + b"\nendobj\n")
    x = out.tell()
    out.write(b"xref\n0 %d\n0000000000 65535 f \n" % (len(body) + 1))
    for o in offs: out.write(b"%010d 00000 n \n" % o)
    out.write(b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(body) + 1, x))
    return out.getvalue()


def stream(d, data, flate=False):
    if flate:
        data = zlib.compress(data)
        d += b" /Filter /FlateDecode"
    return b"<< " + d + b" /Length %d >>\nstream\n" % len(data) + data + b"\nendstream"


def render_ref(path, w, h, ref="poppler"):
    if ref == "gs":     # o poppler põe franja nas bordas das células de ladrilho; o Ghostscript não
        subprocess.run(["gs", "-q", "-dNOPAUSE", "-dBATCH", "-dSAFER", "-sDEVICE=png16m", "-r72", "-dGraphicsAlphaBits=4",
                        "-dTextAlphaBits=4", "-o", path[:-4] + "-ref.png", path], check=True)
    else: subprocess.run(["pdftoppm", "-r", "72", "-png", "-singlefile", path, path[:-4] + "-ref"], check=True)
    im = Image.open(path[:-4] + "-ref.png").convert("RGB")
    if im.size != (w, h): im = im.crop((0, 0, w, h))
    return im


def render_mos(path, w, h):
    out = path[:-4] + ".rgba"
    r = subprocess.run([HOST, path, out, str(w), str(h)], capture_output=True, timeout=60)
    if r.returncode != 0:
        raise RuntimeError("drawhost saiu %d: %s" % (r.returncode, r.stderr.decode(errors="replace")[-400:]))
    return Image.frombytes("RGBA", (w, h), open(out, "rb").read()).convert("RGB")


fails = 0


def check(name, data, mean_max=4.0, bad_max=0.02, w=W, h=H, ref="poppler"):
    global fails
    path = os.path.join(TMP, name + ".pdf")
    open(path, "wb").write(data)
    try:
        refim, got = render_ref(path, w, h, ref), render_mos(path, w, h)
    except Exception as e:
        print("FAIL", name, e); fails += 1; return
    a, b = refim.tobytes(), got.tobytes()
    diff = [abs(a[i] - b[i]) for i in range(len(a))]
    mean = sum(diff) / len(diff)
    bad = sum(1 for i in range(0, len(diff), 3) if max(diff[i:i + 3]) > 64) / (w * h)
    ok = mean <= mean_max and bad <= bad_max
    print("%s %-26s erro médio %.2f  pixels >64: %.2f%%" % ("ok  " if ok else "FAIL", name, mean, bad * 100))
    if not ok:
        fails += 1
        got.save(path[:-4] + "-mos.png")


# ---------------------------------------------------------------- casos
# 1 paths, cores de dispositivo, traço com junção/terminação, tracejado, par-ímpar, cm
check("paths", pdf([], content=b"""
1 0 0 rg 10 10 80 60 re f
0 0 1 RG 6 w 1 J 1 j 110 20 m 190 20 l 150 80 l S
0 g 0.5 G 3 w [8 4] 0 d 10 100 m 190 100 l S [] 0 d
0 0.6 0 rg 100 120 m 180 120 l 180 190 l 100 190 l h 120 140 m 160 140 l 160 170 l 120 170 l h f*
q 0.7071 0.7071 -0.7071 0.7071 50 140 cm 0.2 0.2 0.8 rg -15 -15 30 30 re f Q
0.9 0.5 0 rg 20 190 m 20 150 60 150 60 190 c 40 160 l b
"""))
# 2 recorte W n, recorte par-ímpar, aninhado com q/Q
check("clip", pdf([], content=b"""
q 100 100 m 100 190 l 190 145 l h W n 1 0 0 rg 0 0 200 200 re f Q
q 10 10 80 80 re 30 30 40 40 re W* n 0 0 1 rg 0 0 100 100 re f Q
q 110 10 80 80 re W n 130 0 40 200 re W n 0 0.5 0 rg 0 0 200 200 re f Q
"""))
# 3 CMYK, cinza, Indexed e cs/scn com espaço nomeado
check("colors", pdf([], res=b"<< /ColorSpace << /Ix [/Indexed /DeviceRGB 2 <ff000000ff000000ff>] /CR /DeviceRGB >> >>", content=b"""
0 0 0 0.5 k 10 10 50 50 re f
1 0 0 0 k 70 10 50 50 re f
0.3 g 130 10 50 50 re f
/Ix cs 1 sc 10 80 50 50 re f 2 sc 70 80 50 50 re f
/CR cs 0.2 0.8 0.4 sc 130 80 50 50 re f
"""), mean_max=6, bad_max=0.2)
# 4 imagem RGB 4x4 (Flate) e imagem cinza 16 bits ampliadas; Decode invertido
img = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0] * 4)
g16 = b"".join(bytes([v >> 8, v & 255]) for v in [0, 20000, 40000, 65535] * 4)
check("image", pdf([stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceRGB /BitsPerComponent 8", img, True),
                    stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceGray /BitsPerComponent 16 /Decode [1 0]", g16)],
                   res=b"<< /XObject << /Im 5 0 R /G 6 0 R >> >>",
                   content=b"q 80 0 0 80 10 10 cm /Im Do Q q 80 0 0 80 110 110 cm /G Do Q"), mean_max=8, bad_max=0.08)
# 5 máscara de estêncil pintada com a cor de preenchimento; SMask; máscara por cor; imagem inline
mask = bytes([0b10100000, 0b01010000, 0b10100000, 0b01010000])
smask = bytes([0, 85, 170, 255] * 4)
check("masks", pdf([stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ImageMask true /BitsPerComponent 1", mask),
                    stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceRGB /BitsPerComponent 8 /SMask 7 0 R", img),
                    stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceGray /BitsPerComponent 8", smask),
                    stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Mask [0 0 0 0 250 255]", img)],
                   res=b"<< /XObject << /M 5 0 R /S 6 0 R /K 8 0 R >> >>",
                   content=b"""0 0.5 1 rg q 80 0 0 80 10 10 cm /M Do Q q 80 0 0 80 110 10 cm /S Do Q q 80 0 0 80 10 110 cm /K Do Q
q 80 0 0 80 110 110 cm BI /W 2 /H 2 /CS /G /BPC 8 ID \x00\x80\xc0\xff EI Q"""), mean_max=8, bad_max=0.08)
# 5b SMask com /Matte: cores pré-misturadas com preto; o mOS desfaz a mistura
al = [0, 85, 170, 255]
pre = bytes(v for a in al * 4 for v in (a, a * 128 // 255, 0))
check("matte", pdf([stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceRGB /BitsPerComponent 8 /SMask 6 0 R", pre),
                    stream(b"/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceGray /BitsPerComponent 8 /Matte [0 0 0]", bytes(al * 4))],
                   res=b"<< /XObject << /S 5 0 R >> >>", content=b"0 0 1 rg 0 0 200 100 re f q 160 0 0 160 20 20 cm /S Do Q"), mean_max=5, bad_max=0.06)
# 6 CCITT G4 (gerado pelo PIL como TIFF group4; tiro a faixa)
bw = Image.new("1", (64, 32), 1)
for y in range(32):
    for x in range(64):
        if (x // 8 + y // 8) % 2 or (x - 32) ** 2 + (y - 16) ** 2 < 100: bw.putpixel((x, y), 0)
tb = io.BytesIO(); bw.save(tb, "TIFF", compression="group4"); tim = Image.open(io.BytesIO(tb.getvalue()))
so, sc = tim.tag_v2[273], tim.tag_v2[279]
g4 = tb.getvalue()[so[0]:so[0] + sc[0]]
check("ccitt", pdf([stream(b"/Type /XObject /Subtype /Image /Width 64 /Height 32 /ColorSpace /DeviceGray /BitsPerComponent 1 /Filter /CCITTFaxDecode /DecodeParms << /K -1 /Columns 64 /Rows 32 /BlackIs1 false >>", g4)],
                   res=b"<< /XObject << /C 5 0 R >> >>", content=b"q 192 0 0 96 4 50 cm /C Do Q"), mean_max=8, bad_max=0.08)
# 7 shadings 2 (axial com extensão, função tipo 3 de tipo 2) e 3 (radial) via sh e via padrão
f2a = b"<< /FunctionType 2 /Domain [0 1] /C0 [1 0 0] /C1 [0 0 1] /N 1 >>"
f2b = b"<< /FunctionType 2 /Domain [0 1] /C0 [0 0 1] /C1 [1 1 0] /N 1 >>"
check("shading", pdf([b"<< /ShadingType 2 /ColorSpace /DeviceRGB /Coords [20 0 180 0] /Extend [true true] /Function << /FunctionType 3 /Domain [0 1] /Functions [" + f2a + f2b + b"] /Bounds [0.5] /Encode [0 1 0 1] >> >>",
                      b"<< /ShadingType 3 /ColorSpace /DeviceRGB /Coords [100 150 0 100 150 40] /Function " + f2a + b" >>",
                      b"<< /PatternType 2 /Shading 6 0 R >>"],
                     res=b"<< /Shading << /A 5 0 R >> /Pattern << /P 7 0 R >> >>",
                     content=b"q 0 0 200 90 re W n /A sh Q /Pattern cs /P scn 60 110 80 80 re f"), mean_max=5, bad_max=0.05)
# 8 malha de triângulos (tipo 4) e patch de Coons (tipo 6) com cores por vértice
tri = b"".join(bytes([0]) + x.to_bytes(2, "big") + y.to_bytes(2, "big") + bytes(c) for x, y, c in
              [(0, 0, (255, 0, 0)), (65535, 0, (0, 255, 0)), (32768, 65535, (0, 0, 255))])
pts = [(10, 110), (10, 140), (10, 160), (10, 190), (40, 190), (70, 190), (100, 190), (100, 160), (100, 140), (100, 110), (70, 110), (40, 110)]
coons = bytes([0]) + b"".join(int(x / 200 * 65535).to_bytes(2, "big") + int(y / 200 * 65535).to_bytes(2, "big") for x, y in pts) + bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0])
check("mesh", pdf([stream(b"/ShadingType 4 /ColorSpace /DeviceRGB /BitsPerCoordinate 16 /BitsPerComponent 8 /BitsPerFlag 8 /Decode [10 190 10 100 0 1 0 1 0 1]", tri),
                   stream(b"/ShadingType 6 /ColorSpace /DeviceRGB /BitsPerCoordinate 16 /BitsPerComponent 8 /BitsPerFlag 8 /Decode [0 200 0 200 0 1 0 1 0 1]", coons)],
                  res=b"<< /Shading << /T 5 0 R /C 6 0 R >> >>", content=b"/T sh /C sh"), mean_max=6, bad_max=0.08)
# 9 padrão de ladrilho colorido (com Matrix) e sem cor (PaintType 2)
check("tiling", pdf([stream(b"/PatternType 1 /PaintType 1 /TilingType 1 /BBox [0 0 20 20] /XStep 20 /YStep 20 /Resources << >> /Matrix [1 0 0 1 5 5]", b"1 0 0 rg 0 0 10 10 re f 0 0 1 rg 10 10 10 10 re f"),
                     stream(b"/PatternType 1 /PaintType 2 /TilingType 1 /BBox [0 0 16 16] /XStep 16 /YStep 16 /Resources << >>", b"4 4 8 8 re f")],
                    res=b"<< /Pattern << /P 5 0 R /U 6 0 R >> /ColorSpace << /UP [/Pattern /DeviceRGB] >> >>",
                    content=b"/Pattern cs /P scn 10 10 180 80 re f /UP cs 0 0.6 0 /U scn 10 110 180 80 re f"), mean_max=2, bad_max=0.01, ref="gs")
# 10 form XObject com Matrix/BBox, grupo de transparência, ExtGState ca/CA, blend Multiply, conteúdo opcional desligado
check("form", pdf([stream(b"/Type /XObject /Subtype /Form /BBox [0 0 50 50] /Matrix [2 0 0 2 0 0] /Group << /S /Transparency >> /Resources << >>", b"1 0 0 rg 0 0 40 40 re f 0 0 1 rg 20 20 40 40 re f"),
                   b"<< /Type /ExtGState /ca 0.5 /CA 0.5 >>",
                   b"<< /Type /ExtGState /BM /Multiply >>",
                   b"<< /Type /OCG /Name (off) >>"],
                  res=b"<< /XObject << /F 5 0 R >> /ExtGState << /H 6 0 R /M 7 0 R >> /Properties << /oc 8 0 R >> >>",
                  extra_catalog=b"/OCProperties << /OCGs [8 0 R] /D << /OFF [8 0 R] >> >>",
                  content=b"""0 1 0 rg 0 100 200 20 re f q /H gs 1 0 0 1 10 10 cm /F Do Q
q /M gs 1 1 0 rg 120 120 60 60 re f 0 1 1 rg 150 150 40 40 re f Q
/OC /oc BDC 0 0 0 rg 0 0 200 200 re f EMC"""), mean_max=5, bad_max=0.05)
# 10b máscara suave do ExtGState (luminosidade de um form com grupo): metade branca, faixa cinza, resto preto
check("softmask", pdf([b"<< /Type /ExtGState /SMask << /Type /Mask /S /Luminosity /G 6 0 R >> >>",
                       stream(b"/Type /XObject /Subtype /Form /BBox [0 0 200 200] /Group << /S /Transparency /CS /DeviceGray >> /Resources << >>",
                              b"1 g 0 0 100 200 re f 0.5 g 100 0 50 200 re f"),
                       b"<< /Type /ExtGState /SMask /None >>"],
                      res=b"<< /ExtGState << /S 5 0 R /N 7 0 R >> >>",
                      content=b"q /S gs 1 0 0 rg 10 10 180 180 re f Q 0 0 1 rg 20 20 20 20 re f"), mean_max=3, bad_max=0.02)
# 11 fonte Type3 e modos de texto (preencher, traço, recorte 7)
check("type3", pdf([b"<< /Type /Font /Subtype /Type3 /FontBBox [0 0 1000 1000] /FontMatrix [0.001 0 0 0.001 0 0] /CharProcs << /a 6 0 R /b 7 0 R >> /Encoding << /Differences [97 /a /b] >> /FirstChar 97 /LastChar 98 /Widths [1000 1000] /Resources << >> >>",
                    stream(b"", b"1000 0 0 0 1000 1000 d1 0 0 800 800 re f"),
                    stream(b"", b"1000 0 0 0 1000 1000 d1 0 0 m 800 0 l 400 800 l h f")],
                   res=b"<< /Font << /T 5 0 R >> >>",
                   content=b"BT /T 40 Tf 1 0 0 rg 10 140 Td (abab) Tj 0 0 1 rg 0 -60 Td 10 Tc (ba) Tj ET"), mean_max=5, bad_max=0.05)
# 12 anotação com aparência /N (e uma oculta que não deve aparecer)
check("annot", pdf([b"<< /Type /Annot /Subtype /Square /Rect [20 20 120 80] /AP << /N 6 0 R >> >>",
                    stream(b"/Type /XObject /Subtype /Form /BBox [0 0 10 6]", b"1 0 0 rg 0 0 10 6 re f"),
                    b"<< /Type /Annot /Subtype /Square /F 2 /Rect [100 100 190 190] /AP << /N 6 0 R >> >>"],
                   page_extra=b"/Annots [5 0 R 7 0 R]", content=b"0 0 1 rg 60 50 100 100 re f"), mean_max=4, bad_max=0.03)

# 13–15 texto do Ghostscript: CFF (fonte padrão do gs), TrueType (DejaVu) e Type1 (-dNoOutputFonts não; força Type1 com CompatibilityLevel 1.3)
PS = b"""%!PS
<< /PageSize [200 200] >> setpagedevice
/%s findfont 28 scalefont setfont
10 150 moveto (Hamburg) show
0 0 1 setrgbcolor 10 100 moveto (Wxyz 123) show
1 0 0 setrgbcolor 10 40 moveto 2 setlinewidth (Ag) true charpath stroke
showpage
"""
def gs_pdf(name, font, extra=()):
    ps = os.path.join(TMP, name + ".ps")
    open(ps, "wb").write(PS.replace(b"%s", font))
    out = os.path.join(TMP, name + "-gs.pdf")
    subprocess.run(["gs", "-q", "-dNOPAUSE", "-dBATCH", "-dSAFER", "-sDEVICE=pdfwrite", "-dEmbedAllFonts=true", "-dPDFSETTINGS=/prepress", *extra, "-o", out, ps], check=True)
    return open(out, "rb").read()
check("text-cff", gs_pdf("text-cff", b"Helvetica"), mean_max=4, bad_max=0.03)
check("text-tt", gs_pdf("text-tt", b"DejaVuSans", ["-sFONTPATH=/usr/share/fonts/truetype/dejavu"]), mean_max=4, bad_max=0.03)
# Type 1 de verdade (eexec): Nimbus Roman da URW embutida à mão em FontFile, larguras do AFM
T1 = "/usr/share/fonts/type1/urw-base35/NimbusRoman-Regular"
t1 = open(T1 + ".t1", "rb").read()
l1 = t1.index(b"eexec") + 6
while t1[l1:l1 + 1] in (b"\r", b"\n"): l1 += 1
z = t1.rindex(b"cleartomark"); l3s = z
while t1[l3s - 1:l3s] in (b"0", b"\r", b"\n"): l3s -= 1
wx = {}
for ln in open(T1 + ".afm"):
    if ln.startswith("C "):
        f = dict(x.strip().split(" ", 1) for x in ln.split(";") if x.strip())
        if int(f["C"]) >= 32: wx[int(f["C"])] = int(f["WX"])
widths = " ".join(str(wx.get(c, 0)) for c in range(32, 127)).encode()
check("text-t1", pdf([b"<< /Type /Font /Subtype /Type1 /BaseFont /NimbusRoman-Regular /FirstChar 32 /LastChar 126 /Widths [" + widths + b"] /Encoding /WinAnsiEncoding /FontDescriptor 6 0 R >>",
                      b"<< /Type /FontDescriptor /FontName /NimbusRoman-Regular /Flags 34 /FontBBox [-200 -300 1200 1000] /ItalicAngle 0 /Ascent 700 /Descent -200 /CapHeight 660 /StemV 80 /FontFile 7 0 R >>",
                      stream(b"/Length1 %d /Length2 %d /Length3 %d" % (l1, l3s - l1, len(t1) - l3s), t1)],
                     res=b"<< /Font << /F 5 0 R >> >>",
                     content=b"BT /F 30 Tf 10 150 Td (Hamburg) Tj 0 0 1 rg 0 -50 Td 2 Tr 1 w (Wxyz 123) Tj 0 Tr 0 -50 Td 2 Tw 3 Tc (a b \\(c\\)) Tj ET"),
      mean_max=4, bad_max=0.03)

# texto da página (base do PDFKit): as palavras na mesma ordem que o pdftotext
for name in ("text-cff", "text-tt", "text-t1", "type3"):
    path = os.path.join(TMP, name + ".pdf")
    mine = subprocess.run([HOST, path, "text"], capture_output=True).stdout.decode("utf-8", "replace").split()
    ref = subprocess.run(["pdftotext", path, "-"], capture_output=True).stdout.decode("utf-8", "replace").split()
    ok = mine == ref
    print("%s %-26s %s" % ("ok  " if ok else "FAIL", name + " (texto)", " ".join(mine)[:60]))
    if not ok:
        print("     esperado:", " ".join(ref)[:80]); fails += 1
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
