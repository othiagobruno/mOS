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
3. **Lowering** (`mtlair_lower.inc`, em andamento): traduzir as instruções SSA e os intrínsecos do AIR (`air.sample`,
   `air.convert`, acesso a `[[buffer]]`/`[[texture]]`...) para a árvore de nós do interpretador (`mtlint.h`), que
   `mtlexec.c` já executa. O que o lowering não cobrir faz a função devolver NULL com a razão, e o Metal volta ao erro
   de "AIR não suportado" — mas só nesse caso.

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

## Limites atuais

- O lowering das instruções ainda não está ligado: o parser extrai a estrutura, mas `newLibraryWithData:` com AIR
  continua devolvendo o erro até o lowering cobrir os shaders.
- Um módulo por biblioteca (o layout normal do `.metallib`).
- Mach-O x86_64 continua sendo a arquitetura; o AIR em si é independente de arquitetura.
