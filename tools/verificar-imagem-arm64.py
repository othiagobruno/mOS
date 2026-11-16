#!/usr/bin/env python3
"""Teste de host da ESP: tudo o que o mOS instala para os apps existe também para o processo arm64.

O processo arm64 traduzido é inteiro arm64 (docs/tradutor-arm64.md, "Por que o processo inteiro é arm64"): uma dylib
ou framework que só tem fatia x86_64 não liga num app arm64. Este teste confere, sem QEMU:

1. /Sistema/Biblioteca/a64/a64rt.mos é ELF x86_64 e /Sistema/Biblioteca/a64/libSystem.mos é ELF AArch64 (o que
   kernel/compat/macho.c lê em A64RT_PATH e A64RT_LIBSYSTEM).
2. Toda biblioteca Mach-O (MH_DYLIB, MH_BUNDLE) da ESP fora de /Aplicativos e /Users tem fatia arm64.
   Executáveis não entram: um executável só x86_64 roda como processo x86_64.
3. Cada LC_LOAD_DYLIB não fraco da fatia arm64 resolve: nome do libSystem (libsystem_dylib_ok em
   kernel/compat/macho.c, lida de lá) ou arquivo da ESP que também tem fatia arm64.

  tools/verificar-imagem-arm64.py <esp>              # sai 1 se faltar algo
  tools/verificar-imagem-arm64.py --relatorio <esp>  # só lista, sai 0
"""
import os
import re
import struct
import sys

CPU_X86_64, CPU_ARM64 = 0x01000007, 0x0100000C
MH_DYLIB, MH_BUNDLE = 6, 8
LC_LOAD = {0x0C: "load", 0x80000018: "weak", 0x8000001F: "reexport", 0x20: "lazy", 0x80000023: "upward"}
LC_RPATH = 0x8000001C
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def fatias(data):
    """{cpu: bytes} de um Mach-O de 64 bits fino ou universal; {} se não é Mach-O"""
    if len(data) < 32:
        return {}
    be = struct.unpack_from(">I", data, 0)[0]
    if be in (0xCAFEBABE, 0xCAFEBABF):
        b64 = be == 0xCAFEBABF
        n = struct.unpack_from(">I", data, 4)[0]
        if n == 0 or n > 32:
            return {}   # 0xCAFEBABE também é class do Java
        out = {}
        for i in range(n):
            if b64:
                cpu, _, off, size, _ = struct.unpack_from(">IIQQI", data, 8 + 32 * i)
            else:
                cpu, _, off, size, _ = struct.unpack_from(">IIIII", data, 8 + 20 * i)
            if off + size <= len(data):
                out.setdefault(cpu, data[off:off + size])
        return out
    if struct.unpack_from("<I", data, 0)[0] == 0xFEEDFACF:
        return {struct.unpack_from("<I", data, 4)[0]: data}
    return {}


def cabecalho(fat):
    """(filetype, [(tipo, nome)], [rpath])"""
    ft, ncmds = struct.unpack_from("<II", fat, 12)[0], struct.unpack_from("<I", fat, 16)[0]
    deps, rpaths = [], []
    off = 32
    for _ in range(ncmds):
        if off + 8 > len(fat):
            break
        cmd, size = struct.unpack_from("<II", fat, off)
        if size < 8:
            break
        if cmd in LC_LOAD or cmd == LC_RPATH:
            s = struct.unpack_from("<I", fat, off + 8)[0]
            nome = fat[off + s:off + size].split(b"\0", 1)[0].decode("utf-8", "replace")
            if cmd == LC_RPATH:
                rpaths.append(nome)
            else:
                deps.append((LC_LOAD[cmd], nome))
        off += size
    return ft, deps, rpaths


def libsystem_substitui(root=ROOT):
    """(nomes exatos, prefixos de framework) de libsystem_dylib_ok() em kernel/compat/macho.c"""
    with open(os.path.join(root, "kernel", "compat", "macho.c"), encoding="utf-8", errors="replace") as f:
        src = f.read()
    m = re.search(r"bool libsystem_dylib_ok\(const char \*n\) \{(.*?)\n\}", src, re.S)
    if not m:
        raise SystemExit("verificar-imagem-arm64: não achei libsystem_dylib_ok em kernel/compat/macho.c")
    corpo = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    nomes = re.findall(r'"(/[^"]+)"', corpo)
    exatos = {n for n in nomes if not n.endswith("/")}
    prefixos = tuple(n for n in nomes if n.endswith("/"))
    return exatos, prefixos


def substituido(nome, exatos, prefixos):
    if nome.startswith("/System/iOSSupport"):
        nome = nome[len("/System/iOSSupport"):]
    return nome in exatos or nome.startswith(prefixos)


