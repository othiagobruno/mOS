#!/usr/bin/env python3
"""Teste de host da imagem: bundles, Mach-O e bibliotecas reais.

Recusa dylib cujo conteúdo é o stub de tools/gen-libswift-stubs.c
("STUB mOS libswift"). Não sobe QEMU. Sai 1 se faltar peça real.

As dylibs de /usr/lib e os programas de /usr/bin compilados no build
(libsqlite3, libcurl, libz, git, security) têm de ser Mach-O x86_64 com o
install name certo, e cada LC_LOAD_DYLIB deles tem de estar na imagem ou
na lista de nomes que o libSystem.mos substitui (libsystem_dylib_ok em
kernel/compat/macho.c, lida daqui para não duplicar a lista).
"""
import os
import re
import struct
import sys

MAGIC = (
    b"\xcf\xfa\xed\xfe",
    b"\xfe\xed\xfa\xcf",
    b"\xca\xfe\xba\xbe",
    b"\xbe\xba\xfe\xca",
)
STUB = b"STUB mOS libswift"

APPS = (
    "Claude.app/Contents/MacOS/Claude",
    "Cursor.app/Contents/MacOS/Cursor",
    "Slack.app/Contents/MacOS/Slack",
    "Grok Bot.app/Contents/MacOS/Grok Bot",
    "ChatGPT.app/Contents/MacOS/ChatGPT",
    "Visual Studio Code.app/Contents/MacOS/Code",
    "WhatsApp.app/Contents/MacOS/WhatsApp",
    "OpenBot.app/Contents/MacOS/OpenBot",
    "Codex CLI/codex",
)
# Compilados de fonte neste build. libswift* gerada por gen-libswift-stubs.c
# não entra nesta lista: o teste só a classifica.
REAL_DYLIB = (
    "usr/lib/libc++.1.dylib",
    "usr/lib/libz.1.dylib",
    "usr/lib/libsqlite3.dylib",
    "usr/lib/libcurl.4.dylib",
)
# caminho na imagem -> install name (LC_ID_DYLIB) esperado; x86_64 obrigatório
INSTALL_NAME = {
    "usr/lib/libz.1.dylib": "/usr/lib/libz.1.dylib",
    "usr/lib/libsqlite3.dylib": "/usr/lib/libsqlite3.dylib",
    "usr/lib/libcurl.4.dylib": "/usr/lib/libcurl.4.dylib",
}
# executáveis Mach-O x86_64 do build cujas dependências têm de estar resolvidas na imagem
PROGRAMAS = (
    "usr/bin/security",
    "usr/bin/git",
    "usr/libexec/git-core/git-remote-http",
    "usr/libexec/git-core/git-remote-https",
    "usr/libexec/git-core/git-upload-pack",
    "usr/libexec/git-core/git-receive-pack",
    "usr/libexec/git-core/git-upload-archive",
)
# arquivos de dados que precisam existir (não vazios)
DADOS = (
    "usr/share/curl/cacert.pem",
    "usr/share/git-core/templates/description",
)
CPU_X86_64 = 0x01000007
LC_ID_DYLIB = 0x0D
LC_LOAD = {0x0C: "load", 0x80000018: "weak", 0x8000001F: "reexport", 0x20: "lazy", 0x80000023: "upward"}
WHATSAPP_MIN = (
    "WhatsApp.app/Contents/Resources",
    "WhatsApp.app/Contents/PlugIns",
)


def macho(path):
    with open(path, "rb") as f:
        b = f.read(4)
    return b in MAGIC


