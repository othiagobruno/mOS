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
| 4, 6–8 | aberto | — |

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

- `dma_alloc32(bytes, &phys)` e `pmm_alloc_below(pages, limit)` (Felipe, Wi-Fi RTL8821CE com DMA de 32 bits): bloco contíguo, zerado, inteiro abaixo de 4 GiB; libera com `pmm_free`. Não há zona reservada abaixo de 4 GiB: chamar na subida do driver. Busca com limite testada no host (harness temporário).
