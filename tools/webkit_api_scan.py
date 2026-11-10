#!/usr/bin/env python3
"""Mapeia APIs de WebKit importadas por um .app do macOS.

Lê cada Mach-O (fino ou fat, x86_64 e arm64) do mesmo jeito que tools/machocheck.c:
comandos de biblioteca, binds do dyld_info e a tabela de imports do
LC_DYLD_CHAINED_FIXUPS. Também lê os seletores em __objc_methname das imagens que
ligam o WebKit, e compara com as classes e métodos declarados em WebKit.m.

Uso:
  python3 tools/webkit_api_scan.py --framework userland/frameworks/WebKit/WebKit.m \\
      --out build/webkit-apis.json /caminho/Open\\ Bot.app ...
"""
from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from collections import defaultdict

CPU_NAMES = {0x01000007: "x86_64", 0x0100000C: "arm64"}
LC_SEGMENT_64 = 0x19
LC_SYMTAB = 0x2
LC_LOAD_DYLIB = 0x0C
LC_ID_DYLIB = 0x0D
LC_LOAD_WEAK_DYLIB = 0x80000018
LC_REEXPORT_DYLIB = 0x8000001F
LC_LAZY_LOAD_DYLIB = 0x20
LC_LOAD_UPWARD_DYLIB = 0x80000023
LC_DYLD_INFO = 0x22
LC_DYLD_INFO_ONLY = 0x80000022
LC_DYLD_CHAINED_FIXUPS = 0x80000034
DEP_CMDS = {LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB, LC_LAZY_LOAD_DYLIB, LC_LOAD_UPWARD_DYLIB}

WEB_DYLIB_MARKERS = ("WebKit.framework", "JavaScriptCore.framework", "WebCore.framework")
# Símbolos que o app pede ao WebKit (ou ao JavaScriptCore, reportado à parte).
WEB_SYMBOL = re.compile(
    r"(?:_OBJC_(?:CLASS|METACLASS)_\$_(?:WK|Web)|(?:^|_)WK[A-Z]|WebKit|JavaScriptCore|JSContext|JSValue|JSVirtualMachine)"
)
SELECTOR_HINT = re.compile(
    r"(JavaScript|Navigation|URLScheme|MediaPlayback|ContentWorld|WebArchive|BackForward|"
    r"userContentController:|ScriptMessage|Snapshot|Download|Fullscreen|WebsiteData|"
    r"^webView:|^loadRequest:|^loadHTMLString:|^loadFileURL:|^loadData:|^goBack$|^goForward$|"
    r"^reload$|^stopLoading$|^evaluateJavaScript|^callAsyncJavaScript|^customUserAgent|"
    r"^configuration$|^navigationDelegate$|^UIDelegate$|^takeSnapshot|^findString:|"
    r"^createPDF|^setURLSchemeHandler:|^addScriptMessageHandler)"
)


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def be64(b, o):
    return struct.unpack_from(">Q", b, o)[0]


def cstr(b, o):
    if o < 0 or o >= len(b):
        return ""
    e = b.find(b"\0", o)
    if e < 0:
        e = min(len(b), o + 4096)
    return b[o:e].decode("utf-8", "replace")


def uleb(b, o, end):
    v = 0
    s = 0
    while o < end:
        by = b[o]
        o += 1
        if s < 64:
            v |= (by & 0x7F) << s
        s += 7
        if not by & 0x80:
            break
    return v, o


def sleb(b, o, end):
    v = 0
    s = 0
    by = 0
    while o < end:
        by = b[o]
        o += 1
        if s < 64:
            v |= (by & 0x7F) << s
        s += 7
        if not by & 0x80:
            break
    if s < 64 and (by & 0x40):
        v |= -1 << s
    return v, o


def _fix_symbol_opcode(blob, p, end):
    """SET_SYMBOL_TRAILING_FLAGS_IMM: o nome é a sequência de bytes até o NUL."""
    sym_at = p
    nul = blob.find(b"\0", p, end)
    if nul < 0:
        return "", end
    return blob[sym_at:nul].decode("utf-8", "replace"), nul + 1


