# Interface Darwin/BSD/Mach do mOS (frente da Renata)

Dona: Renata (`renata/*`). Escopo (TIME.md, linha 5): syscalls BSD e traps Mach que apps usam, semântica XNU
(task/thread ports, exceções Mach, ptrace, sysctl, proc_info, kevent), XPC e launchd completos, sandbox ignorável sem
quebrar apps. Este doc é o plano e o placar honesto: cada item diz se está pronto, com qual teste, ou o que falta.

## Como a camada funciona hoje

- O app Mach-O chama o **libSystem** (`userland/libsystem`, ELF estático ligado por nome). O libSystem fala com o
  kernel por `int 0x80` com a ABI própria do mOS (`kernel/include/syscall.h`), não com os números do XNU.
- Não há entrada `syscall` com números Darwin (`0x2000000 + n`): `LSTAR` não é programado. Binário que faz syscall
  crua (raro: Go antigo, anti-depuração, alguns runtimes estáticos) cai em `#UD`.
- Mach: portas reais entre processos em `kernel/proc/machipc.c` (syscalls 80..83). Task, host e thread ainda são
  **nomes fixos** no libSystem (`TASK_PORT`, `HOST_PORT`, `0x1000 + tid`), não objetos do kernel.

## Levantamento (15 nov. 2026)

Medido com `tools/machocheck.c` contra o `libSystem.mos` do build de hoje e listas de referência das APIs públicas
(nomes de `libsystem_kernel`, `libsystem_info`, `libxpc`, `liblaunch`, `libsystem_sandbox`):

| Grupo | Referência | Não exportados | Observação |
|---|---:|---:|---|
| BSD (syscalls e wrappers) | 527 | 266 | maioria `*_nocancel`, `__mac_*`, `necp_*`, `psynch_*` (privados); faltam SysV IPC (`sem*`/`shm*`/`msg*`), `getattrlistbulk`, `setattrlist`, `fsgetpath`, `pid_suspend/resume`, `memorystatus_*`, `audit*` |
| proc_info (`libproc`) | 54 | 42 | `proc_pidinfo` só com 3 sabores; faltam `proc_pid_rusage`, `proc_pidfdinfo` completo, `proc_terminate`, `proc_*_dirty` |
| Mach (traps e MIG) | 341 | 208 | 184 rotinas MIG de cliente (lista do Bruno: `Shared/mos-time/bruno/mig-faltando.md`) |
| XPC | 258 | 106 | `xpc_session_*`, `xpc_listener_*`, `xpc_activity_*`, `xpc_shmem_*`, `xpc_*_set_fd/connection`, `xpc_create_from_plist` |
| launchd/vproc/sandbox/os | 120 | 81 | `launch_data_*` quase todo, `vproc_*`, `os_sync_wait_on_address`, `os_unfair_recursive_lock_*` |

Nos 28 bundles do corpus local (Firefox, GIMP, Inkscape, iTerm2, Rectangle, VLC e os de `build/apps-dl`) só
`semop/semget/semctl` e `bootstrap_parent` faltam **no link** da minha área. O problema maior não é link: é
**semântica falsa** em coisas que já ligam:

