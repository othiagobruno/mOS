#!/usr/bin/env python3
"""Inventário dos stubs da frente Foundation em userland/libsystem/sys_zerados.c.

  python3 tools/fdn-inventario.py SWIFT_DEMANGLE > docs/foundation-stubs.md

SWIFT_DEMANGLE é o swift-demangle de qualquer toolchain Swift (o de Linux serve).
Agrupa pelo tipo de topo do Foundation e põe o nome demanglado ao lado do mangled.
"""
import collections, re, subprocess, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
src = (root / "userland/libsystem/sys_zerados.c").read_text(encoding="utf-8")
names = re.findall(r"^void \*?([^ (;]+)", src, re.M)
fdn = [n for n in names if "10Foundation" in n or "14CoreFoundation" in n
       or (re.match(r"(NS|CF|kCF|_CF|__CF)", n) and not re.match(r"kCF(Network|Stream)", n))]
dem = subprocess.run([sys.argv[1], "--compact"], input="\n".join(fdn) + "\n",
                     capture_output=True, text=True).stdout.split("\n")
groups = collections.defaultdict(list)
for n, d in zip(fdn, dem):
    m = re.match(r"\$s(?:10Foundation|So|14CoreFoundation)(\d+)([A-Za-z_]+)", n)
    k = m.group(2)[:int(m.group(1))] if m else ("C" if not n.startswith("$") else "extensões")
    groups[k].append((n, d))
print("# Stubs restantes da frente Foundation/CoreFoundation\n")
print("Gerado por `tools/fdn-inventario.py` a partir de `sys_zerados.c`. Total: **%d**.\n" % len(fdn))
print("| Grupo | Quantidade |\n|---|---:|")
for k, v in sorted(groups.items(), key=lambda kv: -len(kv[1])):
    print("| %s | %d |" % (k, len(v)))
for k, v in sorted(groups.items(), key=lambda kv: -len(kv[1])):
    print("\n## %s (%d)\n" % (k, len(v)))
    for n, d in v:
        print("- `%s`  \n  %s" % (n, d.replace("|", "\\|")))
