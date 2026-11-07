# Runtime Swift com interoperabilidade ObjC (binários Swift da Apple)

Estado (2026-11-07): **fase 1 pronta**; fases 2–7 em plano. Ponto de partida: o Postico 2.3.5 (9817)
(`.tools/postico-x/`), o primeiro app Swift + SwiftUI da Apple que medimos símbolo a símbolo.

## Por que

Todo binário Swift compilado pela Apple para macOS usa a ABI **com** interop ObjC. Todo o Swift do mOS
(runtime em `tools/build-swift-runtime.ps1`, stdlib em `tools/build-swift-full.ps1`, Foundation e OpenSwiftUI
em `tools/build-openswiftui-deps.ps1`) é compilado com `SWIFT_OBJC_INTEROP=0` / `-disable-objc-interop` e
ligado **estaticamente** nos apps que nós mesmos compilamos. As diferenças que importam:

| Ponto | ABI da Apple (interop) | mOS hoje |
|---|---|---|
| Metadata de classe | começa com um `objc_class` (isa, superclass, cache, vtable, data com bit Swift) | sem a parte ObjC |
| Classe raiz | `Swift._SwiftObject`, uma classe ObjC raiz com -retain/-release | nenhuma |
| `AnyObject`, `unknownObject*` | pode ser objeto Swift ou ObjC | só Swift |
| `String`/`Array`/`Dictionary` | fazem ponte com `NSString`/`NSArray`/`NSDictionary` | sem ponte |
| Entrega | `/usr/lib/swift/libswift*.dylib`, `SwiftUI.framework`, overlay Swift dentro do `Foundation.framework` | `.a` estático |

O carregador (`kernel/compat/macho.c`) já resolve por ordinal e cai na busca plana e no libSystem.
Hoje o `machotest --report` do Postico lista **31 bibliotecas que faltam**, entre elas `SwiftUI.framework` e todas as `libswift*`.

## O que o Postico importa (x86_64)

| Biblioteca | Símbolos | Observação |
|---|---|---|
| libswiftCore | 584 | 111 funções `swift_*`, 25 delas da parte ObjC (`swift_unknownObject*` ×17, `swift_bridgeObject*`, `swift_dynamicCastObjC*`, `swift_getObjCClass*`) |
| Foundation | 418 | 243 do overlay Swift (`$s10Foundation…`); o resto é ObjC/C |
| SwiftUI | 238 | 203 já existem com o nome da Apple (`-module-abi-name SwiftUI` + library evolution); faltam 35 |
| AppKit | 201 | classes/constantes ObjC do `appkit.c` |
| libobjc | 55 | todas já estão no `objc.c`/`objcrt.c` |
| libswiftDispatch, _Concurrency, ObjectiveC, CoreFoundation, Observation, UTI e outras 15 | 139 | overlays pequenos |

## Restrições

- Licenças ([[referencias-cocoa-licencas]]): o fonte do Swift (Apache 2.0) e o Cocotron (MIT) podem ser
  copiados. O objc4 da Apple (APSL) e o GNUstep servem só de referência. Os cabeçalhos `<objc/runtime.h>` que os `.mm` do
  runtime precisam são escritos por nós (ou vêm do Cocotron), espelhando o `objc.h` do libSystem.
- O runtime Swift e o Dispatch são da sessão **"Compatibilidade DMG no OS"**. As fases 2–4 abaixo precisam ser
  combinadas com ela antes de começar (ou repassadas a ela).
- A variante sem ObjC continua existindo enquanto a nova amadurece: os apps de `userland/samples-swiftui` e os
  testes `tools/test-openswiftui-*.ps1` não podem quebrar.

## Fases

Cada fase termina com um teste no harness (`build/machotest.exe`) que roda sozinho.

### 1. libobjc do mOS preparada para classes Swift (`userland/libsystem/objc.c`, `objcrt.c`)

**Pronta (2026-11-07).**