| Item | Hoje | Por que quebra app |
|---|---|---|
| `sysctl` | tabela fixa: 2 GiB de RAM, "mOS Virtual CPU" GenuineIntel, 1 CPU nas MIB, sem `kern.boottime`, `kern.procargs2`, `KERN_PROC_ALL`, `vm.swapusage`, `{0,3}` name2oid | Chromium/Electron, Activity monitors, Go runtime e `ps`/`top` leem tudo isso; a máquina alvo é AMD Ryzen 4c/8t com 14 GB |
| `task_info`/`host_info`/`host_statistics` | números inventados (64 MiB residentes, 65536 páginas livres) | gerência de memória do V8, `os_proc_available_memory`, monitores |
| portas de exceção | aceitam e esquecem; `get` devolve 0 portas | Crashpad (todo Electron/Chromium), PLCrashReporter, Sentry, depuradores, JITs que tratam `EXC_BAD_ACCESS` |
| `task_threads` | só a thread atual | coletores (JSC, Mono, .NET, Boehm), profilers, Crashpad |
| `thread_set_state`, `thread_abort` | `KERN_NOT_SUPPORTED` | GC com safepoints, .NET, Mono, depuradores |
| `task_for_pid` | só o próprio processo | Crashpad handler, `lldb`, `sample` |
| `ptrace` | `PT_DENY_ATTACH` aceito; resto falha | anti-depuração e depuradores |
| `kevent` | `kqueue.c` com leitura/escrita/timer em userland | `EVFILT_PROC`, `EVFILT_SIGNAL`, `EVFILT_MACHPORT`, `EVFILT_USER`, `EVFILT_VNODE` (este com o Vinicius) |
| XPC | conexões e dicionários prontos; sem sessões, listeners, activities, shmem | apps Swift novos (`XPCSession`), helpers, Sparkle 2 |
| launchd | `smjob.c`/`launchjob.inc` parciais; `launch_data_*` ausente | `launchctl`, login items, agentes |
| sandbox | `sandbox_init`/`apply` marcam e seguem | já não quebra o Chromium; falta `sandbox_check`, extensões e `sandbox_container_path_for_pid` coerentes |

## Plano (fases, cada uma com PR próprio, teste de host e doc atualizado)

1. **F1 — informação do sistema e sysctl completo.** Syscall `SYS_DARWIN = 110` (multiplexada, `MOS_DW_*`):
   memória total/livre, boot, uptime, hostname (get/set), dados por processo (pids, nome, caminho, pai, grupo,
   sessão, uids, início, CPU, memória, threads, estado, argv/env). libSystem: árvore sysctl inteira com MIB
   numéricas (`CTL_KERN`, `CTL_HW`, `CTL_VM`, `CTL_MACHDEP`, `CTL_NET` básico), `sysctlnametomib`, OIDs internos
   `{0,1}` `{0,2}` `{0,3}` `{0,4}`, CPUID real (marca, fornecedor, família, recursos AVX/AVX2/AES/…), `kern.proc`
   (ALL/PID/PGRP/SESSION/TTY/UID/RUID), `kern.procargs2`, `kern.boottime`, `vm.swapusage`, `vm.loadavg`,
   `kern.hostname` gravável. Teste: `tools/sysctl_host.c`.
2. **F2 — proc_info e task/host info reais.** `proc_pidinfo` com todos os sabores públicos (BSDINFO, TASKINFO,
   TASKALLINFO, THREADINFO, LISTTHREADS, LISTFDS, VNODEPATHINFO, REGIONINFO, WORKQUEUEINFO, T_SHORTBSDINFO,
   UNIQIDENTIFIERINFO), `proc_pidfdinfo`, `proc_pid_rusage` (V0..V6), `proc_terminate`, `proc_*_dirty`,
   `task_info` (todos os sabores), `host_info`/`host_statistics64` com os números do kernel, `os_proc_available_memory`.
3. **F3 — task e thread como objetos Mach do kernel.** Portas `KP_KOBJ` de task/thread por processo e por thread,
   `task_threads` real, `task_for_pid` (mesmo uid), `pid_for_task`, `task_suspend/resume`, `thread_create(_running)`,
   `thread_terminate` de outra thread, `thread_set_state`, `thread_abort(_safely)`, `mach_port_names`,
   `mach_port_space_info`, `mach_port_get_srights` e as rotinas MIG de VM (`mach_vm_write/copy/read_list/...`).
