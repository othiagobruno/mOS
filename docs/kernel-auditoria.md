# Auditoria do núcleo (kernel/proc, kernel/mm), novembro de 2026

O que foi corrigido neste ramo, como foi testado, e os pontos abertos que a leitura achou, por prioridade.
Ambiente: Linux, zig 0.17.1 ELF, QEMU 8 com OVMF 4M, TCG `-cpu max -smp 4`. Não houve Windows nem máquina física.

## Corrigido

### 1. Proteção máxima da memória compartilhada

Antes, três caminhos davam escrita a uma página que o processo só podia ler:

| Caminho | Efeito antes |
|---|---|
| `mprotect(PROT_WRITE)` (`SYS_MMAP b = 7`) num `MAP_SHARED` de fd `O_RDONLY` | `uvm_protect` ligava `PTE_RW`; o `munmap` gravava a página de volta no arquivo (`lazy_sync`) |
| `read()` (ou qualquer `uvm_copy_out`) com destino num mapa só-leitura | o kernel escrevia na página física compartilhada; só o COW era respeitado |
| `mach_vm_map` de uma entrada criada só-leitura, depois `mprotect` ou `read()` | a memória do outro processo mudava; o `KERN_INVALID_RIGHT` do `machipc.c` só valia na hora do mapa |
| `MACH_VM_REMAP` sem cópia de uma página assim | o apelido nascia gravável |

O Chromium confere que uma região só-leitura é mesmo só-leitura (comentário em `machipc.c`, `CheckPlatformHandlePermissionsCorrespondToMode`); essa garantia não valia.

Agora:

- `PTE_NOW` (bit 53, ignorado pelo hardware numa PTE presente; o 52 é a marca de swap numa PTE ausente) marca a página compartilhada cuja proteção máxima não tem escrita.
- `uvm_map_shared` põe `PTE_NOW` quando o prot não tem escrita nem `UVM_PROT_MAYW` (8). `lazy_add_shared_file` recebe `MAYW` só se o fd tem `O_WRONLY|O_RDWR`; a anônima compartilhada sempre tem.
- `uvm_map_phys(p, va, phys, UVM_PHYS_W | UVM_PHYS_MAYW)`: o `mach_vm_map` passa `MAYW` só se a entrada (`mprot`) permite escrita.
- `lazy_protect` devolve `-EACCES` se a faixa compartilhada não tem `MAYW`; `uvm_protect` recusa ligar `PTE_RW` com `PTE_NOW`.
- `uvm_copy_out` devolve falso (EFAULT) numa página sem `PTE_RW` que seja `PTE_SHARED`, `PTE_NOW` ou com mais de um dono. Só-leitura privada (texto, JWP) continua aceitando: o carregador e o `tracepoint.c` escrevem nela.
- O apelido do `MACH_VM_REMAP` herda o `PTE_NOW`.

Teste: `userland/mmshared.c`, oito checagens novas no fim. Mesmo teste nos dois kernels, mesma ESP:

| | `main` (eafefae) | este ramo |
|---|---|---|
| `mprotect(PROT_WRITE)` sem escrita no fd | 0 (FALHOU) | `-EACCES` |
| escrita na página depois disso | o filho escreve (FALHOU) | SIGBUS |
| `read()` para o mapa só-leitura | 16 bytes gravados, arquivo vira `W` (FALHOU) | `-EFAULT`, arquivo intacto |
| fd `O_RDWR`, mapa `PROT_READ`, `mprotect` RW | ok | ok (controle) |
| entrada Mach só-leitura: `mprotect` / `read()` | 0 / 8 bytes no original (FALHOU) | `-EACCES` / `-EFAULT` |
| `mmshared` inteiro | 6 FALHA(S) | TODOS OS TESTES OK |

### 2. Pilha do kernel

`clang -fstack-usage` em todo o `kernel/`: `proc_syscall_` tinha frame de **14 152 bytes** numa pilha de 32 KiB (`KSTACK_SIZE`, bloco de `kmalloc` sem página de guarda). O vetor `mos_spawn_fa_t acts[48]` (13 KiB) do `SYS_SPAWNVEC` entrava no frame de toda syscall. Agora vai para o heap: o frame caiu para **1 928 bytes**. O `path` de cada ação, que vem do ring 3, passa a terminar em NUL.

`kstack_check` (sched.c): duas palavras de marca no fundo de cada pilha do kernel, conferidas em toda troca de thread. Um estouro vira `panic("pilha do kernel estourou: thread N ...")` em vez de corromper o cabeçalho do heap e aparecer longe dali.

Teste: `userland/parateste.c` ganhou um `SYS_SPAWNVEC` com 48 ações (47 `CLOSE` com `path` sem NUL e um `OPEN` de stdout para arquivo).

