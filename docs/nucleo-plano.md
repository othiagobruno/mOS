# Núcleo do kernel: plano e estado (frente do Gustavo)

Frente: SMP sem trava grande, escalonador com prioridades e QoS do Darwin, memória virtual completa
(swap, compressor, memória compartilhada, limites), desempenho e zero panic. Combina com o Henrique
(líder de kernel). Syscalls BSD/Mach são da Renata; quando um item aqui precisa de uma syscall nova,
a interface é combinada com ela.

Cada item diz o que está pronto, com que teste, e o que falta. Nada aqui foi para a imagem completa
antes do build final (REGRAS.md).

## Levantamento (main em 97c611b, 15 nov. 2026)

| Área | Como estava | Problema |
|---|---|---|
| "Swap" (`mm/swap.c`) | 1/16 da RAM tirada do PMM na subida, cópia crua da página | Troca página de RAM por página de RAM: não ganha nada. Teto de 4096 slots (16 MiB) |
| Pressão (`mem_pressure`) | Chamada de dentro do `pmm_alloc`, pega as 32 primeiras páginas anônimas do 1º processo | Sem `invlpg`/shootdown: outro núcleo (ou o próprio TLB) continua escrevendo no frame já liberado. Rodava com travas de faixa do chamador seguras (corrida com `uvm_map`) |
| Cópia do kernel (`uvm_copy_in/out`, `uvm_alias_map`) | `uvm_pte` → `lazy_fault` | Página trocada: o `lazy_fault` mapeava página **zerada** por cima da PTE de swap. Perda de dado e slot vazado |
| `munmap`, `MADV_DONTNEED`, `mm_release_span` | `*e = 0` | PTE de swap zerada sem soltar o slot |
| Volta do swap | `PTE_RW` sempre | Página anônima com `mprotect(PROT_READ)` voltava gravável |
| `mprotect` | Só PTE presente | Página trocada ignorada |
| `mm_span_taken` | Só PTE presente | Página trocada contava como livre para `MAP_FIXED` sem `MAP_FIXED_NOREPLACE` |
| Escalonador | 5 filas de QoS por núcleo, fatia por classe | Sem prioridade dentro da classe (0–127 do Mach), sem decaimento por uso de CPU, `sweep_ready`/`wake_sleepers` O(threads) em todo tique, futex O(threads) |
| SMP | Trava grande (BKL) | Toda syscall serializada |
| Limites | Nenhum | Sem `RLIMIT_AS`/`RLIMIT_DATA`, sem limite por processo do tipo memorystatus/jetsam |

## Plano, em ordem

1. **Compressor de memória** (`mm/vmcomp.c`) — **feito neste PR**. Codificador próprio por palavras
   (dicionário de 16 entradas, zero e corrida de zeros, igual, 22 bits altos iguais, literal, cópia de
   palavras para trás por hash de pares), zona por classes de
   tamanho que devolve a página ao PMM quando esvazia, página toda zero sem armazenamento. Teste de host
   `tools/vmcomp_host.c`.
2. **Swap sobre o compressor** (`mm/swap.c`) — **feito neste PR**. Slots dinâmicos (até 2^24), teto
   da zona em 1/4 da RAM, PTE de swap guarda RW, JWP e NX. Sem reserva fixa na subida.
3. **Correções de swap no `process.c`** — **feito neste PR**: troca por thread `vm_pageout` (fora do
   `pmm_alloc`), relógio com bit Accessed, PTE tirada e shootdown antes de comprimir, volta do swap em toda
   cópia do kernel, `munmap`/`DONTNEED`/saída soltam o slot, `mprotect` e `mm_span_taken` enxergam a PTE
   trocada.
4. Swap em disco para página incompressível (arquivo `/private/var/vm/swapfile*` no volume do sistema,
   via `storage`), com a regra de slot só reutilizado depois de sair de todas as PTEs.
5. Escalonador: prioridade Mach 0–127 por thread dentro da faixa da QoS, decaimento por uso
   (como o `sched_clutch`/timeshare), `setpriority`/`thread_policy_set` (com a Renata), IPI de
   acordar núcleo ocioso, lista de dormentes ordenada por prazo e futex em tabela de hash
   por (espaço, endereço) — tira as varreduras O(threads).
