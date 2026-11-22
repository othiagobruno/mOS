#!/usr/bin/env python3
"""Testes de host no macOS (tools/build-mac.sh): gera um .c com cada símbolo indefinido dos objetos dados que o
processo do host não resolve (dlsym em RTLD_DEFAULT) como dado zerado. É o equivalente do
-Wl,--unresolved-symbols=ignore-all do ld do Linux, onde esses símbolos viram 0; no macOS o dyld aborta no primeiro
dado ausente. Uso: mac_undef_stubs.py saida.c objeto.o [...]"""
import ctypes, subprocess, sys

out, objs = sys.argv[1], sys.argv[2:]
nm = subprocess.run(['nm', '-u', '-j', *objs], capture_output=True, text=True, check=True).stdout.split()
defs = set(subprocess.run(['nm', '-g', '-j', '--defined-only', *objs], capture_output=True, text=True).stdout.split())
proc = ctypes.CDLL(None)
falta = []
for s in sorted(set(nm)):
    if not s.startswith('_') or s in defs or s.endswith(':'):
        continue
    try:
        getattr(proc, s[1:])
    except AttributeError:
        falta.append(s)
with open(out, 'w') as f:
    f.write('/* gerado por tools/mac_undef_stubs.py: símbolos que o ld do Linux deixaria em 0 */\n')
    for i, s in enumerate(falta):
        f.write('char mac_undef_%d[64] __asm__("%s");\n' % (i, s))
print(f'mac_undef_stubs: {len(falta)} símbolo(s) zerado(s)', file=sys.stderr)
