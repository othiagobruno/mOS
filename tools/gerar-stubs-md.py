#!/usr/bin/env python3
# mOS — regenera a contagem de stubs de docs/stubs.md e a lista inteira de docs/stubs-full.txt.
# Lê userland/libsystem/sys_zerados.c e userland/libsystem/swiftui_imports.c.
# Em docs/stubs.md só mexe no que está entre marcadores:
#   <!-- gerado:NOME início --> ... <!-- gerado:NOME fim -->   (bloco: tabela e soma)
#   <!--n:CHAVE-->123<!--/n-->                                   (número no meio da frase)
# CHAVE é o nome de um grupo da tabela (ex.: "Foundation", "swift_* (runtime C)") ou
# total-sys, total-swiftui, ponteiros. O texto fora dos marcadores é manual.
# docs/stubs-full.txt é reescrito inteiro.
# Uso: python3 tools/gerar-stubs-md.py [--check]
# Com --check não escreve nada: sai 1 se algum dos dois arquivos estiver defasado, 0 se não.
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SYS = "userland/libsystem/sys_zerados.c"
SWIFTUI = "userland/libsystem/swiftui_imports.c"
MD = "docs/stubs.md"
FULL = "docs/stubs-full.txt"

FUNC = re.compile(r"^void ([A-Za-z_0-9$]+)\(void\) *\{ *\}")
PTR = re.compile(r"^void \*([A-Za-z_0-9$]+);")

SISTEMAS = "Sistemas"
RUNTIME = "Runtime C/C++ e Swift"
FRAMEWORKS = "Frameworks Apple"
INTERFACE = "Interface"
KERNEL = "Kernel"
STORAGE = "Storage"
A_DEFINIR = "a definir"

STDLIB = "Swift (stdlib)"
OBJC_SEM_MODULO = "ObjC (sem módulo reconhecido)"
LIBC = "libc/libm (C)"

# Módulo Swift (nome dentro do mangled) -> dono. O nome do grupo é o próprio módulo.
MODULOS = {
    "Foundation": SISTEMAS,
    "CoreFoundation": SISTEMAS,
    "CoreMedia": SISTEMAS,
    "Compression": SISTEMAS,
    "UIKit": INTERFACE,
    "PhotosUI": INTERFACE,
    "CoreGraphics": INTERFACE,
    "Combine": INTERFACE,
    "Network": RUNTIME,
    "Dispatch": RUNTIME,
    "System": RUNTIME,
    "Darwin": RUNTIME,
    "os": RUNTIME,
    "ObjectiveC": RUNTIME,
    "_StringProcessing": RUNTIME,
    "RegexBuilder": RUNTIME,
    "Observation": RUNTIME,
    "Accelerate": RUNTIME,
    "UniformTypeIdentifiers": FRAMEWORKS,
    "AVFoundation": FRAMEWORKS,
    "CoreAudio": FRAMEWORKS,
    "XPC": KERNEL,
    "AppleArchive": STORAGE,
}

# Símbolo C: (grupo, dono, teste). Vale o primeiro que casar; o que sobra é libc/libm.
GRUPOS_C = (
    ("swift_* (runtime C)", RUNTIME, lambda s: s.startswith("swift_")),
    ("Foundation (C, NS*)", SISTEMAS, lambda s: s.startswith("NS")),
    ("Network (C, nw_*)", RUNTIME, lambda s: s.startswith("nw_")),
    ("CommonCrypto (C)", RUNTIME, lambda s: s.startswith("CC")),
    ("libdispatch (C)", RUNTIME, lambda s: s.startswith("dispatch_")),
    ("pthread (C)", RUNTIME, lambda s: s.startswith("pthread_")),
    ("mach (C)", KERNEL, lambda s: s.startswith(("mach_", "vm_"))),
    ("mach exception (C)", KERNEL, lambda s: s.startswith("exception_")),
    ("xpc (C)", KERNEL, lambda s: s.startswith("xpc_")),
    ("Security (C)", SISTEMAS, lambda s: s.startswith("Sec")),
    ("filesystem (C)", STORAGE,
     lambda s: s.endswith("fsent") or s.startswith(("nftw", "open_dprotected_np", "xattr_"))),
)

DONOS = dict(MODULOS)
DONOS.update({g: d for g, d, _ in GRUPOS_C})
DONOS.update({STDLIB: RUNTIME, OBJC_SEM_MODULO: A_DEFINIR, LIBC: RUNTIME})


def identificador(s, i):
    """Lê <tamanho><nome> do mangled a partir de i. Devolve o nome ou None."""
    m = re.match(r"\d+", s[i:])
    if not m:
        return None
    j = i + len(m.group(0))
    return s[j:j + int(m.group(0))]


def classifica(s):
    """Símbolo -> grupo. O dono sai de DONOS[grupo]."""
    if s.startswith("$s"):
        # $s<tamanho><Módulo>...: o primeiro identificador é o módulo. Se não for um
        # módulo conhecido (Swift, associated type solto), é stdlib.
        nome = identificador(s, 2)
        if nome in MODULOS:
            return nome
        if nome is None and s.startswith("$sSo"):
            # Tipo ObjC (So...) estendido: vale o último módulo conhecido que aparece
            # como identificador no nome (o da extensão ou o do protocolo).
            achados = []
            for i in range(4, len(s)):
                if s[i].isdigit() and not s[i - 1].isdigit():
                    nome = identificador(s, i)
                    if nome in MODULOS:
                        achados.append(nome)
            return achados[-1] if achados else OBJC_SEM_MODULO
        return STDLIB
    for grupo, _, teste in GRUPOS_C:
        if teste(s):
            return grupo
    return LIBC


