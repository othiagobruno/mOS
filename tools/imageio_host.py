#!/usr/bin/env python3
"""Confere os codecs do ImageIO do mOS contra o PIL (lê o que o PIL grava, o PIL lê o que o mOS grava).
gcc -O1 -DIIO_HOST -Iuserland/libsystem tools/imageio_host.c userland/libsystem/imageio.c \
    -Itools/iio_host_inc userland/libsystem/iio_webp.c -lz -lm -o /tmp/imageio_host
python3 tools/imageio_host.py [/tmp/imageio_host]"""
import io, os, random, struct, subprocess, sys, tempfile
from PIL import Image, ImageDraw

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/imageio_host"
T = tempfile.mkdtemp(prefix="iio-")
fails = 0

def sample(mode="RGBA", w=67, h=45, seed=1):
    random.seed(seed)
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i in range(25):
        x0, y0 = random.randrange(w), random.randrange(h)
        d.rectangle([x0, y0, x0 + random.randrange(1, 30), y0 + random.randrange(1, 20)],
                    fill=(random.randrange(256), random.randrange(256), random.randrange(256), random.choice([0, 90, 255, 255])))
    return im.convert(mode)

def mos_decode(path):
    out = os.path.join(T, "dec.raw")
    r = subprocess.run([BIN, "decode", path, out], capture_output=True)
    if r.returncode: return None
    b = open(out, "rb").read()
    hdr, rest = b.split(b"\n", 1)
    f = hdr.split()
    w, h, n = int(f[0]), int(f[1]), int(f[2])
    frames = []
    for i in range(n):
        raw = rest[i * w * h * 4:(i + 1) * w * h * 4]
        px = [((v >> 16) & 255, (v >> 8) & 255, v & 255, v >> 24) for v in struct.unpack("<%dI" % (w * h), raw)]
        frames.append(px)
    return dict(w=w, h=h, n=n, uti=f[3].decode(), loop=int(f[4]), orient=int(f[5]), dpi=float(f[6]), frames=frames)

def diff(a, b, alpha_matters=True):
    m = 0
    for p, q in zip(a, b):
        if alpha_matters and p[3] == 0 and q[3] == 0: continue
        m = max(m, max(abs(x - y) for x, y in zip(p[:3] if not alpha_matters else p, q[:3] if not alpha_matters else q)))
    return m

def check(name, ok, info=""):
    global fails
    print(("ok   " if ok else "FALHA ") + name + ("" if ok else "  " + info))
    if not ok: fails += 1

def mean_err(a, b):
    return sum(abs(x - y) for p, q in zip(a, b) for x, y in zip(p[:3], q[:3])) / (3 * len(a))

def dec_case(name, im, fmt, tol=0, ref=None, mean=False, **kw):
    p = os.path.join(T, name)
    im.save(p, fmt, **kw)
    if ref is None: ref = list(Image.open(p).convert("RGBA").get_flattened_data())
    got = mos_decode(p)
    if not got: return check(name, False, "não decodificou")
    dd = mean_err(ref, got["frames"][0]) if mean else diff(ref, got["frames"][0])
    check(name, got["w"] == im.width and got["h"] == im.height and dd <= tol, "dif %d" % dd)