- **Refcount:** objetos Swift não têm o `objhdr_t` do mOS. O `objc.c` mantém uma tabela de classes com RR próprio,
  formada pelas classes raiz que têm `-retain` no próprio método (o `Swift._SwiftObject`) e pelas subclasses delas.
  O teste é só pelo ponteiro da classe, porque `objc_retain` também recebe objetos Swift sem ObjC do resto do mOS.
  Para essas classes, `objc_retain`/`release`/`autorelease`/`retainCount` mandam a mensagem ao objeto, e as
  referências fracas perguntam `allowsWeakReference` e avisam com `_setWeaklyReferenced`. `objc_setAssociatedObject`
  chama `_noteAssociatedObjects`, e `objc_destructInstance` (que o `swift_deallocClassInstance` chama) zera as
  referências fracas e solta os objetos associados.
- **Realização de classes Swift:** não precisou mudar nada. A metadata Swift começa com os mesmos 5 words do
  `objc_class`, e o `cx_rw_t` do mOS fica no 2º word do `cache_t` (`_objc_empty_vtable`), que o compilador Swift também
  emite vazio. O `realize_ro` já ignorava os bits Swift de `data` e já deslizava as variáveis de offset dos ivars.
- **API que o runtime Swift chama:** `_objc_realizeClassFromSwift`, `objc_readClassPair`,
  `_objc_supportsLazyRealization`, os ganchos `objc_setHook_getClass` (usado por `objc_getClass` quando o nome não
  existe), `objc_setHook_getImageName`, `objc_setHook_lazyClassNamer`, `class_getImageName` (via `dladdr`),
  `objc_addLoadImageFunc` (sobre `_dyld_register_func_for_add_image`), `object_isClass`, `class_setSuperclass`,
  `objc_isUniquelyReferenced`, `_objc_setClassCopyFixupHandler` (guardado, sem uso) e as máscaras
  `objc_debug_isa_class_mask` (a do x86_64 da Apple, que preserva o isa puro) e `objc_debug_taggedpointer_mask` = 0.
  Ficou de fora `objc_absolute_packed_isa_class_mask`, que é um símbolo absoluto e só aparece num `assert` com `!NDEBUG`.
- **Teste:** `tools/build-swiftobjctest.ps1` + `userland/tests-swiftobjc/main.m`, com uma raiz no estilo do `SwiftObject`
  (malloc, contagem dentro do objeto, dealloc via `objc_destructInstance`) e uma subclasse → **23/23**.
  Regressão: `objcrttest` 25/25 e `test-openswiftui-deps` TUDO OK. Os apps grandes no QEMU (VS Code/Chrome)
  ainda não foram testados.

### 2. Runtime Swift com `SWIFT_OBJC_INTEROP=1` (`tools/build-swift-runtime.ps1`, nova saída `build/swift/objc/runtime`)

- Compilar os `.mm` como Objective-C++ de verdade (`-x objective-c++ -fobjc-runtime=macosx-14`):
  `SwiftObject.mm` (classe raiz `_SwiftObject`), `SwiftValue.mm` (`__SwiftValue`, a caixa de valores Swift
  como `id`), `ErrorObject.mm` (`__SwiftNativeNSError`, que precisa de NSError no `foundation.c`),
  `ReflectionMirrorObjC.mm` e `ObjCRuntimeGetImageNameFromClass.mm`. Hoje eles viram C++ sem ObjC.
- Cabeçalhos `objc/*.h` do mOS em `userland/sdk/objc/include` (novo).
- **Teste:** o runtime ligado a um programa C++ que cria uma classe Swift via metadata estática e passa ela
  por `objc_msgSend`.

### 3. Stdlib e overlays com `-enable-objc-interop` (`tools/build-swift-full.ps1`, variante)

- A stdlib com interop traz a ponte de coleções e strings (`_SwiftNativeNSString`, `_SwiftDeferredNSArray`
  etc. e os stubs `SwiftNativeNSXXXBase.mm`). As funções `_swift_stdlib_CFString*` que a ponte de `String`
  chama vêm do CF do `foundation.c`.
