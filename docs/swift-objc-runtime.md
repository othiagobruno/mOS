# Runtime Swift com interoperabilidade ObjC (binários Swift da Apple)

Estado (2026-11-07): **fases 1 e 2 prontas; na fase 3, a stdlib e os overlays pequenos estão prontos** (testes Swift
13/13, 18/18, Dispatch 14/14 e _Concurrency 9/9); da fase 3 só falta UniformTypeIdentifiers, que depende do Foundation.
**Fase 4 pronta para as libswift** (22 dylibs com os nomes da Apple; os testes passam carregando do disco). O resto em plano. Ponto de partida: o Postico 2.3.5 (9817)
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

**Pronta para as libswift (2026-11-07).** O SwiftUI.framework fica para a fase 6.

- `tools/link-swift-dylibs-objc.ps1` → `build/swift/objc/dylib/usr/lib/swift/*.dylib`: 22 dylibs x86_64 ligadas pelo zig,
  com `-install_name /usr/lib/swift/libswift<X>.dylib` e `-undefined dynamic_lookup`. A `libswiftCore` leva a stdlib e
  o runtime inteiros (uma cópia por processo). As outras resolvem contra ela pela busca plana do carregador do mOS,
  que lê do VFS as dylibs de caminho absoluto que o libSystem não substitui.
  A libc++ do zig entra estática em cada uma. Como na Apple, o FORCE_LOAD do `_Builtin_float` fica na `libswiftDarwin`.
- `tools/link-swift-objc.ps1 -Dylib` liga um teste contra as dylibs, como um binário da Apple. Roda com
  `build/swift/objc/dylib` como raiz do VFS do harness.
- **Teste:** concurrency 9/9 (23 imagens carregadas do disco), nsobject 18/18 e dispatch 14/14 no modo dylib.
- **Postico** (raiz `build/postico-root` = `usr/lib/swift` + `Applications/Postico 2.app`): as `libswift*` resolvem.
  O carregador para nas bibliotecas que o mOS ainda não tem: `libz.1.dylib`, `libc++.1.dylib`, SecurityInterface,
  WebKit, Metal (o framework do mOS existe na imagem, não na raiz do harness), AudioToolbox, CoreData, QuickLookUI,
  **SwiftUI** (fase 6) e `libswiftUniformTypeIdentifiers` (fase 5).
- **Falta:** pôr as dylibs na imagem do mOS (`build.ps1`) e o `machotest --report` aceitar uma raiz de VFS.

### 5. Foundation Swift com ponte ObjC

O Postico importa **316** símbolos Swift do Foundation, em três blocos: (1) as pontes `_ObjectiveCBridgeable`;
(2) os tipos-valor (URL 52, Data 25 com código *inlinável* — `Data._Representation`, `__DataStorage` —, JSON e
PropertyList, UUID, URLComponents, Calendar, Locale, FormatStyle...); (3) as extensões Swift sobre classes ObjC
(StringProtocol, NSError/LocalizedError/CustomNSError, KVO `observe`, `NSURLSession.data(for:)`, NSCoder, NSLog). As 30
classes ObjC que as pontes precisam já existem no libSystem do mOS.

**Marco A — pontes centrais (pronto, 2026-11-07).** Overlay `Foundation` em `userland/sdk/swift-objc/Foundation`
(`Bridging.swift`, `Literals.swift`, `Misc.swift`), compilado pelo `tools/build-swift-overlays-objc.ps1` sobre um módulo
clang `Foundation` próprio (`userland/sdk/swift-objc/sdk/Foundation/Foundation.h`). Esse cabeçalho declara, no formato
da Apple, só o que o overlay usa; os apps ObjC do mOS continuam com `userland/sdk/include/Foundation`.
- Pontes de String/Substring, de todos os inteiros, Double/Float/CGFloat/Bool ↔ NSNumber, e de Array/Dictionary/Set ↔
  NSArray/NSDictionary/NSSet; literais de NSString/NSNumber/NSArray/NSDictionary; iteração por `NSFastEnumerationIterator`;
  `NSNotFound`, `NSLog`, `String(format:)`, `String: CVarArg` e `init(truncating: NSNumber)`.
