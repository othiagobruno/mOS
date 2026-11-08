# Tradutor arm64: o que ainda falta

Levantamento de 8 nov. 2026, depois que as sessões das frentes foram interrompidas. A arquitetura, o
critério de "100% completo" e o placar completo estão em [tradutor-arm64.md](tradutor-arm64.md).
**Nada do tradutor foi commitado ainda.**

## Resumo

O núcleo está completo e verificado. O interpretador cobre todas as instruções A64 dos recursos
anunciados e bate com o QEMU em todos os grupos. O JIT bate com o interpretador. A suíte em C dá o mesmo
resultado que o x86_64. O que falta está todo na ponta de processo e de apps (itens 1 a 4), mais
desempenho e limpeza.

Placar atual (`tools/a64placar.ps1`, semente 20261115, com JIT):

| Frente | Casos que executam | Batem com o QEMU | Divergências | JIT ≠ interp |
|---|---:|---:|---:|---:|
| INT | 65 764 | 100% | 0 | 0 |
| FP | 15 545 | 100% | 0 | 0 |
| SIMD | 72 359 | 100% (2 casos do bug de FMA do QEMU) | 0 | 0 |

Testes ELF: 39 de 39. Suíte C x86_64 × arm64: 21 de 21.

## 1. Aceitação final: apps da meta em arm64 (critério 5 do doc)

Este é o maior item que falta. Rodada no `machotest`, com a mesma raiz nas duas arquiteturas:

| App | x86_64 | arm64 | O que falta |
|---|---|---|---|
| Claude, Cursor, Slack, Grok Bot, Codex desktop | FATAL `mach_port_construct` (o harness não emula portas Mach) | o mesmo FATAL | o machotest não vai além disso; rodar na **VM do mOS** e comparar |
| WhatsApp | cortado pelo watchdog de 100 s | ~42 mil blocos traduzidos, parava em `_bsearch_b` | o `_bsearch_b` já foi feito (`userland/libsystem/sortb.c`); rodar de novo e seguir até o próximo símbolo ou bug |
| VS Code | FATAL `mach_port_construct` | não carregou: os symlinks do framework viraram arquivos de texto na extração | terminar a troca dos "symlinks em texto" (estava rodando quando a sessão parou) e rodar de novo |

Ainda falta:
- **Coluna da VM:** nenhum app arm64 foi rodado no QEMU do mOS. A VM estava ocupada com a validação dos
  apps x86_64 (item 4). Por falta de memória no host (7,8 GB), roda uma VM por vez.
- **Tabela app × ponto alcançado** (arm64 × x86_64, machotest e VM) no `tradutor-arm64.md`.
- **App SwiftUI/Combine/Concurrency arm64 de ponta a ponta.** A cadeia Swift universal está pronta e os
  testes de stdlib/runtime/Foundation passam pelo tradutor, mas nenhum app de verdade rodou.

Responsáveis: RUNTIME, com a LIBSYS para símbolos e bibliotecas que faltarem.

## 2. Proteção de página no kernel (rede de segurança contra código que se reescreve)

O kernel do mOS não aplica proteção por página. Por isso a rede de segurança do JIT para código
auto-modificável (`a64_host_jit_wprotect`) fica desligada. Hoje só vale a invalidação explícita
(IC IVAU / `sys_icache_invalidate`), que os JITs corretos (V8, JSC, wasm) fazem. Um programa que reescreve
código sem invalidar pode executar a tradução antiga. Responsáveis: RUNTIME + JIT.

## 3. Desempenho do JIT nos apps reais

A correção está completa: toda instrução executa, nativa ou pelo interpretador. Ainda passam pela
"chamada leve" ao interpretador:
- SHA1/SHA256/SHA512 (o SHA-NI do x86 divide a rodada de outro jeito; só pode virar nativo com prova de
  equivalência);
- estimativas FP (FRECPE/FRSQRTE: tabelas do ARM que o `rcpps` não reproduz);
- SQSHL/SQRSHL/UQSHL e afins;
- FCVT D→H;
- o resto do SIMD/FP sem forma nativa.

Falta:
- priorizar pelo que os apps realmente usam. As listas `jit-fbstat-<app>.txt` foram geradas pela RUNTIME
  e enviadas ao JIT, mas o JIT ainda não trabalhou nelas;
- medir de novo com a máquina livre. Os números atuais (inteiros 1595 MIPS, memória 670, chamadas 432)
  foram medidos com o host carregado.

## 4. Validação dos apps x86_64 depois do desenrolador novo (LIBSYS)

O libunwind do LLVM agora também está no libSystem **x86_64**. Antes, qualquer exceção terminava o
programa; agora ela desenrola como no macOS. Isso muda o comportamento de todos os apps x86_64.
- A LIBSYS estava validando na VM, um app por vez, e estava no Cursor quando foi interrompida. Ela relatou
  "não regrediu nada" nos que já tinha testado, mas o resultado não foi registrado no doc.
- Falta terminar (VS Code, Claude, Slack, Cursor, WhatsApp, Codex) e registrar o resultado.
- Se algum app regredir, use a chave de volta: `MOS_DESENROLAR=0` no ambiente ou o arquivo
  `/Sistema/Biblioteca/sem-desenrolar` na imagem. Também dá para deixá-la ligada por padrão no `build.ps1`.

## 5. Sobras menores do placar

- **br.reg:** 1 caso "deveria ser UDF" (palavra de recurso não anunciado). A TESTES mudou o fuzzer para
  classificar esses casos. Só falta confirmar no próximo placar que o caso sumiu.
- **Sequências (K=8):** 2 casos divergentes de 27 900, com 0 de JIT ≠ interp. Falta analisar os casos
  minimizados em `tools/a64tests/regress_*` e dizer se é bug nosso ou do QEMU.
- **SIMD:** 2 casos na coluna "FMA?" (`simdv.indexed`), atribuídos ao bug de FMA do QEMU para Windows e
  que o fuzzer não conseguiu isolar. Falta confirmar.

## 6. Achados fora do tradutor (no libSystem x86_64)

Encontrados ao comparar com a referência x86_64. Não são do tradutor, mas afetam os apps x86_64:
- o `printf` lê `long double` (80 bits) como 8 bytes e desalinha os argumentos seguintes;
- o `NSInvocation` não passa argumentos na pilha (mais de 6 inteiros ou 8 FP).

## 7. Limites conhecidos (documentados, não bloqueiam)

- Não há commpage arm64: a região baixa não é do usuário no mOS. Até agora nada precisou dela.
- `hw.pagesize` = 16 KiB é o que o libSystem arm64 informa, mas as páginas reais do mOS são de 4 KiB.
- O `svc #0x80` "cru" do Darwin só trata exit/read/write/getpid/kill/thread_selfid/TSD. O resto vira
  SIGSYS. Os apps usam o libSystem, então isso só aparece em binários que fazem syscall direto.

## 8. Fechamento

- Commitar o trabalho (com pathspec, `git commit -F msg -- caminhos`, porque o checkout é compartilhado) quando você pedir.
  O tradutor está em:
  - `userland/a64/`, `userland/a64rt/`;
  - `tools/a64*`, `tools/build-a64*`, `tools/build-*-arm64.ps1`;
  - partes arm64 de `kernel/compat/macho.c` e do `userland/libsystem`;
  - os docs;
  - o filtro de `zig.exe` em ~46 scripts.
- Atualizar a seção "Estado" do `tradutor-arm64.md`. A tabela aparece duas vezes no arquivo e precisa
  ficar só uma.