def parse_binds_raw(blob, start, size, lazy):
    """Máquina de opcodes de tools/machocheck.c (bind_names). Devolve (nome, ordinal, weak)."""
    out = []
    if size <= 0 or start < 0 or start >= len(blob):
        return out
    p = start
    end = min(len(blob), start + size)
    ordinal = 0
    weak = 0
    sym = ""
    while p < end:
        b = blob[p]
        p += 1
        op, imm = b & 0xF0, b & 0x0F
        if op == 0x00:
            if not lazy:
                break
        elif op == 0x10:
            ordinal = imm
        elif op == 0x20:
            ordinal, p = uleb(blob, p, end)
            ordinal = int(ordinal)
        elif op == 0x30:
            ordinal = (imm - 16) if imm else 0
        elif op == 0x40:
            sym, p = _fix_symbol_opcode(blob, p, end)
            weak = imm & 1
        elif op == 0x50:
            pass
        elif op == 0x60:
            _, p = sleb(blob, p, end)
        elif op in (0x70, 0x80):
            _, p = uleb(blob, p, end)
        elif op in (0x90, 0xB0):
            out.append((sym, ordinal, weak))
        elif op == 0xA0:
            _, p = uleb(blob, p, end)
            out.append((sym, ordinal, weak))
        elif op == 0xC0:
            _, p = uleb(blob, p, end)
            _, p = uleb(blob, p, end)
            out.append((sym, ordinal, weak))
        else:
            break
    return out


def parse_chained_imports(blob, off, size):
    """Tabela de imports do LC_DYLD_CHAINED_FIXUPS, como em machocheck.c."""
    out = []
    if size < 28 or off < 0 or off + size > len(blob):
        return out
    h = off
    io = u32(blob, h + 8)
    so = u32(blob, h + 12)
    cnt = u32(blob, h + 16)
    fmt = u32(blob, h + 20)
    isz = {1: 4, 2: 8, 3: 16}.get(fmt)
    if not isz:
        return out
    he = h + size
    for i in range(cnt):
        e = h + io + i * isz
        if e + isz > he:
            break
        if fmt == 3:
            lib = struct.unpack_from("<h", blob, e)[0]
            weak = u16(blob, e + 2) & 1
            no = u32(blob, e + 4)
        else:
            v = u32(blob, e)
            lib = v & 0xFF
            if lib >= 0xF0:
                lib = lib - 256
            weak = (v >> 8) & 1
            no = v >> 9
        name_at = h + so + no
        if name_at >= he:
            continue
        out.append((cstr(blob, name_at), int(lib), weak))
    return out


def slice_ranges(blob):
    if len(blob) < 8:
        return []
    magic_be = be32(blob, 0)
    if magic_be in (0xCAFEBABE, 0xCAFEBABF):
        n = be32(blob, 4)
        fat64 = magic_be == 0xCAFEBABF
        step = 32 if fat64 else 20
        ranges = []
        for i in range(min(n, 32)):
            a = 8 + i * step
            if a + 20 > len(blob):
                break
            cpu = be32(blob, a)
            if fat64:
                off, sz = be64(blob, a + 8), be64(blob, a + 16)
            else:
                off, sz = be32(blob, a + 8), be32(blob, a + 12)
            if off + sz <= len(blob):
                ranges.append((cpu, off, sz))
        return ranges
    if u32(blob, 0) == 0xFEEDFACF:
        return [(u32(blob, 4), 0, len(blob))]
    return []


