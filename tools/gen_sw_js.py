#!/usr/bin/env python3
# mOS — gera kernel/web/web_sw_js.h (fonte JS do Service Worker embutida como string C) a partir de kernel/web/web_sw.js.
import os, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(root, 'kernel/web/web_sw.js'), encoding='utf-8').read()
out = ['/* gerado por tools/gen_sw_js.py a partir de kernel/web/web_sw.js — não edite à mão */',
       'static const char web_sw_src[] =']
for line in src.split('\n'):
    esc = line.replace('\\', '\\\\').replace('"', '\\"')
    out.append('    "' + esc + '\\n"')
out[-1] += ';'
open(os.path.join(root, 'kernel/web/web_sw_js.h'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
