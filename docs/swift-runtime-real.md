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

## Foundation Swift real (`tools/build-swift-foundation-objc.sh`)

Porte para Linux do `.ps1` de mesmo nome; o `tools/build.sh` o chama logo depois
do runtime. Sai em `build/swift/objc/dylib/usr/lib/swift`:

| Dylib | Conteúdo |
|---|---|
| `libswiftFoundation.dylib` (9,6 MB) | FoundationEssentials como módulo `Foundation` (292 fontes com os de `userland/sdk/swift-objc/Foundation`), `_FoundationRefs`, `_FoundationCShims`, Synchronization, swift-collections 1.7.3 |
| `libswift_StringProcessing.dylib`, `libswift_RegexParser.dylib` | swift-experimental-string-processing 6.4.2 (Regex; o FoundationEssentials importa 37 símbolos dele) |
| `libswiftUniformTypeIdentifiers.dylib` | `UTType.swift`, que só compila com o módulo Foundation acima |
| `libswiftObservation.dylib` | OpenObservation (MIT, commit `b030e44d6471`) com o nome do módulo da Apple |

Fontes fixadas por SHA-256 no script (baixa se faltarem). O swift-foundation é o
commit `d53ce9ce2a15` do main: a tag 6.4.2 não tem `CharacterSet` nem
`_FoundationRefs` no Essentials. Esse commit já traz `trimmingCharacters(in:)`,
`components(separatedBy:)`, `rangeOfCharacter(from:…)` e o percent encoding em
`StringProtocol`; as cópias do `StringAPI.swift` saíram (mesmo nome mangled).

Diferenças do caminho do Windows, todas no script:
- `-Xcc -nostdlibinc`: sem isso o `<threads.h>`, que o Darwin não tem, vinha do
  `/usr/include` do Linux.
- Cópia do `_FoundationCShims` com os `kOSThermalPressureLevel*` trocados pelos
  valores do cabeçalho público: o cabeçalho já entrou pelo módulo Darwin e o
  enum ficava invisível no módulo do shim.
- Cache de módulos próprio (`modcache-foundation`).

Stubs vazios alcançados (a métrica do `tools/meta/placar.py`), mesmo
`libSystem.mos`, sem e com as quatro dylibs acima, medidos em 15 nov. no host:

| App | Antes | Depois | Só Foundation Swift |
|---|---|---|---|
| Postico 2.3.5 | 209 | 9 | 181 → 0 |
| WhatsApp 2.26.45.13 | 1509 | 912 | 717 → 174 |
| OpenBot 0.66.0 | 9 | 1 | 0 → 0 |

O "depois" inclui a `libswiftos.dylib`: o `build-libswift-real.sh` compilava o
overlay `os` e não o ligava, e o `os_log(_:dso:log:_:_:)` dos apps caía no stub.
Os 9 do Postico: 6 de `View`/`ViewModifier` (`_makeView`, `_makeViewList`,
`_viewListCount`), que dependem do SwiftUI.framework (ainda sem porte Linux do
`build-swiftui-objc.ps1`), e 3 de `exception_raise*` (servidor MIG de exceções Mach).

No WhatsApp, os 174 que sobram de "Foundation" são em boa parte extensões de
outros módulos sobre tipos do Foundation (UIKit, Network, CloudKit, AVFoundation)
e API de classes ObjC que o SDK ainda não declara (`NSScanner`, `NSProgress`,
`NSBundle`, `URLSession` assíncrono). As que entraram nesta rodada, com o nome
mangled conferido contra o binário: `IndexSet.RangeView`/`shift`/`filteredIndexSet`,
`NSIndexSet: Sequence`, `URLResourceValues` (tamanhos, volume, `fileResourceIdentifier`),
`URL.checkResourceIsReachable`, `Data.range(of:options:in:)` (`NSDataSearchOptions`),
`Calendar` (nomes de dia/mês em pt e en), `Locale.components(fromIdentifier:)`,
`isoCurrencyCodes`, `FileHandle` (`readToEnd`, `read(upToCount:)`, `offset`,
`seekToEnd`), `NSRange` (faixas de String, `intersection`, `Hashable`, ponte com
`NSValue`), a ponte `CGPoint`/`CGSize`/`CGRect` ↔ `NSValue`, os inicializadores de
cópia de `NSArray`/`NSDictionary`/`NSSet`/`NSString` e `NSDictionary.Iterator`
como classe (era struct: o nome mangled levava V no lugar de C).

## Ligação em dois níveis contra o libswiftCore

O libSystem exporta 209 `swift_*` de outro runtime (o do caminho sem ObjC). Uma
dylib ligada só com `-undefined dynamic_lookup` deixa os `swift_*` em busca plana,
e o carregador os acha primeiro no libSystem. Na VM isso deu:

- `URL(string:)` no `libswiftFoundation`: proteção geral em
  `swift_bridgeObjectRetain` do libSystem;
- `Regex<AnyRegexOutput>` do `_StringProcessing`: `swift_getGenericMetadata` do
  libSystem devolveu nulo, e `__swift_instantiateConcreteTypeFromMangledName`
  caiu lendo `[nulo-8]`.

Agora o `libswiftCore.dylib` entra no link (vira `LC_LOAD_DYLIB` e ligação por
ordinal) no `link_one` do `build-libswift-real.sh`, no Combine e no Dispatch do
`build-swift-combine.sh`, nas overlays Swift do `build-frameworks-objc.sh` e em
todas as dylibs do `build-swift-foundation-objc.sh` (que ligam também Darwin,
ObjectiveC, CoreFoundation, CoreGraphics, Dispatch, `_Concurrency` e os). Conferir:
`llvm-objdump-21 --macho --bind --lazy-bind <dylib> | grep flat-namespace | grep -c ' _swift_'`
dá 0. O Combine.framework também entra no link do `libswiftFoundation` e do
`libswiftDispatch`, que importam `$s7Combine...`: sem ele, 23 símbolos ficavam
nulos ("símbolo(s) ausente(s)" no serial).

## Teste na VM (`tools/fdnprobe.sh`)

`tools/fdnprobe.sh <OutDir de um tools/build.sh> <saída>` gera uma imagem de
teste com dois serviços do launchd: `fdnprobe` (URL, URLComponents, Data, JSON,
plist, UUID, Date, Calendar, CharacterSet, IndexSet, NSRange, FileManager;
imprime `FDN-OK n/m`) e `rxprobe` (Regex; `RX-OK n/m`). Em 15 nov., QEMU com
KVM e OVMF, os dois boots antes da correção acima registraram as duas quedas
descritas. Depois da correção, o QEMU desta máquina deixou de dar boot até com
uma imagem feita só da esp do build, sem os testes: com KVM o OVMF para antes do
`BdsDxe`; com TCG o kernel diz "a partição de origem não é FAT com \mOS\" e
fica com 0 programas. O `fsck.vfat` passa na partição. Então **não há resultado
`FDN-OK` registrado** para esta versão. O QEMU precisa de `TMPDIR` fora do
`/tmp`: com `snapshot=on`, a sobreposição vai para lá, e a cota cheia também
produziu a mensagem acima.

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