def parse_slice(blob, base, size):
    if size < 32 or base + 32 > len(blob) or u32(blob, base) != 0xFEEDFACF:
        return None
    cputype = u32(blob, base + 4)
    ncmds = u32(blob, base + 16)
    sizeofcmds = u32(blob, base + 20)
    c = base + 32
    cend = min(len(blob), c + sizeofcmds)
    dylibs = []
    imports = []
    sections = []
    hints = []
    for _ in range(ncmds):
        if c + 8 > cend:
            break
        cmd, csz = u32(blob, c), u32(blob, c + 4)
        if csz < 8 or c + csz > len(blob):
            break
        if cmd in DEP_CMDS or cmd == LC_ID_DYLIB:
            name_off = u32(blob, c + 8)
            path = cstr(blob, c + name_off)
            if cmd in DEP_CMDS:
                dylibs.append({"path": path, "weak": cmd == LC_LOAD_WEAK_DYLIB})
        elif cmd == LC_SEGMENT_64 and csz >= 72:
            nsects = u32(blob, c + 64)
            sec = c + 72
            for _s in range(nsects):
                if sec + 80 > c + csz:
                    break
                sect = blob[sec:sec + 16].split(b"\0", 1)[0].decode("latin1")
                seg = blob[sec + 16:sec + 32].split(b"\0", 1)[0].decode("latin1")
                ssize = u64(blob, sec + 40)
                soff = u32(blob, sec + 48)
                if sect in ("__objc_methname", "__objc_classname", "__cstring") and ssize and soff:
                    sections.append((sect, soff, ssize))
                sec += 80
        elif cmd in (LC_DYLD_INFO, LC_DYLD_INFO_ONLY):
            # offsets são no arquivo inteiro do slice (base 0 do mach-o, não do fat)
            for part, lazy in ((1, False), (2, False), (3, True)):
                o = u32(blob, c + 8 + part * 8)
                s = u32(blob, c + 12 + part * 8)
                if not s:
                    continue
                weak_part = part == 2
                for name, ordinal, weak in parse_binds_raw(blob, base + o, s, lazy):
                    imports.append((name, ordinal, weak or weak_part))
        elif cmd == LC_DYLD_CHAINED_FIXUPS:
            o, s = u32(blob, c + 8), u32(blob, c + 12)
            imports.extend(parse_chained_imports(blob, base + o, s))
        c += csz
    # seletores e nomes de classe: o offset da seção é relativo ao início do slice
    strings = {"__objc_methname": [], "__objc_classname": [], "__cstring": []}
    for sect, soff, ssize in sections:
        abs_off = base + soff
        if abs_off < 0 or abs_off >= len(blob):
            continue
        chunk = blob[abs_off:min(len(blob), abs_off + ssize)]
        if sect not in strings:
            continue
        # limita o volume de __cstring (pode ser enorme); methname e classname inteiros
        if sect == "__cstring" and len(chunk) > 2_000_000:
            chunk = chunk[:2_000_000]
        for raw in chunk.split(b"\0"):
            if not raw or len(raw) > 300:
                continue
            try:
                text = raw.decode("utf-8")
            except UnicodeDecodeError:
                continue
            if sect == "__cstring":
                if "WebKit" in text or "WKWebView" in text or "JavaScriptCore" in text:
                    hints.append(text[:240])
            else:
                strings[sect].append(text)
    return {
        "cpu": CPU_NAMES.get(cputype, hex(cputype)),
        "dylibs": dylibs,
        "imports": imports,
        "meth": strings["__objc_methname"],
        "classes": strings["__objc_classname"],
        "hints": hints,
    }


def parse_macho(blob):
    slices = []
    for cpu, off, sz in slice_ranges(blob):
        one = parse_slice(blob, off, sz)
        if one:
            slices.append(one)
    return slices


def dylib_for(ordinal, dylibs):
    if isinstance(ordinal, int) and ordinal >= 1 and ordinal <= len(dylibs):
        return dylibs[ordinal - 1]["path"]
    return ""


def is_web_symbol(name, provider):
    if any(m in provider for m in WEB_DYLIB_MARKERS):
        return True
    return bool(name) and bool(WEB_SYMBOL.search(name))


def class_from_symbol(name):
    for prefix in ("_OBJC_CLASS_$_", "_OBJC_METACLASS_$_"):
        if name.startswith(prefix):
            return name[len(prefix):]
    return ""