base = sample()
dec_case("rgba.png", base, "PNG")
dec_case("rgb.png", base.convert("RGB"), "PNG")
dec_case("gray.png", base.convert("L"), "PNG")
dec_case("la.png", base.convert("LA"), "PNG")
dec_case("pal.png", base.convert("P"), "PNG")
dec_case("1bit.png", base.convert("1"), "PNG")
dec_case("adam7.png", base, "PNG", interlace=1) if False else None
g8 = base.convert("L")
i16 = Image.frombytes("I;16", g8.size, b"".join(struct.pack("<H", v * 257) for v in g8.get_flattened_data()))
dec_case("i16.png", i16, "PNG", ref=list(g8.convert("RGBA").get_flattened_data()))
dec_case("q.jpg", base.convert("RGB"), "JPEG", tol=5, mean=True, quality=95)
dec_case("prog.jpg", base.convert("RGB"), "JPEG", tol=5, mean=True, quality=95, progressive=True)
dec_case("444.jpg", base.convert("RGB"), "JPEG", tol=2, mean=True, quality=95, subsampling=0)
dec_case("cmyk.jpg", base.convert("CMYK"), "JPEG", tol=3, mean=True, quality=95)
dec_case("gray.jpg", base.convert("L"), "JPEG", tol=2, mean=True, quality=95)
dec_case("g.gif", base.convert("RGB").convert("P", palette=Image.ADAPTIVE), "GIF")
dec_case("raw.tif", base, "TIFF")
dec_case("lzw.tif", base, "TIFF", compression="tiff_lzw")
dec_case("zip.tif", base.convert("RGB"), "TIFF", compression="tiff_adobe_deflate")
dec_case("pb.tif", base.convert("L"), "TIFF", compression="packbits")
dec_case("cmyk.tif", base.convert("CMYK"), "TIFF", tol=1)
dec_case("pal.tif", base.convert("P"), "TIFF")
dec_case("rgb.bmp", base.convert("RGB"), "BMP")
dec_case("p8.bmp", base.convert("P"), "BMP")
dec_case("1.bmp", base.convert("1"), "BMP")
dec_case("a.ico", base.resize((32, 32)), "ICO", sizes=[(32, 32)])

# WebP: sem perda bate pixel a pixel com o PIL (libwebp); com perda, erro médio pequeno
dec_case("ll.webp", base, "WEBP", lossless=True)
dec_case("llrgb.webp", base.convert("RGB"), "WEBP", lossless=True)
dec_case("lossy.webp", base.convert("RGB"), "WEBP", tol=2, mean=True, quality=90)
dec_case("alpha.webp", base, "WEBP", tol=2, mean=True, quality=90)
# PNG entrelaçado: o PIL não grava Adam7; usa um arquivo feito à mão via zlib
import zlib
def png_adam7(im):
    w, h = im.size; px = im.load(); raw = b""
    for (sx, sy, dx, dy) in [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]:
        xs = range(sx, w, dx); ys = range(sy, h, dy)
        if not len(xs) or not len(ys): continue
        for y in ys: raw += b"\0" + b"".join(bytes(px[x, y]) for x in xs)
    def ch(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    return b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 1)) + ch(b"IDAT", zlib.compress(raw)) + ch(b"IEND", b"")
p = os.path.join(T, "adam7.png"); open(p, "wb").write(png_adam7(base))
g = mos_decode(p); check("adam7.png", g and diff(list(base.get_flattened_data()), g["frames"][0]) == 0)

# animações
frames = [sample("RGB", 40, 30, s) for s in range(4)]
p = os.path.join(T, "anim.gif")
frames[0].save(p, save_all=True, append_images=frames[1:], duration=[100, 200, 300, 400], loop=3)
g = mos_decode(p)
ok = g and g["n"] == 4 and g["loop"] == 3
if ok:
    gi = Image.open(p)
    for i in range(4):
        gi.seek(i); ref = list(gi.convert("RGBA").get_flattened_data())
        if diff(ref, g["frames"][i]) > 0: ok = False
check("anim.gif (4 quadros, loop 3)", ok)
p = os.path.join(T, "anim.png")
rgba = [sample("RGBA", 40, 30, s) for s in range(3)]
rgba[0].save(p, save_all=True, append_images=rgba[1:], duration=[50, 60, 70], loop=0, disposal=1, blend=0)
g = mos_decode(p)
ok = g and g["n"] == 3
if ok:
    ai = Image.open(p)
    for i in range(3):
        ai.seek(i); ref = list(ai.convert("RGBA").get_flattened_data())
        if diff(ref, g["frames"][i]) > 0: ok = False; print("  quadro", i, diff(ref, g["frames"][i]))
check("anim.png (APNG, 3 quadros)", ok)
p = os.path.join(T, "anim.webp")
rgba[0].save(p, save_all=True, append_images=rgba[1:], duration=[80, 90, 100], loop=4, lossless=True)
g = mos_decode(p)
ok = g and g["n"] == 3 and g["loop"] == 4
if ok:
    wi = Image.open(p)
    for i in range(3):
        wi.seek(i); ref = list(wi.convert("RGBA").get_flattened_data())
        if diff(ref, g["frames"][i]) > 0: ok = False; print("  quadro", i, diff(ref, g["frames"][i]))
