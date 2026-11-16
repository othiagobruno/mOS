#!/usr/bin/env python3
"""XMP do ImageIO (iio_xmp.c) no host: roda tools/iio_xmp_host.c sobre JPEG/PNG/TIFF/GIF feitos pelo PIL e confere que
o PIL abre os arquivos com o XMP embutido e acha o pacote onde a norma manda (APP1, iTXt, tag 700, extensão).
  gcc -O1 -DXMP_HOST -Iuserland/libsystem tools/iio_xmp_host.c userland/libsystem/iio_xmp.c -o /tmp/iio_xmp_host
  python3 tools/iio_xmp_host.py /tmp/iio_xmp_host"""
import os, subprocess, sys
from PIL import Image
HOST = sys.argv[1] if len(sys.argv) > 1 else "/tmp/iio_xmp_host"
D = "/tmp/mariana-xmp"
os.makedirs(D, exist_ok=True)
im = Image.new("RGB", (16, 8), (200, 100, 50))
im.save(D + "/a.jpg"); im.save(D + "/a.png"); im.save(D + "/a.tif"); im.convert("P").save(D + "/a.gif")
r = subprocess.run([HOST, D], capture_output=True, text=True, env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
print(r.stdout, end="")
fails = 0 if r.returncode == 0 else 1
def check(ok, m):
    global fails
    print(("ok   " if ok else "FAIL ") + m); fails += not ok
for name, getter in [("xmp-a.jpg", lambda i: i.info.get("xmp")), ("xmp-a.png", lambda i: i.info.get("XML:com.adobe.xmp")),
                     ("xmp-a.tif", lambda i: i.tag_v2.get(700))]:
    try:
        i = Image.open(os.path.join(D, name)); i.load()
        x = getter(i)
        if isinstance(x, str): x = x.encode()
        check(x is not None and b"Sunset" in bytes(x) and i.size == (16, 8), "PIL lê %s com o XMP no lugar da norma" % name)
    except Exception as e:
        check(False, "PIL lê %s (%s)" % (name, e))
# GIF: o PIL não expõe XMP de GIF; confere que o arquivo continua válido (a extensão foi bem pulada)
g = Image.open(os.path.join(D, "xmp-a.gif")); g.load()
check(g.size == (16, 8) and g.getpixel((3, 3)) == Image.open(os.path.join(D, "a.gif")).getpixel((3, 3)), "PIL abre o GIF com a extensão XMP e os pixels são os mesmos")
print("%d falha(s)" % fails if fails else "todos ok")
sys.exit(1 if fails else 0)
