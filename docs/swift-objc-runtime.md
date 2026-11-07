# Runtime Swift com interoperabilidade ObjC (binários Swift da Apple)

Estado (2026-11-07): **fases 1 e 2 prontas; na fase 3, a stdlib e os overlays pequenos estão prontos** (testes Swift
13/13, 18/18, Dispatch 14/14 e _Concurrency 9/9); da fase 3 só falta UniformTypeIdentifiers, que depende do Foundation.
O resto em plano. Ponto de partida: o Postico 2.3.5 (9817)
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

### 2. Runtime Swift com `SWIFT_OBJC_INTEROP=1` (`tools/build-swift-runtime-objc.ps1` → `build/swift/objc/runtime`)

**Pronta (2026-11-07)**, com 89 objetos. Script próprio, separado do `tools/build-swift-runtime.ps1` da outra sessão.

- Os `.mm` do runtime e dos stubs são compilados como Objective-C++ de verdade (`-x objective-c++
  -fobjc-runtime=macosx-14.0`): `SwiftObject`, `SwiftValue`, `ErrorObject`, `ReflectionMirrorObjC`,
  `ObjCRuntimeGetImageNameFromClass`, `FoundationHelpers`, `OptionalBridgingHelper`, `SwiftNativeNSObject`,
  `Availability`. Também entra o `SwiftNativeNSXXXBase.mm`, que o script gera com `utils/gyb.py` e o Python do toolchain.
- Os cabeçalhos `objc/*.h` vêm do zig (`any-darwin-any`). Para Foundation e CoreFoundation há versões mínimas só para
  o runtime em `userland/sdk/objc/include`; o Foundation dos apps (`userland/sdk/include`) e o CF do importador
  conflitam com os do zig.
- O libSystem ganhou `_objc_rootAutorelease`, `_dyld_is_objc_constant` (sempre NÃO, porque o mOS reescreve as CFString
  constantes) e `os_system_version_get_current_version` (14.0.0, igual ao `kern.osproductversion`).

### 3. Stdlib e overlays com `-enable-objc-interop` (`tools/build-swift-full-objc.ps1` → `build/swift/objc`)

**Stdlib pronta (2026-11-07)**: 259 fontes, 7,3 MB, em ~5 min. As flags são as da stdlib da Apple
(`-disable-objc-attr-requires-foundation-module`, `SWIFT_OBJC_INTEROP=1`). Os overlays continuam pendentes.

- **Teste:** `tools/link-swift-objc.ps1` (ligação estática: app + stdlib + runtime com ObjC) com
  `userland/tests-swiftobjc/swift/main.swift` → **13/13** no harness. A libobjc vê `_TtC9SwiftObjc5Caixa` com
  superclasse `_TtCs12_SwiftObject`, `objc_retain`/`release` chegam ao refcount Swift, `-description` responde,
  a referência fraca ObjC é zerada no `deinit`, e casts de `AnyObject`, `String` e `Dictionary` funcionam.
- **Visto no teste:** `+[NSString newTaggedNSStringWithASCIIBytes_:length_:]` não existe no `foundation.c`
  (a ponte de `String` pede e recebe nil). Fica para a fase 5.
- Com `SWIFT_RUNTIME_OS_VERSIONING` (como na Apple), a stdlib exporta os **584/584** símbolos que o Postico importa da
  `libswiftCore`. Ela chama `__isPlatformVersionAtLeast`/`__isPlatformOrVariantPlatformVersionAtLeast` do compiler-rt,
  que agora estão em `objc.c` (macOS 14.0.0).

**Overlays (2026-11-07)**: `tools/build-swift-overlays-objc.ps1` → `build/swift/objc/overlays`. Fontes nossas em
`userland/sdk/swift-objc/<Módulo>`, com os nomes e layouts do SDK da Apple. Cada um sai com
`-module-link-name swift<Módulo> -autolink-force-load` (o `_swift_FORCE_LOAD_$_swift<Módulo>` que os binários importam).