def interesting_selector(sel, catalog):
    if not sel or any(c in sel for c in "\"',$ \t") or len(sel) > 180:
        return False
    if sel in catalog:
        return True
    return bool(SELECTOR_HINT.search(sel))


def scan_file(path):
    try:
        st = os.stat(path)
    except OSError:
        return []
    if st.st_size < 32 or st.st_size > 800_000_000:
        return []
    with open(path, "rb") as f:
        head = f.read(4)
        if head not in (b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf", b"\xcf\xfa\xed\xfe"):
            return []
        f.seek(0)
        blob = f.read()
    found = []
    for sl in parse_macho(blob):
        web_dylibs = [d for d in sl["dylibs"] if any(m in d["path"] for m in WEB_DYLIB_MARKERS)]
        symbols = []
        for name, ordinal, weak in sl["imports"]:
            provider = dylib_for(ordinal, sl["dylibs"])
            if is_web_symbol(name, provider):
                symbols.append({"name": name, "provider": provider, "weak": bool(weak), "class": class_from_symbol(name)})
        if not web_dylibs and not symbols and not sl["hints"]:
            continue
        found.append({
            "cpu": sl["cpu"],
            "dylibs": web_dylibs,
            "symbols": symbols,
            "meth": sl["meth"],
            "classes_defined": [c for c in sl["classes"] if c.startswith("WK") or c.startswith("Web")],
            "hints": sl["hints"][:40],
        })
    return found


def scan_tree(root):
    images = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in (".git",)]
        for fn in filenames:
            path = os.path.join(dirpath, fn)
            hits = scan_file(path)
            if hits:
                images.append({"path": os.path.relpath(path, root), "slices": hits})
    return images


def selector_from_method_line(line):
    line = line.split("//", 1)[0].strip()
    if not line or line[0] not in "+-":
        return ""
    i = line.find("(")
    if i < 0:
        return ""
    depth = 0
    rest = ""
    for j in range(i, len(line)):
        if line[j] == "(":
            depth += 1
        elif line[j] == ")":
            depth -= 1
            if depth == 0:
                rest = line[j + 1:]
                break
    else:
        return ""
    depth = 0
    cut = []
    for ch in rest:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth = max(0, depth - 1)
        elif ch in "{;" and depth == 0:
            break
        cut.append(ch)
    rest = "".join(cut)
    parts = re.findall(r"([A-Za-z_][A-Za-z0-9_]*)\s*:", rest)
    if parts:
        return "".join(p + ":" for p in parts)
    m = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)", rest)
    return m.group(1) if m else ""


def framework_surface(path):
    """Classes (@implementation) e seletores (-/+, @property e @selector) do fonte do framework."""
    classes = set()
    selectors = set()
    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    for m in re.finditer(r"@implementation\s+([A-Za-z_][A-Za-z0-9_]*)", text):
        classes.add(m.group(1))
    for m in re.finditer(r"@interface\s+([A-Za-z_][A-Za-z0-9_]*)", text):
        classes.add(m.group(1))
    for m in re.finditer(r"@selector\s*\(\s*([A-Za-z_][A-Za-z0-9_:]*)\s*\)", text):
        selectors.add(m.group(1))
    for line in text.splitlines():
        sel = selector_from_method_line(line)
        if sel:
            selectors.add(sel)
        pm = re.search(r"@property\b([^;]*);", line)
        if not pm:
            continue
        body = pm.group(1)
        gm = re.search(r"getter\s*=\s*([A-Za-z_][A-Za-z0-9_]*)", body)
        sm = re.search(r"setter\s*=\s*([A-Za-z_][A-Za-z0-9_]*)", body)
        name_m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*$", body.strip())
        if not name_m:
            continue
        name = name_m.group(1)
        if name in ("nonatomic", "atomic", "copy", "strong", "weak", "assign", "readonly", "readwrite", "nullable", "class"):
            continue
        getter = gm.group(1) if gm else name
        setters = sm.group(1) if sm else "set" + name[:1].upper() + name[1:] + ":"
        selectors.add(getter)
        if "readonly" not in body:
            selectors.add(setters)
    consts = set(re.findall(r"\b(WK[A-Za-z0-9_]*|Web[A-Za-z0-9_]*)\b", text))
    return {"classes": classes, "selectors": selectors, "names": consts}


