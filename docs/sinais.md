# Sinais POSIX no mOS

Tratadores de sinal no espaço do usuário, com o comportamento do macOS (xnu `bsd/kern/kern_sig.c` como
referência de semântica; nada copiado). Código: bloco "sinais" de `kernel/proc/process.c`, ABI em
`kernel/include/syscall.h` (`SYS_SIG` = 58, `MOS_SIGOP_*`, `mos_siginfo_t`/`mos_ucontext_t`/`mos_mcontext_t`),
libSystem em `userland/libsystem/libc.c` e `sigjmp.S`. Teste: `userland/tests-signals/main.c` (`sinaisteste`,
chamado pelo `teste` nativo no `build.ps1 -Tests`).

## Modelo

- **Ações por processo**: tratador, `sa_mask`, `sa_flags` (valores do macOS: `SA_ONSTACK`, `SA_RESTART`,
  `SA_RESETHAND`, `SA_NODEFER`, `SA_SIGINFO`) e o trampolim do libSystem. `fork` copia; `execve` volta os
  tratadores ao padrão (o "ignorar" fica) e desliga a sigaltstack.
- **Máscara e pendentes por thread** (`thread_t.sig_mask`, `sig_pending`): a thread nova herda a máscara de
  quem a criou; `sigprocmask` = `pthread_sigmask`. `kill()` deixa o sinal pendente no **processo**
  (`process_t.sig_pending`) e ele vai para a primeira thread que não o bloqueie; faltas e `pthread_kill`/`raise`
  ficam na **thread**. KILL e STOP não se bloqueiam.
- **Numeração**: o kernel numera como o Linux nos pontos em que o macOS difere (USR1 10, USR2 12, CHLD 17,
  STOP 20, BUS 30, SYS 31); o libSystem traduz número e máscara (`cx_sig_to_mos`, `cx_sigmask_to_mos`), e o
  `WTERMSIG` do `waitpid` volta no número do macOS.

## Entrega

Sempre na volta ao ring 3: fim de syscall (`proc_syscall` embrulha o corpo e confere o frame que vai voltar,
mesmo o de outra thread depois de uma troca), fim de interrupção (`sched_irq_exit`) e fim de falta
(`proc_fault`). `proc_signal_check` pega o menor sinal não bloqueado (os da thread primeiro), descarta os
ignorados, encerra o processo na ação padrão e, havendo tratador, monta na pilha do usuário (128 bytes abaixo do
`rsp`, fora da red zone, ou no topo da sigaltstack com `SA_ONSTACK`) um quadro com:

| campo | layout |
|---|---|
| `siginfo_t` | `__siginfo` do Darwin, 104 bytes (`si_signo`, `si_code`, `si_pid`, `si_uid` 501, `si_status`, `si_addr`) |
| `ucontext_t` | `__darwin_ucontext`, 56 bytes (`uc_onstack`, `uc_sigmask` = máscara a restaurar, `uc_stack`, `uc_mcontext`) |
| `mcontext_t` | `__darwin_mcontext64`, 712 bytes: `__es` (trapno, err, faultvaddr), `__ss` (registradores), `__fs` (imagem FXSAVE) |

e desvia para `cx_sigtramp(handler, estilo, sinal, siginfo*, ucontext*)`, como o `_sigtramp` do macOS. O
trampolim traduz número e máscara para os do macOS, guarda o `errno`, chama `handler(sinal, siginfo,
ucontext)` e volta por `MOS_SIGOP_RETURN`, que restaura registradores, FPU/SSE e máscara a partir do mcontext:
um tratador que muda `uc_mcontext->__ss.__rip` retoma noutro ponto (o `sinaisteste` pula a instrução que
falhou assim). Durante o tratador ficam bloqueados `sa_mask` e o próprio sinal (salvo `SA_NODEFER`).
Programas antigos de `userland/mos.h` (`SYS_SIGACTION` com restorer) recebem o mesmo quadro em chamada direta
`handler(sinal, siginfo*, ucontext*)` e voltam por `SYS_SIGRETURN`.