check("anim.webp (3 quadros, loop 4)", ok)
p = os.path.join(T, "multi.tif")
frames[0].save(p, save_all=True, append_images=frames[1:3], compression="tiff_lzw")
g = mos_decode(p); check("multi.tif (3 páginas)", g and g["n"] == 3)

# EXIF orientação num JPEG
p = os.path.join(T, "ori.jpg")
ex = Image.Exif(); ex[0x0112] = 6
base.convert("RGB").save(p, "JPEG", exif=ex.tobytes(), dpi=(300, 300))
g = mos_decode(p); check("ori.jpg (Orientation 6, 300 dpi)", g and g["orient"] == 6 and abs(g["dpi"] - 300) < 0.5)

# gravadores: o mOS grava, o PIL lê
def write_raw(frames):
    w, h = frames[0].size
    b = b"%d %d %d\n" % (w, h, len(frames))
    for f in frames:
        b += b"".join(struct.pack("<I", a << 24 | r << 16 | g << 8 | bl) for (r, g, bl, a) in f.convert("RGBA").get_flattened_data())
    p = os.path.join(T, "in.raw"); open(p, "wb").write(b); return p

def enc_case(name, uti, frames, tol=0, extra=(), alpha=True, mean=False):
    p = os.path.join(T, name)
    r = subprocess.run([BIN, "encode", uti, write_raw(frames), p] + list(extra), capture_output=True)
    if r.returncode: return check(name, False, "não gravou")
    try:
        im = Image.open(p); im.load()
    except Exception as e:
        return check(name, False, str(e))
    n = getattr(im, "n_frames", 1)
    ok = n == len(frames)
    for i in range(min(n, len(frames))):
        im.seek(i)
        a, b = list(frames[i].convert("RGBA").get_flattened_data()), list(im.convert("RGBA").get_flattened_data())
        dd = mean_err(a, b) if mean else diff(a, b, alpha)
        if dd > tol: ok = False; print("  quadro", i, "dif", dd)
    check(name + " (gravado)", ok, "quadros %d" % n)

enc_case("w.png", "public.png", [base])
enc_case("wgray.png", "public.png", [base.convert("L").convert("RGBA")])
enc_case("w.jpg", "public.jpeg", [base.convert("RGB")], tol=3, extra=["0.95"], mean=True)
enc_case("w.gif", "com.compuserve.gif", [base.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=200).convert("RGB")])
enc_case("wa.gif", "com.compuserve.gif", frames)
enc_case("w.bmp", "com.microsoft.bmp", [base.convert("RGB")])
enc_case("wa.bmp", "com.microsoft.bmp", [base])
enc_case("w.tif", "public.tiff", [base])
enc_case("wlzw.tif", "public.tiff", [base, frames[1].convert("RGBA")], extra=["-1", "5"])
enc_case("wzip.tif", "public.tiff", [base], extra=["-1", "8"])
enc_case("wpb.tif", "public.tiff", [base], extra=["-1", "32773"])
enc_case("wa.png", "public.png", rgba)
# muitas cores no GIF: corte mediano, só confere que abre e o erro médio é razoável
big = Image.radial_gradient("L").convert("RGB").resize((128, 128))
big = Image.merge("RGB", (big.split()[0], big.split()[0].rotate(90), big.split()[0].rotate(180)))
p = os.path.join(T, "q.gif")
subprocess.run([BIN, "encode", "com.compuserve.gif", write_raw([big]), p])
q = list(Image.open(p).convert("RGB").get_flattened_data()); ref = list(big.get_flattened_data())
err = sum(abs(a - b) for x, y in zip(ref, q) for a, b in zip(x, y)) / (len(ref) * 3)
check("quantização GIF (erro médio %.1f)" % err, err < 6)

print("imageio_host: %s (%d falhas)" % ("ok" if not fails else "FALHOU", fails))
sys.exit(1 if fails else 0)