- **Diferente da Apple, de propósito:** a ponte para ObjC cria objetos do Foundation do mOS (cópia), em vez de devolver o
  armazenamento Swift como subclasse de NSString/NSArray. Os métodos do `foundation.c` leem a representação interna
  deles, e assim só recebem objetos que entendem. Na volta, a leitura é pelos métodos públicos.
- O `NSNumber` do mOS só tem `initWithInt:`/`initWithInteger:`/`initWithDouble:`; o overlay mapeia os outros tipos
  para eles (Bool vira 0/1).
- **Teste:** `userland/tests-swiftobjc/foundation/main.swift` → **23/23** (pontes nos dois sentidos, casts por
  `Any`/`AnyObject`, literais, for-in). **32 dos 316** símbolos do Postico batem.

**Marco B — tipos-valor do swift-foundation (em andamento, 2026-11-07).** `tools/build-swift-foundation-objc.ps1` compila
para `build/swift/objc/overlays`, com interop e como pacote no macOS: `Darwin`/`os` de verdade, sem as reescritas
MOSDarwin/MOSos. Entram:
- Synchronization, swift-collections 1.7.3 (InternalCollectionsUtilities, SpanPreview, BasicContainers, DequeModule,
  OrderedCollections, _RopeModule), `_FoundationCShims` (C) e `_FoundationRefs`. O `_FoundationRefs` vai sem library
  evolution, porque o código dele não a aceita.
- **FoundationEssentials: 280 fontes, 9,3 MB**, com library evolution (os binários importam *dispatch thunks* como
  `JSONDecoder.decode`), `-module-abi-name Foundation` (os tipos saem `$s10Foundation…`) e `-D DATA_LEGACY_ABI`. É a
  representação de `Data` do ABI da Apple (`enum _Representation`, `RangeReference`), que o `Package.swift` liga nas
  plataformas Apple e que o código inlinável dos binários usa.
- O SDK ganhou os submódulos `Darwin.crt_externs/sysdir/uuid` e `MachO.dyld` (sem cabeçalho próprio), e o overlay `os`
  ganhou `Logger`/`OSLogMessage`.
- **Um módulo só, `Foundation`.** O FoundationEssentials (cópia preparada em `build/swift/objc/foundation-src`) e os
  fontes do mOS em `userland/sdk/swift-objc/Foundation/*.swift` compilam juntos, com `-import-underlying-module` sobre o
  módulo clang `Foundation`. Em módulos separados as pontes viravam extensões de outro módulo e o nome mangled ganhava o
  contexto de extensão (`AAE`), diferente do da Apple; e Foundation ↔ FoundationEssentials virava dependência circular.
  Dentro do próprio módulo `Foundation` o importador **não** faz a ponte NSString ↔ String nem NSDictionary ↔
  Dictionary, então os fontes do mOS usam os tipos ObjC explicitamente; e os wrappers `NS_TYPED_EXTENSIBLE_ENUM`
  (`URLResourceKey`, `NSNotification.Name`) ganham `Hashable` à mão (`TypedEnums.swift`).
- **Opções `NS_OPTIONS` (pronto):** módulo clang `_MOSNSOptions` (`sdk/Foundation/NSFoundationOptions.h`, também
  incluído pelo Foundation.h) com `NSDataReadingOptions`, `NSDataWritingOptions`, Base64, `NSStringCompareOptions` e as
  opções de bookmark, com os `swift_name` da Apple. A preparação troca os `#if FOUNDATION_FRAMEWORK` desses tipos por
  apelidos (`-D MOS_NS_OPTIONS`), põe `import _MOSNSOptions` em todo arquivo (MemberImportVisibility) e tira o
  `public struct URLResourceKey {}` do pacote, que escondia o tipo ObjC.
- **Pontes (pronto):** Data/URL/Date/UUID/Locale/TimeZone ↔ NSData/NSURL/NSDate/NSUUID/NSLocale/NSTimeZone
  (`ValueBridging.swift`). `AnyHashable(NSString)`/`AnyHashable(NSNumber)` viram o valor Swift
  (`_HasCustomAnyHashableRepresentation`), como na Apple, para chaves de `userInfo` voltarem iguais.
- **Tipos que o swift-foundation não tem (pronto):** `IndexSet`, `Notification` (+ ponte), `URLRequest` (+ ponte),
  `NSURLSession.data(for:delegate:)` async, `URLResourceValues` com `URL.resourceValues/setResourceValues`, bookmarks e
  escopo de segurança, e o KVO do Swift (`observe(_:options:changeHandler:)`, `NSKeyValueObservation`).
