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

A rede existe: `uvm_jit_wprotect` / `PTE_JWP`, shootdown de TLB no `vm_flush`, e o runtime trata a falta
de escrita. Por cima disso, o JIT guest (V8) no processo traduzido usa `mmap(MAP_JIT)` e
`pthread_jit_write_protect_np` por thread: com a proteção ligada, escrever numa página MAP_JIT que já
foi executada é SIGBUS do guest. `mach_vm_remap` sem cópia compartilha o físico e as duas vistas caem
juntas na invalidação. Uma página MAP_JIT que ainda não foi executada continua gravável (a reserva do
V8 é grande demais para materializar). O fork de um apelido duplo pode separar as duas vistas, porque
cada PTE vira COW por conta própria. IC IVAU / `sys_icache_invalidate` continuam obrigatórios para quem
publica código.

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

## 5. Sobras menores do placar — fechadas em 2026-11-10

Reproduzidas no Linux com QEMU 11.1.0 `-cpu max` e LLVM 23.1.5, sementes do placar (20261115 e, nas
sequências, 20261116). Nenhuma é bug do tradutor. O JIT não foi reexecutado: o placar original tinha
JIT≠ 0 nesses itens, e o interpretador é quem classifica o UDF e o FAULT.

- **br.reg, "deveria ser UDF":** `tools/a64tests/regress_br_reg_d65f0160.S`. A palavra é `ret x11`, que o
  LLVM nomeia com os nossos recursos. O desvio cai num `stlxp` cujo endereço atravessa 16 bytes, com o
  monitor exclusivo vazio. Nós damos FAULT (o ARM manda conferir o alinhamento antes); o QEMU devolve
  status 1 sem acessar — IMPLEMENTATION DEFINED — e só então dá UDF numa palavra `<unknown>` até com
  `+all` (`0x76c123a6`). Não era recurso não anunciado que nós executamos. O fuzzer agora trata esse
  STXR* do rascunho como o caso já tolerado do STXR.
- **Sequências, 2 divergências de 27 900:** `regress_br_reg_503c5df0.S` e `regress_dp_csel_33e9fc55.S`.
  Os dois terminam em UDF nosso numa palavra que o LLVM com `A64_FEATURES` não conhece e com `+all`
  nomeia `cbge` / `cbeq` (FEAT_CMPBR, não anunciado). O `-cpu max` executa o CMPBR e dá UDF mais adiante
  (`sttp`, ou uma palavra `<unknown>` até com `+all`). O UDF nosso está certo. O fuzzer passa a aceitar
  esse UDF mesmo quando o QEMU também termina em UDF, desde que não seja na mesma palavra.
- **SIMD, 2 casos FMA? em `simdv.indexed`:** contra o QEMU 11.1 do Linux (o `fma()` da libm arredonda
  uma vez, como o FPMulAdd do ARM) o grupo fecha 1839/1839, o mesmo número de execuções do placar, com
  0 na coluna FMA? e 0 lanes corrigidas. A diferença só existe no QEMU para Windows, cujo `fma()` do
  mingw arredonda o produto em 80 bits e de novo no destino quando FPSR.IXC já está ligado, o modo é RN
  e as entradas são normais (ver a tabela de limitações em `tradutor-arm64.md`). Um modelo desse
  arredondamento, em cima das FMLA/FMLS/FCMLA/FMLAL/BFMLAL indexadas desta semente, não achou lane em
  que a regra do hardfloat do QEMU ficaria com o resultado errado (a única diferença numérica era um
  NaN, e nesse caso o QEMU usa o softfloat). As duas palavras do placar não foram recuperadas: o fuzzer
  não as isolou e, no Linux, elas batem. Não há correção em `jit_tr.c`, `jit_simd.h` nem no SIMD/FP do
  interpretador. A frente T1 já está na main (PR #18, JIT nativo de SSHR/USHL, CNT, REV, FCVT, FRINT e
  FMAX) e não mexe nesses casos.

Com o fuzzer ajustado, a mesma semente dá 0 divergências, 0 "deveria ser UDF" e 0 FMA? nos 186 000
casos de uma instrução (o `br.reg` passa a 1614 execuções: o `ret x11` entra na conta e é tolerado;
avisos de STXR sobem de 182 para 183) e 0 divergências nas 27 900 sequências. O JIT não entrou nessas
rodadas.

## 6. Achados fora do tradutor (no libSystem x86_64)

Encontrados ao comparar com a referência x86_64. Não são do tradutor, mas afetam os apps x86_64:
- o `printf` lê `long double` (80 bits) como 8 bytes e desalinha os argumentos seguintes;
- o `NSInvocation` não passa argumentos na pilha (mais de 6 inteiros ou 8 FP).

## 7. Limites conhecidos (documentados, não bloqueiam)

- Não há commpage arm64: a região baixa não é do usuário no mOS. Até agora nada precisou dela.
- A página de 16 KiB do processo arm64 (anúncio e operações) está em `docs/pagesize-arm64.md`. O kernel segue em 4 KiB. O que ainda não é página guest: a pilha principal não desmapeia, `MAP_FIXED` de arquivo não honra o endereço, e não há commpage.
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