def le_simbolos(texto):
    """Devolve (funções, ponteiros); cada item é (linha, símbolo ou linha inteira)."""
    funcs, ptrs = [], []
    for n, linha in enumerate(texto.splitlines(), 1):
        m = FUNC.match(linha)
        if m:
            funcs.append((n, m.group(1)))
            continue
        if PTR.match(linha):
            ptrs.append((n, linha.strip()))
    return funcs, ptrs


def agrupa(funcs):
    grupos = {}
    for n, s in funcs:
        grupos.setdefault(classifica(s), []).append((n, s))
    # Mais símbolos primeiro; empate pela ordem do nome.
    return sorted(grupos.items(), key=lambda kv: (-len(kv[1]), kv[0]))


def gera_full(texto_sys, texto_swiftui):
    funcs, ptrs = le_simbolos(texto_sys)
    ui, _ = le_simbolos(texto_swiftui)
    out = [
        "Lista completa. %d funcoes `void nome(void) {}` em %s." % (len(funcs), SYS),
        "Formato: arquivo:linha simbolo",
        "Donos combinados: Sistemas = CF*/CoreFoundation, Foundation, Security, CoreMedia, "
        "Compression, libcurl, ImageIO, VideoToolbox, Metal, libcompression, libz.",
        "Runtime = libc++, libc++abi, libunwind/_Unwind_*/unw_*, stdlib e runtime Swift "
        "(swift_* e modulos Swift/Dispatch/Darwin/os/System/ObjectiveC neste arquivo).",
        "ImageIO, VideoToolbox, Metal, libcurl e libz: 0 funcoes void nome(void) {} aqui. CryptoKit: 0.",
        "a definir que sobrou: ObjC sem modulo reconhecido. posixmore.c e compat_agentes.c tambem, no stubs.md.",
        "Network/nw_*, libc/libm, CommonCrypto e Accelerate: Runtime C/C++ e Swift.",
        "AVFoundation, CoreAudio, UniformTypeIdentifiers e CloudKit: Frameworks Apple.",
        "Combine: Interface.",
        "swiftui_imports.c (%d corpos vazios) entra na secao seguinte, dono Interface." % len(ui),
        "",
    ]
    for grupo, itens in agrupa(funcs):
        out.append("## %s | %d | %s" % (grupo, len(itens), DONOS[grupo]))
        out += ["%s:%d %s" % (SYS, n, s) for n, s in itens]
        out.append("")
    out.append("## ponteiros nulos (dados, nao funcao) | %s" % SISTEMAS)
    out += ["%s:%d %s" % (SYS, n, s) for n, s in ptrs]
    out.append("")
    out.append("## %s | %s" % (SWIFTUI, INTERFACE))
    out += ["%s:%d %s" % (SWIFTUI, n, s) for n, s in ui]
    return "\n".join(out) + "\n"


def gera_md(md, texto_sys, texto_swiftui):
    funcs, ptrs = le_simbolos(texto_sys)
    ui, _ = le_simbolos(texto_swiftui)
    grupos = agrupa(funcs)
    numeros = {g: len(itens) for g, itens in grupos}
    numeros.update({g: 0 for g in DONOS if g not in numeros})
    numeros.update({"total-sys": len(funcs), "total-swiftui": len(ui), "ponteiros": len(ptrs)})

    tabela = ["| Quantidade | Grupo | Dono | Primeiro símbolo |", "|---:|---|---|---|"]
    for g, itens in grupos:
        tabela.append("| %d | %s | %s | `%s` |" % (len(itens), g, DONOS[g], itens[0][1]))
    tabela.append("")
    tabela.append(
        "Soma: **%d**. Há ainda %d `void *kCF…` / `kCFURL…` sem inicializador (ponteiro nulo), "
        "listados no anexo. Não são função." % (len(funcs), len(ptrs)))
    blocos = {"tabela-grupos": "\n".join(tabela)}

    def bloco(m):
        nome = m.group(1)
        if nome not in blocos:
            sys.exit("%s: bloco gerado desconhecido: %s" % (MD, nome))
        return "<!-- gerado:%s início -->\n%s\n<!-- gerado:%s fim -->" % (nome, blocos[nome], nome)

    def numero(m):
        chave = m.group(1)
        if chave not in numeros:
            sys.exit("%s: número gerado desconhecido: %s" % (MD, chave))
        return "<!--n:%s-->%s%d%s<!--/n-->" % (chave, m.group(2), numeros[chave], m.group(2))

    md = re.sub(r"<!-- gerado:(\S+) início -->\n.*?\n<!-- gerado:\1 fim -->", bloco, md, flags=re.S)
    return re.sub(r"<!--n:(.+?)-->(\**)\d+\**<!--/n-->", numero, md)


def le(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        return f.read()


def main():
    args = sys.argv[1:]
    if args not in ([], ["--check"]):
        sys.exit("uso: %s [--check]" % sys.argv[0])
    texto_sys, texto_swiftui = le(SYS), le(SWIFTUI)
    novos = {
        MD: gera_md(le(MD), texto_sys, texto_swiftui),
        FULL: gera_full(texto_sys, texto_swiftui),
    }
    defasados = [rel for rel, texto in novos.items() if le(rel) != texto]
    if args:
        for rel in defasados:
            print("defasado: %s (rode python3 tools/gerar-stubs-md.py)" % rel)
        return 1 if defasados else 0
    for rel in defasados:
        with open(os.path.join(ROOT, rel), "w", encoding="utf-8") as f:
            f.write(novos[rel])
        print("atualizado: %s" % rel)
    return 0


if __name__ == "__main__":
    sys.exit(main())