- "SDK": cabeçalhos Darwin do zig copiados para `build/swift/objc/sdk/include`, mais `userland/sdk/swift-objc/sdk/module.modulemap`
  com os módulos clang `Darwin`, `ObjectiveC` (`umbrella "objc"` + `AvailabilityMacros.h`), `os` (um `os/log.h` próprio no
  formato OS_OBJECT do Swift: `os_log_t` = classe `OS_os_log`, `os_log_type_t` = `OSLogType`) e `CoreGraphics` (o
  cabeçalho do SDK do mOS com `-DMOS_SWIFT_OBJC`, que desliga os nomes `MOSCG*` do caminho sem ObjC). O `CoreFoundation`
  vem dos cabeçalhos do importador.
- `ObjectiveC`: `ObjCBool`, `Selector`, `NSZone`, `autoreleasepool`, `NSObject` Equatable/Hashable/CVarArg e as pontes
  BOOL/SEL que o compilador chama. `Darwin`: gerado do `MOSDarwin.swift` (errno, POSIXErrorCode...). `CoreFoundation`:
  `_CFObject`. `CoreGraphics`: `CGFloat` (gerado do `CGFloat.swift` do OpenSwiftUI), Codable de CGPoint/CGSize/CGRect
  e `CGRect(dictionaryRepresentation:)`. Na Apple o `CGFloat` mora na libswiftCoreFoundation, mas aqui isso não
  importa: o carregador resolve pela busca plana. `os`: `OSLog`/`OSLogType`, `os_log` (erro e falha vão para o stderr).
  `Observation`: o OpenObservation (MIT) com o nome da Apple. `_RegexParser`/`_StringProcessing`/`_CUnicode`:
  swift-experimental-string-processing 6.4.2. Vazios (só o FORCE_LOAD): `_Builtin_float`, `simd`, `XPC`, `QuartzCore`,
  `Metal`, `IOKit`, `OSLog`, `Spatial`, `CoreAudio`, `CoreImage`, `CoreMIDI`, `QuickLookUI`.
- **Postico:** todos os símbolos que ele importa de `libswiftCore`, `ObjectiveC`, `CoreFoundation`, `Darwin`, `os`,
  `CoreGraphics`, `Observation`, `_StringProcessing` e dos 12 módulos vazios existem com os nomes mangled da Apple.
- **Teste:** `userland/tests-swiftobjc/nsobject/main.swift` (subclasses Swift de NSObject, `@objc`, `#selector`, `isKind`,
  `==`/`Set` via `isEqual`/`hash`, `deinit`, `autoreleasepool`, Selector, ObjCBool) → **18/18**. O teste das fases 2–3
  continua em 13/13.
- **Dispatch (2026-11-07): 38/38.** O módulo clang `Dispatch` usa os cabeçalhos reais do macOS que o zig traz (`dispatch/*.h`,
  já com `DISPATCH_SWIFT_NAME` e OS_OBJECT do Swift). Com isso `DispatchQueue` é `So17OS_dispatch_queueC` e
  `DispatchSourceRead` é o protocolo `OS_dispatch_source_read`, como na Apple. Na cópia, o script isola a declaração
  `dispatch_workloop_set_os_workgroup`: no importador Swift o `os_workgroup_t` não fica visível em modo de módulos,
  embora no clang puro fique. O overlay (`userland/sdk/swift-objc/Dispatch/Dispatch.swift`, nosso) tem DispatchQoS,
  DispatchWorkItemFlags, DispatchWorkItem, DispatchTime/WallTime/TimeInterval e as extensões de
  DispatchQueue/Group/Semaphore/Source/SourceProtocol/SourceTimer, sobre as funções C do `dispatch.c` via `@_silgen_name`.
  O `sync` usa `dispatch_sync_f`, com a closure na pilha: o `dispatch_sync` do mOS copia o bloco, e o Swift
  acusaria fuga de closure não-escapável. O libSystem ganhou `swiftobjc.c`, com as classes `OS_object`,
  `OS_dispatch_*` e `OS_os_log` (o código cliente referencia `_OBJC_CLASS_$_OS_dispatch_queue`), e
  `_availability_version_check`, que o compiler-rt dos binários chama.
