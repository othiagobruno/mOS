# Frente LIBSYS do tradutor arm64: o que ainda falta (8 nov. 2026)

O que já está pronto está em [tradutor-arm64.md](tradutor-arm64.md), seção "libSystem e bibliotecas do sistema em arm64
(frente LIBSYS)". Resumo do pronto: `build/libSystem-arm64.mos` com a ABI da Apple (IR igual ao de `-target aarch64-macos`
em 101/101 arquivos, `tools/check-abi-arm64.ps1`), libunwind e exceções ObjC de verdade nas duas arquiteturas, libc++/
libc++abi, sqlite, ICU, zlib e WebKit universais (`build\universal`), a cadeia Swift universal (`build\universal-swift`,
só na imagem com `build.ps1 -ComSwift`), `tests-a64libsys` 37/37 no arm64 e 6/6 com as dylibs do disco. Nada commitado.

## 1. Validação dos apps x86_64 na VM com o desenrolador novo (interrompida)

O libSystem x86 passou a desenrolar exceções C++/ObjC (antes toda exceção virava terminate). Isso muda o comportamento
de todos os apps x86, então cada app da meta precisa rodar na VM e chegar ao mesmo ponto de `docs/compat-apps.md`.

| App | Resultado | Como foi rodado |
|---|---|---|
| VS Code | **sem regressão**: janela e conteúdo iguais com a chave ligada e desligada; o aviso "Request 2 failed" é a falha do módulo do Copilot que falta na imagem de teste, igual nos dois casos | `tools\vscode-teste.ps1 -App build-vscode\app-src -OutDir build-unw -Accel whpx -Port 5961` |
| Slack | **sem regressão**: chega ao "Sign In to Slack" desenhado, sem exceção não tratada | `vscode-teste.ps1 -Electron -App build\apps\x\slack-x64\Slack\Slack.app -Nome Slack -OutDir build-unw-e -Memory 2816` |
| Cursor | **falta**: a 1ª rodada usou a receita errada (sem `-MaxModuloMiB 128`, janela branca por módulo de 45 MiB faltando, nada do desenrolador); a com a receita certa foi interrompida | `... -Electron -App build\apps\x\cursor\Cursor.app -Nome Cursor -Memory 3400 -MaxModuloMiB 128 -SizeMiB 1536` (receita na memória "Cursor na VM") |
| Claude | falta | `-Electron -App build\apps\x\claude\Claude.app -Nome Claude` |
| Codex desktop | falta | `tools\stage-chromeapp.ps1 -App build\apps\x\codex\Codex.app` + `run.ps1` (layout do Chrome, não Electron) |
| WhatsApp | falta | harness `build\wa-run` (memória "WhatsApp no harness") ou VM com `build.ps1 -ComSwift` |

Rede de segurança pronta e testada: `MOS_DESENROLAR=0` no ambiente, ou o arquivo `/Sistema/Biblioteca/sem-desenrolar` na
imagem, volta ao comportamento antigo (`unwind_ls.c`, `_dyld_find_unwind_sections` em `dlfcn.c`). Se algum app regredir:
ligar a chave por padrão no `build.ps1` (criar o arquivo no esp) até corrigir, e registrar aqui e no tradutor-arm64.md.

Cuidados para rodar: um app por vez (o host tem 7,8 GB; combinar a vez da VM com a sessão RUNTIME, que testa os apps
arm64); o staging do Cursor deixa caminhos com mais de 260 caracteres que o `build.ps1` não apaga
(`cmd /c rd /s /q "\\?\<caminho>\esp"`); `-SemBuild` no `vscode-teste.ps1` pula também o staging do app.

## 2. Pedidos da sessão RUNTIME (apps arm64 pelo tradutor)

- WhatsApp arm64: parava em `_bsearch_b`, já feito (`sortb.c`, com heapsort/mergesort/psort e variantes `_b`). Falta
  rodar de novo e resolver o próximo símbolo que aparecer (raiz `build/a64rt-test/roots/whatsapp-arm64`, executor
  `build/a64rt-test/apps.sh`, `APPS=whatsapp ARCHS=arm64`).
- Os Electron arm64 (Claude, Cursor, Slack, Grok, Codex) param no FATAL do `mach_port_construct`, igual ao x86 no
  harness: não é do libSystem arm64, mas é preciso acompanhar o que vier depois dele.
- Nenhum app SwiftUI arm64 rodou de ponta a ponta ainda (os testes Swift arm64 passam: 13/13 e 23/23).

## 3. Achados no libSystem x86 (fora do tradutor, vistos pelo teste de referência)

- O printf lê `long double` (80 bits no x86_64) como 8 bytes e desalinha os argumentos seguintes (`%Lf`).
- O NSInvocation x86 não passa argumentos na pilha (mais de 6 inteiros ou 8 FP): o item "NSInvocation com 16
  argumentos" falha só no x86. A versão arm64 (`foundation_extra.c` + `ffi_arm64.S`) mostra o caminho.
- `qsort_r` é por inserção (O(n²)); o `qsort_b` de `vscode_libc.c` usa uma variável global com trava (uma exceção no
  comparador deixa a trava presa).

## 4. Regras para quem mexe no `userland/libsystem` (senão o build arm64 quebra)

- Compound literal dentro do `MSG(...)` vai entre parênteses: `MSG(id, v, "initWithFrame:", ((NSRect){0, 0, 10, 10}))`
  (comentário no `objc.h`). Hoje o `appkit_postico.c` (novo, de outra sessão) precisou dessa correção três vezes.
- asm x86 inline e builtins `__builtin_ia32_*` só com `#if defined(__x86_64__)` / ramo `__aarch64__`
  (`__builtin_ia32_pause` já tem equivalente no `sys.h`).
- `__attribute__((alias))` não existe no triplo do Darwin: usar uma função que chama a outra.
- Conferir com `tools\build-libsystem-arm64.ps1` e `tools\check-abi-arm64.ps1` depois de mudar o libSystem.

## 5. Outros

- `.tools\zig-macos-dl` (da sessão "MOS ambiente de desenvolvimento") casa com o filtro `zig-*` antes do zig de
  verdade; 43 scripts em `tools\*.ps1` ainda pegam a primeira pasta sem conferir o `zig.exe` (os da LIBSYS já conferem).
- Commit: nada foi commitado; fazer com pathspec (`git commit -F msg -- <arquivos>`) quando o usuário pedir.
