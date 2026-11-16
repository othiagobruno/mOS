# libm do musl no mOS

Fonte: musl 1.2.5 (`https://musl.libc.org/releases/musl-1.2.5.tar.gz`, sha256
`a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4`), licença MIT (`COPYRIGHT`).

Copiado sem mudança: `src/math` (genérico, `x86_64/` e `aarch64/`), `src/internal/libm.h`, `arch/generic/fp_arch.h` e
`arch/aarch64/fp_arch.h`. Mudanças marcadas com "mOS:" no fonte: `src/math/tgammal.c`, `src/math/powl.c` e
`src/math/x86_64/exp2l.s` (ver `docs/runtime-c-cxx.md`).

Do mOS (`mos/`): cabeçalhos mínimos (`include/`: `math.h` do musl com os FP_* da Apple, `fenv.h` com as constantes da
Apple, `features.h`, `endian.h`, `bits/alltypes.h`, `atomic.h`), `aliases.c` (os `weak_alias` como funções, porque o
triplo do Darwin não tem alias), `apple.c` (nomes que só a libm da Apple exporta) e `srcs.sh` (lista de fontes por
arquitetura). As funções de `<fenv.h>` estão em `userland/libsystem/fenv.c`.

Compilação: `tools/build-libm-musl.sh`; teste: `tools/libm_hosttest.sh`.
