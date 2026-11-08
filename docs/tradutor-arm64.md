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
o placar então só exige saída 0 (autoverificação).

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
| não implementa SCTLR.SA0 (sp desalinhado como base) | o fuzzer só gera sp alinhado; SA0 fica com os testes da INT |
| STXR com monitor aberto não verifica alinhamento/tradução | aceita "abort" × "status 1 sem acesso" (IMPLEMENTATION DEFINED no ARM) |
| DC CVAC/CVAU/CIVAC e IC IVAU não sondam o endereço | aceita "nós FAULT" × "QEMU segue" com o resto igual; DC CVAP/CVADP o QEMU sonda: estrito |
| DCZID_EL0 = 512 B + DZP=1 (efeito do trap do DC ZVA), CNTFRQ 62,5 MHz, contadores de tempo, PACGA (QARMA) | fora do gerador; valores do M1 ficam nos testes próprios (`int_sys.S`) |
| `-cpu max` tem recursos que não anunciamos (SVE, SME, MTE, MOPS, AFP, XS, SPECRES, FP8, LUT...) | validade pelo LLVM (abaixo): o que só existe com `+all` tem que ser UDF |
| console de semihosting troca LF por CRLF | desfeito em `common.ps1` (troca exata) |

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
  a palavra é `udf`, só existe com `+all`, ou o QEMU dá UDF; senão, o mesmo estado do QEMU.
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
  região baixa não é do usuário no mOS); hw.pagesize 16 KiB é o que o libSystem arm64 informa.

Testes:
```
build/a64rt-test/run.sh <mach-o arm64> [args]       # machotest (MOS_A64_ENGINE, MOS_A64_DEBUG=1|2, MOS_ARCH=arm64)
.\build.ps1 -OutDir build-a64; .\tools\build-a64rt-teste.ps1 -OutDir build-a64 [-Real <binário>]
.\run.ps1 -Headless -Memory 1536 -Image build-a64/mOS.img -Serial build-a64/serial.log -Accel whpx
```
`userland/tests-a64rt/main.cpp` (zig c++ -target aarch64-macos): threads, mutex, thread_local, sinais (raise,
máscara, pendentes, pthread_kill, SIGSEGV com far/esr e mcontext alterado, brk, udf, sigaltstack), exceções C++
(também dentro de thread), uname/sysctl, posix_spawn arm64→x86_64 e arm64→arm64.

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
  `_dyld_find_unwind_sections` (dlfcn.c) devolve o `.eh_frame` do blob (`cx_ls_unwind_sections`, darwin_arm64.c; o
  deslocamento é gravado depois do link). Exceções C++ atravessam quadros do libSystem (qsort, dispatch, NSInvocation).
- Hardware visto pelo processo: sysctl do Apple Silicon (`darwin_arm64.c`, de `A64RT_SYSCTL_INTS/STRS` e
  `A64_FEATURES`), `uname -m` = arm64, páginas de 16 KiB (`vm_page_size`, `getpagesize`, `sysconf`, `host_page_size`;
  o mmap anônimo alinha a 16 KiB).

**Demais bibliotecas.** `tools/build-libs-arm64.ps1` faz os slices arm64 (`build-libcxx.ps1`, `build-sqlite.ps1`,
`build-icu.ps1` com `-Arch arm64` → `build\<lib>-arm64`; WebKit.framework, cujo `wk_sys` ganhou `svc #0x4d4f`) e junta
com o x86_64 em binários **universais** nos caminhos da Apple (`build\universal\usr\lib\libc++.1.dylib`,
`libsqlite3.dylib`, `libicucore.A.dylib`, `System\Library\Frameworks\WebKit.framework`; `tools/lipo-mos.ps1` é o
`lipo -create`). O build.ps1 copia por cima das só x86_64; o carregador escolhe o slice do processo, sem caminho novo.

**Testes** (`tools/build-libsys-arm64-test.ps1`, no machotest com o VFS do runtime): `userland/tests-a64libsys/abi.mm`
(o mesmo fonte em arm64 e x86_64 de referência): printf com 13 variádicos e long double, va_list repassado, sscanf,
NSStringFromRect (HFA), objc_msgSend com HFA/x8/float/16 argumentos/9º double na pilha/nil, NSInvocation com 16
argumentos, HFA e x8, `imp_implementationWithBlock`, dispatch_sync/async, exceção C++ atravessando o qsort do
libSystem, setjmp/siglongjmp, páginas de 16 KiB, MAP_JIT + `sys_icache_invalidate` com código trocado, sysctl, libm,
NSApplication/NSWindow/NSView (retângulo HFA de ida e volta): **31/31 no arm64** (JIT e interpretador).
`userland/tests-a64libsys/dylibs.cpp`: libc++/libc++abi, sqlite e ICU carregados dos universais do disco: **5/5**.

