#!/usr/bin/env python3
# mOS — mede macho_load no Linux (L06, L08). Não altera MAX_IMAGES.
# Saída: /tmp/mos-medir/resultado.txt e o código de saída 0 quando a sonda de 97 imagens
# ainda recusa com "bibliotecas demais no bundle (máximo 96)".
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = "/tmp/mos-medir"
VFS = os.path.join(OUT, "vfs")
HOST = os.path.join(OUT, "machoload_host")
LOG = []


def log(s):
    print(s)
    LOG.append(s)


def u32(n):
    return struct.pack("<I", n)


def u64(n):
    return struct.pack("<Q", n)


def macho(filetype, dylibs, rpath=None):
    cmds = bytearray()
    ncmds = 1
    seg = bytearray(72)
    seg[0:4] = u32(0x19)
    seg[8:14] = b"__TEXT"
    seg[56:60] = u32(7)
    seg[60:64] = u32(5)
    cmds += seg
    if filetype == 2:
        main = bytearray(24)
        main[0:4] = u32(0x80000028)
        main[4:8] = u32(24)
        cmds += main
        ncmds += 1
    if rpath:
        raw = rpath.encode() + b"\0"
        pad = (8 - (12 + len(raw)) % 8) % 8
        cmd = bytearray(12 + len(raw) + pad)
        cmd[0:4] = u32(0x8000001C)
        cmd[4:8] = u32(len(cmd))
        cmd[8:12] = u32(12)
        cmd[12:12 + len(raw)] = raw
        cmds += cmd
        ncmds += 1
    for name in dylibs:
        raw = name.encode() + b"\0"
        pad = (8 - (24 + len(raw)) % 8) % 8
        cmd = bytearray(24 + len(raw) + pad)
        cmd[0:4] = u32(0x0C)
        cmd[4:8] = u32(len(cmd))
        cmd[8:12] = u32(24)
        cmd[24:24 + len(raw)] = raw
        cmds += cmd
        ncmds += 1
    total = 32 + len(cmds)
    vmsize = max(0x4000, (total + 0x3FFF) & ~0x3FFF)
    struct.pack_into("<Q", cmds, 24, 0)
    struct.pack_into("<Q", cmds, 32, vmsize)
    struct.pack_into("<Q", cmds, 40, 0)
    struct.pack_into("<Q", cmds, 48, total)
    struct.pack_into("<I", cmds, 4, 72)
    flags = 0x200000 if filetype == 2 else 0
    hdr = u32(0xFEEDFACF) + u32(0x0100000C) + u32(0) + u32(filetype) + u32(ncmds) + u32(len(cmds)) + u32(flags) + u32(0)
    return hdr + bytes(cmds)


def elf_stub(machine):
    # ET_DYN, um PT_LOAD, um símbolo global cx_start. O libSystem arm64 e o a64rt precisam disso.
    e = bytearray(0x300)
    e[0:4] = b"\x7fELF"
    e[4] = 2
    e[5] = 1
    e[6] = 1
    struct.pack_into("<H", e, 16, 3)
    struct.pack_into("<H", e, 18, machine)
    struct.pack_into("<I", e, 20, 1)
    struct.pack_into("<Q", e, 32, 64)
    struct.pack_into("<H", e, 52, 64)
    struct.pack_into("<H", e, 54, 56)
    struct.pack_into("<H", e, 56, 1)
    struct.pack_into("<H", e, 58, 64)
    ph = 64
    struct.pack_into("<I", e, ph, 1)
    struct.pack_into("<I", e, ph + 4, 5)
    struct.pack_into("<Q", e, ph + 8, 0)
    struct.pack_into("<Q", e, ph + 16, 0)
    struct.pack_into("<Q", e, ph + 32, len(e))
    struct.pack_into("<Q", e, ph + 40, 0x1000)
    struct.pack_into("<Q", e, ph + 48, 0x1000)
    strtab = b"\0cx_start\0"
    syms = bytearray(48)
    struct.pack_into("<I", syms, 24, 1)
    syms[24 + 4] = 0x12
    struct.pack_into("<H", syms, 24 + 6, 1)
    stroff = 0x100
    symoff = 0x120
    e[stroff:stroff + len(strtab)] = strtab
    e[symoff:symoff + len(syms)] = syms
    shoff = 0x180
    struct.pack_into("<Q", e, 40, shoff)
    struct.pack_into("<H", e, 60, 3)

    def shdr(i, typ, off, size, link, entsz):
        o = shoff + i * 64
        struct.pack_into("<I", e, o + 4, typ)
        struct.pack_into("<Q", e, o + 24, off)
        struct.pack_into("<Q", e, o + 32, size)
        struct.pack_into("<I", e, o + 40, link)
        struct.pack_into("<Q", e, o + 48, 1)
        struct.pack_into("<Q", e, o + 56, entsz)

    shdr(1, 2, symoff, len(syms), 2, 24)
    shdr(2, 3, stroff, len(strtab), 0, 0)
    return bytes(e)


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def run_load(mos_path):
    env = os.environ.copy()
    env["MOS_ARCH"] = "arm64"
    env["MACHOTEST_TOLERANT"] = "1"
    p = subprocess.run([HOST, VFS, mos_path], env=env, capture_output=True, text=True)
    text = (p.stderr or "") + (p.stdout or "")
    return p.returncode, text.strip()


