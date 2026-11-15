#!/usr/bin/env python3
# Gera userland/libsystem/mig_nomes.inc: (msgh_id, rotina) dos subsistemas MIG públicos, pela ordem das rotinas
# nos .defs depois do cpp (lado do usuário: sem KERNEL_SERVER). Só ids e nomes de API; nenhum código sai dos .defs.
# uso: python3 tools/gerar-mig-nomes.py <pasta osfmk com mach/*.defs> > userland/libsystem/mig_nomes.inc
# (os .defs vêm do SDK ou do código aberto do XNU; a lista de subsistemas é a do padrão abaixo)
import re, sys, os, subprocess
osfmk = sys.argv[1]
out = []
subs = sys.argv[2] if len(sys.argv) > 2 else ('mach_port,task,thread_act,mach_vm,vm_map,host_priv,mach_host,clock,'
    'clock_priv,clock_reply,processor,processor_set,exc,mach_exc,notify,mach_voucher,memory_entry,lock_set,semaphore,task_access')
print('/* gerado por tools/gerar-mig-nomes.py: msgh_id e nome das rotinas MIG públicas, em ordem de id */')
for sub in subs.split(','):
    p = os.path.join(osfmk, 'mach', sub + '.defs')
    if not os.path.exists(p): print('// sem', sub, file=sys.stderr); continue
    t = subprocess.run(['cpp', '-P', '-undef', '-DLIBSYSCALL_INTERFACE=1', '-D__LP64__=1', '-I', osfmk, p],
                       capture_output=True, text=True).stdout
    m = re.search(r'\bsubsystem\s+(?:KernelServer\s+|KernelUser\s+)*(\w+)\s+(\d+)\s*;', t)
    if not m: print('// sem subsystem em', sub, file=sys.stderr); continue
    base = int(m.group(2)); i = base
    for tok in re.finditer(r'\b(routine|simpleroutine|skip)\b\s*(\w*)', t[m.end():]):
        if tok.group(1) == 'skip': i += 1; continue
        out.append((i, tok.group(2))); i += 1
seen = set()
for i, n in sorted(out):
    if i in seen: continue
    seen.add(i); print('    {%d, "%s"},' % (i, n))