**Pendências.** (1) Swift (stdlib/runtime/overlays/Foundation/Combine/SwiftUI, `tools/build-swift-*-objc.ps1`,
`build-frameworks-objc.ps1`, CryptoKit) ainda só x86_64: os scripts têm o alvo fixo em poucos lugares, mas a cadeia é
longa. (2) zlib do Homebrew (`/usr/local/opt/zlib`) sem slice arm64 (bottle `arm64_*`). (3) Nas duas arquiteturas,
`objc_exception_throw` aborta (sem @try/@catch de ObjC); o x86 também não desenrola C++ dentro do libSystem. (4) Achados
no x86 pela referência: o printf do libSystem x86 lê `long double` (80 bits) como 8 bytes e desalinha os argumentos
seguintes; o NSInvocation x86 não passa argumentos na pilha (mais de 6 inteiros ou 8 FP).

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
`jit_smc_wp`, `jit_bench_int`, `jit_bench_mem`.

Desempenho (harness no Windows, instruções contadas pelo interpretador):

| benchmark | instruções | interp | JIT |
|---|---|---|---|
| `jit_bench_int` | 252 M | ~110 MIPS | ~1750 MIPS |
| `jit_bench_mem` | 66 M | ~90 MIPS | ~610 MIPS |

## Estado

