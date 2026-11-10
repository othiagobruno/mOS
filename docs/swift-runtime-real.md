# Runtime Swift real (STOR-SWIFT-REAL)

As `libswift*` de `tools/gen-libswift-stubs.c` continuam existindo só para o nome de
dylib que esta cadeia não chegou a compilar. O que está em
`build/swift/objc/dylib/usr/lib/swift/` e entra na imagem por `tools/build.sh` e
`build.ps1` é objeto Mach-O gerado pelo compilador, não um `ret`.

O PR #43 já está na `main`. Este ramo parte dela. `kernel/compat/macho.c` e
`kernel/proc/process.c` não foram editados.

## Versão

Swift **6.4.2** (tag `swift-6.4.2-RELEASE`), a mesma de `tools/build-swift-full-objc.ps1`.
A ABI estável é a do Swift 5 (`-enable-library-evolution`, `-swift-version 5`).
O VS Code 1.145.2 e o OpenBot 0.66.0 importam símbolos dessa ABI. Um stdlib mais
novo exporta os símbolos estáveis antigos; o que falta abaixo é overlay, não a
versão do `libswiftCore`.

## Como reproduzir

Host: Linux x86_64, zig 0.17.1 ELF, o `swift-frontend` do toolchain Ubuntu 24.04
do Swift 6.4.2. O `swiftc` do Linux não carrega a stdlib de
`x86_64-apple-macosx` sozinho (`unable to load standard library for target`).
O `-frontend -c -target x86_64-apple-macos26` emite Mach-O. O link é
`zig c++ -target x86_64-macos -shared -Wl,-undefined,dynamic_lookup`.

```
# toolchain e fonte (não vão para o git; .tools/ está no .gitignore)
# swift-6.4.2-RELEASE-ubuntu24.04.tar.gz em download.swift.org
# https://github.com/swiftlang/swift/archive/refs/tags/swift-6.4.2-RELEASE.tar.gz
# zig 0.17.1: https://ziglang.org/download/0.17.1/zig-x86_64-linux-0.17.1.tar.xz
# sha256 do zig: 3581863e3cbcfd0fce0efc682ce7e9e93da96668a977791d356a26d2a7e9b17a
```

1. `gyb.py` em `stdlib/public/core/*.gyb` com `-D CMAKE_SIZEOF_VOID_P=8` para
   `IntegerTypes`, `FloatingPointTypes` e os quatro `SIMD*`. Saída em
   `.tools/swift-src/gen/`.
2. Stdlib: os flags de `tools/build-swift-full-objc.ps1` (`-parse-stdlib`,
   `-enable-library-evolution`, `-DSWIFT_OBJC_INTEROP=1`, alvo
   `x86_64-apple-macos26`). Objeto: `build/swift/objc/swiftCore.o`.
3. Runtime C++/ObjC++: os flags de `tools/build-swift-runtime-objc.ps1`
   (`zig c++ -target x86_64-macos`). 89 arquivos, 0 erros nesta máquina.
4. `libswiftCore.dylib`: esse objeto mais `build/swift/objc/runtime/*.o`,
   `-install_name /usr/lib/swift/libswiftCore.dylib`.
5. Overlays Darwin, ObjectiveC, CoreFoundation, CoreGraphics, Dispatch, os e
   os módulos pequenos: flags de `tools/build-swift-overlays-objc.ps1`.
   O `-resource-dir` do toolchain Linux redefine os módulos dos shims; a
   compilação usa uma cópia do resource dir sem `shims/module.modulemap` e o
   `-I` do fonte.
6. `_Concurrency`: flags de `tools/build-swift-concurrency-objc.ps1` (C++ e Swift).
7. Metal: `userland/sdk/swift-objc/Metal/MTLBuffer.swift` mais
   `mos_mtl_modify.c` (`objc_msgSend` variádico o Swift 6.4 recusa).
8. `libswiftCoreGraphics`: o getter `CGColor.components` sai do dylib. Com
   `@_alwaysEmitIntoClient` o OpenBot não o achava.

`tools/build-libswift-real.sh` não refaz essa cadeia sozinho: se o dylib real
já existe, ele para. Sem toolchain, avisa e o `stage-libswift.sh` completa com
stub. `MOS_SWIFT_REBUILD=1` também só avisa — a receita é esta página, de
propósito, para não apagar um `libswiftCore` bom com um script pela metade.

## Registro `__swift5_*`