- Overlays pequenos que o Postico usa: `ObjectiveC`, `Darwin`, `Dispatch`, `CoreFoundation`, `_Concurrency`,
  `Observation`, `os`, `UniformTypeIdentifiers`, `_StringProcessing`, `simd` (só os símbolos importados).
- **Teste:** um app Swift nosso, compilado com `-enable-objc-interop` contra os cabeçalhos ObjC do mOS, com
  `class Foo: NSObject`, `String` → `NSString` → `String` e `[Any]` passado ao AppKit.

### 4. Entrega como dylibs Mach-O

- Ligar cada biblioteca como dylib x86_64 com o nome de instalação da Apple (`/usr/lib/swift/libswiftCore.dylib`,
  `/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI`…), com o linker do zig, e colocá-las no VFS da
  imagem do mOS e no `machotest`.
- Uma cópia só do runtime por processo: todas as dylibs dependem da `libswiftCore.dylib`, e ninguém liga
  `libswiftRuntime.a` de forma estática.
- **Teste:** `machotest --report` do Postico com as `libswift*` e o SwiftUI marcados `[ok]`.

### 5. Foundation Swift com ponte ObjC

- O overlay `$s10Foundation…` (243 símbolos no Postico) mora dentro do `Foundation.framework`. Recompilar
  FoundationEssentials + o Foundation do mOS com interop, com conformidades `_ObjectiveCBridgeable` sobre
  as classes do `foundation.c` (NSString, NSData, NSURL, NSDate, NSError, NSNumber, coleções).
- O `_MOSCFObject` (CF como objeto Swift, [[openswiftui-deps-port]]) deixa de ser necessário nesta variante:
  aqui os objetos CF do mOS são objetos ObjC de verdade.

### 6. SwiftUI.framework

- OpenSwiftUI e OpenSwiftUICore com `-enable-objc-interop`, nome ABI `SwiftUI` (já feito) e library evolution
  (já feito).
- `NSHostingView<Content>` e `NSHostingController<Content>` como subclasses Swift reais de `NSView` e
  `NSViewController` do `appkit.c`. Para isso o importador precisa dos cabeçalhos ObjC do subconjunto do AppKit
  (`userland/sdk/appkit/include`, novo), e as classes reaproveitam o host do `MOSApp.swift` (window server).
- Os outros 31 símbolos que faltam: `ScrollView`, `TextEditor`, `List`, `TabView`/`tabItem`, `popover`,
  `glassEffect`/`Glass`, `Color(nsColor:)`, `Image(_:bundle:)` etc.

### 7. Postico de ponta a ponta

- As bibliotecas não-Swift que faltam: libz, libc++ (a do `libcxx.c` exposta como `libc++.1.dylib`), Security/
  SecurityInterface, CoreData, CFNetwork, IOKit, AudioToolbox, QuickLookUI, Metal (já existe como
  framework do mOS), WebKit (provavelmente só stub).
- **Teste final:** a janela de conexão do Postico aparece no harness (`MACHOTEST_DUMP`).

## Ordem e dependências

As fases 1 → 2 → 3 → 4 formam uma cadeia, e cada uma só pode ser testada com a anterior pronta. As fases 5 e 6 dependem da 3, e podem andar em
paralelo uma com a outra. A 7 pode começar já, porque libz e libc++ não dependem de nada disto.

## Riscos

- **Duas variantes do stack Swift** dobram o tempo de build. Se a variante com interop pegar, dá para migrar os
  apps do mOS para ela e aposentar a sem ObjC.
- **O compilador Swift do Windows gerando código Darwin com interop** sem o SDK da Apple: precisa dos nossos
  cabeçalhos ObjC e de um `-sdk` mínimo. Validar logo no começo da fase 3, com um arquivo de teste.
