#!/usr/bin/env python3
"""Teste de host da imagem: bundles, Mach-O e bibliotecas reais.

Recusa dylib cujo conteúdo é o stub de tools/gen-libswift-stubs.c
("STUB mOS libswift"). Não sobe QEMU. Sai 1 se faltar peça real.
"""
import os
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
)
WHATSAPP_MIN = (
    "WhatsApp.app/Contents/Resources",
    "WhatsApp.app/Contents/PlugIns",
)


def macho(path):
    with open(path, "rb") as f:
        b = f.read(4)
    return b in MAGIC


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
