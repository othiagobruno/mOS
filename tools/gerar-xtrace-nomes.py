#!/usr/bin/env python3
# Gera kernel/proc/xtrace_nomes.inc (nome de cada SYS_* do primeiro enum de kernel/include/syscall.h) para o
# rastreio de syscalls (kernel/proc/xtrace.c). Rode de novo quando entrar syscall nova: python3 tools/gerar-xtrace-nomes.py
import re, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(root, 'kernel/include/syscall.h'), encoding='utf-8').read()
body = src[src.index('enum {') + len('enum {'):]
body = body[:body.index('};')]
body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
names, n = [], -1
for item in body.split(','):
    item = item.strip()
    m = re.match(r'(SYS_\w+)\s*(?:=\s*(\d+))?$', item)
    if not m: continue
    n = int(m.group(2)) if m.group(2) else n + 1
    if m.group(1).endswith(('_FIRST', '_LAST', '_COUNT')): continue
    names.append((n, m.group(1)[4:].lower()))
out = ['/* gerado por tools/gerar-xtrace-nomes.py a partir de kernel/include/syscall.h: não edite à mão */']
for n, s in sorted(set(names)):
    out.append('    [%d] = "%s",' % (n, s))
open(os.path.join(root, 'kernel/proc/xtrace_nomes.inc'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print(len(names), 'syscalls')
