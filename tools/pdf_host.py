#!/usr/bin/env python3
"""Confere o núcleo de PDF do mOS em PDFs gerados aqui (xref clássica, xref stream + object stream, atualização
incremental, xref quebrada, filtros, as 4 revisões de criptografia) e em PDFs do Ghostscript.
gcc -O1 -DPDF_HOST -Iuserland/libsystem tools/pdf_host.c userland/libsystem/pdf_core.c -lz -o /tmp/pdf_host
python3 tools/pdf_host.py [/tmp/pdf_host]"""
import hashlib, os, struct, subprocess, sys, tempfile, zlib
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
try:
    from cryptography.hazmat.decrepit.ciphers.algorithms import ARC4
except ImportError:
    from cryptography.hazmat.primitives.ciphers.algorithms import ARC4

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/pdf_host"
T = tempfile.mkdtemp(prefix="pdf-")
fails = 0

def info(path, pw=None):
    r = subprocess.run([BIN, "info", path] + ([pw] if pw is not None else []), capture_output=True, text=True, timeout=60)
    out = {}
    for line in r.stdout.splitlines():
        if "=" in line:
            k, v = line.split("=", 1); out[k] = v
    return out

def check(name, ok, extra=""):
    global fails
    print(("ok   " if ok else "FALHA ") + name + ("" if ok else "  " + extra))
    if not ok: fails += 1

PAD = bytes([0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
             0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80, 0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A])
def rc4(k, d): return Cipher(ARC4(k), None).encryptor().update(d)
def aes_cbc(k, iv, d):
    p = 16 - len(d) % 16; d = d + bytes([p]) * p
    return iv + Cipher(algorithms.AES(k), modes.CBC(iv)).encryptor().update(d)

class Enc:
    """security handler padrão: R2 (RC4-40), R3 (RC4-128), R4 (AESV2), R6 (AESV3)"""
    def __init__(self, R, user, owner, fid):
        self.R, self.fid, self.P = R, fid, -3904
        if R == 6:
            self.key = os.urandom(32)
            def h(pw, salt, udata=b""):
                K = hashlib.sha256(pw + salt + udata).digest(); i = 0
                while True:
                    K1 = (pw + K + udata) * 64
                    E = Cipher(algorithms.AES(K[:16]), modes.CBC(K[16:32])).encryptor().update(K1)
                    m = sum(E[:16]) % 3
                    K = (hashlib.sha256, hashlib.sha384, hashlib.sha512)[m](E).digest()
                    if i >= 63 and E[-1] <= i - 32: break
                    i += 1
                return K[:32]
            uv, uk, ov, ok = os.urandom(8), os.urandom(8), os.urandom(8), os.urandom(8)
            self.U = h(user, uv) + uv + uk
            self.UE = Cipher(algorithms.AES(h(user, uk)), modes.CBC(bytes(16))).encryptor().update(self.key)
            self.O = h(owner, ov, self.U) + ov + ok
            self.OE = Cipher(algorithms.AES(h(owner, ok, self.U)), modes.CBC(bytes(16))).encryptor().update(self.key)
            return
        n = 5 if R == 2 else 16
        op = (owner + PAD)[:32]; hk = hashlib.md5(op).digest()
        if R >= 3:
            for _ in range(50): hk = hashlib.md5(hk).digest()
        hk = hk[:n]
        up = (user + PAD)[:32]
        O = rc4(hk, up)
        if R >= 3:
            for i in range(1, 20): O = rc4(bytes(b ^ i for b in hk), O)
        self.O = O
        k = hashlib.md5(up + O + struct.pack("<i", self.P) + fid).digest()
        if R >= 3:
            for _ in range(50): k = hashlib.md5(k[:n]).digest()
        self.key = k[:n]
        if R == 2: self.U = rc4(self.key, PAD)
        else:
            u = rc4(self.key, hashlib.md5(PAD + fid).digest())
            for i in range(1, 20): u = rc4(bytes(b ^ i for b in self.key), u)
            self.U = u + bytes(16)
    def objkey(self, num, gen, aes):
        if self.R == 6: return self.key
        b = self.key + struct.pack("<I", num)[:3] + struct.pack("<H", gen) + (b"sAlT" if aes else b"")
        return hashlib.md5(b).digest()[:min(len(self.key) + 5, 16)]
    def enc(self, num, data):
        aes = self.R >= 4
        k = self.objkey(num, 0, aes)
        return aes_cbc(k, os.urandom(16), data) if aes else rc4(k, data)
    def dict(self):
        hx = lambda b: "<" + b.hex() + ">"
        if self.R == 6:
            return ("<< /Filter /Standard /V 5 /R 6 /Length 256 /CF << /StdCF << /CFM /AESV3 /AuthEvent /DocOpen /Length 32 >> >> "
                    "/StmF /StdCF /StrF /StdCF /O %s /U %s /OE %s /UE %s /P %d /Perms %s >>" % (hx(self.O), hx(self.U), hx(self.OE), hx(self.UE), self.P, hx(bytes(16))))
        if self.R == 4:
            return ("<< /Filter /Standard /V 4 /R 4 /Length 128 /CF << /StdCF << /CFM /AESV2 /AuthEvent /DocOpen /Length 16 >> >> "
                    "/StmF /StdCF /StrF /StdCF /O %s /U %s /P %d >>" % (hx(self.O), hx(self.U), self.P))
        return "<< /Filter /Standard /V %d /R %d /Length %d /O %s /U %s /P %d >>" % (1 if self.R == 2 else 2, self.R, 40 if self.R == 2 else 128, hx(self.O), hx(self.U), self.P)

