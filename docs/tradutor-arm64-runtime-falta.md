# Tradutor arm64, frente RUNTIME: o que ainda falta (8 nov. 2026)

Visão geral, arquitetura e estado das frentes: [tradutor-arm64.md](tradutor-arm64.md). Este arquivo lista só as
pendências da frente RUNTIME (`userland/a64rt`, partes arm64 de `kernel/compat/macho.c`, harness `tools/machotest.c`).

## Já feito (resumo)
- Processo Mach-O arm64 de ponta a ponta no kernel e no machotest, com o interpretador e com o JIT.
- `userland/tests-a64rt` passa nos dois motores. No QEMU passou 21/21, antes de entrarem os testes de código que se
  reescreve (SMC); no machotest passa também o de SMC.
- Proteção de página real para a rede de SMC do JIT (`MACH_VM_JIT_WPROTECT`), testada no machotest.
- Binários arm64 reais rodam: `disclaimer` e `claude-ssh-askpass` do Claude.app, e o Codex CLI. O Codex CLI vai até o
  mesmo ponto do x86 no harness.

## Falta

### 1. Aceitação nos apps da meta (tabela app × ponto alcançado ainda não está no doc)
Executor do harness: `build/a64rt-test/apps.sh`. Raízes em `build/a64rt-test/roots/<app>-<arch>`, logs em
`build/a64rt-test/logs`. Os apps arm64 baixados estão em `build/apps/x-arm64/`.

| App | machotest x86_64 | machotest arm64 | VM x86_64 (referência) | VM arm64 |
|---|---|---|---|---|
| Claude | FATAL `mach_port_construct` (harness) | o mesmo | tela "Sign In" | **pendente**: 1ª rodada sem janela em 15 min e caiu (ver item 2) |
| Cursor | o mesmo FATAL | o mesmo | tela de login | pendente |
| Slack | o mesmo FATAL | o mesmo | tela de boas-vindas/login | pendente |
| Grok Bot | o mesmo FATAL | o mesmo | tela de login | pendente |
| Codex desktop | o mesmo FATAL | o mesmo | tela de login | pendente |
| VS Code | o mesmo FATAL | **não carrega**: os symlinks do framework viraram arquivos de texto na extração (item 3) | usável | pendente |
| WhatsApp | roda até o watchdog (100 s) | parava em `_bsearch_b` (a LIBSYS já acrescentou) | só no harness | pendente |
| Codex CLI | `failed to create UnixStream` (harness) | o mesmo | até "Not logged in" | pendente |

- Rodar de novo o WhatsApp arm64 no machotest com o `_bsearch_b` novo e repassar à LIBSYS o próximo símbolo que faltar.
- Na VM, quando for a vez da RUNTIME (a VM está com a LIBSYS; uma por vez, o host tem 7,8 GB): um app arm64 por vez com
  `tools\stage-electron.ps1 -Fatia arm64` sobre `build-a64e`. Para o WhatsApp, imagem com `.\build.ps1 -ComSwift`.
  Para o Codex desktop, o stage do Chrome (`stage-chromeapp`).
- Escrever a tabela final (machotest / VM, arm64 vs x86_64) em `docs/tradutor-arm64.md`.

### 2. Claude arm64 caiu na VM depois de ~15 min
- Falta no `a64_jit_recover` de uma thread nova: o estado por thread do JIT estava numa página que sumiu.
- Hipótese principal: buracos deixados pelo `munmap` das sobras da pilha do host (`alloc_hstack`).
- Mudança feita, ainda não confirmada na VM: o runtime passou a usar `SYS_MMAP` sob demanda, sem `munmap` das sobras.
- Se cair de novo: rodar com `/Sistema/Biblioteca/rastrear-munmap` para ver quem desmapeou a página.
- Mesmo antes de cair, o app não abriu janela em 15 min. Medir onde gasta o tempo:
  - estatísticas do JIT na saída (`/Sistema/Biblioteca/a64/depurar` já está na imagem `build-a64e`);
  - volume de SIGBUS da rede de SMC; o JIT já passou a marcar páginas voláteis depois de 2 invalidações.

### 3. VS Code arm64: extração
O `Expand-Archive` gravou os symlinks do `.framework` como arquivos de texto. Extrair de novo de um jeito que resolva
os links: copiar o alvo por cima de cada arquivo-link. A primeira tentativa (varredura com `find`) foi interrompida.

### 4. fbstat do Electron na VM
Mandar à frente JIT (`local_2e0c6d48-0a9b-4c21-ade6-75fa7a3596bb`) a lista de instruções que caem no interpretador,
colhida no Electron arm64 rodando de verdade na VM. As listas do machotest já estão na pasta de notas, em
`jit-fbstat-<app>.txt`.

### 5. Testes que faltam confirmar no QEMU
- O teste `smc` (código reescrito com e sem `sys_icache_invalidate`) no kernel com `MACH_VM_JIT_WPROTECT`.
- O fork arm64 com `a64_jit_fork_child`: falta um teste com fork de verdade no `tests-a64rt` (hoje só há
  `posix_spawn`).
- O `read()` que escreve sobre código traduzido (invalidação com `a64_jit_has_code`).

### 6. Limites conhecidos (documentar ou resolver)
- Escritas do kernel em memória do usuário (por exemplo `read()` direto num buffer de código) não passam pela PTE.
  Cobertas só pelo `a64_invalidate` depois de `read`/`pread`; outras syscalls que escrevem em memória do guest não
  invalidam.
- Não há commpage arm64: a região baixa não é memória do usuário no mOS.
- Hoje o build usa `tools\build-a64rt.ps1 -Parcial`, que troca por stub os símbolos do motor que as outras frentes
  ainda não escreveram. Tirar o `-Parcial` do `build.ps1` quando o motor estiver completo.
- O harness (machotest) não emula portas Mach, `socketpair` nem `futex`. No Electron ele sempre para no mesmo ponto
  nas duas arquiteturas; a comparação real é na VM.

### 7. Fechamento
- Atualizar a linha RUNTIME do Estado em `docs/tradutor-arm64.md` com o resultado dos apps.
- Commit com pathspec, só dos arquivos da frente e só se o usuário pedir.