| Parte | Estado |
|---|---|
| Contrato (`a64.h`, `a64_mem.h`), laço do interpretador, harness | pronto |
| INT | **pronto** (interpretador). `interp_int.c` (dados imediato/registrador), `interp_int_br.c` (desvios, exceções, sistema), `interp_int_ls.c` (loads/stores, exclusivos, LSE/LSE2, RCpc/RCpc2, LOR, LDRAA/LDRAB), `interp_int.h`. Decodificação pelo ARM ARM, com as escolhas CONSTRAINED UNPREDICTABLE do QEMU (`a64.decode`) documentadas no topo de cada arquivo. EL0 como o XNU configura (`SCTLR_EL1_REQUIRED` em `osfmk/arm64/proc_reg.h`): SA0 = 1 (SP desalinhado num load/store → FAULT com `exit_info` = SP), nAA = 0 (com LSE2: exclusivos, atômicos e acquire/release só falham ao atravessar 16 B), DC CVA*/CIVAC/IC IVAU sondam a linha; PAC "identidade" (PACGA = 0); CTR_EL0 = 0x84448004, DCZID = 4, CNTFRQ = 24 MHz. Testes: `tools/a64tests/int_{dpimm,dpreg,branch,ldst,atomic}.S` com autoverificação, iguais byte a byte ao oráculo EL0 (ignorando `\r`); `int_sys.S` (valores do M1, PAC, contadores: não comparável). Fuzz (`a64fuzz`, 42 grupos INT, build com `-DA64I_ORACLE_COMPAT`, que só desliga as quatro diferenças do QEMU listadas em `interp_int.h`): K=1 210 mil casos e K=4 328 mil casos, **0 divergências atribuíveis ao INT**; nAA = 0 conferido com monitor de nAA = 0 (24 mil casos de excl/rcpc/atomic). Sobras conhecidas, que não são do INT: STXR com base `0xffff…ffff` (o QEMU usa −1 como `exclusive_addr` vazio), endereços que no Windows são do próprio harness, desvios para o rascunho que executam dados (FP/SIMD de outras frentes) e LDLAR/STLLR marcados "deveria_udf" porque o objdump do fuzzer roda sem `+lor` (LOR é obrigatório desde o v8.1). |
| FP | **pronto** (interpretador). Todas as primitivas `a64_fp_*` de `a64.h` em software (`a64_fp.c`, escrito do zero a partir do pseudocódigo do ARM ARM; nenhuma instrução de FP do host) e todos os grupos de FP escalar (`interp_fp.c`: FP↔fixo, FP↔inteiro incl. FMOV D[1] e FJCVTZS, 1 fonte incl. FRINT32/64 e BFCVT, compare, imediato, compare condicional, 2 e 3 fontes, FCSEL; H/S/D). Placar contra QEMU 11.1 `-cpu max` (oráculo próprio `tools/a64tests/fp_oracle/fp_oracle.ps1`: 268 modelos, todas as combinações RMode/FZ/DN/FZ16/AHP, subnormais, NaN com payload, empates, limites de overflow/underflow/conversão, mais palavras aleatórias do grupo para UDF): **~10,8 milhões de vetores, 0 divergências** em resultado, FPSR, NZCV e UDF, incluindo as primitivas usadas só pelo AdvSIMD (FMULX, FRECPE/FRSQRTE/FRECPS/FRSQRTS/FRECPX, FABD, FCM*/FAC*, FCVTXN, half↔int16, URECPE/URSQRTE). Ponta a ponta: `tools/a64tests/fp_escalar.c` igual byte a byte ao oráculo EL0 (`a64cmp`). Diferenças esperadas e documentadas: FEAT_FPRCVT (o QEMU executa, nós UDF: não anunciado) e o FMA "hardfloat" do QEMU para Windows quando o IXC já está ligado (resultado do QEMU errado; zerar FPSR antes de cada instrução fundida nos testes). |
| SIMD | **pronto** (interpretador). `interp_simd.c` (decodificação de C4.1.95/96 + todos os loads/stores com V=1: LDR/STR B/H/S/D/Q em todas as formas, literal, LDP/STP/LDNP/STNP, LDUR/STUR, LD1–LD4/ST1–ST4 múltiplos e de lane, LD1R–LD4R, pós-índice imediato e registrador, regra de reinício), `interp_simd_int.c` (three same/different, two-reg misc, across lanes, pairwise, shift por imediato incl. narrowing/saturação/arredondamento/SRI/SLI, por elemento, copy, modified immediate, ZIP/UZP/TRN, EXT, TBL/TBX 1–4, RDM, DotProd, I8MM), `interp_simd_fp.c` (lanes FP16/32/64 só via `a64_fp_*`, FCVTN/L/XN, BFCVTN, FMLAL/FMLSL e *2 com FPMulAddH exato, BFDOT/BFMMLA com a aritmética "BFloat16 computation behaviors", BFMLALB/T, FCMLA/FCADD), `interp_simd_crypto.c` (AES, SHA1, SHA256, SHA512, EOR3/RAX1/XAR/BCAX, PMULL 8/64). Placar contra QEMU 11.1 `-cpu max` (oráculo diferencial próprio: uma instrução por caso, estado inteiro aleatório com valores FP especiais e todas as combinações de FPCR, compara x0..x30, sp, NZCV, FPSR, v0..v31, UDF e hash da memória): **360 mil casos** cobrindo todas as classes de codificação + palavras aleatórias do espaço SIMD, **0 divergências reais**; as 7 restantes são FMLA/FMLS/FCMLA na lane ≥1 em RN, o bug de FMA "hardfloat" do QEMU para Windows (casos conferidos com `vfmadd` do x86 dão o nosso resultado; com uma lane só, batem). O QEMU executa e nós damos UDF, como esperado, para FEAT_FP8/FP8DOT/FP8FMA, LUT, FAMINMAX, SM3/SM4 e F8F16MM (não anunciados). Ponta a ponta no harness: `tools/a64tests/simd_crypto.c` (FIPS-197, SHA-1/SHA-256 de "abc", PMULL, SHA3), `simd_int.c`, `simd_fp.c` passam. |
| JIT | núcleo pronto: toda instrução executa (nativa ou via interpretador), 30 testes + fuzzing INT sem divergência; bench inteiros ~1750 MIPS (15× interp), memória/strings ~610 MIPS (7×). Falta: exclusivos/LSE nativos, FP nativo, pilha de retorno (ver seção JIT) |
| TESTES / oráculo | **pronto** (ver [Verificação](#verificação-frente-testes)): oráculo QEMU bare-metal em EL0 (`tools/a64oracle/`, `a64oracle.ps1`, `a64cmp.ps1`), fuzzing diferencial interp × JIT × QEMU por grupo de codificação com validade pelo LLVM, sequências com minimização e regressões (`tools/a64fuzz.c`), suíte diferencial C x86_64 × arm64 -O0/-O2/-O3 (`tools/a64oracle/csuite/`), placar (`tools/a64placar.ps1`, tabela abaixo). Limitações do QEMU para Windows tratadas e documentadas (FMA hardfloat, SA0, STXR, DC/IC sem sonda). |
| RUNTIME | pronto (8 nov. 2026): no QEMU do mOS (WHPX, 4 CPUs) e no machotest, `tests-a64rt` passa 21/21 com o interpretador e com o JIT; binários arm64 reais (slices do Claude.app: `disclaimer` em C e `claude-ssh-askpass` em Go) rodam pelo tradutor; falta só a proteção de página no kernel para a rede de SMC do JIT |
| LIBSYS | **pronto** para C/ObjC/C++ (8 nov. 2026, ver [seção](#libsystem-e-bibliotecas-do-sistema-em-arm64-frente-libsys)): `build/libSystem-arm64.mos` com o triplo `arm64-apple-macos11-elf` (ABI da Apple), IR idêntico ao de `-target aarch64-macos` em 99/99 arquivos; asm arm64 próprio, libunwind do LLVM, compiler-rt, sysctl/páginas do Apple Silicon; libc++/libc++abi, sqlite, ICU e WebKit universais. `tests-a64libsys` 31/31 (Foundation e AppKit pelo tradutor) e 5/5 com as dylibs do disco; `tests-a64rt` 0 falhas com este libSystem. Falta: cadeia Swift em arm64, zlib arm64, exceções ObjC (as duas arquiteturas) |