- **libSystem:** KVO de verdade no NSObject (`userland/libsystem/kvo.c`; antes `addObserver:forKeyPath:` era um stub
  vazio). O setter da chave é trocado na classe que o define por um trampolim que chama `willChangeValueForKey:` /
  `didChangeValueForKey:` em volta do original — sem trocar o isa, porque uma subclasse ObjC criada em tempo de execução
  não teria a vtable de uma classe Swift. Opções New/Old/Initial/Prior; setters com struct não são interceptados.
  Também: `NSNotification` passou a reter nome/objeto/userInfo, e os métodos do `NSUUID` (`initWithUUIDString:`...)
  passam a ser instalados (`cx_foundation_gaps`, chamado do `cx_swiftobjc_init`: no boot o CoreBluetooth subia antes
  do NSUUID existir).
- **Entrega (pronto):** `libswiftFoundation.dylib` (Foundation.o + `_FoundationRefs` + `_FoundationCShims` +
  Synchronization + coleções) pelo `tools/link-swift-dylibs-objc.ps1`. O carregador (`kernel/compat/macho.c`), quando
  uma imagem depende de `/System/Library/Frameworks/Foundation.framework/`, também carrega
  `/usr/lib/swift/libswiftFoundation.dylib` do VFS, se existir — os binários da Apple importam os símbolos Swift do
  Foundation pelo próprio `Foundation.framework`.
- **Testes:** `essentials` 21/21 e `foundation2` 19/19 (pontes, IndexSet, Notification, URLRequest,
  URLResourceValues, KVO), além das suítes anteriores (main.m 23, swift 13, nsobject 18, dispatch 14, concurrency 9,
  foundation 23).
- **Postico: 272 dos 316** símbolos batem (eram 32 com o marco A, 197 no começo do marco B). Os 44 que faltam são do
  marco C: erros (`CustomNSError`, `LocalizedError`, `_convertErrorToNSError`, `POSIXError`, `URLError`),
  `StringProtocol` sobre NSString, NSCoder, FileManager, UndoManager, variantes de `String(contentsOf:)`/
  `String(format:)`, e `IntegerFormatStyle`/`BinaryInteger.formatted` (FoundationInternationalization, depende de ICU).
- **WhatsApp:** 7.783 → **4.730** símbolos ausentes distintos com os dylibs Swift do mOS (`libswiftCore` 1.957 → 0,
  `_Concurrency` 190 → 0, `libswiftFoundation` 1.119 → 425, `libswiftDispatch` 122 → 28). Ver [compat-apps.md](compat-apps.md).
- **C — extensões sobre ObjC:** StringProtocol (sobre NSString), erros (NSError, CustomNSError, LocalizedError,
  `_convertErrorToNSError`, os ganchos do runtime em `ErrorObject.mm`), KVO, NSURLSession async, NSCoder, FileManager.
- **D — entrega:** `Foundation.framework` hoje conta como "do libSystem" no carregador, então o overlay Swift precisa
  entrar como imagem extra (p. ex. carregar `Foundation.framework/Versions/C/Foundation` do VFS quando existir) ou
  como `/usr/lib/swift/libswiftFoundation.dylib` puxada pela `libswiftCore`.
- O `_MOSCFObject` (CF como objeto Swift, [[openswiftui-deps-port]]) deixa de ser necessário nesta variante:
  aqui os objetos CF do mOS são objetos ObjC de verdade.

### 5b. Combine.framework (pronto, 2026-11-07)

O WhatsApp importa 283 símbolos do Combine. `tools/build-swift-combine-objc.ps1` compila o OpenCombine (MIT, já em
`.tools/openswiftui/deps`) como o módulo `Combine`, com interop e library evolution, e liga
`/System/Library/Frameworks/Combine.framework/Versions/A/Combine` em `build/swift/objc/dylib`. A cópia preparada do
fonte (`build/swift/objc/combine-src`):
- `OpenCombine.` → `Combine.`; `@_implementationOnly import COpenCombineHelpers` (o C++ do CombineIdentifier e da
  trava fica fora da interface; o `.o` vai para `overlays`, onde o link estático o pega).