def build(objs, mode="table", enc=None, info_title=None, fid=b"0123456789abcdef", broken=False):
    """objs: lista (corpo do dicionário, stream ou None); objeto 1 = catálogo. Devolve bytes."""
    out = bytearray(b"%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")
    offs = {}
    allobjs = list(objs)
    if info_title is not None:
        allobjs.append(("<< /Title %s >>", None, info_title))
    if enc:
        allobjs.append(("__ENC__", None, None))
    n_info = len(objs) + 1 if info_title is not None else None
    n_enc = len(allobjs) if enc else None
    for i, ob in enumerate(allobjs, 1):
        body, stream = ob[0], ob[1]
        if body == "__ENC__": body = enc.dict()
        elif len(ob) > 2 and ob[2] is not None:
            t = ob[2].encode("latin-1")
            if enc: t = enc.enc(i, t)
            body = body % ("<" + t.hex() + ">")
        offs[i] = len(out)
        if stream is not None:
            data = enc.enc(i, stream) if enc else stream
            out += b"%d 0 obj\n" % i + body.replace("LEN", str(len(data))).encode() + b"\nstream\n" + data + b"\nendstream\nendobj\n"
        else:
            out += b"%d 0 obj\n" % i + body.encode() + b"\nendobj\n"
    size = len(allobjs) + 1
    trailer = "/Size %d /Root 1 0 R /ID [<%s> <%s>]" % (size, fid.hex(), fid.hex())
    if n_info: trailer += " /Info %d 0 R" % n_info
    if n_enc: trailer += " /Encrypt %d 0 R" % n_enc
    xpos = len(out)
    if mode == "table":
        out += b"xref\n0 %d\n0000000000 65535 f \n" % size
        for i in range(1, size):
            o = offs[i] + (7 if broken else 0)
            out += b"%010d 00000 n \n" % o
        out += b"trailer\n<< " + trailer.encode() + b" >>\nstartxref\n%d\n%%%%EOF\n" % xpos
    else:
        rows = b"\x00\x00\x00\xff\xff" + b"".join(b"\x01" + struct.pack(">I", offs[i]) for i in range(1, size))
        rows = b"\x00" + struct.pack(">I", 0)[1:] + b"\xff"
        rows = b"".join([bytes([0]) + (0).to_bytes(4, "big")] + [bytes([1]) + offs[i].to_bytes(4, "big") for i in range(1, size)] + [bytes([1]) + xpos.to_bytes(4, "big")])
        z = zlib.compress(rows)
        out += b"%d 0 obj\n<< /Type /XRef /W [1 4 0] /Size %d %s /Filter /FlateDecode /Length %d >>\nstream\n" % (size, size + 1, trailer.replace("/Size %d" % size, "").encode(), len(z)) + z + b"\nendstream\nendobj\nstartxref\n%d\n%%%%EOF\n" % xpos
    return bytes(out)