def summarize(root, surface):
    images = scan_tree(root)
    by_cpu = defaultdict(int)
    symbols = {}
    selectors = {}
    dylibs = set()
    hints = set()
    defined = set()
    for im in images:
        for sl in im["slices"]:
            by_cpu[sl["cpu"]] += 1
            for d in sl["dylibs"]:
                dylibs.add(d["path"])
            for s in sl["symbols"]:
                key = s["name"]
                cur = symbols.get(key)
                if cur is None or (cur["weak"] and not s["weak"]):
                    symbols[key] = {"name": key, "provider": s["provider"], "weak": s["weak"], "class": s["class"], "image": im["path"]}
            linked = bool(sl["dylibs"]) or any(s["class"].startswith("WK") or s["class"].startswith("Web") for s in sl["symbols"])
            if linked:
                for sel in sl["meth"]:
                    if interesting_selector(sel, surface["selectors"] if surface else set()):
                        selectors.setdefault(sel, im["path"])
            defined.update(sl["classes_defined"])
            hints.update(sl["hints"])
    missing_classes = []
    present_classes = []
    jsc = []
    other = []
    for name, info in sorted(symbols.items()):
        prov = info["provider"]
        if "JavaScriptCore" in prov or "JavaScriptCore" in name or name.startswith("_JS") or "JSContext" in name or "JSValue" in name:
            jsc.append(info)
            continue
        cls = info["class"]
        if cls:
            if surface and cls in surface["classes"]:
                present_classes.append(info)
            else:
                missing_classes.append(info)
        else:
            bare = name[1:] if name.startswith("_") else name
            if surface and (bare in surface["names"] or bare in surface["classes"]):
                present_classes.append(info)
            else:
                other.append(info)
    missing_selectors = []
    present_selectors = []
    for sel, image in sorted(selectors.items()):
        row = {"selector": sel, "image": image}
        if surface and sel in surface["selectors"]:
            present_selectors.append(row)
        else:
            missing_selectors.append(row)
    return {
        "root": root,
        "images_with_webkit_signal": len(images),
        "slices": dict(by_cpu),
        "dylibs": sorted(dylibs),
        "symbols": list(symbols.values()),
        "classes_present_in_framework": present_classes,
        "classes_missing_from_framework": missing_classes,
        "other_webkit_symbols": other,
        "javascriptcore_symbols": jsc,
        "selectors_implemented": present_selectors,
        "selectors_not_in_framework_source": missing_selectors,
        "classes_defined_by_app": sorted(defined),
        "string_hints": sorted(hints)[:80],
    }


def main(argv):
    ap = argparse.ArgumentParser(description="Mapeia APIs de WebKit exigidas por apps Mach-O")
    ap.add_argument("--framework", help="WebKit.m do mOS, para marcar o que já existe no fonte")
    ap.add_argument("--out", help="JSON de saída")
    ap.add_argument("roots", nargs="+")
    args = ap.parse_args(argv)
    surface = framework_surface(args.framework) if args.framework else None
    reports = []
    for root in args.roots:
        if os.path.isdir(root):
            reports.append(summarize(root, surface))
        elif os.path.isfile(root):
            parent = os.path.dirname(root) or "."
            # um arquivo só: embrulha num diretório lógico
            one = scan_file(root)
            fake = summarize(parent, surface) if False else None
            reports.append({"root": root, "file_slices": one, "note": "arquivo único; use um diretório para o bundle"})
        else:
            reports.append({"root": root, "error": "não encontrado"})
    text = json.dumps(reports, indent=2, ensure_ascii=False)
    if args.out:
        os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(text)
            f.write("\n")
    else:
        sys.stdout.write(text)
        sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