`setjmp`/`sigsetjmp(env, 1)` salvam a máscara e `longjmp`/`siglongjmp` a restauram (`_setjmp`/`_longjmp` não),
como no macOS: sair de um tratador por `siglongjmp` (o ^C do readline) não deixa o sinal bloqueado.

## Esperas, EINTR e SA_RESTART

Um sinal para uma thread parada numa espera **interrompível** (`SYS_SLEEP`, pipe, stdin da sessão,
pseudoterminal, `wait`/`waitpid`, futex, `sigsuspend`, `sigwait`) a acorda com `-EINTR`, desfazendo a
referência que o pipe/terminal guardava dela. Na entrega:

- com `SA_RESTART` e syscall refazível (`read`, `write`, `wait`, `waitpid`, `waitany`, futex), o `rip` volta 2
  bytes (para o `int 0x80`) e o `rax` volta a ser o número: depois do tratador a syscall roda de novo;
- sem `SA_RESTART`, ou em `sleep`/`poll`/`sigsuspend`, a chamada devolve `EINTR`;
- se nenhum tratador chegou a rodar nessa thread (o sinal foi para outra, ou virou "ignorar"), a espera é
  refeita sem `EINTR` à toa.

`signal()` instala com `SA_RESTART` (semântica BSD; `siginterrupt(s, 1)` desliga). Esperas Mach (`mach_msg`) e
o `pthread_join` não são interrompidos: o sinal fica pendente para a próxima thread que voltar ao ring 3.

## Faltas

`#PF` em página ausente vira `SIGSEGV`/`SEGV_MAPERR`; em página presente da região de usuário (proteção) vira
`SIGBUS` como no macOS; fora da região de usuário é sempre `SIGSEGV`. `#GP` vira `SIGSEGV`, exceto `int3`
(que no ring 3 chega como `#GP` com erro `0x1A`): vira `SIGTRAP` com o `rip` depois da instrução. `#DE` e
`#MF`/`#XM` viram `SIGFPE`, `#UD` `SIGILL`, `#AC` `SIGBUS`. Bloqueado, ignorado ou sem tratador, o processo
morre com o diagnóstico de sempre na serial; pilha estourada na entrega também mata (`SIGSEGV`).

## Outras chamadas

- `sigsuspend` (`MOS_SIGOP_SUSPEND`): troca a máscara e espera um tratador rodar; o `uc_sigmask` do quadro é a
  máscara antiga, então a volta do tratador a restaura. `pause()` = `sigsuspend` com a máscara atual.
- `sigwait` (`MOS_SIGOP_WAIT`, com prazo opcional): pega um pendente do conjunto ou espera; um sinal do conjunto
  enviado depois vai direto para quem espera, mesmo bloqueado.
- `sigaltstack` (`MOS_SIGOP_ALTSTACK`): por thread, mínimo de 32 KiB (`MINSIGSTKSZ`), `EPERM` se está nela.
- `pthread_kill`/`raise` (`MOS_SIGOP_TKILL`); `kill`/`killpg` de grupo vão pelo `SYS_TTY` (`pty.c`).
- **SIGCHLD**: todo filho que termina manda SIGCHLD ao pai com `CLD_EXITED`/`CLD_KILLED`, `si_pid` e
  `si_status`; com a ação padrão (ignorar) ele some sem acordar ninguém, salvo um `sigwait` por ele.
- `alarm`/`setitimer(ITIMER_REAL)` continuam numa thread auxiliar do libSystem, que bloqueia todos os sinais
  para o SIGALRM cair numa thread do programa.

## Falta

Parar/continuar processos (STOP/TSTP/CONT são ignorados), fila de sinais de tempo real (`sigqueue`),
temporizadores no kernel (`ITIMER_VIRTUAL`/`PROF`) e `SA_NOCLDWAIT`/`SIG_IGN` no SIGCHLD colhendo filhos sozinho.