def page_objs(content, extra_res=""):
    z = zlib.compress(content)
    return [("<< /Type /Catalog /Pages 2 0 R >>", None),
            ("<< /Type /Pages /Kids [3 0 R] /Count 1 /MediaBox [0 0 612 792] /Rotate 90 >>", None),
            ("<< /Type /Page /Parent 2 0 R /Contents 4 0 R /Resources << %s >> >>" % extra_res, None),
            ("<< /Length LEN /Filter /FlateDecode >>", z)]

CONTENT = b"0 0 1 rg 10 10 100 50 re f BT /F1 12 Tf 72 700 Td (Ol\\341 mundo) Tj ET"
def write(name, data):
    p = os.path.join(T, name); open(p, "wb").write(data); return p

def basic_checks(name, i, pages=1, title="Teste"):
    check(name + ": abre", i.get("open") == "1", str(i))
    check(name + ": 1 página com MediaBox herdada", i.get("pages") == str(pages) and i.get("page0.box") == "0 0 612 792", str(i))
    check(name + ": Rotate herdado", i.get("page0.rotate") == "90", i.get("page0.rotate", ""))
    check(name + ": conteúdo decodificado", i.get("page0.content", "").startswith("0 0 1 rg 10 10 100 50 re f"), i.get("page0.content", ""))
    check(name + ": léxico (8 operadores, 10 números, 1 string)", i.get("page0.ops") == "8 nums=10 strs=1", i.get("page0.ops", ""))
    if title: check(name + ": /Info /Title", i.get("title") == title, i.get("title", ""))

i = info(write("table.pdf", build(page_objs(CONTENT), info_title="Teste")))
basic_checks("xref clássica", i)
i = info(write("xstm.pdf", build(page_objs(CONTENT), mode="stream", info_title="Teste")))
basic_checks("xref stream", i)
i = info(write("broken.pdf", build(page_objs(CONTENT), broken=True, info_title="Teste")))
basic_checks("xref com deslocamentos errados (reparo)", i)
# sem xref nenhuma
data = build(page_objs(CONTENT), info_title="Teste")
cut = data[:data.index(b"xref")] + b"%%EOF\n"
i = info(write("noxref.pdf", cut))
check("sem xref nem trailer: reparo acha o catálogo", i.get("open") == "1" and i.get("pages") == "1", str(i))
# atualização incremental: o objeto 4 (conteúdo) muda numa seção nova com /Prev
base = build(page_objs(CONTENT), info_title="Teste")
prev = int(base[base.rindex(b"startxref") + 10:].split()[0])
new = zlib.compress(b"1 0 0 rg 0 0 5 5 re f")
upd = bytearray(base)
o4 = len(upd)
upd += b"4 0 obj\n<< /Length %d /Filter /FlateDecode >>\nstream\n" % len(new) + new + b"\nendstream\nendobj\n"
x2 = len(upd)
upd += b"xref\n4 1\n%010d 00000 n \ntrailer\n<< /Size 6 /Root 1 0 R /Info 5 0 R /Prev %d >>\nstartxref\n%d\n%%%%EOF\n" % (o4, prev, x2)
i = info(write("incr.pdf", bytes(upd)))
check("atualização incremental: o objeto novo ganha", i.get("page0.content", "").startswith("1 0 0 rg 0 0 5 5 re f"), i.get("page0.content", ""))
check("atualização incremental: o resto vem da seção antiga", i.get("title") == "Teste", i.get("title", ""))
# filtros encadeados e predictor PNG
import base64
raw = CONTENT
def png_pred(d, cols):
    rows = [d[k:k + cols] for k in range(0, len(d), cols)]
    out = b""
    prevr = bytes(cols)
    for r in rows:
        r = r.ljust(cols, b" ")
        out += b"\x02" + bytes((r[j] - prevr[j]) & 255 for j in range(cols)); prevr = r
    return out