### 3. Menores

- `copy_str_in` copiava um byte por `uvm_copy_in` (quatro níveis de tabela por byte de caminho). Agora copia por trecho de página.
- `tools/build.sh`, `tools/build-libcxx.sh` e `tools/swift-rt-grok-teste.sh`: a pasta do zig sai do binário real (`readlink -f`). Com `~/.local/bin/zig` como link, procuravam `lib/` em `~/.local` e paravam em "libunwind: falta any-darwin-any" / "libcxx do zig ausente".
- `ahci.c:stage_at`: com NCQ, o tag só é reusado com PxCI e PxSACT limpos (mesma regra do `ahci_reap` do PR #231). Hoje o reuso já é protegido pelo lote e pelo `recover_port` nos caminhos de erro; é uma defesa a mais.

### Revisão do PR #231 (disco), depois do merge

`ahci_reap` com `PxCI | PxSACT`, página do `pcache` nascendo com `stamp`, `evict_one` pulando `valid == 0`, política no `blk_write` e `blk_flush` no `storage_revoke_write`: corretos. Os caminhos de falha e de prazo do NCQ (`wait_batch`) passam por `recover_port`, que limpa PxCI e PxSACT antes de o tag voltar. Única sobra: página criada por um `load_run` cuja leitura falhou fica com `valid == 0` e agora não sai mais pelo `evict_one`; é reaproveitada na próxima leitura do mesmo deslocamento, então o custo é no máximo uma janela de leitura antecipada por erro de disco.

## Suíte completa na VM (TCG, este ramo rebaseado em 7e3d471)

ESP mínima com o kernel e o libSystem do `tools/build.sh`, os ELF de `userland/` e os três Mach-O do `-Tests`:
`posixteste` TODOS OK, `sinaisteste` TODOS OK, `dispatchteste` 30/30, `unixipc` TODOS OK, `mmshared` TODOS OK,
`smpteste` OK, `parateste` TODOS OK (rodado à parte depois de acertar o teste de spawn: o ELF não recebe `argv[1]`
pelo `SYS_SPAWNVEC`), 193 linhas `ok` no `teste`. Sem `panic` nem marca de pilha estourada.

Falham `sleep 100 ms` e trechos do `teste-sh` (Ctrl+C no `spin`): dependem de prazo de relógio e falham também na
`main` sob TCG com o host carregado (variam de rodada para rodada: `sleep` 101 ms no KVM, 1440 a 30754 ms no TCG).

## Exceções Mach, outras tasks e outras threads (crashpad, profiler, hang watcher)

Antes, o libSystem aceitava `task_set_exception_ports` e esquecia; `task_threads` devolvia só a thread atual; `thread_set_state`, `thread_abort` e `thread_terminate` de outra thread davam `KERN_NOT_SUPPORTED`; e a memória de outra task não era legível. O `0x103` (a task) mandado numa mensagem virava um objeto do kernel sem identidade. O crashpad de todo Electron nunca via um crash.

Agora (`kernel/proc/machipc.c`, `kernel/proc/process.c`):

- **Portas com identidade:** `KP_TASK` (pid) e `KP_THREAD` (tid). O `0x103` e o `0x1000 + tid`, quando vão numa mensagem, viram essas portas. Quem tem a porta da task opera nela, como no XNU.
- **`SYS_MACH_VM` novas (`syscall.h`):** `MACH_VM_TASK_RW` (`mach_vm_read`, `mach_vm_read_overwrite`, `mach_vm_write`), `MACH_VM_TASK_REGION`, `MACH_VM_TASK_PID` (`pid_for_task`), `MACH_VM_TASK_THREADS` e `MACH_VM_TASK_SUSPEND`. `MACH_VM_THREAD_CTL` aplica `MOS_THR_*` numa thread de outra task.
- **`MOS_THR_*` novas:** `MOS_THR_SETSTATE` (`thread_set_state`), `MOS_THR_TERMINATE` (outra thread; a principal leva o processo) e `MOS_THR_ABORT` (a espera em curso termina com EINTR ou MACH_RCV_INTERRUPTED). A lógica comum está em `proc_threadctl`. GETSTATE e SETSTATE valem para thread suspensa e também para thread presa numa exceção.
- **Portas de exceção:** `MACH_VM_EXC_SET` e `MACH_VM_EXC_GET` guardam a porta por task (`mspace_t.texc`) e por thread (`thexc`), com behavior e flavor. O kernel guarda um direito de envio.
- **Entrega na falta (`proc_fault`, antes do sinal):** `machipc_raise` monta `exception_raise` ou `mach_exception_raise` (2401/2405), as variantes `_state` e `_state_identity` (x86_THREAD_STATE64, x86_THREAD_STATE e x86_EXCEPTION_STATE64), com um send-once para uma porta `KP_EXCREPLY`. A thread espera (`wait_pid -13`), e os sinais ficam pendentes enquanto isso.
  - Resposta `KERN_SUCCESS`: a thread retoma, com o estado novo se a variante for state.
  - Resposta com outro valor, ou o tratador soltou o direito ou saiu: a mesma instrução falta de novo com `exc_skip` e segue para o sinal.
  - `int3` (que chega como #GP) é visto com o rip depois da instrução, como no XNU.
- **`EXC_CRASH`:** na morte por sinal que gera core (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP, SIGABRT, SIGQUIT, SIGSYS), tanto no `proc_signal_check` quanto na falta sem tratador, a task para inteira. A mensagem vai à porta da task com `code[0] = sinal << 24 | exceção << 20`. O processo só morre na resposta, ou quando o tratador some.

Teste: `userland/tests-excports/main.c` (Mach-O; ligado ao `build.ps1 -Tests` e ao `teste`).

## Pontos abertos, por prioridade

1. ~~**fd guarda só o caminho**~~ — resolvido sem trocar o fd por vnode: arquivo apagado (ou substituído por `rename`) enquanto um fd ou um `mmap` o usa vai para `/tmp/.orfaos/N`; fds e faixas do `lazy.c` passam a apontar para lá e `orphan_gc` apaga no último `close`/`munmap`/saída. `rename` também atualiza as faixas mapeadas. Teste: `userland/orfaoteste.c` (main: 6 FALHA(S), inclusive `mmap` lendo zeros; agora TODOS OK). Sobra: o `SYS_READ` ainda refaz `vfs_lookup` por chamada, e o fileport (machipc) guarda uma cópia do caminho que não acompanha.
2. **Trava grande (BKL)** e varreduras O(threads) sob ela: `sweep_ready` em todo `next_ready`, `wake_sleepers` a cada tique, `SYS_FUTEX_WAKE` percorre todas as threads do sistema. Com Electron (centenas de threads) isso é custo por troca de contexto. Próximo passo medido: fila de futex por (processo, endereço) e lista de dormentes ordenada por prazo.
3. ~~**Futex só privado**~~ — resolvido: numa página compartilhada de verdade (`PTE_SHARED`, ou sem COW e com mais de um dono) a chave do futex inclui o endereço físico. Teste em `mmshared.c` (main: wake 0 e o filho sai por ETIMEDOUT; agora acorda).
4. **Outros frames grandes no kernel** (`-fstack-usage`): `pkgextract.c:add_component` 32 936 bytes (roda na thread do desktop, pilha de boot de 1 MiB; numa thread do kernel estouraria), `launchsvc.c` 6 KiB em quatro funções, `web/layout.c:layout_block` 6,4 KiB com recursão.
5. **Página anônima nasce RWX** (`SYS_MMAP b = 3` sem prot). Só o `mprotect` liga o NX.
5b. **persist carrega o disco inteiro na RAM**: com `-m 512` e a ESP completa (464 MB) falta memória para o `app.asar` de 130 MB. O log agora diz "sem memória" com o tamanho e o livre; carregar sob demanda continua aberto.
6. **KVM nesta máquina**: com `-accel kvm` o OVMF 4M trava antes do `BdsDxe` na maioria das partidas (3 de 3 com 80 s); quando passa, o mOS sobe em 2 s e o autoteste passa. É do ambiente (Ryzen R2544, OVMF do Ubuntu), não do kernel, mas impede rodar a suíte rápida aqui.
7. `tools/build.sh` não fecha a imagem sem rede: depois do kernel ele baixa o `Claude.zip` (`downloads.claude.ai`) e para se o download falha. O kernel e o libSystem saem antes disso.

## Como repetir

```bash
# kernel (libSystem já em build/): flags do build.ps1
zig cc -target x86_64-uefi-msvc -O2 ... $(find kernel -name '*.c' -o -name '*.S') -o BOOTX64.EFI
# ESP mínima: EFI/BOOT/BOOTX64.EFI, root/mOS, userland/*.c como ELF em Sistema/Programas, .autoteste = "teste",
# e os três Mach-O do -Tests (posixteste, dispatchteste, sinaisteste)
qemu-system-x86_64 -machine q35 -m 1024 -cpu max -smp 4 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=vars.fd \
  -drive file=fat:rw:esp,format=raw,if=ide,index=0 \
  -vga std -display none -serial file:serial.log -nic none -no-reboot -monitor none
```
