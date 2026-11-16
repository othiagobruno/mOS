#!/usr/bin/env python3
"""Motor do ColorSync do mOS contra o LittleCMS (PIL.ImageCms) nos perfis ICC do sistema e nos que o mOS grava.
gcc -O1 -DCS_HOST -Iuserland/libsystem tools/colorsync_host.c userland/libsystem/colorsync.c -lm -o /tmp/colorsync_host
python3 tools/colorsync_host.py [/tmp/colorsync_host]"""
import os, random, subprocess, sys, tempfile
from PIL import Image, ImageCms

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/colorsync_host"
T = tempfile.mkdtemp(prefix="cs-")
fails = 0
# saída: o sRGB v2 que o próprio mOS grava (o sRGB embutido do LittleCMS é v4 e força compensação de ponto preto)
_s = os.path.join(T, "out-srgb.icc")
subprocess.run([BIN, "write", "kCGColorSpaceSRGB", _s], check=True)
srgb = ImageCms.getOpenProfile(_s)

def mos(icc, vals):
    vals = list(vals) + [0] * (3 - len(vals))
    r = subprocess.run([BIN, "srgb", icc] + ["%.6f" % v for v in vals], capture_output=True, text=True, env=dict(os.environ, CS_INTENT="1"))
    if r.returncode: return None
    return [float(x) for x in r.stdout.split()]

def lcms(icc, mode, vals):
    prof = ImageCms.getOpenProfile(icc)
    # colorimétrico relativo dos dois lados (A2B1): o perceptual do LittleCMS mexe no ponto preto
    tr = ImageCms.buildTransform(prof, srgb, mode, "RGB", renderingIntent=ImageCms.Intent.RELATIVE_COLORIMETRIC,
                                 flags=ImageCms.Flags.NOOPTIMIZE | ImageCms.Flags.HIGHRESPRECALC)
    px = tuple(int(round(v * 255)) for v in vals)
    if mode == "L": px = px[0]
    im = Image.new(mode, (1, 1), px)
    return [c / 255 for c in ImageCms.applyTransform(im, tr).getpixel((0, 0))]

def case(name, icc, mode, nch, tol):
    global fails
    random.seed(3)
    worst = 0
    for _ in range(120):
        v = [random.randrange(256) / 255 for _ in range(nch)]
        a = mos(icc, v)
        if a is None:
            print("FALHA %s: o mOS não leu o perfil" % name); fails += 1; return
        b = lcms(icc, mode, v)
        # fora do gamut do sRGB (canal recortado) cada CMM recorta do seu jeito: só mede dentro
        if any(round(x * 255) in (0, 255) for x in b): continue
        worst = max(worst, max(abs(x - y) for x, y in zip(a, b)) * 255)
    ok = worst <= tol
    print(("ok   " if ok else "FALHA ") + "%s (pior diferença %.2f/255, limite %d)" % (name, worst, tol))
    if not ok: fails += 1

gs = "/usr/share/color/icc/ghostscript"
cd = "/usr/share/color/icc/colord"
for f, mode, n, tol in [("srgb.icc", "RGB", 3, 2), ("a98.icc", "RGB", 3, 2), ("rommrgb.icc", "RGB", 3, 3), ("sgray.icc", "L", 1, 2),
                        ("default_cmyk.icc", "CMYK", 4, 4), ("ps_cmyk.icc", "CMYK", 4, 4)]:
    p = os.path.join(gs, f)
    if os.path.exists(p): case(f, p, mode, n, tol)
for f, mode, n, tol in [("ColorMatchRGB.icc", "RGB", 3, 2), ("FOGRA45L_lwc.icc", "CMYK", 4, 4), ("SwappedRedAndGreen.icc", "RGB", 3, 2)]:
    p = os.path.join(cd, f)
    if os.path.exists(p): case(f, p, mode, n, tol)
# os perfis que o mOS grava abrem no LittleCMS e convertem igual
for name in ["kCGColorSpaceSRGB", "kCGColorSpaceDisplayP3", "kCGColorSpaceAdobeRGB1998", "kCGColorSpaceITUR_2020", "kCGColorSpaceLinearSRGB",
             "kCGColorSpaceGenericGrayGamma2_2"]:
    out = os.path.join(T, name + ".icc")
    if subprocess.run([BIN, "write", name, out]).returncode:
        print("FALHA gravar", name); fails += 1; continue
    gray = "Gray" in name
    case("gravado " + name, out, "L" if gray else "RGB", 1 if gray else 3, 2)
print("colorsync_host: %s (%d falhas)" % ("ok" if not fails else "FALHOU", fails))
sys.exit(1 if fails else 0)
