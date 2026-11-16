# Runtime C/C++ do mOS (frente do Leonardo)

Escopo: libc++ completa, libc++abi, libunwind (`unw_*`, `_Unwind_*`, `-O1`/`-O2`), exceções C++ e ObjC, e o resto de
libc/libm/libSystem C que ainda é stub. Não entram aqui: Swift/Dispatch (Fernanda), Network/`nw_*` (Patricia),
CommonCrypto/Security (Natalia), `CF*`/`NS*` (Eduardo), arquivos e `getfsent` (Vinicius), mach/xpc (Renata).

Este documento separa o que está pronto e testado do que falta. "Teste de host" = roda no Linux, sem o kernel.

## Estado (15 nov. 2026)

| Peça | Como está | Teste |
|---|---|---|
| **libm** (double, float, long double) | **Pronto.** musl 1.2.5 (MIT) em `userland/third_party/musl`, compilado à parte por `tools/build-libm-musl.sh` (Linux) e pelos dois `.ps1`, ligado no `libSystem.mos` e no `libSystem-arm64.mos`. Substitui a libm antiga (fsin sem redução, `log1p = log(1+x)`, `asin` por `atan2`, `round` errado em 0.49999999999999994, todo `*l` reduzido a double, 21 stubs vazios de `acosl`…`tgammal`). Nomes só da Apple em `mos/apple.c`: `__fpclassifyd`, `__isnand`…, `__sinpi/__cospi/__tanpi` (redução exata, < 1 ULP), `__sincos_stret`, `__sincospi_stret`, `__exp10`. FP_* com os valores da Apple. | `tools/libm_hosttest.sh`: **137/137**, 20 mil entradas por função contra a glibc em long double. Também roda dentro do `tools/build.sh`. |
| **fenv** | **Pronto.** `userland/libsystem/fenv.c` com o `fenv_t` da Apple (x86_64: x87 + MXCSR juntos; arm64: FPSR/FPCR), `_FE_DFL_ENV`, `fegetenv/fesetenv/feholdexcept/feupdateenv`, flags e arredondamento. | No mesmo teste (x86_64). arm64: só `-fsyntax-only`. |
| **libc++ / libc++abi** | A do LLVM 22.1.8 (a do zig), `build/libcxx/libc++.1.dylib`, instalada em `/usr/lib` e escolhida pelo carregador (`kernel/compat/macho.c`). Os 163 imports dela resolvem no `libSystem.mos` (machocheck, 15 nov.). | `tools/libcxx-host-test.cpp` no `build-libcxx.sh`. |
| **Unwind x86_64** | Motor próprio em `cxx_unwind.c` (DWARF CFI + compact unwind, `_Unwind_*`, `unw_*`, personality). | `tools/unw_hosttest.c`, `tools/cxx_unwind_host.sh`. |
| **Unwind arm64** | libunwind do LLVM no blob (`build-libsystem-arm64.ps1`). | `tests-a64libsys`. |

## O que falta (ordem de trabalho)

1. **Locale de verdade.** Hoje `newlocale` ignora o nome e devolve sempre o "C"/UTF-8 (`posixmore.c`); todos os `*_l`
   (`strtod_l`, `strftime_l`, `strcoll_l`, `localeconv_l`, `__maskrune_l`…) jogam o locale fora. A libc++ do disco usa
   esses `*_l` para `std::locale("pt_BR.UTF-8")`, `num_put`, `money_put`, `time_put`, `collate`. Plano: tabelas
   LC_NUMERIC/LC_MONETARY/LC_TIME/LC_COLLATE/LC_MESSAGES por locale (dados do CLDR, licença Unicode), `setlocale`/
   `newlocale`/`uselocale` por thread, `querylocale`, `localeconv` com os dados certos, `nl_langinfo`.
2. **Caracteres largos e multibyte.** Núcleo UTF-8 **feito** (`userland/libsystem/multibyte.c`,
   `tools/multibyte_hosttest.sh` 2074/2074): `mbrtowc`/`mbtowc`/`mblen`/`mbrlen`/`wctomb`/`wcrtomb`/`mbsinit`
   restartáveis e estritos (rejeitam codificação longa demais, substitutos e > U+10FFFF; guardam a sequência pela
   metade no `mbstate_t`). Classes/caixa de caractere largo **feitas** (`userland/libsystem/wctype_wide.c` com as
   tabelas Unicode do musl, `tools/wctype_hosttest.sh` 35/35): `iswalpha`/…/`towupper`/`towlower`/`wcwidth`/`wctype`/
   `iswctype` e os nomes extendidos da Apple `___runetype`/`___tolower`/`___toupper` (e `_l`), antes só ASCII.
   Falta: `getwc`/`ungetwc`/`fputwc` acima de ASCII (`vscode_libc.c`), `swprintf`/`vfwprintf`, `wcstod`/`wcstold`.
