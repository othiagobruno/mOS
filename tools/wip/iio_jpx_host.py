#!/usr/bin/env python3
"""JPEG 2000 do mOS (iio_jpx.c) contra o openjpeg (PIL): imagens sintéticas e uma foto, codificadas com várias
opções (reversível/irreversível, resoluções, tiles, as 5 progressões, precintos, blocos, camadas, subamostragem,
16 bits), decodificadas pelos dois lados e comparadas amostra a amostra. Compilação: ver tools/iio_jpx_host.c."""
import struct, subprocess, sys, os
from PIL import Image, ImageDraw
HOST = sys.argv[1] if len(sys.argv) > 1 else "/tmp/mariana-jpx/jpxhost"
T = "/tmp/mariana-jpx"
def scene(w, h, mode="RGB"):
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = ((x * 7 + y * 3) % 256, (x * y) % 256, (255 - x * 2 + y) % 256)
    d = ImageDraw.Draw(im)
    d.ellipse((w // 5, h // 6, w * 3 // 4, h * 5 // 6), fill=(250, 40, 30), outline=(0, 0, 0))
    d.rectangle((w // 2, h // 3, w - 3, h - 3), fill=(20, 200, 90))
    d.line((0, h - 1, w - 1, 0), fill=(255, 255, 255), width=3)
    return im.convert(mode)
def mine(path):
    out = T + "/m.bin"
    r = subprocess.run([HOST, path, out], capture_output=True, text=True)
    if r.returncode: return None, r.stderr.strip()
    with open(out, "rb") as f:
        hdr = f.readline().split()
        w, h, nc = int(hdr[0]), int(hdr[1]), int(hdr[2])
        data = f.read()
    planes = [struct.unpack("<%di" % (w * h), data[c * w * h * 4:(c + 1) * w * h * 4]) for c in range(nc)]
    return (w, h, nc, planes), ""
def ref_planes(path):
    im = Image.open(path); im.load()
    bands = im.split() if im.mode not in ("I;16", "I") else [im]
    return im.size, [list(b.getdata()) for b in bands]
fails = 0
def case(name, im, exact, **opts):
    global fails
    path = "%s/%s.%s" % (T, name, "j2k" if opts.get("no_jp2") else "jp2")
    im.save(path, "JPEG2000", **opts)
    (rw, rh), rp = ref_planes(path)
    got, err = mine(path)
    if got is None:
        print("FAIL %-28s %s" % (name, err)); fails += 1; return
    w, h, nc, planes = got
    if (w, h) != (rw, rh) or nc != len(rp):
        print("FAIL %-28s tamanho %dx%dx%d × %dx%dx%d" % (name, w, h, nc, rw, rh, len(rp))); fails += 1; return
    worst = 0; total = 0; cnt = 0
    for c in range(nc):
        a, b = planes[c], rp[c]
        for i in range(len(a)):
            dlt = abs(a[i] - b[i]); worst = max(worst, dlt); total += dlt; cnt += 1
    ok = worst == 0 if exact else worst <= 1
    print("%s %-28s máx %d, média %.4f" % ("ok  " if ok else "FAIL", name, worst, total / cnt))
    fails += not ok
os.makedirs(T, exist_ok=True)
big = scene(173, 119)
case("rev_padrao", big, True)
case("rev_cinza", scene(64, 48, "L"), True)
case("rev_rgba", scene(70, 50, "RGBA"), True)
case("rev_1res", big, True, num_resolutions=1)
case("rev_tiles", big, True, tile_size=(64, 48))
case("rev_offset", big, True, offset=(5, 3), tile_size=(64, 64), tile_offset=(2, 1))
for prog in ("LRCP", "RLCP", "RPCL", "PCRL", "CPRL"):
    case("rev_" + prog, big, True, progression=prog, precinct_size=((32, 32), (32, 32), (64, 64)), quality_layers=[40, 20, 0], quality_mode="rates")
case("rev_blocos16", big, True, codeblock_size=(16, 16))
case("rev_blocos_ret", big, True, codeblock_size=(64, 8))
case("rev_j2k", big, True, no_jp2=True)
case("irr_padrao", big, False, irreversible=True)
case("irr_camadas", big, False, irreversible=True, quality_layers=[50, 20, 10], quality_mode="rates")
case("irr_tiles", big, False, irreversible=True, tile_size=(50, 40))
case("irr_cinza", scene(97, 61, "L"), False, irreversible=True)
case("irr_1res", scene(40, 30, "L"), False, irreversible=True, num_resolutions=1)
for prog in ("RPCL", "CPRL"):
    case("irr_" + prog, big, False, irreversible=True, progression=prog, precinct_size=((32, 32), (32, 32), (64, 64)), codeblock_size=(16, 16), num_resolutions=4, tile_size=(96, 64))
case("rev_impar", scene(7, 37), True)
case("rev_linha", scene(41, 7), True)
im16 = scene(60, 40, "L").point(lambda v: v * 250).convert("I;16")
case("rev_16bits", im16, True)

# ---- o que o PIL não expõe: chaves de modo do bloco, SOP/EPH, sinal, subamostragem e origem ímpar, pelo
# codificador do openjpeg via ctypes (sem perdas: a referência é a própria entrada)
import ctypes, array
oj = ctypes.CDLL("libopenjp2.so.7")
U = ctypes.c_uint32
class CmptParm(ctypes.Structure):
    _fields_ = [(n, U) for n in ("dx", "dy", "w", "h", "x0", "y0", "prec", "bpp", "sgnd")]
class Comp(ctypes.Structure):
    _fields_ = [(n, U) for n in ("dx", "dy", "w", "h", "x0", "y0", "prec", "bpp", "sgnd", "resno", "factor")] + \
               [("data", ctypes.POINTER(ctypes.c_int32)), ("alpha", ctypes.c_uint16)]
class Img(ctypes.Structure):
    _fields_ = [(n, U) for n in ("x0", "y0", "x1", "y1", "numcomps")] + [("cs", ctypes.c_int), ("comps", ctypes.POINTER(Comp)),
               ("icc", ctypes.c_void_p), ("icc_len", U)]
oj.opj_image_create.restype = ctypes.POINTER(Img)
oj.opj_create_compress.restype = ctypes.c_void_p
oj.opj_stream_create_default_file_stream.restype = ctypes.c_void_p
def oj_encode(path, w, h, comps, mode=0, csty=0, x0=0, y0=0, nres=4, cblk=(32, 32), irreversible=0, layers=1):
    """comps: lista de (dx, dy, prec, sgnd, função(x, y) → valor na grade da componente)"""
    parms = (CmptParm * len(comps))()
    for i, (dx, dy, prec, sg, fn) in enumerate(comps):
        cw = -(-(x0 + w) // dx) - (-(-x0 // dx)); ch = -(-(y0 + h) // dy) - (-(-y0 // dy))
        parms[i] = CmptParm(dx, dy, cw, ch, x0, y0, prec, prec, sg)
    img = oj.opj_image_create(len(comps), parms, 1 if len(comps) == 1 else 2)
    im = img.contents
    im.x0, im.y0, im.x1, im.y1 = x0, y0, x0 + w, y0 + h
    want = []
    for i, (dx, dy, prec, sg, fn) in enumerate(comps):
        c = im.comps[i]
        vals = [fn(x, y) for y in range(c.h) for x in range(c.w)]
        for k, v in enumerate(vals): c.data[k] = v
        # referência na grade de referência (vizinho mais próximo, como o iio_jpx)
        cx0, cy0 = -(-x0 // dx), -(-y0 // dy)
        want.append([vals[max(0, (y0 + y) // dy - cy0) * c.w + max(0, (x0 + x) // dx - cx0)] for y in range(h) for x in range(w)])
    buf = ctypes.create_string_buffer(200000)
    oj.opj_set_default_encoder_parameters(buf)
    ints = array.array("i", bytes(buf.raw[:20000]))
    o = next(i for i in range(len(ints) - 2) if ints[i:i + 3].tolist() == [6, 64, 64]) * 4
    put = lambda off, v: ctypes.memmove(ctypes.addressof(buf) + off, ctypes.byref(ctypes.c_int(v)), 4)
    put(o, nres); put(o + 4, cblk[0]); put(o + 8, cblk[1]); put(o + 12, mode); put(o + 16, irreversible)
    put(o - 804, layers)                      # tcp_numlayers (tcp_rates[0] = 0: sem perdas)
    put(20, 1)                                # cp_disto_alloc
    put(48, csty)                             # SOP/EPH
    codec = oj.opj_create_compress(0)
    assert oj.opj_setup_encoder(ctypes.c_void_p(codec), buf, img)
    st = oj.opj_stream_create_default_file_stream(path.encode(), 0)
    ok = oj.opj_start_compress(ctypes.c_void_p(codec), img, ctypes.c_void_p(st)) and oj.opj_encode(ctypes.c_void_p(codec), ctypes.c_void_p(st)) \
        and oj.opj_end_compress(ctypes.c_void_p(codec), ctypes.c_void_p(st))
    oj.opj_stream_destroy(ctypes.c_void_p(st)); oj.opj_destroy_codec(ctypes.c_void_p(codec)); oj.opj_image_destroy(img)
    assert ok, "openjpeg não codificou " + path
    return want
def case_raw(name, w, h, comps, **kw):
    global fails
    path = "%s/%s.j2k" % (T, name)
    want = oj_encode(path, w, h, comps, **kw)
    got, err = mine(path)
    if got is None: print("FAIL %-28s %s" % (name, err)); fails += 1; return
    gw, gh, nc, planes = got
    bad = (gw, gh, nc) != (w, h, len(comps)) or any(list(planes[c]) != want[c] for c in range(nc))
    diff = 0 if bad is False else sum(1 for c in range(min(nc, len(want))) for a, b in zip(planes[c], want[c]) if a != b)
    print("%s %-28s %s" % ("FAIL" if bad else "ok  ", name, "%d amostras diferentes" % diff if bad else "igual à entrada"))
    fails += bool(bad)
gray = [(1, 1, 8, 0, lambda x, y: (x * 9 + y * 13 + (x * y) % 7) % 256)]
for mode, nm in ((1, "bypass"), (2, "reset"), (4, "termall"), (8, "vsc"), (16, "pterm"), (32, "segsym"), (63, "todos_modos"), (5, "bypass_termall")):
    case_raw("modo_" + nm, 67, 45, gray, mode=mode)
case_raw("sop_eph", 67, 45, gray, csty=6, layers=1)
case_raw("sinal_12bits", 40, 30, [(1, 1, 12, 1, lambda x, y: ((x * 97 + y * 31) % 4096) - 2048)])
case_raw("sub_420_origem", 61, 43, [(1, 1, 8, 0, lambda x, y: (x * 5 + y) % 256), (2, 2, 8, 0, lambda x, y: (x * 11) % 256),
         (2, 2, 8, 0, lambda x, y: (y * 17) % 256)], x0=3, y0=1)
case_raw("sub_origem_1res", 61, 43, [(1, 1, 8, 0, lambda x, y: (x * 5 + y) % 256), (2, 2, 8, 0, lambda x, y: (x * 11) % 256)], x0=3, y0=1, nres=1)
case_raw("sub_horizontal", 50, 20, [(1, 1, 8, 0, lambda x, y: (x + y * 3) % 256), (2, 1, 8, 0, lambda x, y: (x * 7) % 256)])
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