6. Limites de memória: `RLIMIT_AS`, `RLIMIT_DATA`, `RLIMIT_STACK`; limite por processo e nível de
   pressão (normal/aviso/crítico) com notificação ao `DISPATCH_SOURCE_TYPE_MEMORYPRESSURE` e
   encerramento do maior processo de fundo quando o crítico persiste.
7. SMP sem trava grande, por partes: syscalls só de leitura (tempo, getpid, futex sem espera) sem BKL;
   PMM e heap já têm trava própria; depois Mach IPC, pipes/PTY, VFS e rede, cada um com trava de
   subsistema e ordem de travas documentada; threads do kernel nos APs.
8. Zero panic: `-fstack-usage` dos frames grandes que sobram (`pkgextract.c`, `launchsvc.c`,
   `web/layout.c`), página de guarda nas pilhas do kernel, `panic` virando morte do processo quando a
   falha é de um processo só.

## Estado

| Item | Estado | Teste |
|---|---|---|
| 1 compressor | pronto | `tools/vmcomp_host.c`: zero 3 B, texto 251 B (6 %), esparso 24 %, página cheia 13 %; 20 000 páginas mistas ida e volta iguais; fluxo cortado ou com sobra recusado; ~400 MiB/s comprimindo no host |
| 2 swap no compressor | pronto | `tools/vmcomp_host.c`: 4000 páginas, fork com dois donos, munmap solta o slot, zona volta a 0 páginas; `resto_host` com a API nova |
| 3 correções de swap | pronto, sem VM | `clang -fsyntax-only` nos arquivos do kernel; QEMU só no build final |
| 5 escalonador | pronto, sem VM | `tools/futexq_host.c`: 99 657 wakes iguais aos da varredura antiga (processo+va ou chave física), n limitado, chave física entre processos; prioridade QoS→Mach, decaimento e faixa 0..127 |
| 6 limites | pronto, sem VM | `tools/vmpolicy_host.c`: pressão 1/2/4, rlimit (padrões, EPERM, EINVAL, corte no teto), vítima do jetsam, limite de footprint; `clang -fsyntax-only` no kernel e no libSystem |
| 7 SMP sem trava grande | pronto no escalonador, sem VM | trava própria do escalonador (`sched_lk`: filas, estados, lista de threads, prazos, futex), solta no `sched_post` depois da troca de pilha. Sem a trava grande: tempo/pid/uid/CPUs; nos APs, `FUTEX_WAIT`/`FUTEX_WAKE`/`YIELD` e o tique do LAPIC (escalona mesmo com a grande ocupada; reap e sinais esperam a grande). `in_nobkl` faz o kick de thread morta e o shootdown esperarem o núcleo sair da memória do processo. Mach IPC, pipes/PTY, VFS e rede continuam sob a grande; sem QEMU até o build final |
| 8 zero panic | parcial | `pkgextract.c:add_component` 33 KiB de frame → heap (`pkgtest` 58/58, 0 alocações vivas). `clang -fstack-usage -O2` em 189 arquivos do kernel (16 nov.): maior frame agora 9,4 KiB (`apps/arquivos.c:paint`, na thread do desktop, pilha de 1 MiB); no caminho de syscall o maior é `launchsvc.c` com 6,4 KiB (pilha de 32 KiB). Pilhas do kernel com página de guarda: 32 KiB do PMM com uma página ausente embaixo (`vmm_guard` divide a página de 2 MiB do mapa de identidade); estouro vira falta na hora e o `exc_panic` diz qual thread (`sched_kstack_guard_hit`), sem varrer a pilha estourada |
| 4 | aberto | — |

### Limites (item 6), como ficou

- `SYS_VMCTL` (120): `MOS_VM_STATS` (RAM, compressor, pageins/outs, nível de pressão, jetsam), `GETRLIM`/`SETRLIM`
  por processo (herdados no fork, vfork e spawn), `SETLIMIT` (footprint em MiB, como o memorystatus), `SETJPRIO`
  (prioridade de jetsam 0..210) e `FOOTPRINT` (residente + comprimido).