3. **long double no stdio.** `strtold`/`strtold_l`/`wcstold` reduzem a double e o `printf("%Lf")` converte para double
   (`stdio.c`). Plano: `floatscan` e conversão de 80 bits exatas (algoritmo do musl, MIT).
4. **Stubs C de `sys_zerados.c` desta frente.** **Feito** (`userland/libsystem/libc_bsd.c` + `swiftsupport.c`,
   teste `tools/libc_bsd_hosttest.sh` 37/37): o gerador `random`/`srandom`/`initstate`/`setstate` da BSD (porte do
   musl, substitui o atalho para `rand()`), `ffs`/`ffsl`/`ffsll`/`fls`/`flsl`/`flsll`, `lldiv`, `timingsafe_bcmp`,
   `NXGetArchInfoFromCpuType`/`NXGetLocalArchInfo`, `backtrace_from_fp`/`backtrace_async` (e o `backtrace` de
   `commoncrypto.c`, que devolvia 0, agora anda pelos ponteiros de frame), `syscall`, `sys_nerr`, `vasprintf_l`,
   `strtoll_l`/`strtoull_l`, `open_dprotected_np`, `xattr_name_with_flags`, os `*_np` de pthread/posix_spawn, os
   ganchos `malloc_logger`/`__syscall_logger`, e `__ashlti3`/`__lshrti3`/`__ashrti3`. Falta ainda: `matrix_identity_*`
   e `__invert_f*` (SIMD; mais perto da Accelerate). Os de `getfsent`/`vm_read`/`mach_*` vão aos donos.
5. **libc++ fallback.** `libcxx.c`/`cxxabi.c` só valem sem a dylib no disco; o `__cxa_throw` de lá ainda aborta.
   Plano: com a dylib sempre na imagem, tirar o caminho de reserva ou fazê-lo chamar o motor de exceção real.
6. **Unwind `-O1`/`-O2`.** Núcleo **provado** no host x86_64 (`tools/cxx_unwind_host.sh`, passa em `-O0`/`-O1`/`-O2`
   com `-fomit-frame-pointer`): throw através de frames, destrutores, `catch(...)`, callee-saved (rbx/r12-r15) e XMM
   vivos no catch, `current_exception`/`rethrow_exception`, rethrow aninhado e `throw;`; mais o compact unwind
   (immediate/indirect/rbp/DWARF) e `backtrace`. Falta cobrir, com teste: catch através de um frame C, `alloca`,
   tail call, `std::nested_exception`/`throw_with_nested`, catch por classe base (RTTI), e a bateria no **arm64** e em
   **Mach-O real** (hoje só host x86_64). ObjC↔C++ tem `tools/objc_eh_hosttest.c` à parte.
7. ~~`qsort_r` O(n²)~~ — já resolvido: `qsort`/`qsort_r`/`heapsort`/`mergesort`/`psort` são O(n log n) (`libc.c`,
   `sortb.c`). A nota de `docs/libsys-pendencias.md` §3 está desatualizada.
8. **libc++ de ponta a ponta**: `iostream`, `filesystem`, `regex`, `thread`, `atomic` (`__ulock_wait/wake`), `locale`
   rodando num Mach-O de teste pelo `machotest`, além do teste de host.

## Limites conhecidos da libm

- `powl` (long double) é o algoritmo do Cephes: até 5 ULPs na faixa comum e ~20 perto do overflow. Em double e float o
  `pow` fica abaixo de 1 ULP.
- `tgamma` até 9 ULPs (o mesmo limite que a glibc documenta), `sinh`/`tanh`/`asinh`/`atanh`/`atan2` até 2, `erfc` até 3.
- Três correções no musl 1.2.5, marcadas com "mOS:" no fonte: sinal de `tgammal` para x < -13 com fração > 1/2,
  sinal de `powl` no overflow/underflow com base negativa e expoente ímpar, e precisão do `expm1l` (e por ele
  `sinhl`, `coshl`, `tanhl`) para |x| > ln 2.

## Como testar

```bash
tools/libm_hosttest.sh                 # libm + fenv x86_64 contra a glibc
tools/build-libm-musl.sh "$(command -v zig)" aarch64 /tmp/libm-a64 2   # confere que a libm arm64 compila
```
