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
2. **Caracteres largos e multibyte.** `getwc`/`ungetwc`/`fputwc` só ASCII (`vscode_libc.c`); revisar `mbrtowc`,
   `wcrtomb`, `mbsnrtowcs`, `wcsnrtombs`, `wcwidth`, `towupper`/`iswalpha` com tabela Unicode, `swprintf`/`vfwprintf`,
   `wcstod`/`wcstold`.
3. **long double no stdio.** `strtold`/`strtold_l`/`wcstold` reduzem a double e o `printf("%Lf")` converte para double
   (`stdio.c`). Plano: `floatscan` e conversão de 80 bits exatas (algoritmo do musl, MIT).
4. **Stubs C de `sys_zerados.c` desta frente** (todos `void f(void) {}`): `strtoll_l`, `strtoull_l`, `lldiv`, `flsl`,
   `timingsafe_bcmp`, `initstate`, `setstate`, `vasprintf_l`, `__ashlti3`, `__lshrti3`, `sys_nerr` (é variável),
   `syscall`, `backtrace_from_fp`, `backtrace_async`, `NXGetArchInfoFromCpuType`, `NXGetLocalArchInfo`,
   `malloc_logger`, `__syscall_logger`, `os_unfair_lock_assert_not_owner`, `os_proc_available_memory`,
   `pthread_fchdir_np`, `pthread_from_mach_thread_np`, `pthread_get_qos_class_np`, `pthread_introspection_hook_install`,
   `__pthread_workqueue_setkill`, `posix_spawnattr_setexceptionports_np`, `open_dprotected_np`, `xattr_name_with_flags`,
   `matrix_identity_*`. (Os de libm saíram neste turno.) Os de `getfsent`/`vm_read`/`mach_*` vão aos donos.
5. **libc++ fallback.** `libcxx.c`/`cxxabi.c` só valem sem a dylib no disco; o `__cxa_throw` de lá ainda aborta.
   Plano: com a dylib sempre na imagem, tirar o caminho de reserva ou fazê-lo chamar o motor de exceção real.
6. **Unwind `-O1`/`-O2`.** Bateria de casos (frame pointer omitido, `-fomit-frame-pointer`, funções folha, tail call,
   `alloca`, sinais, `__builtin_unwind_init`, catch através de C, `std::rethrow_exception`, `std::nested_exception`,
   exceção estrangeira ObjC↔C++) em x86_64 e arm64, no host e com Mach-O real.
7. **`qsort_r` O(n²)** e outras pendências de `docs/libsys-pendencias.md` §3 que são libc.
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
