# Executar .metallib (AIR) no Metal por software do mOS

Muitos apps do macOS trazem só `default.metallib` (AIR — bitcode LLVM), sem o fonte `.metal`. O Metal por software do
mOS (`docs/metal.md`) só compilava shaders do fonte, então esses apps caíam no erro "AIR não suportado". Este é o
caminho para **ler e executar o `.metallib`**.

## Etapas

1. **Contêiner MTLB** (`userland/libsystem/mtlair.c`): cabeçalho, lista de funções (nome, estágio, deslocamentos) e o
   blob de bitcode (um módulo LLVM por biblioteca, com o wrapper `0x0B17C0DE`). **Pronto.**
2. **Módulo de bitcode LLVM** (`mtlair_bc.inc` + `mtlair_mod.inc`): o bitstream (abreviações, BLOCKINFO, VBR, char6,
   blob), a tabela de tipos, as funções (nomes via STRTAB), as constantes, os metadados (strings em lote, nós, nós
   nomeados `air.vertex`/`air.fragment`/`air.kernel`) e os corpos de função decodificados em SSA. **Pronto (estrutura).**
3. **Lowering** (`mtlair_lower.inc` + `mtlair_emit.inc`): os 17 opcodes do `FUNCTION_BLOCK` e os intrínsecos
   `air.*` / `llvm.*` conhecidos viram nós do interpretador (`mtlint.h`), que `mtlexec.c` executa. Uma função que
   não baixa é omitida da biblioteca. Se nenhuma baixa, `newLibraryWithData:` devolve erro. `metal.c` chama
   `mtlair_lower` no contêiner MTLB.

## Cabeçalho MTLB (little endian)

| Offset | Campo |
|---|---|
| 0x00 | `"MTLB"` |
| 0x04 | versão (u16 maior, u16 menor) |
| 0x10 | tamanho do arquivo (u64) |
| 0x18 | offset da lista de funções (u64) |
| 0x28 | offset dos metadados públicos (u64) — é o fim real da lista de funções |
| 0x48 | offset do bitcode (u64) |
| 0x50 | tamanho do bitcode (u64) |

Lista de funções: `u32 nFuncs`, e por função um `u32 tamanho` (que **inclui o próprio campo**) seguido das tags de 4
letras com `u16` de tamanho: `NAME`, `TYPE` (u8: 0 vertex, 1 fragment, 2 kernel), `HASH`, `OFFT` (3×u64), `VERS`,
`MDSZ`, terminadas por `ENDT`. O campo de tamanho em 0x20 **não** delimita a lista; o fim é a seção seguinte.

O blob de bitcode começa com o wrapper `0x0B17C0DE` (magic, versão, offset, tamanho, cputype — 20 bytes) e depois a
magia `BC C0 DE`; o bitstream vem a seguir, com largura de abreviação 2 no topo.

## Teste no host (sem VM)

`tools/airdump.c` lê um `.metallib` e imprime o que o parser extrai — funções, estágios, tipos, metadados e
instruções por corpo. Valida contra arquivos reais.

```bash
zig cc -DMTLAIR_HOST -O2 tools/airdump.c -o build/airdump.exe
build/airdump.exe caminho/para/shader.metallib
```

Validado nos **14 `.metallib` reais do WhatsApp** (`build/apps/x/whatsapp/.../*.metallib`): todos lêem sem erro —
contêiner, módulo, tabela de tipos, metadados `air.*` e todos os corpos de função com nome
(`fragmentShader`, `vertexShader`, `statisticsKernel`, `inverseTonemapKernel`, `lanczosHorizontal`,
`compositeBrightnessPass1`, `unsharpMask5`...), de 2 a 220 instruções cada.

## Conjunto de instruções a cobrir (medido)

`airdump -h` sobre os 14 shaders do WhatsApp dá o conjunto exato de opcodes do `FUNCTION_BLOCK` que o lowering precisa
traduzir (17 distintos; as `CALL` são os intrínsecos `air.*`/`metal::`):

| Opcode | Qtd | Opcode | Qtd |
|---|---|---|---|
| BINOP | 404 | LOAD | 54 |
| CALL | 172 | SHUFFLEVEC | 54 |
| EXTRACTELT | 113 | CMP2 | 49 |
| INSERTELT | 90 | CAST | 35 |
| GEP | 61 | EXTRACTVAL | 34 |
| BR | 55 | STORE | 33 |
| PHI | 21 | VSELECT | 13 |
| RET | 19 | INSERTVAL | 9 |
| | | ALLOCA | 2 |

Ou seja, o lowering é finito e bem definido: a tabela de valores SSA (args, constantes, resultados), o mapeamento de
tipos LLVM para os de `mtlint.h`, esses 17 opcodes (com controle de fluxo: `BR`/`PHI`) e a lista de intrínsecos `air.*`
que as `CALL` usam (amostragem de textura, conversões, acesso a buffer).

## O que o host executou

`tools/airlower_test.c` compila C com `clang -emit-llvm`, baixa o bitcode e compara a saída com o mesmo cálculo
compilado do fonte pelo `msl_compile`. Não é teste de VM e não usa os `.metallib` do WhatsApp (eles não estão
neste checkout). O que passou, com 0 falhas:

- kernel escalar com desvio, `llvm.fmuladd` e comparação sem sinal (`tid` 0 → 7, `tid` 2 → `out[2] = 7`, `tid` 1 → 4 e 8);
- laço com PHI em `-O1` (n = 4 soma 10, n = 0 soma 0);
- dois ponteiros sem bind explícito (caem em `[[buffer]]` pela ordem);
- função irmã em `double` omitida, a função em float permanece (saída 5);
- `shufflevector` + `extractelement` + `insertelement` → `[4, 4, 2, 1]`;
- `extractvalue` / `insertvalue` de `{float, i32}` → 6.5;
- `icmp` vetorial com sinal + `select` → `[10, 2, 30, 4]`.

## Limites atuais

- Função que não baixa é omitida, não vira sucesso falso. `double`, `icmp` vetorial sem sinal, atômicos, `fcmp`
  não ordenado e pack/unpack falham a função. `dfdx`/`dfdy`/`fwidth` emitem o nó que o interpretador já avalia como 0.
  `air.convert` é um cast, sem saturação.
- Sem metadado `air.kernel` nem bind explícito, `thread_position_in_grid` fica 0. Textura e sampler só entram se o
  metadado disser. O teste de host passa os binds na chamada; o caminho de metadado está no emissor e não foi o
  caso diferencial.
- Não há execução conferida de um `.metallib` de app (WhatsApp, Chromium). O parser desses arquivos, quando existem
  no ambiente de build, continua sendo o do `airdump`.
- Um módulo por biblioteca (o layout normal do `.metallib`).
- Mach-O x86_64 continua sendo a arquitetura; o AIR em si é independente de arquitetura.