def fatias(data):
    """[(cputype, bytes da fatia)] de um Mach-O fino de 64 bits ou universal"""
    m = data[:4]
    if m in (b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca"):
        n = struct.unpack_from(">I", data, 4)[0]
        out = []
        for i in range(n):
            cpu, _, off, size, _ = struct.unpack_from(">IIIII", data, 8 + 20 * i)
            out.append((cpu, data[off:off + size]))
        return out
    if m == b"\xcf\xfa\xed\xfe":
        return [(struct.unpack_from("<I", data, 4)[0], data)]
    return []


def comandos(fat):
    """install name e lista de dylibs (LC_LOAD_*) de uma fatia Mach-O de 64 bits"""
    ncmds = struct.unpack_from("<I", fat, 16)[0]
    off = 32
    ident = None
    deps = []
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", fat, off)
        if cmd == LC_ID_DYLIB or cmd in LC_LOAD:
            noff = struct.unpack_from("<I", fat, off + 8)[0]
            nome = fat[off + noff:off + size].split(b"\0", 1)[0].decode("utf-8", "replace")
            if cmd == LC_ID_DYLIB:
                ident = nome
            else:
                deps.append(nome)
        off += size
    return ident, deps


def libsystem_substitui(root):
    """nomes de libsystem_dylib_ok() em kernel/compat/macho.c"""
    src = open(os.path.join(root, "kernel/compat/macho.c"), encoding="utf-8").read()
    m = re.search(r"bool libsystem_dylib_ok\(.*?static const char \*ok\[\] = \{(.*?)\};", src, re.S)
    if not m:
        return set()
    return set(re.findall(r'"(/[^"]+)"', m.group(1)))


def main():
    esp = sys.argv[1]
    mos = os.path.join(esp, "mOS")
    apps = os.path.join(mos, "Aplicativos")
    fails = 0

    def bad(msg):
        nonlocal fails
        fails += 1
        print("FALHA:", msg)

    for rel in APPS:
        p = os.path.join(apps, rel)
        if not os.path.isfile(p):
            bad("ausente " + rel)
            continue
        if not macho(p):
            bad("não é Mach-O: " + rel)
        else:
            print("real Mach-O", rel, os.path.getsize(p))
    for rel in WHATSAPP_MIN:
        if not os.path.isdir(os.path.join(apps, rel)):
            bad("WhatsApp sem " + rel)
    for rel in REAL_DYLIB:
        p = os.path.join(mos, rel)
        if not os.path.isfile(p):
            bad("ausente " + rel)
            continue
        data = open(p, "rb").read()
        if STUB in data:
            bad("stub, não conta: " + rel)
            continue
        if not macho(p):
            bad("não é Mach-O: " + rel)
            continue
        print("real dylib", rel, len(data))
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    substitui = libsystem_substitui(root)
    if not substitui:
        bad("não achei libsystem_dylib_ok em kernel/compat/macho.c")

    def x86(rel, quer_id):
        p = os.path.join(mos, rel)
        if not os.path.isfile(p):
            bad("ausente " + rel)
            return
        data = open(p, "rb").read()
        fs = dict(fatias(data))
        if CPU_X86_64 not in fs:
            bad("sem fatia x86_64: " + rel)
            return
        ident, deps = comandos(fs[CPU_X86_64])
        if quer_id and ident != quer_id:
            bad("%s: install name %r, esperado %r" % (rel, ident, quer_id))
            return
        faltam = [d for d in deps if d not in substitui and not os.path.isfile(os.path.join(mos, d.lstrip("/")))]
        if faltam:
            bad("%s: dylib fora da imagem: %s" % (rel, ", ".join(faltam)))
            return
        print("Mach-O x86_64", rel, ident or "", len(data), "deps:", " ".join(deps))

    for rel, ident in sorted(INSTALL_NAME.items()):
        x86(rel, ident)
    for rel in PROGRAMAS:
        x86(rel, None)
    for rel in DADOS:
        p = os.path.join(mos, rel)
        if not os.path.isfile(p) or os.path.getsize(p) == 0:
            bad("ausente ou vazio " + rel)
    pem = os.path.join(mos, "usr/share/curl/cacert.pem")
    if os.path.isfile(pem):
        n = open(pem, "rb").read().count(b"-----BEGIN CERTIFICATE-----")
        if n < 100:
            bad("usr/share/curl/cacert.pem com %d certificados" % n)
        else:
            print("cacert.pem", n, "certificados")
    swift = os.path.join(mos, "usr/lib/swift")
    if os.path.isdir(swift):
        for name in sorted(os.listdir(swift)):
            p = os.path.join(swift, name)
            if not os.path.isfile(p):
                continue
            data = open(p, "rb").read()
            if STUB in data:
                print("stub", "usr/lib/swift/" + name, len(data))
            elif data[:4] in MAGIC:
                print("real dylib", "usr/lib/swift/" + name, len(data))
            else:
                bad("usr/lib/swift/%s não é Mach-O nem stub marcado" % name)
    if fails:
        print("%d falha(s)" % fails)
        return 1
    print("verificar-imagem-apps: ok")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("uso: verificar-imagem-apps.py <esp>", file=sys.stderr)
        sys.exit(2)
    sys.exit(main())