4. **F4 — exceções Mach.** Portas de exceção por task/thread/host com máscara, comportamento (DEFAULT, STATE,
   STATE_IDENTITY, `MACH_EXCEPTION_CODES`) e sabor; o kernel converte #PF/#GP/#UD/#DE/int3/`ud2` em mensagem
   `mach_exception_raise*` antes do sinal BSD, espera a resposta (KERN_SUCCESS retoma, falha vira sinal),
   `exc_server`/`mach_exc_server`, `task_swap_exception_ports`. Teste: tratador que corrige um `EXC_BAD_ACCESS`.
5. **F5 — ptrace e syscalls Darwin cruas.** `ptrace` (TRACEME, ATTACHEXC, CONTINUE, KILL, STEP, DETACH, DENY_ATTACH,
   SIGEXC) sobre as portas de exceção; entrada `syscall` com números Darwin (classe BSD 2 e Mach 1) roteada para os
   mesmos tratadores do libSystem.
6. **F6 — kevent completo.** EVFILT_READ/WRITE/TIMER/USER/PROC/SIGNAL/MACHPORT/EXCEPT/VNODE (VNODE com os eventos do
   VFS do Vinicius), `kevent64`, `kevent_qos`, `EV_DISPATCH`/`EV_CLEAR`/`EV_ONESHOT`/`EV_RECEIPT`, no kernel para
   valer entre threads e com `select`/`poll` no mesmo fd.
7. **F7 — BSD restante.** SysV IPC (`sem*`, `shm*`, `msg*`), POSIX `sem_open/sem_unlink/sem_trywait`, `getattrlistbulk`,
   `setattrlist`, `fsgetpath`, `pid_suspend/resume`, `memorystatus_*`, `audit*` (sem auditoria: respostas coerentes),
   `os_sync_wait_on_address*`, `os_unfair_recursive_lock_*`, `psynch_*`, `__pthread_kill/sigmask`, `getdirentries64`.
8. **F8 — XPC completo.** Sessões, listeners, `xpc_activity_*` (agenda real), `xpc_shmem_*`, fd/conexão em
   dicionário e array, `xpc_create_from_plist`, entitlements do bundle, endpoints entre processos, XPC services
   dentro de `Contents/XPCServices` lançados sob demanda.
9. **F9 — launchd completo.** `launch_data_*`, `launch_activate_socket`, `vproc_*`, `launchctl`
   (bootstrap/bootout/kickstart/list/print), LaunchAgents/LaunchDaemons com KeepAlive, RunAtLoad, StartInterval,
   StartCalendarInterval, WatchPaths, Sockets, MachServices, `SMAppService`/`SMJobBless`.
10. **F10 — sandbox coerente.** `sandbox_check*`, extensões (emitidas e consumidas com token real), contêiner de app
    em `~/Library/Containers/<id>` quando o app tem `com.apple.security.app-sandbox`, sem negar nada que o app precise.

## Placar