- **Teste:** `userland/tests-swiftobjc/dispatch/main.swift` → **14/14** (fila serial/concorrente, sync com valor e erro,
  global().async + semáforo, DispatchGroup async/wait/notify, DispatchWorkItem e cancel, DispatchTime, asyncAfter).
- **Limite conhecido:** os objetos do dispatch do mOS ainda não têm as classes `OS_*` no isa (o 1º word é um "magic"). O
  Swift só os passa adiante, e retain/release da libobjc não fazem nada com eles. Uma mensagem ObjC a um deles,
  ou `type(of:)`, quebraria.
- **_Concurrency (2026-11-07): 30/30.** `tools/build-swift-concurrency-objc.ps1` compila o fonte oficial
  (`stdlib/public/Concurrency`, 22 C++ + 78 Swift) com o executor do Darwin, `SWIFT_CONCURRENCY_USES_DISPATCH` e as
  versões reais do `availability-macros.def`, que os `@backDeployed` exigem. Os tipos saem com o nome de módulo `Swift`
  no ABI, como na Apple. Foi preciso baixar `include/swift/Concurrency/Actor.h` (swift 6.4.2), que faltava no recorte local.
  O `ThreadingError.cpp` do Concurrency vai para `overlays/dylib-only`: na ligação estática ele duplicaria o do runtime.
  - **Executor global do mOS** (`userland/sdk/swift-objc/Concurrency/mos_executor.cpp`): instalado pelos ganchos do
    próprio Concurrency (`swift_task_enqueueGlobal*_hook`), com 4 threads de trabalho e uma lista de prazos. Assim não
    é preciso dar threads às filas globais do `dispatch.c`. O MainActor continua na fila principal do dispatch.
  - **libSystem** (`swiftobjc.c`): `dispatch_async_swift_job` (roda o job pelo `VTableInvoke` da DispatchClassMetadata,
    no offset 48), `dispatch_main`, `dispatch_assert_queue[_not]$V2`, `voucher_*` (sem vouchers).
  - **Dispatch do mOS mais perto do macOS:**
    - `dispatch_time` agora é absoluto (ns na base do `mach_absolute_time`); antes era relativo, e os prazos absolutos
      do Concurrency viravam "atraso = uptime".
    - `dispatch_after` em fila não principal espera numa thread própria; antes rodava na hora.
    - Fontes TIMER funcionam (thread por fonte, repetição, cancel), e o handler `_f` recebe o `dispatch_set_context`.
    - `dispatch_semaphore_wait` bloqueia de verdade, com prazo; antes devolvia "tempo esgotado" na hora.
    - A fila principal, depois de usada, segura o `CFRunLoopRun` da thread principal como fonte, igual ao port do
      libdispatch; o MainActor do Swift conta com isso.
  - **Teste:** `userland/tests-swiftobjc/concurrency/main.swift` → **9/9**, estável em 3 execuções (async, Task.value,
    actor, withTaskGroup, continuations, cancel, MainActor.run, Task.sleep). Regressão: dispatch 14/14, nsobject 18/18,
    swiftobjc 13/13, swiftobjctest 23/23, objcrttest 25/25, deps do OpenSwiftUI TUDO OK.
  - **Não testado:** VS Code/Chrome no QEMU com as mudanças do dispatch acima (semáforo que bloqueia, `dispatch_time`
    absoluto, run loop principal que não termina mais por "sem fontes" depois que a fila principal é usada).
- **Falta:** `UniformTypeIdentifiers` (9), que fica para a fase 5 porque usa `URLResourceValues` do Foundation.

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