`userland/libsystem/swiftsect.c` continua guardando endereço e tamanho (o teste
de host lê isso). Quem entrega o descritor é o próprio runtime:
`ImageInspectionMachO.cpp` chama `objc_addLoadImageFunc` (em
`userland/libsystem/objc.c`), que registra `_dyld_register_func_for_add_image`.
O callback usa `getsectiondata` e consome types, protos, proto, replace e
acfuncs. reflstr, fieldmd, typeref, assocty e capture são lidos depois, pelo
mesmo `getsectiondata`. O `libswiftCore` desta build importa
`_objc_addLoadImageFunc` e `_getsectiondata`.

## O que o machocheck ainda marca como Swift

Medido com `--disco build/swift/objc/dylib/usr/lib/swift`, libSystem vazio,
os zips do inventário em `docs/compat-open-usage.md`:

- VS Code 1.145.2, sha256 `b6c60d84a8ffb85c5bb4106d6ff74773fec07e6c1a4bb944f8af68e3bfe5abbe`: **Swift 19**, todos em `libswiftFoundation.dylib` (`__DataStorage`, `Data`).
- OpenBot 0.66.0, sha256 `473091aac28ccab16be5921c9477f6deef8f2d9523d6c87962c99581821f817f`: **Swift 8**, todos em `libswiftUniformTypeIdentifiers.dylib` (`UTType`).

`libswiftCore`, `libswiftDispatch`, `libswift_Concurrency`, `libswiftObjectiveC`,
`libswiftCoreFoundation`, `libswiftMetal` e `libswiftCoreGraphics` fecharam em 0
nessa medição. Antes: 53 no VS Code e 230 no OpenBot.

`libswiftMetal.dylib` exporta `_$sSo9MTLBufferP5MetalE14didModifyRangeyySnySiGF`
(`MTLBuffer.didModifyRange`), implementado em `mos_mtl_modify.c`: `Range<Int>`
vira `NSRange` e chama `-didModifyRange:`. O `build-libswift-real.sh` recusa o
dylib se esse nome não estiver no binário.

O overlay `UniformTypeIdentifiers/UTType.swift` não compila sem o módulo
Foundation. O erro exato:

```
UTType.swift:175:11: error: cannot find type 'URLResourceValues' in scope
UTType.swift:8:66: error: value of type 'String' has no member '_bridgeToObjectiveC'
UTType.swift:9:71: error: type 'String' has no member '_unconditionallyBridgeFromObjectiveC'
```

`swift-foundation` e `swift-collections` 1.7.3 foram clonados em
`.tools/openswiftui/deps`, mas `tools/build-swift-foundation-objc.ps1` não rodou
até o fim nesta VM. Sem esse módulo os 19 + 8 símbolos não foram inventados.

## QEMU

Imagem descartável `/tmp/mos-swift-qemu.img` (160 MiB), OVMF, `-snapshot`, KVM
(`sudo -n`; sem sudo o QEMU recusou `/dev/kvm`). Não é hardware real.

O `swiftprobe` (print, classe, protocolo, genérico, `throw`/`catch`) foi lançado
pelo init. No log serial:

```
proc: iniciado swiftprobe (Mach-O, pid 3, 15236 KiB)
SWIFT-OK marca=7
launchd: com.mos.swiftprobe (pid 3) terminou com código 0
```

A linha `SWIFT-OK` apareceu duas vezes, no mesmo instante em que o launchd
registrou o job duas vezes. O processo saiu com 0. O marcador
`/Sistema/Biblioteca/stderr-serial` só foi posto nessa imagem de teste.

Para o `tools/build.sh` fechar com o zig 0.17.1 freestanding, o libSystem
precisou de três ajustes (bot Sistemas, não Kernel): o parâmetro `id` de
`cx_status_kernel_click` sombreava o tipo `id`; `kshim` ganhou `stdlib.h`,
`string.h`, `math.h` e `stdio.h`; `libc.h` inclui `stddef.h` e declara
`ssize_t` e `off_t`. Sem isso o erro era `stdlib.h file not found` e
`unknown type name 'size_t'`.

## Dependências de outros bots

- **Kernel:** nenhum diff em `kernel/compat/macho.c` nem `kernel/proc/process.c`.
  O carregador já resolve `/usr/lib/swift` (`mos_libswift_resolve`) e já chama
  `__mod_init_func`. Nada pendente de revisão do Kernel neste PR.
- **Sistemas:** o `libswiftCore` importa 161 símbolos que não define (libSystem,
  libobjc, `getsectiondata`). CoreFoundation, Foundation do sistema e o resto
  dos 844 do inventário não foram assumidos. `libswiftFoundation` e
  `libswiftUniformTypeIdentifiers` dependem do bot Sistemas / da cadeia
  `build-swift-foundation-objc.ps1`.