| Fase | Estado | Teste |
|---|---|---|
| F1 | **pronto** (15 nov.): `SYS_DARWIN` no kernel (`kernel/proc/darwin.c`), `userland/libsystem/sysctl.c` | `tools/sysctl_host.c`: 89/89 no Ryzen R2544 do Thiago (CPUID real, kernel falso). Não rodou no QEMU (build proibido até o fim) |
| F2 | **pronto** (15 nov.): `userland/libsystem/procinfo.c`, `task_info`/`thread_info`/`host_info` em `mach.c`, `MOS_DW_FDS`/`MOS_DW_CWD` no kernel | `tools/procinfo_host.c`: 50/50 (kernel falso). Não rodou no QEMU |
| F7 (parte 1) | **pronto** (15 nov.): IPC System V (`sysv_ipc.c`), `oslock.c`, `pid_suspend/resume` | `tools/sysv_ipc_host.c` 35/35 entre processos reais (fork); `tools/oslock_host.c` 9/9 com 4 threads |
| F6 | **pronto** (15 nov.): `userland/libsystem/kqueue.c`, gancho de sinais em `libc.c`, `MOS_DW_EXITSTATUS` | `tools/kqueue_host.c` 33/33 (kernel falso) |
| F9 (parte 1) | **pronto** (15 nov.): `userland/libsystem/launchdata.c`, `MOS_LAUNCHD_MACHSERVICES` no `init.c` | `tools/launchdata_host.c` 14/14 |
| F10 | **pronto** (15 nov.): sandbox em `userland/libsystem/darwin.c` | `tools/sandbox_host.c` 11/11 |
| F3 (parte 1) | **pronto** (16 nov.): `MACH_OP_NAMES` no kernel, `userland/libsystem/machports.c` | `tools/machports_host.c` 15/15 |
| F4/F5 (exceções, outra task/thread) | **pronto pelo Henrique** (PR #390, `userland/tests-excports` 18/18 na VM) | — |
| F3 (resto), F5 (ptrace, syscall crua, exceção do host, EXC_RESOURCE/GUARD), F7 (resto), F8, F9 (resto) | a fazer | — |

### F3 (parte 1) em detalhe

`mach_port_names`, `mach_port_space_info` (tabela; a árvore não existe mais no XNU), `mach_port_space_basic_info`,
`mach_port_get_srights` (só com receive), `mach_port_get_set_status`, `mach_port_kobject`/`kernel_object`/
`kobject_description` (task, thread, host, porta comum), `mach_port_allocate_full/qos`, `mach_port_dnrequest_info`,
`mach_port_rename` (KERN_NOT_SUPPORTED, removida do XNU). Só o próprio espaço de nomes.

### F10 em detalhe

- Política: o mOS não nega nada a um app (é a escolha da frente: "sandbox ignorável sem quebrar apps"). O que o app
  pergunta tem resposta coerente: `sandbox_check(pid, NULL)` diz "em sandbox" depois de `sandbox_init/apply` ou num
  helper do Chromium sem `--no-sandbox`; toda operação é permitida; `sandbox_check_by_audit_token`;
  extensões (`issue_file/mach/generic/to_process`, `sandbox_issue_fs_extension`) dão token `<classe>;<alvo>`,
  `consume` dá handle novo, `release` com handle inválido dá EINVAL; `sandbox_container_path_for_pid` dá erro
  (nenhum processo tem contêiner: o HOME não muda); `sandbox_spawnattrs_*`, `sandbox_note`, `suspend/unsuspend`,
  `query_approval_policy`.

### F9 (parte 1) em detalhe

- O launchd do mOS já existia (`userland/init.c`, `launchctl`, ativação sob demanda, `docs/servicos.md`). Faltava a
  API cliente: `launch_data_*` inteiro (dicionário, array com buraco, fd, porta, inteiro, real, bool, string, opaco,
  errno, cópia profunda), `launch_msg("CheckIn")` com Label e as portas dos MachServices (o launchd passa os nomes em
  `MOS_LAUNCHD_MACHSERVICES`; as portas vêm do `bootstrap_check_in`), `GetJob` do próprio job, `launch_activate_socket`,
  `vproc_transaction_*`, `vproc_standby_*`, `vproc_swap_integer` (IS_MANAGED, TRANSACTIONS_ENABLED).
- Limites: o launchd do mOS não abre `Sockets` do plist (activate_socket dá ENOENT/ESRCH, como um job sem a chave);
  pedidos de administração por `launch_msg` (GetJobs, SubmitJob) dão EPERM: o caminho é o `launchctl` (protocolo LC_*).
  Falta da F9: `StartCalendarInterval`, `WatchPaths`, `QueueDirectories`, `Sockets` no `init.c`.

### F6 em detalhe

- Filtros: READ (arquivo comum dá o que falta a partir da posição; pipe, terminal, socket e AF_UNIX como antes),
  WRITE, EXCEPT (registra; sem OOB no mOS), VNODE (DELETE, WRITE, EXTEND, ATTRIB, LINK, RENAME, REVOKE pelo journal do
  VFS do Vinicius, casando pelo inode do fd; pasta: filho criado/removido/renomeado), PROC (EXIT, EXITSTATUS sem colher
  o filho, FORK, EXEC), SIGNAL (conta a entrega mesmo com SIG_IGN, como o macOS: o `libc.c` põe um tratador interno
  e repassa à ação do usuário), TIMER (s/ms/us/ns, NOTE_ABSOLUTE, disparos perdidos em `data`), MACHPORT, FS (montar e
  desmontar), USER, VM e MEMORYSTATUS (nível de pressão do kernel). Erros de registro como o XNU: EINVAL (filtro),
  EBADF (fd), ESRCH (pid). Flags: DISPATCH, DISPATCH2, UDATA_SPECIFIC, RECEIPT, ONESHOT, CLEAR, EOF.
- Limites: 256 kqueues e 1024 knotes por kqueue. A espera continua sendo consulta com cochilo crescente (1 a 8 ms,
  nunca além do próximo timer); uma espera no kernel em várias fontes de uma vez ainda não existe.

### F7 (parte 1) em detalhe

- IPC System V completo: `semget/semop/semctl` (tudo ou nada, IPC_NOWAIT, SEM_UNDO desfeito no `_exit`, GETNCNT/
  GETZCNT, GETALL/SETALL, IPC_STAT/SET/RMID), `shmget/shmat/shmdt/shmctl` (SHM_RDONLY, SHM_RND, endereço fixo,
  `nattch`, remoção adiada até o último `shmdt`), `msgget/msgsnd/msgrcv/msgctl` (tipo 0/positivo/negativo,
  MSG_NOERROR, E2BIG, fila cheia), `semsys/shmsys/msgsys`, `ftok`, chaves `kern.sysv.*` no sysctl. Layouts do macOS
  (`semid_ds` 72, `shmid_ds` 76, `msqid_ds` 116, pack 4). Objetos em arquivos MAP_SHARED de `/tmp/.sysv-ipc`, espera
  no futex compartilhado. Limite honesto: morte por sinal não desfaz o SEM_UNDO (o kernel não conhece o semáforo).
- `os_unfair_lock` agora dorme no futex quando há disputa (antes girava com `SYS_YIELD`), guarda o dono e aborta
  com "BUG IN CLIENT OF LIBPLATFORM" em trava recursiva, unlock de outra thread e `assert_owner/not_owner`, como o
  macOS. `os_unfair_recursive_lock_*` e `os_sync_wait_on_address(_with_timeout/_with_deadline)`/`os_sync_wake_*`.
- O que falta da F7: `memorystatus_control` (sobre o `SYS_VMCTL` do Gustavo), `getattrlistbulk`/`setattrlist`
  (combinado com o Vinicius e o Bruno), `__pthread_kill/__pthread_sigmask` e os `*_nocancel`, `audit*`.

### F2 em detalhe

- `proc_pidinfo`: LISTFDS, TASKALLINFO, TBSDINFO, TASKINFO, THREADINFO, LISTTHREADS(IDS), VNODEPATHINFO (pasta atual e
  raiz), PATHINFO, WORKQUEUEINFO, T_SHORTBSDINFO, THREADID64INFO, UNIQIDENTIFIERINFO, T_BSDINFOWITHUNIQID e ARCHINFO
  com os dados do kernel (ids reais, sessão, estado, início, CPU, memória, threads, fds). `proc_pidfdinfo`: VNODEINFO,
  VNODEPATHINFO (com `vinfo_stat` do `stat` real) e PIPEINFO. `proc_pid_rusage` v0..v6 (CPU, residente,
  `phys_footprint` do `SYS_VMCTL` do Gustavo, início em abstime). `proc_listpids` por todos os filtros (pgrp, tty,
  uid, ruid, ppid), `proc_pidpath`/`proc_name` de qualquer processo, `proc_terminate` (SIGKILL se limpo e ocioso),
  rastreio dirty, monitores de CPU/wakeup, `os_proc_available_memory`.
- `task_info`: BASIC_INFO_64, MACH_TASK_BASIC_INFO (com pico), THREAD_TIMES, EVENTS, POWER, DYLD, AUDIT_TOKEN e
  SECURITY_TOKEN com os ids reais, FLAGS e VM_INFO (residente, comprimido, `phys_footprint`, faixa de endereços).
  `thread_info`: BASIC (tempo, estado, suspensões), IDENTIFIER, EXTENDED e SCHED_TIMESHARE. `host_info`: BASIC (RAM
  e núcleos físicos/lógicos reais), SCHED, PRIORITY, CAN_HAS_DEBUGGER, PREFERRED_USER_ARCH, MACH_MSG_TRAP.
- Limites honestos: REGIONINFO/REGIONPATHINFO esperam o `mach_vm_region` de outra task (Henrique) e devolvem
  ENOTSUP; sockets TCP/UDP e kqueues vivem no libSystem, então `proc_pidfdinfo` deles dá EBADF; o kernel não separa
  tempo de usuário e de sistema nem conta faltas e mensagens (TASK_EVENTS_INFO zerado); o rastreio dirty vale para o
  próprio processo (outro pid: EPERM).

### F1 em detalhe

- Kernel: `SYS_DARWIN = 110` com `MOS_DW_SYSINFO` (RAM total/livre do `pmm`, hora do boot, uptime, troca comprimida,
  contagem de processos e threads), `HOSTNAME`/`SETHOSTNAME` (root), `PIDS`, `PROC` (ids, sessão, estado, início,
  CPU, memória, caminho), `ARGS` (formato `KERN_PROCARGS2`, lido da pilha do alvo; o `macho.c` registra o bloco de
  strings em `process_t.args_*`, herdado no `fork`) e `THREADS`.
- libSystem: árvore com ~190 chaves; MIBs fixas do XNU em `CTL_KERN/VM/HW/USER`, `OID_AUTO` a partir de 1000;
  OIDs internos `{0,1}` nome, `{0,2}` próximo, `{0,3}` nome→MIB, `{0,4}` formato; `KERN_PROC` ALL/PID/PGRP/SESSION/
  TTY/UID/RUID com `KERN_PROCSLOP`; `KERN_PROCARGS` e `KERN_PROCARGS2`; bigcheck de 64→32 bits; `kern.hostname`
  gravável; CPUID real (fornecedor, marca, família, folhas 1/7/0x80000001, caches pela folha 4 ou 0x8000001D,
  threads por núcleo, TSC pela folha 0x15 ou medido, AVX só se o XCR0 do kernel salva YMM, AVX-512 só com ZMM).
- Limites honestos: `vm.loadavg` é o número de threads prontas no instante (o kernel não guarda média móvel);
  `hw.cpufamily` = 0 (não existe código Apple para AMD); `kern.maxproc` é o valor de um Mac (o kernel não tem teto);
  `vm.swapusage.total` = usado arredondado a 64 MiB (troca comprimida em RAM).

### Combinado com outros donos

- Henrique (15 nov.) faz agora no kernel e no libSystem: portas de exceção task/thread/host com `exception_raise`
  na falta, `task_threads` completo, `thread_set_state`/`thread_abort(_safely)`/`thread_terminate` de outra thread,
  `mach_vm_read(_overwrite)/write/region` e `task_for_pid` em outra task. Eu reviso o PR. Isso sai da F3/F4 desta
  lista; o resto das rotinas MIG (`bruno/mig-faltando.md`) continua aqui.
- Vinicius: `EVFILT_VNODE` e FSEvents usam os eventos do VFS dele; stat/xattr/chflags são dele (`SYS_FSMETA`).

## Números de syscall reservados por esta frente

- `SYS_DARWIN = 110` (`MOS_DW_*` em `syscall.h`). 106 é do Vinicius (`SYS_FSMETA`).