- Os atributos de layout do Combine da Apple, que a library evolution exige para o código `@inlinable`:
  `@frozen` em `AnyPublisher`, `AnySubscriber`, `Subscribers.Demand` e `ImmediateScheduler…Stride`;
  `@_fixed_layout` nas caixas `@usableFromInline` (`PublisherBoxBase`, `ClosureBasedAnySubscriber`...).
- `Optional.Publisher` e `Result.Publisher` saem do invólucro `OCombine` do OpenCombine (que existe para não colidir com
  o Combine da Apple) e moram direto em `Optional`/`Result`, como na Apple — o nome mangled muda.
- O que o OpenCombine não tem, escrito para o mOS em `userland/sdk/swift-objc/Combine/CombineLatestMerge.swift`:
  `Publishers.CombineLatest`/`3`/`4`, `Merge`/`3`…`8`, `MergeMany` e os operadores `combineLatest`/`merge(with:)`.
  Demanda ilimitada para cima; para baixo, a demanda é respeitada (CombineLatest guarda só a combinação mais recente,
  Merge uma fila).
- **Teste:** `userland/tests-swiftobjc/combine/main.swift` → **13/13** (Just, Sequence, Subjects, combineLatest,
  merge, MergeMany, Optional/Result.Publisher, tryMap/catch, `@Published`/`ObservableObject`, cancelamento).
- **WhatsApp:** Combine 283 → **0** ausentes; total 4.730 → **4.444**.

**Dispatch, o resto do overlay da Apple (pronto, 2026-11-07).** O WhatsApp pedia 28 símbolos que faltavam no overlay
Dispatch do mOS. `userland/sdk/swift-objc/Dispatch/DispatchExtras.swift`: `DispatchSpecificKey` e
`get/setSpecific` (de instância e de classe), `concurrentPerform`, `DispatchSource.MemoryPressureEvent`/`FileSystemEvent`
e as fontes correspondentes (criadas, mas o mOS ainda não as dispara), `DispatchData` (sobre o `dispatch_data_t`, que no
mOS é um CFData) com a ponte ObjC, e `_dispatchPreconditionTest` (sempre verdadeiro). `Dispatch/Combine/DispatchScheduler.swift`:
`DispatchQueue: Scheduler` com `SchedulerTimeType`/`Stride`/`SchedulerOptions` — como na Apple, é o Dispatch que importa
o Combine; o arquivo só entra quando o `Combine.swiftmodule` existe, e o `build-swift-combine-objc.ps1` recompila o
Dispatch no fim. No libSystem:
- `dispatch_queue_set_specific`/`get_specific`/`dispatch_get_specific` de verdade (eram stubs), e a fila em execução
  por thread (`cx_dispatch_current`, também em `dispatch_get_current_queue`): o mOS roda os blocos na thread de quem
  chama, então a fila é registrada em volta de cada execução.
- **Os objetos do dispatch (filas, grupos, fontes, semáforos) têm isa ObjC** (`OS_dispatch_*`, de `swiftobjc.c`) no
  1º word, como no macOS; o magic foi para o 2º. Sem isso o runtime Swift lia o magic como classe ao abrir um
  existencial (`DispatchSourceTimer`) e caía. **Retain/release ObjC** deles (ARC do Chromium, Swift, -retain/-release,
  autorelease pool) vão para a contagem do dispatch.c: `cx_is_dispatch_object` (objc.c) reconhece o isa antes do
  `hdr()`, e `cx_object_free` os recusa — eles não têm `objhdr_t`, e o `hdr()` leria os 16 bytes de outra alocação.
  As classes ganham `-retain/-release/-retainCount/-autorelease` e `-dealloc` vazio (swiftobjc.c). Num app C sem seções
  ObjC o runtime não sobe e as classes ficam indefinidas, o que é inofensivo (ninguém lê o isa).
- **Testes:** dispatch 19/19, combine 16/16 (`receive(on:)`, `debounce` com `DispatchQueue`).
- **WhatsApp:** Dispatch 28 → **0**; total 4.444 → **4.415**.
- **QEMU (WHPX):** `userland/tests-dispatch/main.c` (filas, grupos, fontes, semáforos, `dispatch_after`, retain/release,
  `set/get_specific`, isa) roda no boot de teste (`build.ps1 -Tests`, chamado pelo `teste` como o posixteste): **19/19** e
  "teste: TODOS OS TESTES OK". O VS Code subiu (Code, Renderer, Helpers, workbench) com um libSystem que já tinha o isa.

