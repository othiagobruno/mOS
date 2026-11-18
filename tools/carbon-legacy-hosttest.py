#!/usr/bin/env python3
"""mOS — teste no host do Resource Manager (resmgr.c), do Component Manager (component.c) e do Multiprocessing Services
(mpservices.c). O arquivo de recursos de entrada é montado aqui, no formato do Mac, e o que o resmgr.c grava é lido de
volta por um parser independente.

    python3 -I tools/carbon-legacy-hosttest.py

Imprime "carbon-legacy: ok"."""
import os, shutil, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LS = os.path.join(ROOT, "userland/libsystem")
T = tempfile.mkdtemp(prefix="carbon-")
SHIM = ("#pragma once\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdio.h>\n#include <unistd.h>\n"
        "#include <fcntl.h>\n#include <sys/types.h>\n"
        "typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;\n"
        "typedef int8_t i8; typedef int16_t i16; typedef int32_t i32; typedef int64_t i64;\n")
for h in ("sys.h", "libc.h"):
    open(f"{T}/{h}", "w").write(SHIM)
open(f"{T}/foundation.h", "w").write(SHIM + "typedef void *id;\nconst char *cx_cstr(id s);\n")
for f in ("resmgr.c", "component.c", "mpservices.c"):
    src = open(f"{LS}/{f}").read()
    # o resmgr.c usa os flags de open do macOS; no Linux O_TRUNC é outro valor
    src = src.replace("#define O_TRUNC_ 0x400", "#define O_TRUNC_ O_TRUNC").replace("#define O_CREAT_ 0x200", "#define O_CREAT_ O_CREAT")
    open(f"{T}/{f}", "w").write(src)
BIN = f"{T}/t"
r = subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-builtin-declaration-mismatch",
                    "-I" + T, f"{T}/resmgr.c", f"{T}/component.c", f"{T}/mpservices.c",
                    os.path.join(ROOT, "tools/carbon_legacy_host.c"), "-lpthread", "-o", BIN], capture_output=True, text=True)
if r.returncode:
    print(r.stderr); sys.exit(1)


def build(res):
    """res: lista de (tipo, id, nome ou None, attrs, dados) -> bytes do arquivo de recursos"""
    types = []
    for t, *_ in res:
        if t not in types: types.append(t)
    data = b""; names = b""; refs = {}
    for t, rid, name, attr, d in res:
        noff = 0xFFFF
        if name is not None:
            noff = len(names); names += bytes([len(name)]) + name
        refs.setdefault(t, []).append(struct.pack(">hHB", rid, noff, attr) + len(data).to_bytes(3, "big") + b"\0\0\0\0")
        data += struct.pack(">I", len(d)) + d
    tl = b""; rl = b""; base = 2 + 8 * len(types)
    for t in types:
        tl += t + struct.pack(">HH", len(refs[t]) - 1, base + len(rl))
        rl += b"".join(refs[t])
    tlist = struct.pack(">H", len(types) - 1) + tl + rl
    m = b"\0" * 22 + struct.pack(">HHH", 0, 28, 28 + len(tlist)) + tlist + names
    doff = 256; moff = doff + len(data)
    hdr = struct.pack(">IIII", doff, moff, len(data), len(m))
    return hdr + b"\0" * 240 + data + hdr + m[16:]


def parse(b):
    doff, moff, dlen, mlen = struct.unpack(">IIII", b[:16])
    m = b[moff:moff + mlen]
    tlo, nlo = struct.unpack(">HH", m[24:28])
    nt = struct.unpack(">H", m[tlo:tlo + 2])[0] + 1
    out = {}
    for i in range(nt):
        t, cnt, ro = struct.unpack(">4sHH", m[tlo + 2 + 8 * i: tlo + 10 + 8 * i])
        for k in range(cnt + 1):
            e = m[tlo + ro + 12 * k: tlo + ro + 12 * k + 12]
            rid, noff, attr = struct.unpack(">hHB", e[:5])
            off = int.from_bytes(e[5:8], "big")
            ln = struct.unpack(">I", b[doff + off: doff + off + 4])[0]
            name = None if noff == 0xFFFF else m[nlo + noff + 1: nlo + noff + 1 + m[nlo + noff]]
            out[(t, rid)] = (name, attr, b[doff + off + 4: doff + off + 4 + ln])
    return out


src = build([(b"STR ", 128, b"hello", 0x20, b"\x05Hello"), (b"icns", 1, None, 0, bytes(i & 0xFF for i in range(300))),
             (b"STR ", 129, b"two", 0, b"\x03abc")])
open(f"{T}/in.rsrc", "wb").write(src)
r = subprocess.run([BIN, f"{T}/in.rsrc", f"{T}/arquivo", f"{T}/out.rsrc"], capture_output=True, text=True, timeout=60)
print(r.stdout, end="")
ok = r.returncode == 0
got = parse(open(f"{T}/out.rsrc", "rb").read())
want_text = [k for k in got if k[0] == b"TEXT"]
checks = [
    (got.get((b"STR ", 128)) == (b"hello", 0x20, b"\x05HELLO"), "STR 128 alterado e gravado"),
    ((b"STR ", 129) not in got, "STR 129 removido"),
    (got.get((b"icns", 1)) == (None, 0, bytes(i & 0xFF for i in range(300))), "icns intacto"),
    (len(want_text) == 1 and got[want_text[0]] == (b"novo", 0, b"xyz"), "TEXT novo"),
]
for c, what in checks:
    if not c: ok = False; print("FALHOU:", what)
shutil.rmtree(T, ignore_errors=True) if ok else print("mantido:", T)
print("carbon-legacy: ok" if ok else "carbon-legacy: FALHOU")
sys.exit(0 if ok else 1)