- Falta de página que passa do limite de footprint encerra o processo com SIGKILL (log `memorystatus:`).
- Pressão crítica (livre < metade do piso) por 3 s sem o compressor liberar nada: `jetsam:` encerra o processo de menor
  prioridade de jetsam e maior footprint (pid 1 e prioridade ≥ 180 nunca).
- `RLIMIT_DATA` vale no `brk`. `NOFILE` e `STACK` mostram o teto real do kernel no cur (256 fds, 2 MiB) e aceitam max maior,
  como o XNU.
- libSystem: `getrlimit`/`setrlimit` reais; `kern.memorystatus_vm_pressure_level` do `sysctl.c` (da Renata) lê o nível do
  kernel; `host_statistics64(HOST_VM_INFO64)` com livres, internas, compressor e pageins/outs no layout de 38 palavras;
  `pthread_setschedparam` liga `MOS_THR_SETPRIO` (FIFO/RR fixas).

### Escalonador (item 5), como ficou

- Prioridade Mach por thread: `base_pri` vem da QoS (47/37/31/20/4, como no XNU) ou de `MOS_THR_SETPRIO`
  (`SYS_THREADCTL`, `pri | 0x100` = fixa; acima de 63 só root). Efetiva = base − 1 a cada 8 ms de CPU recente
  (meia-vida de 100 ms), até −16; a fixa não decai. Fila por núcleo em 32 baldes (efetiva/4) com bitmap.
- Preempção: thread pronta num balde acima da atual troca no próximo tique, sem esperar o fim da fatia.
  Núcleo ocioso recebe o IPI 0xF2 na hora em que uma thread entra na fila dele.
- `sched_wake` em todos os 22 pontos que escreviam `TH_READY` direto (pipes, terminal, Mach, unix, GPU, sinais, wait).
  `sweep_ready` virou rede de segurança a cada 100 ms e loga o primeiro resgate.
- Dormentes: a varredura só roda quando o prazo mais próximo vence, quando alguém bloqueia (sched_switch) ou muda
  prazo de fora (`sched_deadline_dirty`), e no máximo a cada 50 ms. Antes: todo tique de todo núcleo.
- Futex: `proc/futexq.c`, hash de 512 baldes por (processo, va) e pela chave física (`futex_shared_key`, PR #236).
- `tools/ptytest.c` ganhou o `proc_job_signal` que faltava para ligar; ele continua caindo (SIGSEGV) também na main,
  antes desta mudança. É harness do terminal, não do núcleo.

## Pedidos de outras frentes

- S3 com vários núcleos (Larissa, `pm_register_cpu_hotplug`): `smp_cpus_offline` tira cada AP do escalonador
  (`sched_cpu_offline_begin`: fila migrada, a thread em execução volta à fila no próximo tique) e espera ele estacionar
  em cli/hlt com o temporizador mascarado (1 s; se falhar, desfaz e devolve false). `smp_cpus_online` religa por
  INIT-SIPI um por vez, com o mesmo índice, pilha de partida e IST (`sched_cpu_online`). Registrado no `smp_init`.

- `dma_alloc32(bytes, &phys)` e `pmm_alloc_below(pages, limit)` (Felipe, Wi-Fi RTL8821CE com DMA de 32 bits): bloco contíguo, zerado, inteiro abaixo de 4 GiB; libera com `pmm_free`. Não há zona reservada abaixo de 4 GiB: chamar na subida do driver. Busca com limite testada no host (harness temporário).
- `gpu_user_map_io(p, va, phys, len, rw)` / `gpu_user_unmap_io` (Thais, VRAM): PTE WC (PWT = PAT1), `PTE_SHARED`, NX, sem escrita com `PTE_NOW` se rw = 0. Exige a faixa registrada por `pmm_mark_device` (o `vmm_map_wc` registra). `page_put`, `page_ref_inc` e `pmm_free` ignoram páginas de dispositivo: munmap, fork e saída do processo nunca devolvem VRAM ao PMM. Tabela testada no host (harness temporário).
