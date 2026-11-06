# Dependências do OpenSwiftUI no mOS

Estado do porte das bibliotecas do [OpenSwiftUIProject](https://github.com/OpenSwiftUIProject) (licença MIT) para o
Swift completo do mOS (`build\swift\full`, sem interoperabilidade Objective-C, alvo `x86_64-apple-macos26`).

```powershell
.\tools\build-openswiftui-deps.ps1     # compila tudo em build\swift\openswiftui\<Módulo>\lib<Módulo>.a
.\tools\test-openswiftui-deps.ps1      # liga um programa de teste e roda no build\machotest.exe ("TUDO OK")
```

| Módulo | O que é | Estado |
|---|---|---|
| Foundation (`userland\sdk\openswiftui\swift\Foundation`) | Foundation mínimo do mOS: CGFloat, CGPoint/Size/Rect, NSObject, Data, String(format:), NSAttributedString, NSRange, NSNumber, CharacterSet, helpers de CF | compila e testado |
| CoreFoundation (`userland\sdk\openswiftui\importer`) | módulo **C** com a API do CF; implementação em `libMOSCFRuntime.a` | compila e testado |
| MOSCFRuntime (`src\cf_mos.cpp` + `swift\SwiftFoundation`) | CFString/Number/Boolean/Array/Dictionary/Data, `_CFRuntimeRegisterClass`, CFSTR | compila e testado |
| Compute | motor de grafo (reimplementação do AttributeGraph) | compila e testado (Rule em Swift) |
| OpenAttributeGraphShims | o que o OpenSwiftUI importa; usa o Compute (`-D OPENATTRIBUTEGRAPH_COMPUTE`) | compila e testado |
| OpenAttributeGraph | o grafo do upstream ainda é `// TODO` (`OAGGraphCreateAttribute/GetValue/SetValue`) | compila, **não usar como motor** |
| OpenRenderBox (C++) | paths, cores, animação | compila; `ORBPathContainsPoints`/`ORBPathCopyCGPath` são TODO no upstream |
| OpenObservation | `ObservationRegistrar`, `withObservationTracking` (sem o plugin de macros: `@Observable` não) | compila e testado |
| OpenCoreGraphics / OpenQuartzCore | tipos CG/CA em Swift | compila e testado |

No libSystem entraram `coregraphics.c` (CGPath, CGAffineTransform, CGColor/ColorSpace, geometria), `cxxabi.c`
(operator new/delete, `__cxa_guard_*`, `std::call_once`), `darwinmath.c` (`__sincos_stret`) e `commoncrypto.c`
(CC_SHA1/CC_SHA256, `uuid_*`, `backtrace`).

## Regras do CF (importante para quem escreve código de plataforma)

* **Sem ObjC, o Swift retém qualquer tipo CF com `swift_retain`.** Por isso todo objeto CF visível ao Swift é um
  objeto Swift: instância da classe `SwiftFoundation._MOSCFObject`, criada por `libMOSCFRuntime.a`. Objetos CF do
  libSystem (NSString, CGPath... do mOS) são objetos ObjC e **não podem** chegar ao Swift como tipo CF.
* Tipos de run loop chegam ao Swift como ponteiros opacos (`CFRunLoop = MOSCFRunLoopRef`), sem ARC.
* `CFSTR("...")` do CoreFoundation do swift-corelibs gera um objeto imortal com `isa = SwiftFoundation._NSCFConstantString`.
* **`x as? CFString` (cast para tipo CF) sempre falha** no runtime sem ObjC, como no Linux. Use `CFGetTypeID` ou
  `_mosAsCF(obj, CFStringGetTypeID(), CFString.self)`.
* Não existe `as CFString` / `as CFDictionary` (bridging). Use `string.cfString`, `String(cfString)`,
  `dict.cfDictionary`, `[CFString: Any](cfDict)`, `CFNumber.make(_:)`, `.doubleValue`.
* Tipos CF novos (ex.: CTFont) se registram com `_CFRuntimeRegisterClass` e são criados com `_CFRuntimeCreateInstance`.

## Carregador Mach-O: rebase de símbolos absolutos (corrigido em 2026-11-06)

Objetos estáticos que o otimizador do Swift grava no binário (arrays/strings constantes, símbolos `...Tv_r`) têm o
contador = `ptrtoint(@_swiftImmortalRefCount)`, símbolo **absoluto** `0x80000004ffffffff`. O linker do zig o codifica
como *rebase* (`DYLD_CHAINED_PTR_64`, target `0x4ffffffff`, high8 `0x80`), e o carregador somava o slide: o contador
virava `0x8000400400ffffff` e o primeiro `swift_release` do objeto travava em `doDecrementSlow`.

`kernel/compat/macho.c` agora trata rebase cujo alvo cai fora de `[vmin, vmax]` como valor absoluto, sem slide, tanto
em `run_chained` (chained fixups) quanto em `run_rebase` (dyld_info clássico). O `build\machotest.exe` foi recompilado
com a correção. Testado no QEMU (imagem de `.\build.ps1 -OutDir build-osui`): no Terminal do mOS, um binário com arrays e strings constantes soltos mil vezes e o `depstest` completo terminam em "TUDO OK".

Limitação restante (linker do zig): quando o otimizador dobra a *leitura* do campo de contador de um objeto estático
conhecido, o código carrega `&__swiftImmortalRefCount` via `X86_64_RELOC_GOT_LOAD`, e o zig relaxa isso para
`leaq sym(%rip)`. Num símbolo absoluto isso dá `0xffffffff + slide`. O objeto em si continua certo; só esse valor
dobrado sai errado. Nas bibliotecas e no `swiftCore.o`, todas as referências ao símbolo são `RELOC_UNSIGNED` (dados),
que o carregador trata.

## CFRetain/CFRelease

`libMOSCFRuntime.a` define `CFRetain`, `CFRelease` e `CFGetRetainCount` para o app inteiro (escolha pelo isa: objeto CF
do mOS → `swift_retain`/`swift_release`; o resto → `objc_retain`/`objc_release`, como o libSystem). Código C não
precisa do prelude para reter objetos CF corretamente.

## Tipos CG (CGFloat, CGPoint, CGRect...) — uma só declaração

* `CGFloat` mora no módulo **Swift `CoreGraphics`** do mOS (`userland\sdk\openswiftui\swift\CoreGraphics`). O importador
  clang só mapeia o typedef C `CGFloat` para um tipo Swift se existir um módulo Swift chamado `CoreGraphics`; o
  Foundation o reexporta. Por isso `canImport(CoreGraphics)` é verdadeiro, e o pré-processamento dos fontes upstream
  troca `canImport(CoreGraphics)` por `false` (como `os(macOS)`), mantendo o caminho não-Darwin.
* `CGPoint`/`CGSize`/`CGRect`/`CGRectEdge` são as structs C de `importer\CoreFoundation\CFCGTypes.h` (mesma guarda do
  swift-corelibs); o Foundation só as estende. Os cabeçalhos C das deps (OpenRenderBox com `OPENRENDERBOX_CF_CGTYPES`)
  ficam com essas mesmas declarações.
* Para o Swift (`__swift__`), os demais tipos CG em C têm nomes `MOS...` (`MOSCGAffineTransform`, `MOSCGPathRef`,
  `MOSCGColorRef`...): o OpenCoreGraphics define os seus em Swift, e os `CG*Ref` do libSystem são objetos ObjC, que
  chegam ao Swift como ponteiros opacos (sem ARC).

## Importador clang (userland\sdk\openswiftui\importer)

Os cabeçalhos de libc do macOS que vêm com o zig não funcionam no modo de módulos do importador do swiftc. O
importador usa: o módulo C `CoreFoundation` do mOS, `CoreGraphics\*.h` (repassam ao CoreGraphics do SDK sem expor os
cabeçalhos ObjC de `userland\sdk\include`), e versões mínimas de `stdio.h`, `pthread.h` e `TargetConditionals.h`.

## OpenSwiftUI_SPI

Compilado com um include próprio (`build\swift\openswiftui\OpenSwiftUI_SPI\include`): só o módulo `OpenSwiftUI_SPI`
(os `*_Private` do upstream, como `CoreText_Private`, ficam de fora porque o mOS tem módulos Swift com esses nomes) e o
mOS como plataforma não-Darwin (`-DOPENSWIFTUI_MOS` no `OpenSwiftUITargetConditionals.h`). Os `.c` de Overlay/Util vão
em `libOpenSwiftUI_SPI.a`; os `.m` (Objective-C) e `Shims\` (interfaces privadas da Apple) ficam de fora.