def esp_mos(esp):
    """raiz do sistema dentro da ESP (a pasta mOS da imagem, ou a própria pasta se já for ela)"""
    m = os.path.join(esp, "mOS")
    return m if os.path.isdir(m) else esp


def candidatos(nome, quem_rel, rpaths, mos):
    """caminhos relativos à raiz do mOS que o carregador tentaria para um LC_LOAD_DYLIB"""
    base = os.path.dirname("/" + quem_rel)
    if nome.startswith("@rpath/"):
        resto = nome[len("@rpath/"):]
        out = []
        for r in rpaths:
            r = r.replace("@loader_path", base).replace("@executable_path", base)
            out.append(os.path.normpath(os.path.join(r, resto)))
        out.append(os.path.normpath(os.path.join(base, resto)))
        return [p.lstrip("/") for p in out]
    if nome.startswith("@loader_path/") or nome.startswith("@executable_path/"):
        return [os.path.normpath(os.path.join(base, nome.split("/", 1)[1])).lstrip("/")]
    p = nome.lstrip("/")
    out = [p]
    # o FAT não guarda symlinks: Versions/A e Versions/Current são tentados os dois (link_framework no macho.c)
    if "/Versions/Current/" in p:
        out.append(p.replace("/Versions/Current/", "/Versions/A/"))
    if "/Versions/A/" in p:
        out.append(p.replace("/Versions/A/", "/Versions/Current/"))
    m = re.match(r"(.*/([^/]+)\.framework)/\2$", p)
    if m:
        out.append(m.group(1) + "/Versions/A/" + m.group(2))
    return out


def verificar(esp, root=ROOT):
    """lista de problemas (texto) e quantas bibliotecas foram conferidas"""
    mos = esp_mos(esp)
    prob = []
    a64 = os.path.join(mos, "Sistema", "Biblioteca", "a64")
    for nome, maq, desc in (("a64rt.mos", 62, "x86_64"), ("libSystem.mos", 183, "AArch64")):
        p = os.path.join(a64, nome)
        try:
            with open(p, "rb") as f:
                h = f.read(20)
        except OSError:
            prob.append("Sistema/Biblioteca/a64/%s: não está na imagem" % nome)
            continue
        if h[:4] != b"\x7fELF" or len(h) < 20 or struct.unpack_from("<H", h, 18)[0] != maq:
            prob.append("Sistema/Biblioteca/a64/%s: não é ELF %s" % (nome, desc))
    exatos, prefixos = libsystem_substitui(root)
    libs = {}
    for d, subdirs, files in os.walk(mos):
        rel_d = os.path.relpath(d, mos)
        top = rel_d.split(os.sep, 1)[0]
        if top in ("Aplicativos", "Applications", "Users"):
            subdirs[:] = []
            continue
        for fn in files:
            p = os.path.join(d, fn)
            if os.path.islink(p) or not os.path.isfile(p):
                continue
            try:
                with open(p, "rb") as f:
                    if f.read(4) not in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"):
                        continue
                    f.seek(0)
                    data = f.read()
            except OSError:
                continue
            fs = fatias(data)
            if not fs:
                continue
            rel = os.path.relpath(p, mos).replace(os.sep, "/")
            ft = struct.unpack_from("<I", next(iter(fs.values())), 12)[0]
            if ft in (MH_DYLIB, MH_BUNDLE):
                libs[rel] = fs
    for rel in sorted(libs):
        fs = libs[rel]
        if CPU_ARM64 not in fs:
            prob.append("%s: sem fatia arm64 (só %s)" % (rel, ", ".join(
                {CPU_X86_64: "x86_64"}.get(c, "0x%X" % c) for c in sorted(fs))))
            continue
        _, deps, rpaths = cabecalho(fs[CPU_ARM64])
        for tipo, nome in deps:
            if tipo == "weak" or substituido(nome, exatos, prefixos):
                continue
            achou = None
            for c in candidatos(nome, rel, rpaths, mos):
                if c in libs:
                    achou = c
                    break
            if achou is None:
                prob.append("%s (arm64): depende de %s, que não está na imagem" % (rel, nome))
            elif CPU_ARM64 not in libs[achou]:
                prob.append("%s (arm64): depende de %s, que não tem fatia arm64" % (rel, nome))
    return prob, len(libs)


def main(argv):
    rel = "--relatorio" in argv
    args = [a for a in argv if a != "--relatorio"]
    if len(args) != 1:
        print(__doc__.strip().splitlines()[-2].strip(), file=sys.stderr)
        return 2
    prob, n = verificar(args[0])
    for p in prob:
        print("arm64: " + p)
    print("verificar-imagem-arm64: %d biblioteca(s), %d problema(s)" % (n, len(prob)))
    return 0 if rel or not prob else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
