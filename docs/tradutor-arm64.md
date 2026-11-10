# Tradutor arm64 → x86_64 do mOS

Objetivo: rodar no mOS (x86_64) apps do macOS que só existem para Apple Silicon (ex.: o `Codex.dmg`
principal, bottles do Homebrew sem versão Intel, e o dia em que os apps da meta pararem de sair para
Intel). É o papel inverso do Rosetta 2: o processo inteiro roda código arm64 traduzido.

## O que "100% completo" quer dizer aqui

Sem meio-termo, e com critério verificável:

1. **ISA.** Toda instrução A64 executável em EL0 dos recursos anunciados (lista `A64_FEATURES` em
   `userland/a64/a64.h`: o conjunto do Apple M1 mais BF16/I8MM do M2) está implementada com a semântica
   do ARM ARM, inclusive flags, saturação (`QC`), FPCR (RMode/FZ/FZ16/DN/AHP), FPSR, NaN do ARM,
   divisão por zero = 0, conversões saturantes, monitor exclusivo, LSE/LSE2, RCpc, PAC (ver abaixo).
   Toda codificação fora deles é `A64_EXIT_UDF` (SIGILL), nunca ignorada.
2. **Verificação.** Cada classe de instrução tem teste contra o oráculo (QEMU `qemu-system-aarch64`,
   `-cpu max`, rodando o mesmo programa bare-metal) e fuzzing diferencial interpretador × JIT × oráculo.
   O placar fica na seção [Estado](#estado).
3. **Processo.** Um Mach-O arm64 (ou o slice arm64 de um universal sem slice x86_64) roda no mOS como
   processo normal: threads, sinais com `ucontext` arm64, TLS, `dlopen`, exceções C++/ObjC/Swift,
   código gerado em tempo de execução (V8/JSC/wasm com `MAP_JIT` + `sys_icache_invalidate`), `fork`/
   `exec`, `sysctl hw.optional.*` coerente com o que é implementado.
4. **Bibliotecas do sistema.** Tudo o que o mOS fornece aos apps (libSystem.mos e os dylibs/frameworks
   que o mOS instala: Swift, libc++, ICU, sqlite...) tem build arm64, porque o processo é todo arm64
   (não existe mistura arm64↔x86_64 dentro de um processo — ver "Por que o processo inteiro").
5. **Aceitação final.** Os slices arm64 dos apps da meta (`docs/compat-apps.md`) chegam ao mesmo ponto
   que os slices x86_64 chegam hoje.

## Por que o processo inteiro é arm64

Chamar o libSystem x86_64 direto do código arm64 exigiria converter a convenção de chamada sem saber a
assinatura: AAPCS64 da Apple × System V divergem em variádicos (a Apple põe todos na pilha), structs
(HFA em até 4 registradores FP × classificação por eightbyte), `long double`, retorno indireto (x8 ×
rdi oculto), callbacks/IMPs/blocks vindos do lado nativo. Não há como fazer isso correto em geral sem
recompilar (é por isso que o ARM64EC do Windows exige thunks gerados pelo compilador). Então, como no
Rosetta, **todo** o código do processo é arm64 e a única fronteira é a das syscalls.

## Arquitetura

```
 processo arm64 no mOS
 ┌──────────────────────────────────────────────────────────────────────┐
 │ app arm64 + dylibs arm64 do app + libSystem-arm64.mos (guest)        │  ← só dados para o x86
 ├──────────────────────────────────────────────────────────────────────┤
 │ a64rt.mos (x86_64): carregador → motor (interp/JIT) → ponte de        │
 │ syscalls (svc → int 0x80), sinais, threads, TLS, invalidação de cache │
 └──────────────────────────────────────────────────────────────────────┘
                 kernel do mOS (macho.c reconhece arm64 e monta o processo)
```

- **Mapeamento de memória 1:1.** Endereço do guest = endereço do host. Loads/stores traduzidos acessam
  o ponteiro direto. Falhas viram exceção do host; o motor reconstrói o estado do guest e o runtime
  entrega SIGSEGV/SIGBUS ao handler arm64 com o `pc` exato (V8/wasm dependem disso).
- **Modelo de memória.** O x86 (TSO) é mais forte que o ARM, então loads/stores comuns traduzem direto.
  `LDAR/LDAPR` = load comum; `STLR` = store + `mfence` quando necessário para RCsc; `DMB/DSB` →
  `mfence` (ou nada, quando o tipo só ordena o que o TSO já ordena). Exclusivos: `LDXR` guarda
  (endereço, tamanho, valor); `STXR` faz `lock cmpxchg` (ou `cmpxchg16b` para 128 bits) contra o valor
  guardado; falha devolve 1 (o ARM permite falhas espúrias; ABA é aceito, como no QEMU). LSE → `lock
  xadd/cmpxchg` em laço.
- **PAC.** Anunciamos FEAT_PAuth com algoritmo "identidade": PAC* não alteram o ponteiro, AUT* sempre
  autenticam, XPAC* limpam nada. É uma implementação válida (o algoritmo é IMPLEMENTATION DEFINED) e
  roda arm64 e arm64e. `BLRAA/RETAA/LDRAA...` são implementados com essa semântica.
- **BTI** é aceito e não imposto (válido enquanto as páginas não são "guarded"; o mOS não marca).
- **Contadores/sistema.** `CNTVCT_EL0`/`CNTVCTSS_EL0` a 24 MHz, `CNTFRQ_EL0` = 24 MHz, `DCZID_EL0` =
  bloco de 64 bytes, `CTR_EL0` = `0x84448004` (valor lido em M1 Icestorm e Firestorm: IminLine/DminLine = 64 B, CWG/ERG = 64 B, L1Ip VIPT, DIC = IDC = 0), `MIDR_EL1`
  via `mrs` em EL0 é UNDEFINED (como no macOS).

### Contrato entre as partes

Está em `userland/a64/a64.h` (estado da CPU, códigos de saída, regra de reinício após falha, API das
primitivas de FP, serviços do hospedeiro, recuperação de estado do JIT). Ler antes de qualquer coisa.
Regras de mudança: só acrescentar; mudar semântica existente → avisar a sessão coordenadora.

### Convenção de syscall do libSystem arm64

O libSystem x86_64 chama o kernel com `int 0x80`, número em `rax`, argumentos `rdi rsi rdx r10 r8`.
O libSystem arm64 usa **`svc #0x4d4f`** com número em `x8`, argumentos `x0..x4` (até `x5` se surgir um
sexto) e resultado em `x0`; o runtime traduz 1:1 para `int 0x80`. `svc #0x80` (Darwin cru, número em
`x16`, erro no bit C) é tratado pelo runtime da mesma forma que o kernel trata syscalls Darwin cruas
de apps x86_64.

## Frentes de trabalho (sessões em paralelo)

Cada frente é dona dos seus arquivos; ninguém edita arquivo de outra frente sem combinar. Todas usam o
mesmo checkout (`use_worktree: false`, commit com pathspec — ver memória "Commit com pathspec").

| Frente | Dona de | Entrega |
|---|---|---|
| INT | `userland/a64/interp_int*.c` | dados imediato/registrador, desvios, sistema (MRS/MSR/HINT/barreiras/DC/IC/PAC/BTI), loads/stores inteiros, exclusivos, LSE, RCpc, CRC32, FlagM |
| FP | `userland/a64/interp_fp*.c`, `userland/a64/a64_fp*.c` | todas as primitivas `a64_fp_*` (bit-exatas, sem usar a FPU do host onde ela diverge do ARM), grupos de FP escalar e conversões |
| SIMD | `userland/a64/interp_simd*.c` | AdvSIMD vetorial e escalar inteiro/FP (usando `a64_fp_*`), permutações, TBL/TBX, loads/stores estruturados, AES/SHA1/SHA256/SHA512/SHA3/PMULL, dot product, I8MM, BF16, FHM, FCMA, RDM |
| JIT | `userland/a64/jit*.c`, `userland/a64/engine.c` | tradutor de blocos para x86_64 com cache, encadeamento, fallback para o interpretador, recuperação de estado em falha, invalidação |
| TESTES | `tools/a64test.c`, `tools/build-a64test.ps1`, `tools/a64oracle/`, `tools/a64fuzz*` | oráculo QEMU bare-metal, fuzzing diferencial, suíte C compilada para x86_64 e arm64 comparando saídas, placar |
| RUNTIME | `userland/a64rt/`, partes arm64 de `kernel/compat/macho.c` e o que o kernel precisar | processo arm64 de ponta a ponta no mOS e no `machotest` |
| LIBSYS | `tools/build-libsystem-arm64.ps1`, `*_arm64.S` e `#if __aarch64__` no `userland/libsystem`, builds arm64 das libs do sistema | libSystem e demais bibliotecas do mOS em arm64 com ABI da Apple |

Testes de cada frente: arquivos novos em `tools/a64tests/` com prefixo da frente (`int_*.S`, `fp_*.c`,
`simd_*.c`, `jit_*.S`). A frente TESTES é dona do harness e do oráculo, e as outras só acrescentam
testes.

## Como testar

```
powershell -ExecutionPolicy Bypass -File tools/build-a64test.ps1          # -Jit liga o JIT
build/a64test.exe build/a64tests/hello.elf                                  # --trace, --budget N
```

Programas de teste são ELF AArch64 estáticos (`-mcpu=apple_m2`, base `0x200000000`), com `crt.S` e
`a64t.h`. Syscalls do harness: `svc #0x80` com `x16` = 1 exit, 4 write, `0x4000` despeja o estado
inteiro (x0..x30, sp, nzcv, fpcr, fpsr, v0..v31) no mesmo formato do oráculo. Sem tocar em
registradores: `svc #0x4001` = dump, `svc #0x4002` = exit(0). A pilha fica num endereço fixo
(`FZ_STACK_VA` em `tools/a64oracle/fz.h`), então `sp`/`argv` são iguais no harness e no oráculo.

## Verificação (frente TESTES)

Tudo roda no Windows, sem mOS. Um comando refaz tudo e atualiza o [placar](#estado):

```
powershell -ExecutionPolicy Bypass -File tools/a64placar.ps1 [-Jit] [-Casos 2000] [-K 8]
```

### Oráculo QEMU (`tools/a64oracle/`)

`mon.S` + `mon.c` são um monitor bare-metal para `qemu-system-aarch64 -M virt -cpu max` (EL1, MMU
4 KiB, VA de 48 bits, TBI0 = 0) que roda o **mesmo ELF** do a64test em **EL0 de verdade** e imprime a
mesma saída byte a byte: mesmas syscalls, mesmo dump, mesmos textos e códigos de saída para
UDF (132), BRK (133), HLT (132) e FAULT (139). E/S por semihosting; o ELF entra como blob cru
(`-device loader`), com os PT_LOAD mapeados nos mesmos blocos de 64 KiB que o a64test aloca.

```
powershell -File tools/a64oracle.ps1 build/a64tests/x.elf     # saída do QEMU, igual à do a64test
powershell -File tools/a64cmp.ps1 build/a64tests/x.elf [-Jit]  # a64test (interp, jit) × QEMU
```

`a64cmp` roda o a64test com `--argv0 prog` (o monitor passa `argv = {"prog"}`). Um teste que por
natureza não bate com o QEMU (valores do M1, contadores) marca no fonte `a64cmp: nao-comparavel`;
o placar então exige saída 0 (autoverificação); com `-Jit`, exige que o JIT dê exatamente a saída do interpretador (qualquer código: os testes de falha saem com 139).

EL0 configurado como o XNU (`SCTLR_EL1_REQUIRED`): SA0 = 1, nAA = 0, UCI = 1, UMA = 0, EnRCTX = 0,
TSCXT = 1, MOPS desligado; chaves de PAC desligadas (EnIA/IB/DA/DB = 0), que é exatamente a semântica
"identidade" (PAC*/AUT* não mudam o ponteiro; XPAC* removem o campo como no hardware); SVE/SME com
trap (→ UDF); CNTVCT/CNTPCT legíveis. O monitor emula por trap: `MRS CTR_EL0` (0x84448004, o valor
da frente INT), `DC ZVA` (bloco de 64 B), WFI/WFE (continua, como YIELD).

Limitações conhecidas do oráculo (QEMU 11.1 para Windows) — cada uma tratada no fuzzer, nunca
escondida como "bate":

| Limitação | Tratamento |
|---|---|
| FMA "hardfloat": com FPSR.IXC ligado, RN e entradas normais, o QEMU calcula muladd f32/f64 com o `fma()` estático do mingw, que arredonda duas vezes | casos de FMA começam com IXC = 0 (escalar e lane 0 exatos); FMLA/FMLS/FRECPS/FCMLA vetoriais divergentes são refeitos lane a lane com as lanes anteriores zeradas (exatas, IXC limpo); o que não dá para isolar vira a coluna **FMA?**; a suíte C compara a linha `fma` só com o x86_64 |
| não implementa SCTLR.SA0 (sp desalinhado como base) | o fuzzer só gera sp inicial alinhado; numa sequência em que um pré/pós-índice desalinha o sp, "nós FAULT com addr = sp" × "QEMU segue" corta a comparação ali; SA0 fica com os testes da INT |
| STXR com monitor aberto não verifica alinhamento/tradução | aceita "abort" × "status 1 sem acesso" (IMPLEMENTATION DEFINED no ARM) |
| DC CVAC/CVAU/CIVAC e IC IVAU não sondam o endereço | aceita "nós FAULT" × "QEMU segue" com o resto igual; DC CVAP/CVADP o QEMU sonda: estrito |
| DCZID_EL0 = 512 B + DZP=1 (efeito do trap do DC ZVA), CNTFRQ 62,5 MHz, contadores de tempo, PACGA (QARMA) | fora do gerador; valores do M1 ficam nos testes próprios (`int_sys.S`) |
| `-cpu max` tem recursos que não anunciamos (SVE, SME, MTE, MOPS, AFP, XS, SPECRES, FP8, LUT, FPRCVT...) | validade pelo LLVM (abaixo): o que o LLVM com os nossos recursos não conhece tem que ser UDF; um `MSR FPCR` que deixa AH/FIZ/NEP/EBF ligados no QEMU torna o caso não comparável |
| console de semihosting troca LF por CRLF | desfeito em `common.ps1` (troca exata) |
| (harness) o Windows mapeia 0x7FFE0000..0x7FFEFFFF (KUSER_SHARED_DATA) em todo processo e o a64test não consegue reservar | "QEMU FAULT ali" × "nós seguimos" é tolerado |
| (harness) UDF nosso numa palavra executada fora do caso (desvio para o rascunho, que executa dados) | se o LLVM com os nossos recursos não conhece a palavra (MTE, HBC, SM3...), o UDF conta como correto |

### Fuzzing diferencial (`tools/a64fuzz.c` → `build/a64fuzz.exe`)

```
build/a64fuzz.exe [--grupo G|INT|FP|SIMD|prefixo*]... [--casos N] [--k K] [--semente S] [--jit]
                  [--salvar] [--max-salvos M] [--relatorio x.tsv] [--exemplos x.txt] [--listar]
```

- **Grupos**: 93 classes de codificação da árvore do ARM ARM (C4.1), cada uma uma máscara/valor
  (`--listar`), mais moldes extras para sysregs de EL0, PSTATE, DC/IC, desvios com PAC, hints.
- **Validade**: para cada grupo são geradas 6× mais palavras e o `llvm-objdump` (LLVM 23) classifica
  duas vezes — com `+v8a,+lor` + os recursos de `A64_FEATURES` e com `+all`; ~85% dos casos usam uma
  palavra válida, o resto é aleatório dentro do grupo (cobre o que tem que dar UDF). Esperado: UDF se
  a palavra é `udf`, o LLVM com os nossos recursos não a conhece, é sysreg genérico que só tem nome com
  `+all`, ou (K=1) o QEMU dá UDF; senão, o mesmo estado do QEMU. Sequências usam só palavras válidas e
  sem FMA fora da 1ª posição.
- **Caso**: estado inicial aleatório (inteiros especiais, ponteiros para o rascunho, f16/f32/f64
  especiais por lane, FPCR com RMode/FZ/FZ16/DN/AHP, FPSR, NZCV, DIT/SSBS, TPIDR_EL0), K instruções
  em `0x2E0008000` numa página cheia de `brk #0xA64` (desvio para fora do caso para ali), rascunho
  determinístico de 16 KiB em `0x2E0100000`. Compara x0..x30, sp, NZCV, FPCR, FPSR, DIT/SSBS,
  TPIDR_EL0, v0..v31, hash do rascunho, tipo de saída, pc e imediato; em FAULT, o estado tem que estar
  intacto (regra de reinício do contrato).
- **Lote**: dezenas de milhares de casos por boot do QEMU (~4 mil casos/s nos três motores); um
  diretório temporário por processo (várias sessões rodam o fuzzer ao mesmo tempo).
- **Sequências** (`--k 8`): instruções válidas misturadas; o que diverge é minimizado (instruções
  viram NOP enquanto a divergência persiste) e, com `--salvar`, vira
  `tools/a64tests/regress_<grupo>_<insn>.S` — estado inicial, página de código e rascunho nos mesmos
  endereços (linker script `tools/a64oracle/regress.ld`), reproduzível com `a64cmp`.

### Suíte diferencial C (`tools/a64oracle/csuite/`)

`c_int` (8..128 bits, overflow, divisão, bits, bitfields), `c_fp` (aritmética IEEE com especiais,
conversões, arredondamentos, FMA, libm própria em `m.h`), `c_str` (memória/strings próprias,
parsing, quick/merge/heapsort), `c_rec` (recursão, ponteiros de função, switch, variádicos, HFA e
structs grandes), `c_jmp` (setjmp/longjmp próprios em asm), `c_atom` (C11 8..128 bits, LSE),
`c_vec` (laços que o `-O3` vetoriza). Cada um compila para x86_64-windows (referência nativa) e
aarch64 `-O0/-O2/-O3`; as saídas têm que ser iguais no a64test (interp/JIT) e no QEMU.

```
powershell -File tools/a64oracle/csuite/run.ps1 [-Jit] [-Programa c_fp]
```

## Runtime: processo arm64 de ponta a ponta (frente RUNTIME)

Contrato kernel ↔ runtime ↔ libSystem arm64 em `userland/a64rt/a64rt_abi.h` (layout, `a64rt_boot_t`, syscalls
próprias `0x4D00..`, ucontext/mcontext64 do Darwin arm64, ESR, tabela de sysctl).

- **Kernel (`kernel/compat/macho.c`).** `choose_arch`: x86_64 sempre que houver; arm64/arm64e quando é o único slice,
  ou forçado com `MOS_ARCH=arm64` no ambiente do processo (ou o arquivo `/Sistema/Biblioteca/a64/preferir-arm64`).
  Um libSystem por arquitetura (`ls_db`; o arm64 vem do disco em `/Sistema/Biblioteca/a64/libSystem.mos`,
  `R_AARCH64_RELATIVE`), slices das dylibs da mesma arquitetura, chained fixups arm64e (formatos 1, 12 e 13;
  autenticados viram ponteiros puros), rebase com o byte alto preservado (TBI: RTTI não único da libc++),
  stubs de símbolo ausente em arm64. O runtime `/Sistema/Biblioteca/a64/a64rt.mos` entra em `A64RT_BASE` (montado
  uma vez, cópia na escrita) com uma pilha do host de 1 MiB alinhada; a thread começa nele com `rdi = a64rt_boot_t*`.
- **Runtime (`userland/a64rt`, `tools/build-a64rt.ps1`).** Laço `a64_run` por thread (o `rt_thread_t` mora na base
  da pilha do host: `rsp & ~(1 MiB-1)`); `svc #0x4d4f` → `int 0x80` 1:1; `svc #0x80` Darwin cru (exit/read/write/
  getpid/kill/thread_selfid/TSD; o resto SIGSYS); threads (`SYS_THREAD_CREATE` com estado guest, TPIDRRO pelo
  `0x4D00`); sinais com tratadores próprios no host e quadro arm64 do Darwin na pilha/sigaltstack do guest, SA_RESTART
  decidido pelo runtime, faltas com far/esr (JIT: `a64_jit_write_fault` → `a64_jit_recover` → `a64_jit_unwind`;
  interpretador: estado já no início da instrução + volta ao laço), UDF/HLT → SIGILL, BRK → SIGTRAP com o imediato
  no ESR; `THREADCTL GETSTATE` em formato arm64; `munmap`/`mach_vm_protect` invalidam traduções; fork/exec/
  posix_spawn passam pelo kernel (x86_64 ↔ arm64 nos dois sentidos). `MOS_A64_ENGINE=interp|jit` escolhe o motor.
- **Limites conhecidos.** O kernel não aplica proteção por página, então a rede de segurança de SMC do JIT
  (`a64_host_jit_wprotect`) fica desligada e vale o IC IVAU/`sys_icache_invalidate`; não há commpage arm64 (a
  região baixa não é do usuário no mOS). A página vista pelo processo arm64 é de 16 KiB de verdade
  nas operações de memória (contrato em `docs/pagesize-arm64.md`); o kernel continua em 4 KiB.

Testes:
```
build/a64rt-test/run.sh <mach-o arm64> [args]       # machotest (MOS_A64_ENGINE, MOS_A64_DEBUG=1|2, MOS_ARCH=arm64)
.\build.ps1 -OutDir build-a64; .\tools\build-a64rt-teste.ps1 -OutDir build-a64 [-Real <binário>]
.\run.ps1 -Headless -Memory 1536 -Image build-a64/mOS.img -Serial build-a64/serial.log -Accel whpx
```
`userland/tests-a64rt/main.cpp` (zig c++ -target aarch64-macos): threads, mutex, thread_local, sinais (raise,
máscara, pendentes, pthread_kill, SIGSEGV com far/esr e mcontext alterado, brk, udf, sigaltstack), exceções C++
(também dentro de thread), uname/sysctl, página de 16 KiB (`pagina`), posix_spawn arm64→x86_64 e arm64→arm64.

## libSystem e bibliotecas do sistema em arm64 (frente LIBSYS)

**Forma do build.** `tools/build-libsystem-arm64.ps1` → `build/libSystem-arm64.mos`, no MESMO formato do blob x86
(ELF64 static-pie ligado em 0, só relocações RELATIVE no `DT_RELA`, `.symtab` com os mesmos nomes sem o `_` do Mach-O,
entrada `cx_start`), com `e_machine = EM_AARCH64` e `R_AARCH64_RELATIVE` (1027). O build.ps1 o põe em
`/Sistema/Biblioteca/a64/libSystem.mos`. O ponto central é o **triplo `arm64-apple-macos11-elf`**: o clang e o LLVM
decidem a ABI pelo SO do triplo (Darwin) e o contêiner pelo ambiente (`-elf`), então o código é o de
`-target aarch64-macos` — Darwin PCS completo: variádicos na pilha, argumentos pequenos estendidos pelo chamador,
empacotamento na pilha pelo tamanho natural, x18 nunca usado, `char` com sinal, `long double` = double, va_list =
`char*`, HFA em v0..v3, retorno indireto em x8. O ELF puro com `-target-abi darwinpcs` **não serve**: o va_list e o
va_start continuam os do AAPCS e os variádicos saem errados (conferido no assembly). Flags: `-mcpu=apple-m1`,
`-fPIC -ffixed-x18 -fsigned-char -funwind-tables`; compilação em paralelo por arquivo e `zig ld.lld -pie
--no-dynamic-linker`. O script confere no fim que só há `R_AARCH64_RELATIVE` e nenhum símbolo indefinido.

**Verificação da ABI.** `tools/check-abi-arm64.ps1` compila cada `.c` do `userland/libsystem` das duas formas
(`arm64-apple-macos11` Mach-O × `arm64-apple-macos11-elf`) até o IR do LLVM e compara: a ABI de C (classificação,
HFA, sret, byval, signext/zeroext, layout de structs) fica escrita no IR e o backend escolhe a convenção pelo SO.
Diferenças aceitas, por não serem ABI: triplo/datalayout (mangling `m:o` × `m:e`), alinhamento *preferido* de
i8/i16 em globais e variáveis locais, `llvm.used` × `llvm.compiler.used`. Placar: **99/99 arquivos iguais, 8040
funções**.

**O que mudou no libSystem (só `#if defined(__aarch64__)`, o x86 compila igual).**
- `sys.h`: `cx_syscall` com `svc #0x4d4f` (x8, x0..x4); `kshim/tls_shim.h`: `rdtsc` → `CNTVCT_EL0`.
- asm próprio em `*_arm64.S` (vazios no build x86): `objc_msg_arm64.S` (objc_msgSend/Super/Super2/msgLookup;
  guarda x0..x8 e q0..q7; nil zera x0/x1/d0..d3; `_stret`/`_fpret` são sinônimos), `objc_tramp_arm64.S` (trampolins
  de 32 bytes do `imp_implementationWithBlock`: `x1 = x0`, `x0 = bloco`, `br [bloco+16]`), `sigjmp_arm64.S` (jmp_buf
  do Darwin arm64: x19..x28, fp/lr, sp, d8..d15, máscara em +0xB0), `tlv_arm64.S` (`_tlv_bootstrap` e
  `cx_tlv_bootstrap`: x0 = descritor → x0, preserva x1..x28, fp, lr, q0..q31 e FPSR/FPCR; `__chkstk_darwin`),
  `libc_arm64.S` (memcpy/memmove/memset; `sys_icache_invalidate` = DC CVAU + DSB + IC IVAU + DSB + ISB pelas linhas
  do CTR_EL0; `sys_dcache_flush` = DC CIVAC), `ffi_arm64.S` (`cx_a64_call` do NSInvocation).
- asm inline em C: getters swiftcc (`compat_agentes.c`: self em x20, resultado indireto em x8), `cx_const_fwd`
  (`constobjs.c`), fibras do Metal (`mtlexec.c`: x19..x30 + d8..d15), `feclearexcept`/`feraiseexcept` no FPSR,
  `yield` no lugar de `pause`, cpuid/rdrand → 0 (`kernel/net/crypto.c`), TSD no `TPIDRRO_EL0` (escrito pelo
  `A64RT_SYS_SET_TSD`, lido com `mrs` e `& ~7`, como a Apple), pilha das threads alinhada a 16 sem retorno falso.
- ABI em C: o `MSG()` do `objc.h` chama o objc_msgSend com o protótipo exato montado dos tipos dos argumentos (o
  cast variádico mandaria tudo para a pilha); `objc_msgSend_stret` com ponteiro explícito virou a forma de retorno de
  struct (certa nas duas arquiteturas); NSInvocation com o PCS da Apple (HFA, x8, cópia de structs > 16 bytes,
  pilha empacotada, extensão de char/short); aliases (`__attribute__((alias))` não existe no triplo Darwin) viraram
  funções; `__builtin_setjmp` → `_setjmp`.
- libm: o x87/SSE do x86 vira `fsqrt`/`frint*`; sin/cos/tan/fmod/log2/exp2 vêm do **compiler-rt do zig** compilado
  como bitcode `aarch64-macos` (long double = double) e baixado para o mesmo triplo ELF (também `__udivti3` & cia.);
  atan2 próprio (`libm_arm64.c`). Os símbolos fracos/hidden do compiler-rt são promovidos a GLOBAL/default no
  script (o carregador só indexa GLOBAL, e o lld aplicava hidden ao memcpy do libSystem).
- Desenrolar: o **libunwind do LLVM** (o do zig) entra no blob: C/C++ no modo Apple (compact unwind + DWARF das
  imagens Mach-O por `_dyld_find_unwind_sections`), `.S` sem `__APPLE__`. Para um pc dentro do próprio libSystem,
  `_dyld_find_unwind_sections` (dlfcn.c) devolve o `.eh_frame` do blob (`cx_ls_unwind_sections`, unwind_ls.c; o
  deslocamento é gravado depois do link por `tools/patch-ehframe.ps1`). Exceções C++ atravessam quadros do libSystem
  (qsort, dispatch, NSInvocation). O libSystem **x86_64 também** recebeu o mesmo libunwind (`tools/build-libsystem.ps1`,
  triplo `x86_64-apple-macos11-elf`): antes o `_Unwind_RaiseException` era um stub e toda exceção virava terminate.
- Exceções Objective-C (`objc_exc.c`, as duas arquiteturas), como o objc-exception.mm da Apple: `objc_exception_throw`
  lança `{id, typeinfo ObjC}` pelo `__cxa_throw` da libc++abi do disco (achada por dlopen de `/usr/lib/libc++.1.dylib`),
  `__objc_personality_v0` é o `__gxx_personality_v0`, o `can_catch` do `objc_ehtype_vtable` casa pela hierarquia
  (`OBJC_EHTYPE_id`, `OBJC_EHTYPE_$_NSException` e os do app), `objc_begin/end_catch`/`rethrow` da libc++abi, o
  preprocessador do `objc_setExceptionPreprocessor` e um terminate handler que chama o `NSSetUncaughtExceptionHandler` e
  imprime nome e motivo. Sem a libc++ no disco, imprime e aborta como antes.
- Hardware visto pelo processo: sysctl do Apple Silicon (`darwin_arm64.c`, de `A64RT_SYSCTL_INTS/STRS` e
  `A64_FEATURES`), `uname -m` = arm64, páginas de 16 KiB de verdade no guest (`docs/pagesize-arm64.md`:
  `vm_page_size`, `getpagesize`, `sysconf`, `host_page_size`, mmap/mprotect/munmap e a guarda de pthread).

**Demais bibliotecas.** `tools/build-libs-arm64.ps1` faz os slices arm64 (`build-libcxx.ps1`, `build-sqlite.ps1`,
`build-icu.ps1` com `-Arch arm64` → `build\<lib>-arm64`; WebKit.framework, cujo `wk_sys` ganhou `svc #0x4d4f`) e junta
com o x86_64 em binários **universais** nos caminhos da Apple (`build\universal\usr\lib\libc++.1.dylib`,
`libsqlite3.dylib`, `libicucore.A.dylib`, `System\Library\Frameworks\WebKit.framework`; `tools/lipo-mos.ps1` é o
`lipo -create`). O build.ps1 copia por cima das só x86_64; o carregador escolhe o slice do processo, sem caminho novo.

**Cadeia Swift.** Todos os scripts da variante com interop ObjC (`build-swift-full-objc`, `-runtime-objc`,
`-overlays-objc`, `-concurrency-objc`, `-foundation-objc`, `-combine-objc`, `-uikit-objc`, `build-swiftui-objc`,
`build-frameworks-objc`, `link-swift-dylibs-objc`, `link-swift-objc`) aceitam `-Arch arm64` (`tools/swift-arch.ps1`:
`arm64-apple-macos26` / `aarch64-macos`, módulo `arm64-apple-macos.swiftmodule`, saída em `build\swift-arm64\...`); sem o
parâmetro o x86_64 é o de sempre. `build-openswiftui-deps.ps1 -Arch arm64` faz só os objetos C/C++ nativos que o
SwiftUI.framework liga (Compute, OpenRenderBox, OpenSwiftUI_SPI, ctfont). `tools/build-swift-arm64.ps1` roda as etapas
arm64 (~1 h; o OpenSwiftUICore sozinho ~22 min) e junta com `lipo-mos` todo Mach-O presente em `build\swift\objc\dylib` e
`build\swift-arm64\objc\dylib` em `build\universal-swift` (38 binários, ~110 MB: `/usr/lib/swift/libswift*.dylib`, Combine,
SwiftUI, UIKit do Catalyst e `libswiftUIKit`, CoreData, CryptoKit, WebKit, ImageIO...); só entram na imagem com
`build.ps1 -ComSwift` (ou `MOS_COM_SWIFT=1`), e o `stage-chromeapp.ps1` copia os universais por cima. Não havia asm x86 nesses fontes além do `wk_sys`; o
`Foundation.h` do SDK passou a ter `BOOL` = `bool` quando `__OBJC_BOOL_IS_BOOL` (ObjC arm64, como o `objc/objc.h`).
Teste: `tools/build-swift-arm64-test.ps1` (os `userland/tests-swiftobjc/swift` e `foundation` ligados nas dylibs, como
binário da Apple): **13/13 e 23/23 no arm64 pelo tradutor**, iguais ao x86_64. A variante nativa sem ObjC
(`build-dispatch.ps1`, `build-cryptokit.ps1`, módulos de `build-openswiftui-deps.ps1`) só vai estática nos apps SwiftUI
x86_64 do próprio mOS e fica só x86_64.

**Testes** (`tools/build-libsys-arm64-test.ps1`, no machotest com o VFS do runtime): `userland/tests-a64libsys/abi.mm`
(o mesmo fonte em arm64 e x86_64 de referência): printf com 13 variádicos e long double, va_list repassado, sscanf,
NSStringFromRect (HFA), objc_msgSend com HFA/x8/float/16 argumentos/9º double na pilha/nil, NSInvocation com 16
argumentos, HFA e x8, `imp_implementationWithBlock`, dispatch_sync/async, exceção C++ atravessando o qsort do
libSystem, setjmp/siglongjmp, páginas de 16 KiB, MAP_JIT + `sys_icache_invalidate` com código trocado, sysctl, libm,
NSApplication/NSWindow/NSView (retângulo HFA de ida e volta), @throw/@catch/@finally (por classe, `id`, superclasse,
dentro de bloco do libSystem, com `catch(...)` do C++): **36/36 no arm64** (JIT e interpretador); no x86_64 de referência
35/36 (só o NSInvocation x86 com argumentos na pilha).
`userland/tests-a64libsys/dylibs.cpp`: libc++/libc++abi, sqlite, ICU e zlib carregados dos universais do disco: **6/6**.

**zlib.** `tools/build-zlib.ps1` compila o fonte oficial (zlib 1.3.2, `.tools\zlib-dl`) no slice arm64; o x86_64 continua o
bottle do Homebrew da mesma versão; o universal vai para `/usr/local/opt/zlib/lib/libz.1.dylib` (para onde o carregador
manda `/usr/lib/libz.1.dylib`).

**Pendências.** (1) Cadeia Swift: só falta rodar um app SwiftUI/Combine/Concurrency arm64 de verdade (os testes cobrem
stdlib, runtime e Foundation). (2) Achados no x86 pela referência, fora do tradutor:
o printf do libSystem x86 lê `long double` (80 bits) como 8 bytes e desalinha os argumentos seguintes; o NSInvocation x86
não passa argumentos na pilha (mais de 6 inteiros ou 8 FP).

## JIT (frente JIT)

Arquivos: `jit_core.c` (cache, despachante, threads, invalidação, recuperação), `jit_tr.c`
(tradução, cache de registradores, flags), `jit_simd.h` (AdvSIMD/FMOV exatos), `jit_x86.h`
(codificador), `jit.h` (interno). Liga com `-Jit` (`A64_HAVE_JIT`).

- **Bloco**: até 64 instruções, nunca cruza página de 4 KiB, termina em desvio/exceção/ISB.
  Duas passadas: a 1ª descobre o que é nativo; a 2ª emite com a vida das flags exata.
- **Fronteira dos blocos**: registradores do guest em `a64_cpu`, flags em `reserved[14]` no
  formato x86 (lahf/seto), r15 = `a64_cpu*`. Dentro do bloco: X0..X30/SP em 11 registradores
  do host (LRU); flags no EFLAGS em modo ADD (CF = C) ou SUB (CF = !C). Acessos à memória e
  instruções do interpretador contam como leitura de flags (estado exato em falha/saída).
- **Exclusivos e LSE** (`jit_atom.h`): monitor do contrato (endereço, tamanho, valor), STXR = `lock cmpxchg` contra o valor guardado; CAS = `lock cmpxchg`; LDADD = `lock xadd`; SWP = `xchg`; LDCLR/LDEOR/LDSET/LDSMAX..LDUMIN = laço de `cmpxchg`; LDAPR = load. Alinhamento (atravessar 16 bytes) e SA0 como no INT, pelo caminho lento.
- **FP** (`jit_simd.h`): escalar S/D e vetorial 4S/2S/2D com SSE/FMA3 só quando FPCR = RN e FZ = 0, sem flag do x86 além de PE e com resultado (cada lane) normal, finito e não-NaN; senão a instrução inteira vai ao interpretador. PE → IXC. FCMP/FCCMP: sem NaN (ucomis) e FZ = 0, NZCV montado direto em `reserved[14]`; FCSEL é seleção de bits. FCVTZS/ZU com faixa conferida nos bits (cvtt trunca como o ARM), SCVTF/UCVTF em RN, FCVT S↔D. FABS/FNEG/FMOV são operações de bits. MXCSR do host fica em 0x1F80.
- **Chamada leve**: SIMD/FP sem emissão nativa que só toca registradores V e FPCR/FPSR chama `a64_interp_exec` direto (`jit_h_exec`), gravando os GPRs sujos e descartando só os voláteis da ABI; UDF vai ao caminho lento. Ficam fora as classes que escrevem GPR ou NZCV.
- **Estatística do interpretador**: `a64_jit_fbstat(1)` + `a64_jit_fbstat_dump` (o runtime liga com `MOS_A64_DEBUG`; no harness `A64JIT_FBSTAT=1`) contam por classe de codificação o que ainda vai ao interpretador — é o guia para a próxima emissão nativa.
- **Pilha de retorno**: BL/BLR empilham (pc+4, stub de continuação encadeável) em 16 entradas por thread (`reserved[12]`); RET confere o topo. Cada desvio indireto tem a própria busca no cache de saltos.
- **Fallback**: o que não é nativo chama `a64_interp_exec` com estado sincronizado. Casos
  CONSTRAINED UNPREDICTABLE e escolhas da frente INT (SA0, nAA, padrão de acessos dos pares
  do FEAT_LSE2, LD1 elemento a elemento ao cruzar página) são espelhados: o caminho raro roda
  inteiro no interpretador ("caminho lento"), o comum é nativo.
- **Encadeamento**: saída direta = `jmp rel32` remendado atomicamente; desvios indiretos/RET
  pelo cache por thread (4096 entradas); pedido de parada testado em arestas para trás e
  indiretas (u64 de `reserved[15]`: baixo = runtime, alto = JIT).
- **Multithread**: cache compartilhado, trava global para tradução/encadeamento/invalidação;
  código só é reaproveitado no esvaziamento total, com todas as threads fora (quiescência).
- **Invalidação**: `a64_invalidate`/IC IVAU por intervalo (índice por página). Rede de
  segurança: páginas com código traduzido ficam sem escrita; a falha de escrita devolve a
  escrita, agenda a invalidação e expulsa as threads. Depois de 4 vezes a página vira
  "volátil" e seus blocos conferem o próprio código na entrada.
- **Falhas**: cada acesso à memória é um ponto de falha (offset x86 → instrução guest, onde
  estão os registradores sujos e as flags). `a64_jit_recover` reconstrói o estado;
  `a64_jit_unwind` volta ao despachante a partir do contexto da falha.
- **Host** (fora do Windows): `a64_host_jit_alloc`, `a64_host_jit_wprotect`; o runtime chama
  `a64_jit_write_fault` no SIGSEGV, `a64_invalidate` em mprotect/munmap de código e
  `a64_jit_thread_exit` ao fim da thread (ver `a64.h`).
- **Depuração** (Windows): `A64JIT_STATS=1`, `A64JIT_NONATIVE=1`, `A64JIT_MAXINSNS=n`,
  `A64JIT_NOWP=1`, `A64JIT_FBLOG=1` (lista o que vai ao interpretador).

Testes: `jit_fault_{mid,sub,pair,ldp,q,ld1}` (falha no meio do bloco, STP/LDP/LDP Q/ST1
parciais), `jit_sa0`, `jit_ldar`, `jit_pairs`, `jit_smc` (IC IVAU e só rede de segurança),
`jit_smc_wp`, `jit_badpc`, `jit_udf_first`, `jit_atomics`, `jit_xpair`, `jit_fp`, `jit_fcmp`, `jit_cvt`, `jit_fpv`, `jit_fpsr`, `jit_neon`, `jit_neon2`, `jit_neon3`, `jit_neon4`, `jit_ldn`, `jit_crypto`, `jit_sha`, `jit_f16`, `jit_fpv_fma`, `jit_xpair_tam`, `jit_bench_int`, `jit_bench_mem`, `jit_bench_call`.

Desempenho (harness no Windows, instruções contadas pelo interpretador):

| benchmark | instruções | interp | JIT |
|---|---|---|---|
| `jit_bench_int` | 252 M | ~110 MIPS | ~1750 MIPS |
| `jit_bench_mem` | 66 M | ~90 MIPS | ~610 MIPS |
| `jit_bench_call` | 42 M | ~98 MIPS | ~430 MIPS |

Medição de 8/11 (máquina carregada por outras sessões; interp ~75–87 MIPS): inteiros 1457 MIPS, memória 677, chamadas 346. Chamadas: blocos curtos (fib) dominados pela transição entre blocos; próximo passo seria superblocos.

## Estado

| Parte | Estado |
|---|---|
| Contrato (`a64.h`, `a64_mem.h`), laço do interpretador, harness | pronto |
| INT | **pronto** (interpretador). `interp_int.c` (dados imediato/registrador), `interp_int_br.c` (desvios, exceções, sistema), `interp_int_ls.c` (loads/stores, exclusivos, LSE/LSE2, RCpc/RCpc2, LOR, LDRAA/LDRAB), `interp_int.h`. Decodificação pelo ARM ARM, com as escolhas CONSTRAINED UNPREDICTABLE do QEMU (`a64.decode`) documentadas no topo de cada arquivo. EL0 como o XNU configura (`SCTLR_EL1_REQUIRED` em `osfmk/arm64/proc_reg.h`): SA0 = 1 (SP desalinhado num load/store → FAULT com `exit_info` = SP), nAA = 0 (com LSE2: exclusivos, atômicos e acquire/release só falham ao atravessar 16 B), DC CVA*/CIVAC/IC IVAU sondam a linha; PAC "identidade" (PACGA = 0); CTR_EL0 = 0x84448004, DCZID = 4, CNTFRQ = 24 MHz. Testes: `tools/a64tests/int_{dpimm,dpreg,branch,ldst,atomic}.S` com autoverificação, iguais byte a byte ao oráculo EL0 (ignorando `\r`); `int_sys.S` (valores do M1, PAC, contadores: não comparável). Fuzz (`a64fuzz`, 42 grupos INT, build com `-DA64I_ORACLE_COMPAT`, que só desliga as quatro diferenças do QEMU listadas em `interp_int.h`): K=1 210 mil casos e K=4 328 mil casos, **0 divergências atribuíveis ao INT**; nAA = 0 conferido com monitor de nAA = 0 (24 mil casos de excl/rcpc/atomic). Sobras conhecidas, que não são do INT: STXR com base `0xffff…ffff` (o QEMU usa −1 como `exclusive_addr` vazio), endereços que no Windows são do próprio harness, desvios para o rascunho que executam dados (FP/SIMD de outras frentes) e LDLAR/STLLR marcados "deveria_udf" porque o objdump do fuzzer roda sem `+lor` (LOR é obrigatório desde o v8.1). |
| FP | **pronto** (interpretador). Todas as primitivas `a64_fp_*` de `a64.h` em software (`a64_fp.c`, escrito do zero a partir do pseudocódigo do ARM ARM; nenhuma instrução de FP do host) e todos os grupos de FP escalar (`interp_fp.c`: FP↔fixo, FP↔inteiro incl. FMOV D[1] e FJCVTZS, 1 fonte incl. FRINT32/64 e BFCVT, compare, imediato, compare condicional, 2 e 3 fontes, FCSEL; H/S/D). Placar contra QEMU 11.1 `-cpu max` (oráculo próprio `tools/a64tests/fp_oracle/fp_oracle.ps1`: 268 modelos, todas as combinações RMode/FZ/DN/FZ16/AHP, subnormais, NaN com payload, empates, limites de overflow/underflow/conversão, mais palavras aleatórias do grupo para UDF): **~10,8 milhões de vetores, 0 divergências** em resultado, FPSR, NZCV e UDF, incluindo as primitivas usadas só pelo AdvSIMD (FMULX, FRECPE/FRSQRTE/FRECPS/FRSQRTS/FRECPX, FABD, FCM*/FAC*, FCVTXN, half↔int16, URECPE/URSQRTE). Ponta a ponta: `tools/a64tests/fp_escalar.c` igual byte a byte ao oráculo EL0 (`a64cmp`). Diferenças esperadas e documentadas: FEAT_FPRCVT (o QEMU executa, nós UDF: não anunciado) e o FMA "hardfloat" do QEMU para Windows quando o IXC já está ligado (resultado do QEMU errado; zerar FPSR antes de cada instrução fundida nos testes). |
| SIMD | **pronto** (interpretador). `interp_simd.c` (decodificação de C4.1.95/96 + todos os loads/stores com V=1: LDR/STR B/H/S/D/Q em todas as formas, literal, LDP/STP/LDNP/STNP, LDUR/STUR, LD1–LD4/ST1–ST4 múltiplos e de lane, LD1R–LD4R, pós-índice imediato e registrador, regra de reinício), `interp_simd_int.c` (three same/different, two-reg misc, across lanes, pairwise, shift por imediato incl. narrowing/saturação/arredondamento/SRI/SLI, por elemento, copy, modified immediate, ZIP/UZP/TRN, EXT, TBL/TBX 1–4, RDM, DotProd, I8MM), `interp_simd_fp.c` (lanes FP16/32/64 só via `a64_fp_*`, FCVTN/L/XN, BFCVTN, FMLAL/FMLSL e *2 com FPMulAddH exato, BFDOT/BFMMLA com a aritmética "BFloat16 computation behaviors", BFMLALB/T, FCMLA/FCADD), `interp_simd_crypto.c` (AES, SHA1, SHA256, SHA512, EOR3/RAX1/XAR/BCAX, PMULL 8/64). Placar contra QEMU 11.1 `-cpu max` (oráculo diferencial próprio: uma instrução por caso, estado inteiro aleatório com valores FP especiais e todas as combinações de FPCR, compara x0..x30, sp, NZCV, FPSR, v0..v31, UDF e hash da memória): **360 mil casos** cobrindo todas as classes de codificação + palavras aleatórias do espaço SIMD, **0 divergências reais**; as 7 restantes são FMLA/FMLS/FCMLA na lane ≥1 em RN, o bug de FMA "hardfloat" do QEMU para Windows (casos conferidos com `vfmadd` do x86 dão o nosso resultado; com uma lane só, batem). O QEMU executa e nós damos UDF, como esperado, para FEAT_FP8/FP8DOT/FP8FMA, LUT, FAMINMAX, SM3/SM4 e F8F16MM (não anunciados). Ponta a ponta no harness: `tools/a64tests/simd_crypto.c` (FIPS-197, SHA-1/SHA-256 de "abc", PMULL, SHA3), `simd_int.c`, `simd_fp.c` passam. |
| JIT | pronto: toda instrução executa (nativa ou via interpretador); fuzzing de todos os 93 grupos (sementes 11, 23, 71, 72, 131, 132, 171, 201, 221 e 261 (K=8) × 27 900 casos × 6 instruções, mais 3–4 × 1000 por grupo de atômicos, FP e SIMD) e a suíte sem divergência interp × JIT. Nativos: inteiros, loads/stores, desvios, exclusivos/pares/CAS/CASP/LSE com lock e cmpxchg16b, FP escalar (aritmética, fused, FCMP/FCCMP/FCSEL, conversões FP↔int e S↔D) e FP vetorial 4S/2S/2D no caminho provado, Neon inteiro (lógicos, ADD/SUB, CMxx, MAX/MIN, MUL, deslocamentos, USHLL/SXTL, SHRN, XTN, TBL/TBX, ZIP/UZP/TRN/EXT, ADDV/UMAXV, ADDP, 3diff (ADDL/ADDW/SUBL/SUBW/MULL/MLAL/MLSL), por elemento (MUL/MLA/MLS e FMUL/FMLA/FMLS), FADDP, saturantes com QC (SQADD/UQADD/SQSUB/UQSUB em B/H/S/D, SQXTN/UQXTN/SQXTUN, SQDMULH/SQRDMULH em H), arredondados (SRSHR/URSHR/SRSRA/URSRA/RSHRN), ADDHN/SUBHN/RADDHN/RSUBHN, DUP/INS/UMOV, MOVI, LD1..LD4/ST1..ST4 múltiplos e de lane, LD1R..LD4R), cripto (AESE/AESD/AESMC/AESIMC com AES-NI, PMULL de 64 com PCLMULQDQ, EOR3/BCAX/RAX1/XAR), FP half FADD/FSUB/FMUL/FDIV/FSQRT escalar e 4H/8H, FCVT H↔S/D e FCMP H (F16C), pilha de retorno. Bench: inteiros 1595 MIPS (15×), memória 670 (6,8×), chamadas 432 (4,4×). Pela chamada leve ao interpretador (sem conversão de NZCV nem descarte do cache): SHA1/SHA256/SHA512, estimativas FP (FRECPE/FRSQRTE: tabelas do ARM), SQSHL/SQRSHL e afins, FCVT D→H (dois arredondamentos) e o resto do SIMD/FP sem forma nativa |
| TESTES / oráculo | **pronto** (ver [Verificação](#verificação-frente-testes)): oráculo QEMU bare-metal em EL0 (`tools/a64oracle/`, `a64oracle.ps1`, `a64cmp.ps1`), fuzzing diferencial interp × JIT × QEMU por grupo de codificação com validade pelo LLVM, sequências com minimização e regressões (`tools/a64fuzz.c`), suíte diferencial C x86_64 × arm64 -O0/-O2/-O3 (`tools/a64oracle/csuite/`), placar (`tools/a64placar.ps1`, tabela abaixo). Limitações do QEMU para Windows tratadas e documentadas (FMA hardfloat, SA0, STXR, DC/IC sem sonda). |
| RUNTIME | pronto (8 nov. 2026): no QEMU do mOS (WHPX, 4 CPUs) e no machotest, `tests-a64rt` passa 21/21 com o interpretador e com o JIT; binários arm64 reais (slices do Claude.app: `disclaimer` em C e `claude-ssh-askpass` em Go) rodam pelo tradutor; falta só a proteção de página no kernel para a rede de SMC do JIT |
| LIBSYS | **pronto** para C/ObjC/C++ (8 nov. 2026, ver [seção](#libsystem-e-bibliotecas-do-sistema-em-arm64-frente-libsys)): `build/libSystem-arm64.mos` com o triplo `arm64-apple-macos11-elf` (ABI da Apple), IR idêntico ao de `-target aarch64-macos` em 99/99 arquivos; asm arm64 próprio, libunwind do LLVM (também no x86), exceções ObjC de verdade (as duas arquiteturas), compiler-rt, sysctl/páginas do Apple Silicon; libc++/libc++abi, sqlite, ICU, zlib e WebKit universais. `tests-a64libsys` 36/36 (Foundation, AppKit, @try/@catch pelo tradutor) e 6/6 com as dylibs do disco; `tests-a64rt` 0 falhas com este libSystem. Cadeia Swift universal (stdlib, runtime, overlays, Concurrency, Foundation, Combine, SwiftUI, UIKit e frameworks do mOS, 38 binários; `tools/build-swift-arm64.ps1`): `tests-swiftobjc` swift 13/13 e foundation 23/23 no arm64 pelo tradutor. Falta: app SwiftUI arm64 de ponta a ponta |

<!-- placar:inicio -->
### Placar da verificação

Gerado por `tools/a64placar.ps1` em 2026-11-08 16:22 (semente 20261115, 2000 casos por grupo, com JIT). Oráculo: QEMU 11.1 `-cpu max` em EL0 sob `tools/a64oracle/mon.c`. Colunas: **exec** = casos em que a instrução deveria executar (o LLVM com os recursos de `A64_FEATURES` a conhece e o QEMU executa); **impl.** = desses, quantos o interpretador executa; **batem** = estado final igual ao do QEMU; **div.** = executa e diverge; **UDF ind.** = QEMU executa e nós damos UDF; **dev. UDF** = deveria ser UDF (recurso não anunciado, ou o QEMU dá UDF) e executamos; **FMA?** = divergência só em lanes de FMA que o QEMU do Windows calcula errado e que não deu para isolar; **JIT≠** = JIT diferente do interpretador.

| Frente | Grupos | Casos | Exec | Impl. | Batem | Div. | UDF ind. | Dev. UDF | FMA? | JIT≠ |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| INT | 43 | 86000 | 65764 | 65764 (100%) | 65764 (100%) | 0 | 0 | 1 | 0 | 0 |
| FP | 9 | 18000 | 15545 | 15545 (100%) | 15545 (100%) | 0 | 0 | 0 | 0 | 0 |
| SIMD | 41 | 82000 | 72359 | 72359 (100%) | 72357 (100%) | 0 | 0 | 0 | 2 | 0 |

Sequências (K=8 instruções válidas misturadas por caso, divergências minimizadas): 27900 casos, **2 divergentes**, 0 com JIT≠interp.

<details><summary>Por grupo de codificação</summary>

| Grupo | Frente | Casos | Exec | Impl. | Batem | Div. | UDF ind. | Dev. UDF | FMA? | JIT≠ |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| dp.pcrel | INT | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| dp.addsub_imm | INT | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| dp.addsub_tags | INT | 2000 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| dp.logic_imm | INT | 2000 | 1906 | 1906 | 1906 | 0 | 0 | 0 | 0 | 0 |
| dp.movewide | INT | 2000 | 1877 | 1877 | 1877 | 0 | 0 | 0 | 0 | 0 |
| dp.bitfield | INT | 2000 | 1775 | 1775 | 1775 | 0 | 0 | 0 | 0 | 0 |
| dp.extract | INT | 2000 | 1717 | 1717 | 1717 | 0 | 0 | 0 | 0 | 0 |
| br.cond | INT | 2000 | 1770 | 1770 | 1770 | 0 | 0 | 0 | 0 | 0 |
| br.exception | INT | 2000 | 1824 | 1824 | 1824 | 0 | 0 | 0 | 0 | 0 |
| sys.hint | INT | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| sys.barrier | INT | 2000 | 1075 | 1075 | 1075 | 0 | 0 | 0 | 0 | 0 |
| sys.pstate | INT | 2000 | 933 | 933 | 933 | 0 | 0 | 0 | 0 | 0 |
| sys.sysinstr | INT | 2000 | 966 | 966 | 966 | 0 | 0 | 0 | 0 | 0 |
| sys.sysreg | INT | 2000 | 807 | 807 | 807 | 0 | 0 | 0 | 0 | 0 |
| sys.outros | INT | 2000 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| **br.reg** | INT | 2000 | 1613 | 1613 | 1613 | 0 | 0 | 1 | 0 | 0 |
| br.imm | INT | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| br.cmp | INT | 2000 | 1998 | 1998 | 1998 | 0 | 0 | 0 | 0 | 0 |
| br.test | INT | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_mult | SIMD | 2000 | 1814 | 1814 | 1814 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_mult_post | SIMD | 2000 | 1839 | 1839 | 1839 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_single | SIMD | 2000 | 1832 | 1832 | 1832 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_single_post | SIMD | 2000 | 1868 | 1868 | 1868 | 0 | 0 | 0 | 0 | 0 |
| ls.excl | INT | 2000 | 1155 | 1155 | 1155 | 0 | 0 | 0 | 0 | 0 |
| ls.rcpc_unscaled | INT | 2000 | 1942 | 1942 | 1942 | 0 | 0 | 0 | 0 | 0 |
| ls.literal | INT | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_literal | SIMD | 2000 | 1927 | 1927 | 1927 | 0 | 0 | 0 | 0 | 0 |
| ls.pair | INT | 2000 | 1890 | 1890 | 1890 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_pair | SIMD | 2000 | 1936 | 1936 | 1936 | 0 | 0 | 0 | 0 | 0 |
| ls.imm9 | INT | 2000 | 1943 | 1943 | 1943 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_imm9 | SIMD | 2000 | 1837 | 1837 | 1837 | 0 | 0 | 0 | 0 | 0 |
| ls.atomic | INT | 2000 | 1817 | 1817 | 1817 | 0 | 0 | 0 | 0 | 0 |
| ls.regoff | INT | 2000 | 1837 | 1837 | 1837 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_regoff | SIMD | 2000 | 1803 | 1803 | 1803 | 0 | 0 | 0 | 0 | 0 |
| ls.pac | INT | 2000 | 1783 | 1783 | 1783 | 0 | 0 | 0 | 0 | 0 |
| ls.uimm | INT | 2000 | 1967 | 1967 | 1967 | 0 | 0 | 0 | 0 | 0 |
| simd.ls_uimm | SIMD | 2000 | 1898 | 1898 | 1898 | 0 | 0 | 0 | 0 | 0 |
| ls.outros | INT | 2000 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| dp.2src | INT | 2000 | 1748 | 1748 | 1748 | 0 | 0 | 0 | 0 | 0 |
| dp.1src | INT | 2000 | 1799 | 1799 | 1799 | 0 | 0 | 0 | 0 | 0 |
| dp.logic_reg | INT | 2000 | 1924 | 1924 | 1924 | 0 | 0 | 0 | 0 | 0 |
| dp.addsub_shift | INT | 2000 | 1868 | 1868 | 1868 | 0 | 0 | 0 | 0 | 0 |
| dp.addsub_ext | INT | 2000 | 1737 | 1737 | 1737 | 0 | 0 | 0 | 0 | 0 |
| dp.adc | INT | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| dp.rmif | INT | 2000 | 1722 | 1722 | 1722 | 0 | 0 | 0 | 0 | 0 |
| dp.setf | INT | 2000 | 1691 | 1691 | 1691 | 0 | 0 | 0 | 0 | 0 |
| dp.ccmp_reg | INT | 2000 | 1745 | 1745 | 1745 | 0 | 0 | 0 | 0 | 0 |
| dp.ccmp_imm | INT | 2000 | 1757 | 1757 | 1757 | 0 | 0 | 0 | 0 | 0 |
| dp.csel | INT | 2000 | 1770 | 1770 | 1770 | 0 | 0 | 0 | 0 | 0 |
| dp.3src | INT | 2000 | 1408 | 1408 | 1408 | 0 | 0 | 0 | 0 | 0 |
| dp.outros | INT | 2000 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| fp.fixed | FP | 2000 | 1701 | 1701 | 1701 | 0 | 0 | 0 | 0 | 0 |
| fp.int | FP | 2000 | 1775 | 1775 | 1775 | 0 | 0 | 0 | 0 | 0 |
| fp.1src | FP | 2000 | 1696 | 1696 | 1696 | 0 | 0 | 0 | 0 | 0 |
| fp.cmp | FP | 2000 | 1720 | 1720 | 1720 | 0 | 0 | 0 | 0 | 0 |
| fp.imm | FP | 2000 | 1683 | 1683 | 1683 | 0 | 0 | 0 | 0 | 0 |
| fp.ccmp | FP | 2000 | 1740 | 1740 | 1740 | 0 | 0 | 0 | 0 | 0 |
| fp.2src | FP | 2000 | 1720 | 1720 | 1720 | 0 | 0 | 0 | 0 | 0 |
| fp.csel | FP | 2000 | 1768 | 1768 | 1768 | 0 | 0 | 0 | 0 | 0 |
| fp.3src | FP | 2000 | 1742 | 1742 | 1742 | 0 | 0 | 0 | 0 | 0 |
| crypto.aes | SIMD | 2000 | 1710 | 1710 | 1710 | 0 | 0 | 0 | 0 | 0 |
| crypto.sha3reg | SIMD | 2000 | 1762 | 1762 | 1762 | 0 | 0 | 0 | 0 | 0 |
| crypto.sha2reg | SIMD | 2000 | 1699 | 1699 | 1699 | 0 | 0 | 0 | 0 | 0 |
| crypto.sha512_3 | SIMD | 2000 | 1834 | 1834 | 1834 | 0 | 0 | 0 | 0 | 0 |
| crypto.4reg | SIMD | 2000 | 1866 | 1866 | 1866 | 0 | 0 | 0 | 0 | 0 |
| crypto.xar | SIMD | 2000 | 2000 | 2000 | 2000 | 0 | 0 | 0 | 0 | 0 |
| crypto.sha512_2 | SIMD | 2000 | 1789 | 1789 | 1789 | 0 | 0 | 0 | 0 | 0 |
| simds.copy | SIMD | 2000 | 1687 | 1687 | 1687 | 0 | 0 | 0 | 0 | 0 |
| simds.3same_fp16 | SIMD | 2000 | 1768 | 1768 | 1768 | 0 | 0 | 0 | 0 | 0 |
| simds.2misc_fp16 | SIMD | 2000 | 1754 | 1754 | 1754 | 0 | 0 | 0 | 0 | 0 |
| simds.3same_extra | SIMD | 2000 | 1708 | 1708 | 1708 | 0 | 0 | 0 | 0 | 0 |
| simds.2misc | SIMD | 2000 | 1786 | 1786 | 1786 | 0 | 0 | 0 | 0 | 0 |
| simds.pairwise | SIMD | 2000 | 1743 | 1743 | 1743 | 0 | 0 | 0 | 0 | 0 |
| simds.3diff | SIMD | 2000 | 1711 | 1711 | 1711 | 0 | 0 | 0 | 0 | 0 |
| simds.3same | SIMD | 2000 | 1757 | 1757 | 1757 | 0 | 0 | 0 | 0 | 0 |
| simds.shift | SIMD | 2000 | 1761 | 1761 | 1761 | 0 | 0 | 0 | 0 | 0 |
| simds.indexed | SIMD | 2000 | 1744 | 1744 | 1744 | 0 | 0 | 0 | 0 | 0 |
| simdv.tbl | SIMD | 2000 | 1762 | 1762 | 1762 | 0 | 0 | 0 | 0 | 0 |
| simdv.perm | SIMD | 2000 | 1889 | 1889 | 1889 | 0 | 0 | 0 | 0 | 0 |
| simdv.ext | SIMD | 2000 | 1771 | 1771 | 1771 | 0 | 0 | 0 | 0 | 0 |
| simdv.copy | SIMD | 2000 | 1813 | 1813 | 1813 | 0 | 0 | 0 | 0 | 0 |
| simdv.3same_fp16 | SIMD | 2000 | 1915 | 1915 | 1915 | 0 | 0 | 0 | 0 | 0 |
| simdv.2misc_fp16 | SIMD | 2000 | 1784 | 1784 | 1784 | 0 | 0 | 0 | 0 | 0 |
| simdv.3same_extra | SIMD | 2000 | 1746 | 1746 | 1746 | 0 | 0 | 0 | 0 | 0 |
| simdv.2misc | SIMD | 2000 | 1858 | 1858 | 1858 | 0 | 0 | 0 | 0 | 0 |
| simdv.across | SIMD | 2000 | 1722 | 1722 | 1722 | 0 | 0 | 0 | 0 | 0 |
| simdv.3diff | SIMD | 2000 | 1862 | 1862 | 1862 | 0 | 0 | 0 | 0 | 0 |
| simdv.3same | SIMD | 2000 | 1922 | 1922 | 1922 | 0 | 0 | 0 | 0 | 0 |
| simdv.modimm | SIMD | 2000 | 1865 | 1865 | 1865 | 0 | 0 | 0 | 0 | 0 |
| simdv.shift | SIMD | 2000 | 1778 | 1778 | 1778 | 0 | 0 | 0 | 0 | 0 |
| simdv.indexed | SIMD | 2000 | 1839 | 1839 | 1837 | 0 | 0 | 0 | 2 | 0 |
| simd.outros | SIMD | 2000 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| naoalocado | INT | 2000 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

</details>

Testes ELF (`build/a64tests`, a64test × oráculo): **39 de 39** passam.

Suíte diferencial C (`tools/a64oracle/csuite`: x86_64 nativo × arm64 -O0/-O2/-O3 no a64test e no QEMU): **21 de 21** binários iguais.
<!-- placar:fim -->