### 6. SwiftUI.framework

- OpenSwiftUI e OpenSwiftUICore com `-enable-objc-interop`, nome ABI `SwiftUI` (já feito) e library evolution
  (já feito).
- `NSHostingView<Content>` e `NSHostingController<Content>` como subclasses Swift reais de `NSView` e
  `NSViewController` do `appkit.c`. Para isso o importador precisa dos cabeçalhos ObjC do subconjunto do AppKit
  (`userland/sdk/appkit/include`, novo), e as classes reaproveitam o host do `MOSApp.swift` (window server).
- Os outros 31 símbolos que faltam: `ScrollView`, `TextEditor`, `List`, `TabView`/`tabItem`, `popover`,
  `glassEffect`/`Glass`, `Color(nsColor:)`, `Image(_:bundle:)` etc.

### 7. Postico de ponta a ponta

**Bibliotecas não-Swift prontas (2026-11-07).** Com elas, o carregador só para no SwiftUI (fase 6) e na
`libswiftUniformTypeIdentifiers` (fase 5).

- **Carregador** (`kernel/compat/macho.c`, `libsystem_dylib_ok`): `libc++.1.dylib`/`libc++abi.dylib` (`cxxabi.c` e
  `libcxx.c`), `Metal`/`MetalKit` (`metal.c`) e `UniformTypeIdentifiers` (`UTType` em `vscode_media.c`) passam a contar
  como fornecidos pelo libSystem.
- **Frameworks Mach-O de verdade** (`tools/build-frameworks-objc.ps1`, fontes em `userland/frameworks/<Nome>`), no
  caminho da Apple em `build/swift/objc/dylib`; pastas `lib*` vão para `/usr/lib`:
  - **CoreData:** as classes (NSManagedObjectModel, NSPersistentStoreCoordinator, NSManagedObjectContext,
    NSEntityDescription, NSFetchRequest, NSManagedObject) e as constantes, com um armazenamento **só em memória** (sem
    SQLite nem `.momd`; fetch por entidade, sem predicados). Também o overlay Swift `NSManagedObjectContext.fetch`,
    compilado num módulo clang mínimo próprio.
  - **WebKit:** WKWebView (NSView vazia; carregamentos ignorados) e WKWebViewConfiguration.
  - **SecurityInterface:** SFCertificateTrustPanel responde "cancelar".
  - **QuickLookUI:** QLPreviewView vazia.
  - **AudioToolbox:** AudioServices (o alerta vira NSBeep).
  - **ImageIO:** CGImageSource/Destination, que ainda devolvem NULL.
  - **ServiceManagement:** SMJob* falham.
  - **libcompression:** sem codecs.

  As constantes CFString usam a literal do clang (`__builtin___CFStringMakeConstantString`).
- **libz:** o bottle do Homebrew (zlib 1.3.2) em `/usr/local/opt/zlib/lib`, para onde o carregador já redireciona
  `/usr/lib/libz.1.dylib`.
- Os 11 símbolos do CoreData e os do WebKit, SecurityInterface, QuickLookUI e AudioToolbox que o Postico importa
  batem. Os do Sparkle (ImageIO, ServiceManagement, UTType) também.
- **Teste final:** a janela de conexão do Postico aparecer no harness (`MACHOTEST_DUMP`), depois das fases 5 e 6.

## Ordem e dependências

As fases 1 → 2 → 3 → 4 formam uma cadeia, e cada uma só pode ser testada com a anterior pronta. As fases 5 e 6 dependem da 3, e podem andar em
paralelo uma com a outra. A 7 pode começar já, porque libz e libc++ não dependem de nada disto.

## Riscos

- **Duas variantes do stack Swift** dobram o tempo de build. Se a variante com interop pegar, dá para migrar os
  apps do mOS para ela e aposentar a sem ObjC.
- **O compilador Swift do Windows gerando código Darwin com interop** sem o SDK da Apple: precisa dos nossos
  cabeçalhos ObjC e de um `-sdk` mínimo. Validar logo no começo da fase 3, com um arquivo de teste.