def compile_host():
    cmd = [
        "gcc", "-O2", "-std=gnu11", "-DMOS_HOST_TEST", "-D_GNU_SOURCE",
        "-I" + os.path.join(ROOT, "kernel/include"),
        "-I" + os.path.join(ROOT, "tools"),
        "-fno-strict-aliasing",
        os.path.join(ROOT, "tools/machoload_host.c"),
        os.path.join(ROOT, "kernel/compat/macho.c"),
        "-o", HOST,
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        log("machoload_host FAILED")
        log(r.stderr[-2000:])
        return False
    log("machoload_host: compilou")
    return True


def framework_sintetico():
    base = os.path.join(VFS, "Aplicativos/Sint.app/Contents")
    fw = os.path.join(base, "Frameworks/Electron Framework.framework")
    exe_deps = [
        "@rpath/Electron Framework.framework/Electron Framework",
        "/usr/lib/libSystem.B.dylib",
    ]
    write(os.path.join(base, "MacOS/Sint"), macho(2, exe_deps, "@executable_path/../Frameworks"))
    fw_bin = macho(6, ["@loader_path/Libraries/libffmpeg.dylib", "/usr/lib/libSystem.B.dylib"])
    write(os.path.join(fw, "Versions/A/Electron Framework"), fw_bin)
    write(os.path.join(fw, "Versions/A/Libraries/libffmpeg.dylib"), macho(6, ["/usr/lib/libSystem.B.dylib"]))
    write(os.path.join(fw, "Versions/Current"), b"A")
    write(os.path.join(fw, "Electron Framework"), b"Versions/Current/Electron Framework")
    write(os.path.join(fw, "Libraries"), b"Versions/A/Libraries")
    link = os.path.join(fw, "Electron Framework")
    log("L06 sintético antes: " + subprocess.check_output(["file", "-b", link], text=True).strip())
    subprocess.check_call([sys.executable, os.path.join(ROOT, "tools/extrair-framework.py"), "--reparar", os.path.join(VFS, "Aplicativos/Sint.app")])
    log("L06 sintético depois: " + subprocess.check_output(["file", "-b", link], text=True).strip())
    code, text = run_load("/Aplicativos/Sint.app/Contents/MacOS/Sint")
    log("L06 sintético macho_load rc=%d" % code)
    log(text)


def uniao():
    swift = [
        "libswiftCore", "libswift_Concurrency", "libswiftDispatch", "libswiftObjectiveC", "libswiftDarwin",
        "libswiftCoreFoundation", "libswiftCoreGraphics", "libswiftos", "libswiftObservation", "libswiftFoundation",
        "libswiftUniformTypeIdentifiers", "libswift_RegexParser", "libswift_StringProcessing",
        "libswiftsimd", "libswiftXPC", "libswiftQuartzCore", "libswiftMetal", "libswiftIOKit", "libswiftOSLog",
        "libswiftSpatial", "libswiftCoreAudio", "libswiftCoreImage", "libswiftCoreMIDI", "libswiftQuickLookUI",
    ]
    deps = ["/usr/lib/swift/%s.dylib" % n for n in swift]
    for n in deps:
        write(os.path.join(VFS, n.lstrip("/")), macho(6, ["/usr/lib/libSystem.B.dylib"]))
    extras = [
        "/System/Library/Frameworks/SwiftUI.framework/SwiftUI",
        "/System/Library/Frameworks/Combine.framework/Combine",
        "/System/Library/Frameworks/StoreKit.framework/StoreKit",
        "/System/Library/Frameworks/QuickLookUI.framework/QuickLookUI",
        "/Aplicativos/Sint.app/Contents/Frameworks/Squirrel.framework/Versions/A/Squirrel",
        "/Aplicativos/Sint.app/Contents/Frameworks/Mantle.framework/Versions/A/Mantle",
        "/Aplicativos/Sint.app/Contents/Frameworks/ReactiveObjC.framework/Versions/A/ReactiveObjC",
        "/Aplicativos/Sint.app/Contents/Frameworks/Electron Framework.framework/Versions/A/Libraries/libEGL.dylib",
        "/Aplicativos/Sint.app/Contents/Frameworks/Electron Framework.framework/Versions/A/Libraries/libGLESv2.dylib",
        "/Aplicativos/Sint.app/Contents/Frameworks/Electron Framework.framework/Versions/A/Libraries/libvk_swiftshader.dylib",
    ]
    for n in extras:
        write(os.path.join(VFS, n.lstrip("/")), macho(6, ["/usr/lib/libSystem.B.dylib"]))
    # o executável também pede o framework já medido no L06 (conta de novo se o caminho for outro)
    deps = deps + extras + ["/usr/lib/libSystem.B.dylib"]
    write(os.path.join(VFS, "Aplicativos/Uniao.app/Contents/MacOS/Uniao"), macho(2, deps))
    code, text = run_load("/Aplicativos/Uniao.app/Contents/MacOS/Uniao")
    log("L08 união sintética (Swift do link-swift-dylibs-objc.ps1 + SwiftUI/Combine/StoreKit/QuickLookUI + libs do Electron) rc=%d" % code)
    log(text)
    log("L08 esta união é sintética: cada dylib é um Mach-O mínimo, não o binário da Apple.")


def teto():
    deps = []
    for i in range(96):
        name = "/usr/local/lib/libextra%d.dylib" % i
        deps.append(name)
        write(os.path.join(VFS, name.lstrip("/")), macho(6, ["/usr/lib/libSystem.B.dylib"]))
    write(os.path.join(VFS, "Aplicativos/Teto.app/Contents/MacOS/Teto"), macho(2, deps + ["/usr/lib/libSystem.B.dylib"]))
    code, text = run_load("/Aplicativos/Teto.app/Contents/MacOS/Teto")
    log("L08 sonda 97 imagens (exe + 96 dylibs) rc=%d" % code)
    log(text)


def vscode_real():
    zpath = "/tmp/vscode-darwin-arm64.zip"
    if not os.path.isfile(zpath):
        log("L06 VS Code: zip ausente em /tmp/vscode-darwin-arm64.zip")
        return
    import zipfile
    z = zipfile.ZipFile(zpath)
    names = z.namelist()
    prefix = ""
    for n in names:
        if n.endswith("Contents/MacOS/Code") and "Helper" not in n:
            prefix = n[: -len("Contents/MacOS/Code")]
            break
    if not prefix:
        log("L06 VS Code: Contents/MacOS/Code não está no zip")
        return
    want_suffix = [
        "Contents/Info.plist",
        "Contents/MacOS/Code",
        "Contents/Frameworks/Electron Framework.framework/Electron Framework",
        "Contents/Frameworks/Electron Framework.framework/Versions/Current",
        "Contents/Frameworks/Electron Framework.framework/Versions/A/Electron Framework",
        "Contents/Frameworks/Electron Framework.framework/Libraries",
        "Contents/Frameworks/Electron Framework.framework/Versions/A/Libraries/libffmpeg.dylib",
        "Contents/Frameworks/Squirrel.framework/Squirrel",
        "Contents/Frameworks/Squirrel.framework/Versions/A/Squirrel",
        "Contents/Frameworks/Mantle.framework/Mantle",
        "Contents/Frameworks/Mantle.framework/Versions/A/Mantle",
        "Contents/Frameworks/ReactiveObjC.framework/ReactiveObjC",
        "Contents/Frameworks/ReactiveObjC.framework/Versions/A/ReactiveObjC",
    ]
    dest = os.path.join(VFS, "Aplicativos/VSCode.app")
    got = 0
    for suf in want_suffix:
        name = prefix + suf
        if name not in z.namelist():
            log("L06 VS Code: falta " + suf)
            continue
        info = z.getinfo(name)
        mode = (info.external_attr >> 16) & 0xFFFF
        data = z.read(name)
        path = os.path.join(dest, suf)
        write(path, data)
        got += 1
        if mode == 0o120000:
            log("L06 VS Code symlink-texto %s -> %s" % (suf, data[:80]))
    log("L06 VS Code: %d arquivos do fecho extraídos (não é o .app inteiro)" % got)
    code_path = os.path.join(dest, "Contents/Frameworks/Electron Framework.framework/Electron Framework")
    if os.path.isfile(code_path):
        log("L06 VS Code framework antes: " + subprocess.check_output(["file", "-b", code_path], text=True).strip()[:200])
    subprocess.check_call([sys.executable, os.path.join(ROOT, "tools/extrair-framework.py"), "--reparar", dest])
    if os.path.isfile(code_path):
        log("L06 VS Code framework depois: " + subprocess.check_output(["file", "-b", code_path], text=True).strip()[:200])
    code, text = run_load("/Aplicativos/VSCode.app/Contents/MacOS/Code")
    log("L06 VS Code macho_load rc=%d" % code)
    # o framework tem ~190 MiB; o erro, se houver, cabe nas últimas linhas
    lines = text.splitlines()
    log("\n".join(lines[:8] + (["..."] if len(lines) > 16 else []) + lines[-8:]))


def main():
    os.makedirs(OUT, exist_ok=True)
    if os.path.isdir(VFS):
        subprocess.check_call(["rm", "-rf", VFS])
    a64 = os.path.join(VFS, "Sistema/Biblioteca/a64")
    write(os.path.join(a64, "libSystem.mos"), elf_stub(183))
    write(os.path.join(a64, "a64rt.mos"), elf_stub(62))
    if not compile_host():
        return 1
    framework_sintetico()
    uniao()
    teto()
    vscode_real()
    text = "\n".join(LOG) + "\n"
    with open(os.path.join(OUT, "resultado.txt"), "w", encoding="utf-8") as f:
        f.write(text)
    if "bibliotecas demais no bundle (máximo 96)" not in text:
        log("a sonda de 97 imagens não devolveu o erro esperado")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
