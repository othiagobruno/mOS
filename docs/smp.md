# SMP no mOS: processos de usuário em todos os núcleos

Até aqui os núcleos extras (APs) acordavam (`arch/smp.c`: MADT, LAPIC, HPET, INIT-SIPI) mas só faziam
trabalhos de `par_for` da composição gráfica. Agora eles rodam threads de usuário de verdade, de
processos diferentes ou do mesmo processo, ao mesmo tempo que o BSP.

## Estratégia: trava grande do kernel (primeiro passo)

O kernel inteiro foi escrito supondo um núcleo só (desktop cooperativo na thread 0, VFS, drivers, Mach IPC,
pipes, PTY). Em vez de travar cada subsistema de uma vez, entrou uma **trava grande** (BKL), com dono por
núcleo (`bkl_owner` em `kernel/proc/sched.c`):

| Quem | Roda com a trava? |
|---|---|
| Código de usuário (ring 3), em qualquer núcleo | **não** — é aqui que está o paralelismo |
| Syscall (`int 0x80`) e falta vinda do ring 3 | sim: espera por ela na entrada (`isr_dispatch`, `arch/cpu.c`) |
| Temporizador (PIT no BSP, LAPIC nos APs) e IPI 0xF2 | só *tenta*; se estiver ocupada, a thread de usuário segue rodando |
| Desktop (thread 0) e threads do kernel, no BSP | sim, o tempo todo; a thread 0 solta em todo `hlt()` (`cpu_idle`) |
| Tratadores de IRQ do PIC (PIT, PS/2; só no BSP) | não: só mexem nos próprios buffers |
| Trabalhos de `par_for` e laços ociosos | não |

Como o desktop só larga a trava nos `hlt` — exatamente onde o modelo antigo deixava threads de usuário
rodarem —, a regra antiga continua valendo para todo o código do kernel: **syscalls rodam "com o desktop
parado" e uma de cada vez**. Por isso machipc, pty, pipes, VFS e rede não precisaram de mudança.

A trava só é solta em `sched_post`, que o `isr_common` (`arch/x86.S`) chama **já na pilha da thread que vai
rodar**: até ali o núcleo ainda usa a pilha do kernel da thread anterior, que outro núcleo poderia pegar.

No BSP, a espera pela trava abre as interrupções entre as tentativas (o PIT segue contando para quem mede
tempo em outro núcleo, o PS/2 não perde bytes); essas interrupções aninhadas só fazem o mínimo
(`sched_nested`).

## Escalonador por núcleo

`pcpu_t` guarda, por núcleo: thread atual, thread ociosa, última de usuário (round-robin), início da fatia,
CR3 em uso, marcas pendentes e se está no ring 3. `thread_current()` usa o núcleo atual, que vem do `str`:
cada núcleo tem o próprio TSS na GDT (seletor `0x28 + 16*cpu`, `cpu_tss_init`), com o próprio `rsp0` e a
própria pilha IST.

- **BSP**: política de antes — a thread 0 só cede num `hlt`; threads do kernel e de usuário rodam em fatias de
  `SCHED_SLICE_MS` e, no fim, o desktop volta.
- **APs**: só threads de usuário (as do kernel supõem um núcleo). A thread ociosa de cada AP é o próprio laço
  dele (`ap_loop`), que também atende `par_for`. O temporizador do LAPIC dispara a cada 1 ms (vetor 0xF0);
  no `hlt` ocioso ele escolhe uma thread pronta.
- Uma thread nunca é escolhida se já estiver em outro núcleo (`on_cpu`), e threads mortas só são recolhidas
  quando nenhum núcleo as está rodando.
- O AP assume o modo do BSP na partida: CR0, CR4 (com FSGSBASE), EFER.NXE/SCE, XCR0 e PAT.

## IPIs: TLB e threads mortas

Vetor **0xF2** (`VEC_KICK`). Cada núcleo tem `pending` (`PEND_FLUSH`, `PEND_RESCHED`) e `in_user`, num
protocolo de Dekker (seq_cst nos dois lados): o alvo publica `in_user = 1` e só então relê `pending` antes
de voltar ao ring 3; quem pede marca `pending` e só então lê `in_user` — se o alvo está no usuário, manda o
IPI e espera ele entrar no kernel.

- `smp_tlb_shootdown(cr3)` (kernel.h): os outros núcleos com esse CR3 recarregam o CR3 antes de voltar ao
  usuário. Chamada pelo `fork` (pai perde o RW), `cow_break`/`vm_flush` e `munmap` em `proc/process.c`.
- `smp_kick_proc(p)`: depois de `finish()` ou do `execve` marcarem as outras threads de `p` como
  `TH_DEAD`, tira do ring 3 as que estão rodando em outro núcleo, antes de a memória do processo sair.
  Na volta, `sched_dead_check` troca de thread.

## Teste

`build.ps1 -Tests` também roda `userland/smpteste.c` no boot (saída na serial):

1. um operário sozinho (laço de CPU sem syscalls, com SSE) dá a referência;
2. três operários (processos) ao mesmo tempo: todos têm de devolver o mesmo resultado e, com mais de um
   núcleo, acelerar ao menos 1,3× sobre o tempo serial (a VM medida deu 1,6× com 3 operários, sob TCG e com os
   outros autotestes rodando juntos);
3. três threads do mesmo processo calculando e fazendo `mmap`/escreve/confere/`munmap` 64 vezes cada
   (TLB shootdown entre núcleos).

Na VM (`run.ps1` já usa `-smp 4`) a serial mostra `smpteste: OK`, e os autotestes de `teste`, `posixteste`,
`dispatchteste`, `sinaisteste` e do shell continuam passando com o desktop desenhando.

O paralelismo expôs uma corrida antiga: `SYS_WAIT` num filho que já tinha saído mas cuja thread ainda não
fora recolhida não marcava o zumbi como informado, e um `waitpid(-1)` depois o devolvia de novo.

## Próximos passos

- Travas por subsistema no lugar da trava grande: VFS, Mach IPC, pipes/PTY, rede (já tem `g_net_lock`),
  páginas físicas. A ordem natural é tirar primeiro as syscalls que só leem (tempo, getpid, futex sem espera).
- Tirar a UI e os drivers do BSP, ou pelo menos soltar a trava durante a composição (que já é paralela).
- Rodar threads do kernel nos APs.
- IPI de "acordar" quando uma thread fica pronta (hoje o AP ocioso a vê no próximo tique de 1 ms).
