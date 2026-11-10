#!/usr/bin/env python3
# mOS — extrai um .app e copia o alvo por cima de cada symlink de .framework gravado como texto.
#
# O Expand-Archive (e o git, mode 120000) grava o alvo do symlink num arquivo de texto
# ("Versions/Current/Electron Framework", "A"). `file` então vê ASCII, e o macho_load que
# abre esse caminho não recebe o Mach-O. Esta é a regra única do reparo (docs/staging-linux.md).
# tools/stage-image.sh chama --copiar / --reparar. O mkimg ainda reescreve o texto que sobrar
# para #mOS-link: (canon_fw_links em tools/mkimg.c); um arquivo já Mach-O não é reescrito.
#
#   extrair-framework.py --copiar ORIGEM DESTINO
#   extrair-framework.py --reparar ARVORE
#   extrair-framework.py --simular-expand ZIP DESTINO     # texto, como o Expand-Archive
#   extrair-framework.py --extrair ZIP DESTINO            # simula e repara
import os
import shutil
import sys
import zipfile


def fwlink_macho(b):
    if len(b) < 4:
        return False
    return b[:4] in (
        b"\xcf\xfa\xed\xfe",
        b"\xfe\xed\xfa\xcf",
        b"\xce\xfa\xed\xfe",
        b"\xfe\xed\xfa\xce",
        b"\xca\xfe\xba\xbe",
        b"\xbe\xba\xfe\xca",
    )


def fwlink_ok(ch):
    return ch.isalnum() or ch in "._+-@() /"


def fwlink_target(data):
    """Espelha fwlink_target em kernel/include/fwlink.h. Devolve o alvo ou None."""
    if not data or len(data) > 400:
        return None
    prefixed = False
    i = 0
    if data.startswith(b"#mOS-link:"):
        prefixed = True
        i = 10
    else:
        if fwlink_macho(data):
            return None
        if data[:1] in (b"<", b"{", b"["):
            return None
        if data.startswith(b"bplist"):
            return None
        if data.startswith(b"https://") or data.startswith(b"http://"):
            return None
    out = []
    slash = False
    while i < len(data):
        c = data[i]
        if c in (10, 13):
            if any(ch not in (10, 13, 32) for ch in data[i:]):
                return None
            break
        ch = chr(c)
        if not fwlink_ok(ch):
            return None
        if ch == "/":
            slash = True
        out.append(ch)
        i += 1
    while out and out[-1] in " /":
        out.pop()
    tgt = "".join(out)
    if not tgt or len(tgt) > 300:
        return None
    if tgt[0] == " " or tgt == ".":
        return None
    if not prefixed and not slash and len(tgt) > 80:
        return None
    return tgt


def _read(path):
    try:
        with open(path, "rb") as f:
            return f.read(401)
    except OSError:
        return b""


def resolve_path(path, depth=0):
    """Segue texto de link, #mOS-link: e symlink real até um arquivo ou diretório."""
    if depth >= 8:
        return path
    if os.path.islink(path):
        raw = os.readlink(path)
        nxt = raw if os.path.isabs(raw) else os.path.normpath(os.path.join(os.path.dirname(path), raw))
        return resolve_path(nxt, depth + 1)
    if os.path.isdir(path):
        return path
    if not os.path.isfile(path):
        return path
    if os.path.getsize(path) > 400:
        return path
    tgt = fwlink_target(_read(path))
    if not tgt:
        return path
    base = os.path.dirname(path)
    walked = _walk(base, tgt, depth)
    if not walked:
        return path
    if os.path.abspath(walked) == os.path.abspath(path):
        return path
    if os.path.isdir(walked):
        return walked
    return resolve_path(walked, depth + 1)


def _walk(base, rel, depth):
    acc = base
    parts = rel.split("/")
    for i, part in enumerate(parts):
        if part in ("", "."):
            continue
        if part == "..":
            acc = os.path.dirname(acc)
            continue
        step = os.path.join(acc, part)
        if os.path.isdir(step) and not os.path.islink(step):
            acc = step
            continue
        if os.path.isfile(step) or os.path.islink(step):
            resolved = resolve_path(step, depth + 1)
            if i == len(parts) - 1:
                return resolved
            if os.path.isdir(resolved):
                acc = resolved
                continue
            return None
        return None
    return acc if os.path.exists(acc) else None


def copiar(src, dst):
    os.makedirs(os.path.dirname(os.path.abspath(dst)) or ".", exist_ok=True)
    target = resolve_path(src)
    if os.path.isdir(target):
        if os.path.lexists(dst):
            if os.path.isdir(dst) and not os.path.islink(dst):
                shutil.rmtree(dst)
            else:
                os.remove(dst)
        shutil.copytree(target, dst, symlinks=False)
        return
    if not os.path.isfile(target):
        sys.stderr.write("extrair-framework: não achei o alvo de %s\n" % src)
        return 1
    shutil.copyfile(target, dst)
    return 0


def _links(root):
    found = []
    for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
        for name in dirnames:
            p = os.path.join(dirpath, name)
            if os.path.islink(p):
                found.append(p)
        for name in filenames:
            p = os.path.join(dirpath, name)
            if os.path.islink(p):
                found.append(p)
                continue
            try:
                if os.path.getsize(p) > 400:
                    continue
            except OSError:
                continue
            if fwlink_target(_read(p)):
                found.append(p)
    return found


def reparar(root):
    n = 0
    for path in _links(root):
        if not os.path.lexists(path):
            continue
        target = resolve_path(path)
        if not target or os.path.abspath(target) == os.path.abspath(path):
            continue
        if os.path.isdir(target):
            if os.path.islink(path) or os.path.isfile(path):
                os.remove(path)
            elif os.path.isdir(path):
                continue
            shutil.copytree(target, path, symlinks=False)
            n += 1
            continue
        if os.path.isfile(target):
            if os.path.islink(path):
                os.remove(path)
            shutil.copyfile(target, path)
            n += 1
    return n


def _is_symlink_info(info):
    mode = (info.external_attr >> 16) & 0xFFFF
    return (mode & 0o170000) == 0o120000


def simular_expand(zip_path, dest):
    os.makedirs(dest, exist_ok=True)
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            name = info.filename
            if not name or name.endswith("/"):
                os.makedirs(os.path.join(dest, name), exist_ok=True)
                continue
            out = os.path.join(dest, name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with zf.open(info) as src, open(out, "wb") as dst:
                shutil.copyfileobj(src, dst)
    return 0


def main(argv):
    if len(argv) == 4 and argv[1] == "--copiar":
        rc = copiar(argv[2], argv[3])
        return rc or 0
    if len(argv) == 3 and argv[1] == "--reparar":
        n = reparar(argv[2])
        print("extrair-framework: %d link(s) substituído(s) pelos bytes do alvo" % n)
        return 0
    if len(argv) == 4 and argv[1] == "--simular-expand":
        return simular_expand(argv[2], argv[3])
    if len(argv) == 4 and argv[1] == "--extrair":
        simular_expand(argv[2], argv[3])
        n = reparar(argv[3])
        print("extrair-framework: %d link(s) substituído(s) pelos bytes do alvo" % n)
        return 0
    sys.stderr.write(
        "uso: extrair-framework.py --copiar SRC DST | --reparar ARVORE | --simular-expand ZIP DST | --extrair ZIP DST\n"
    )
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