cols = 8
padded = raw.ljust((len(raw) + cols - 1) // cols * cols, b" ")
fl = zlib.compress(png_pred(padded, cols))
a85 = base64.a85encode(fl, adobe=False) + b"~>"
objs = page_objs(raw)
objs[3] = ("<< /Length LEN /Filter [/ASCII85Decode /FlateDecode] /DecodeParms [null << /Predictor 12 /Columns %d >>] >>" % cols, a85)
i = info(write("filters.pdf", build(objs, info_title="Teste")))
basic_checks("ASCII85 + Flate + predictor PNG", i)
objs = page_objs(raw)
objs[3] = ("<< /Length LEN /Filter /ASCIIHexDecode >>", raw.hex().encode() + b">")
i = info(write("ahx.pdf", build(objs, info_title="Teste")))
basic_checks("ASCIIHexDecode", i)
def rle(d):
    o = b""
    for k in range(0, len(d), 100): ch = d[k:k + 100]; o += bytes([len(ch) - 1]) + ch
    return o + b"\x80"
objs = page_objs(raw)
objs[3] = ("<< /Length LEN /Filter /RunLengthDecode >>", rle(raw))
i = info(write("rl.pdf", build(objs, info_title="Teste")))
basic_checks("RunLengthDecode", i)
def lzw(d):
    tbl = {bytes([k]): k for k in range(256)}; nxt = 258; size = 9; bits = []; w = b""
    def put(c, s): bits.append((c, s))
    put(256, size)
    for b in d:
        wc = w + bytes([b])
        if wc in tbl: w = wc; continue
        put(tbl[w], size); tbl[wc] = nxt; nxt += 1
        if nxt + 1 >= (1 << size) and size < 12: size += 1
        w = bytes([b])
    put(tbl[w], size); nxt += 1
    if nxt + 1 >= (1 << size) and size < 12: size += 1
    put(257, size)
    acc = 0; nb = 0; out = bytearray()
    for c, s in bits:
        acc = acc << s | c; nb += s
        while nb >= 8: out.append(acc >> (nb - 8) & 255); nb -= 8
    if nb: out.append(acc << (8 - nb) & 255)
    return bytes(out)
objs = page_objs(raw)
objs[3] = ("<< /Length LEN /Filter /LZWDecode >>", lzw(raw))
i = info(write("lzw.pdf", build(objs, info_title="Teste")))
basic_checks("LZWDecode", i)
# criptografia
for R in (2, 3, 4, 6):
    fid = os.urandom(16)
    e = Enc(R, b"usuario", b"dono", fid)
    p = write("enc%d.pdf" % R, build(page_objs(CONTENT), enc=e, info_title="Teste", fid=fid))
    i = info(p)
    check("R%d: cifrado e trancado sem senha" % R, i.get("encrypted") == "1" and i.get("unlocked") == "0", str(i))
    i = info(p, "usuario")
    check("R%d: senha de usuário destrava" % R, i.get("unlock") == "1", str(i))
    basic_checks("R%d (usuário)" % R, i)
    i = info(p, "dono")
    check("R%d: senha de dono destrava" % R, i.get("unlock") == "1" and i.get("page0.content", "").startswith("0 0 1 rg"), str(i))
    i = info(p, "errada")
    check("R%d: senha errada não destrava" % R, i.get("unlock") == "0", str(i))
    e2 = Enc(R, b"", b"dono", fid)
    i = info(write("encopen%d.pdf" % R, build(page_objs(CONTENT), enc=e2, info_title="Teste", fid=fid)))
    check("R%d: senha de usuário vazia abre sozinho" % R, i.get("unlocked") == "1" and i.get("title") == "Teste", str(i))
# Ghostscript: PDFs de verdade (xref stream e object streams no 1.5+, RC4 128)
ps = os.path.join(T, "t.ps")
open(ps, "w").write("%!PS\n/Helvetica findfont 24 scalefont setfont\n72 700 moveto (pagina um) show showpage\n72 700 moveto (pagina dois) show showpage\n")
for name, extra in [("gs14.pdf", ["-dCompatibilityLevel=1.4"]), ("gs17.pdf", ["-dCompatibilityLevel=1.7"]),
                    ("gsenc.pdf", ["-dCompatibilityLevel=1.4", "-sOwnerPassword=dono", "-sUserPassword=usu", "-dEncryptionR=3", "-dKeyLength=128"])]:
    out = os.path.join(T, name)
    r = subprocess.run(["gs", "-q", "-dNOPAUSE", "-dBATCH", "-dSAFER", "-sDEVICE=pdfwrite", "-sOutputFile=" + out] + extra + [ps], capture_output=True)
    if r.returncode: print("aviso: gs falhou em", name); continue
    i = info(out, "usu" if "enc" in name else None)
    check("Ghostscript %s: 2 páginas, conteúdo legível" % name, i.get("pages") == "2" and int(i.get("page0.ops", "0 ").split()[0]) > 3, str(i)[:200])
print("pdf_host: %s (%d falhas)" % ("ok" if not fails else "FALHOU", fails))
sys.exit(1 if fails else 0)
