# Stubs exportados do mOS
Auditoria somente de leitura do que ainda liga e não faz o trabalho. Contagem conferida em `userland/libsystem/sys_zerados.c`: <!--n:total-sys-->**5293**<!--/n--> funções no formato `void nome(void) {}`. O recado de «cerca de 5404» não bate com o arquivo nesta main (`0c9c75e`). A lista símbolo a símbolo está em [stubs-full.txt](stubs-full.txt).
## Onde vive cada parte
| Componente | Caminho |
|---|---|
| Stubs de link (libswift e C) | `userland/libsystem/sys_zerados.c` |
| SwiftUI vazio (Claude, ChatGPT, WhatsApp) | `userland/libsystem/swiftui_imports.c` |
| Runtime Swift em C (retain, metadata, concorrência de verdade em outros arquivos) | `userland/libsystem/swiftrt.c`, `swiftobjc.c`, `swiftsect.c`, `swiftsupport.c` |
| Base para tirar os `swift_*` daqui | `tools/vscode-macho-stubs/libswift-runtime.c` (PRs #82 e #94) |
| libc++ parcial e exceção que ainda aborta | `userland/libsystem/libcxx.c` |
| libc++abi (new, guarda, call_once) | `userland/libsystem/cxxabi.c` |
| Unwind DWARF do libSystem | `userland/libsystem/cxx_unwind.c`, `unwind_ls.c`; tabelas em `dlfcn.c` (`_dyld_find_unwind_sections`) |
| Exceção ObjC em cima da libc++ do disco | `userland/libsystem/objc_exc.c` |
| libcompression / zlib | `userland/libsystem/compression_z.c`, `userland/frameworks/libcompression/compression.c` |
| Stubs fracos do kernel (áudio, rede, USB) | `kernel/stubs.c` |

## Fora da lista de pendências
Conferido na main, não são mais stub:

- `_Unwind_DeleteException` em `userland/libsystem/cxx_unwind.c:600` chama o cleanup da exceção. Entrou no #96 junto com os outros `_Unwind_*` que o OpenBot importava.
- `compression_stream_init`, `compression_stream_process` e `compression_stream_destroy` em `userland/libsystem/compression_z.c` (zlib 1.3.2, algoritmo `COMPRESSION_ZLIB`). Entrou no #83. `libcompression` ainda devolve 0 em `compression_encode_buffer` e `compression_decode_buffer` (sem LZFSE/LZ4/LZMA).
- `crc32` da zlib está em `userland/third_party/zlib/crc32.c`, incluído por `compression_z.c`. No Mach-O o nome exportado é `_crc32`. Não há um segundo stub `_crc32` no libSystem.
- O desenrolar de `-O1` do #77 continua com outro agente. Não é item desta lista.

ImageIO, VideoToolbox, Metal, libcurl e libz não têm `void nome(void) {}` neste arquivo. CryptoKit também não. O dono, quando o símbolo aparecer, é Sistemas. libcurl segue com o agente bc-10a98d26. ImageIO segue com bc-315c5f63 (`cursor/grok-imageio-cryptokit-6729`).

## Donos
Combinado com a Sistemas:

- **Sistemas:** tudo de `CF*` / CoreFoundation, Foundation (libswift e os `NS*`), Security, libcurl, ImageIO, CoreMedia, VideoToolbox, Metal, libcompression (o módulo Swift `Compression` incluso) e libz.
- **Runtime C/C++ e Swift:** libc++, libc++abi, libunwind / `_Unwind_*` / `unw_*`, runtime e stdlib do Swift. Entram os `swift_*` e os módulos Swift, Dispatch, Darwin, `os`, System e ObjectiveC de `sys_zerados.c`. Também Network / `nw_*`, libc/libm, CommonCrypto e Accelerate.
- **Frameworks Apple:** AVFoundation, CoreAudio, UniformTypeIdentifiers e CloudKit.
- **Interface:** UIKit, PhotosUI, CoreGraphics, SwiftUI e Combine.
- **Kernel, Storage:** pelo que a área faz.
- **a definir:** o bloco ObjC sem módulo, `posixmore.c` e `compat_agentes.c`.

## Em andamento (outro agente)
Não implementar neste ciclo:

- A API `unw_*` da libunwind.
- Os símbolos de libc++ de `string`, `stringbuf`, `exception`, `filesystem`, `iostream`, `sort`, `__sp_mut` e `__cxa_bad_typeid`.
- O desenrolar de `-O1` do #77 (já citado acima).

`_Unwind_DeleteException`, `crc32` e `compression_stream_*` não entram aqui: já estão na main.

## sys_zerados.c por biblioteca
A libswift de stdlib e de runtime deste arquivo é nossa. Foundation, CoreFoundation, Security, CoreMedia e Compression, mesmo mangled, são da Sistemas.

<!-- gerado:tabela-grupos início -->
| Quantidade | Grupo | Dono | Primeiro símbolo |
|---:|---|---|---|
| 2888 | Swift (stdlib) | Runtime C/C++ e Swift | `$s10_ErrorType10Foundation01_A12CodeProtocolPTl` |
| 1070 | Foundation | Sistemas | `$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg` |
| 248 | UIKit | Interface | `$s5UIKit01_A23NumericRawRepresentablePAAE1poiyxx_0C5ValueQztFZ` |
| 159 | swift_* (runtime C) | Runtime C/C++ e Swift | `swift_allocEmptyBox` |
| 154 | Network | Runtime C/C++ e Swift | `$s7Network10NWEndpointO4HostO13stringLiteralAESS_tcfC` |
| 116 | Dispatch | Runtime C/C++ e Swift | `$s8Dispatch0A11SpecificKeyCACyxGycfC` |
| 69 | ObjC (sem módulo reconhecido) | a definir | `$sSo10CKDatabaseC8CloudKitE17modifyRecordZones6saving8deletingSDySo14CKRecordZoneIDCs6ResultOySo0iJ0Cs5Error_pGG11saveResults_SDyAhJyytsAM_pGG06deleteO0tSayALG_SayAHGtYaKF` |
| 65 | CoreMedia | Sistemas | `$s9CoreMedia15CMTIME_IS_VALIDySbSo6CMTimeaF` |
| 61 | libc/libm (C) | Runtime C/C++ e Swift | `NXGetArchInfoFromCpuType` |
| 60 | CoreGraphics | Interface | `$s12CoreGraphics14CGPathFillRuleO7evenOddyA2CmFWC` |
| 47 | UniformTypeIdentifiers | Frameworks Apple | `$s22UniformTypeIdentifiers10UTTagClassV04mimeB0ACvgZ` |
| 34 | AVFoundation | Frameworks Apple | `$s12AVFoundation15AVAsyncPropertyC6StatusO11descriptionSSvg` |
| 32 | System | Runtime C/C++ e Swift | `$s6System14FileDescriptorV11OpenOptionsVMn` |
| 27 | Foundation (C, NS*) | Sistemas | `NSLocaleCalendar` |
| 27 | PhotosUI | Interface | `$s8PhotosUI14PHPickerFilterV6imagesACvgZ` |
| 27 | os | Runtime C/C++ e Swift | `$s2os0A4_log_3dso0B04type_ys12StaticStringV_SVSgSo03OS_a1_B0CSo0a1_b1_D2_tas7CVarArg_pdtF` |
| 25 | Network (C, nw_*) | Runtime C/C++ e Swift | `nw_connection_cancel` |
| 21 | ObjectiveC | Runtime C/C++ e Swift | `$s10ObjectiveC8ObjCBoolVMn` |
| 21 | _StringProcessing | Runtime C/C++ e Swift | `$s17_StringProcessing13_RegexFactoryV10accumulateyAA0C0VyxGq__q0_tAA0C9ComponentR_AaHR0_r1_lF` |
| 18 | RegexBuilder | Runtime C/C++ e Swift | `$s12RegexBuilder0a9ComponentB0O15buildExpressionyxx17_StringProcessing0aC0RzlFZ` |
| 15 | XPC | Kernel | `$s3XPC0A10_TYPE_BOOLs13OpaquePointerVvg` |
| 13 | CoreAudio | Frameworks Apple | `$s9CoreAudio013UnsafeMutableB17BufferListPointerV06unsafeG0SPySo0beF0VGvg` |
| 11 | CommonCrypto (C) | Runtime C/C++ e Swift | `CCCalibratePBKDF` |
| 11 | Darwin | Runtime C/C++ e Swift | `$s6Darwin0A7BooleanVMn` |
| 10 | AppleArchive | Storage | `$s12AppleArchive0B10ByteStreamC015withCompressionD05using9writingTo9blockSize5flags11threadCount_xAA0bF0V_ACSiAA0B5FlagsVSixACKXEtKlFZ` |
| 9 | Compression | Sistemas | `$s11Compression12OutputFilterC5writeyyxSgK10Foundation12DataProtocolRzlFTj` |
| 9 | Observation | Runtime C/C++ e Swift | `$s11Observation04withA8Tracking_8onChangexxyXE_yyYbcyXKtlF` |
| 7 | Accelerate | Runtime C/C++ e Swift | `$s10Accelerate6vImageO7OptionsV7noFlagsAEvgZ` |
| 7 | libdispatch (C) | Runtime C/C++ e Swift | `dispatch_apply` |
| 6 | filesystem (C) | Storage | `endfsent` |
| 5 | Combine | Interface | `$sSo12NSURLSessionC10FoundationE17DataTaskPublisherV7Combine0E0ACMc` |
| 5 | CoreFoundation | Sistemas | `$s14CoreFoundation9_CFObjectMp` |
| 5 | mach (C) | Kernel | `mach_port_get_attributes` |
| 4 | pthread (C) | Runtime C/C++ e Swift | `pthread_fchdir_np` |
| 3 | mach exception (C) | Kernel | `exception_raise` |
| 2 | Security (C) | Sistemas | `SecTaskCopyTeamIdentifier` |
| 2 | xpc (C) | Kernel | `xpc_dictionary_get_audit_token` |

Soma: **5293**. Há ainda 29 `void *kCF…` / `kCFURL…` sem inicializador (ponteiro nulo), listados no anexo. Não são função.
<!-- gerado:tabela-grupos fim -->

### O que o grupo deveria fazer
- **Swift (stdlib), <!--n:Swift (stdlib)-->2888<!--/n-->. Runtime.** Metadados e witness tables. Corpo vazio: o binário liga e a chamada não materializa tipo nem compara. O primeiro símbolo da tabela é um associated type cujo módulo não parseou como Foundation; os <!--n:Foundation-->1070<!--/n--> com módulo Foundation estão na linha de baixo.
- **Foundation, <!--n:Foundation-->1070<!--/n--> + <!--n:Foundation (C, NS*)-->27<!--/n--> `NS*`. Sistemas.** `URLRequest`, `JSONDecoder`, `Data`, `Date`, erros Cocoa/POSIX. Sem isso o app não decodifica JSON nem monta pedido HTTP. Não é PR do Runtime.
- **CoreFoundation, <!--n:CoreFoundation-->5<!--/n-->, e os <!--n:ponteiros-->29<!--/n--> ponteiros `kCF*`. Sistemas.** O mesmo vale para qualquer `CF*` que aparecer fora deste arquivo.
- **CoreMedia, <!--n:CoreMedia-->65<!--/n-->, e Compression, <!--n:Compression-->9<!--/n-->. Sistemas.** Junto com VideoToolbox, Metal, ImageIO, libcurl e libz (zero stubs `void nome(void) {}` aqui).
- **Security, <!--n:Security (C)-->2<!--/n-->. Sistemas.** `SecTaskCopyTeamIdentifier` e o vizinho. `SecTransformCreateGroupTransform` em `security.c` também.
- **swift_*, <!--n:swift_* (runtime C)-->159<!--/n-->, Dispatch <!--n:Dispatch-->116<!--/n-->, libdispatch <!--n:libdispatch (C)-->7<!--/n-->, pthread <!--n:pthread (C)-->4<!--/n-->, Darwin, os, System, ObjectiveC, _StringProcessing, RegexBuilder, Observation. Runtime.** `swift_retain` / `allocObject` / cast / task. Referência: `tools/vscode-macho-stubs/libswift-runtime.c` (#82/#94).
- **UIKit <!--n:UIKit-->248<!--/n-->, PhotosUI <!--n:PhotosUI-->27<!--/n-->, CoreGraphics <!--n:CoreGraphics-->60<!--/n-->. Interface.** SwiftUI dos <!--n:total-swiftui-->918<!--/n--> de `swiftui_imports.c` também.
- **mach, exception, xpc e o módulo XPC. Kernel.**
- **filesystem <!--n:filesystem (C)-->6<!--/n--> e AppleArchive <!--n:AppleArchive-->10<!--/n-->. Storage.**
- **Network <!--n:Network-->154<!--/n-->, `nw_*` <!--n:Network (C, nw_*)-->25<!--/n-->, libc/libm <!--n:libc/libm (C)-->61<!--/n-->, CommonCrypto <!--n:CommonCrypto (C)-->11<!--/n-->, Accelerate <!--n:Accelerate-->7<!--/n-->. Runtime C/C++ e Swift.**
- **AVFoundation <!--n:AVFoundation-->34<!--/n-->, CoreAudio <!--n:CoreAudio-->13<!--/n-->, UniformTypeIdentifiers <!--n:UniformTypeIdentifiers-->47<!--/n-->. Frameworks Apple.** CloudKit (`CloudKit.c`) também.
- **Combine, <!--n:Combine-->5<!--/n-->. Interface.**
- **a definir:** <!--n:ObjC (sem módulo reconhecido)-->69<!--/n--> símbolos ObjC sem módulo reconhecido (há CloudKit mangled no meio; o arquivo `CloudKit.c` já tem dono). `posixmore.c` e `compat_agentes.c` também ficam assim: não entraram nesta leva.

## Outros corpos vazios ou que só devolvem 0/NULL
Uma linha, fora de `third_party` e fora de `sys_zerados.c`. Métodos de amostra em `userland/samples` não entram.

| Quantidade | Arquivo | Dono | Impacto |
|---:|---|---|---|
| <!--n:total-swiftui-->918<!--/n--> | `userland/libsystem/swiftui_imports.c` | Interface | SwiftUI importado por Claude, ChatGPT e WhatsApp liga e não desenha. Lista no anexo. |
| 31 | `kernel/stubs.c` | Kernel | Áudio, rede, ping e DHCP fracos (`weak`): o subsistema real, se existir, substitui. |
| 6 | `userland/libsystem/vscode_frameworks.c` | Interface | Símbolos de framework do VS Code que só retornam vazio. |
| 5 | `userland/libsystem/posixmore.c` | a definir | `endservent`, `getservent` (NULL), `sync`, `gethostid` (0), utmpx. |
| 4 | `userland/libsystem/chrome.c` | Interface | Kill ring do AppKit (`_NSInitializeKillRing` e vizinhos). |
| 4 | `kernel/arch/cpu.c` | Kernel | Entradas de CPU vazias ou que retornam 0. |
| 2 | `userland/libsystem/vscode_media.c` | Interface | `MIDIGetNumberOfSources` e `MIDIGetNumberOfDestinations` devolvem 0. |
| 2 | `userland/libsystem/cgdisplay.c` | Interface | Display que devolve 0. |
| 2 | `userland/libsystem/xpc.c` | Kernel | XPC que devolve 0. |
| 2 | `userland/libsystem/openbot_swift.c` | Runtime C/C++ e Swift | Símbolo Swift do OpenBot vazio. |
| 2 | `userland/libsystem/compat_agentes.c` | a definir | Compat de agentes. |
| 1 | `userland/libsystem/security.c:946` `SecTransformCreateGroupTransform` | Sistemas | Devolve NULL. O resto de Security no mesmo arquivo não é este caso. |
| 1 | `userland/frameworks/CloudKit/CloudKit.c` | Frameworks Apple | Uma entrada vazia. |
| 1 | `kernel/bt/btintel.c` | Kernel | Bluetooth Intel. |
| 1 | `kernel/lib/vfs.c` | Storage | Uma entrada do VFS que devolve 0. |

### Runtime que não é corpo vazio, mas mente
| Onde | Símbolo | Esperado | Impacto |
|---|---|---|---|
| `libcxx.c:306` | `__cxa_throw` | Desenrolar com `_Unwind_RaiseException` | Exceção C++ impressa e `abort`. O caminho ObjC em `objc_exc.c` usa a libc++ do disco e recusa este símbolo de propósito. |
| `libcxx.c:321` | `__cxa_end_catch` | Soltar a exceção ativa | Corpo vazio. |
| `libcxx.c:324` | `__gxx_personality_v0` | Ler LSDA e instalar handler | Devolve 8 (`_URC_CONTINUE_UNWIND`) sem ler o frame. `cxx_unwind.c` tem outra personalidade; as duas não podem ser a definitiva ao mesmo tempo. |
| `libcxx.c:237` | `__shared_weak_count::__get_deleter` | Achar o deleter pelo `type_info` | Devolve NULL. |
| `libcxx.c:328` | vtables `__class_type_info` | `dynamic_cast` / `catch` por base | Entradas nulas. O arquivo diz isso. |
| `swiftrt.c:15` | `_swift_stdlib_getNormData` e a decomposição | Tabelas Unicode (NFC/NFD) | Devolve 0: só casa texto já igual. Composição devolve `0xFFFFFFFF`. |
| `swiftobjc.c:68` | `voucher_adopt` / `voucher_copy` | Voucher de QoS | Devolve NULL. O comentário assume que o mOS não tem voucher. |
| `frameworks/libcompression/compression.c:20` | `compression_encode_buffer` / `decode_buffer` | LZFSE, LZ4, LZMA além do zlib | Devolve 0. Dono Sistemas (libcompression). `compression_stream_*` de zlib já está em `compression_z.c`. |

## Próximos PRs do Runtime (um comportamento por PR)
Base dos `swift_*`: `tools/vscode-macho-stubs/libswift-runtime.c` dos PRs #82 e #94. Fora deste ciclo: `_Unwind_DeleteException`, `crc32`, `compression_stream_*`, a API `unw_*`, os símbolos libc++ de string/stringbuf/exception/filesystem/iostream/sort/`__sp_mut`/`__cxa_bad_typeid`, e o unwind de `-O1` do #77. Foundation, CF, Security, CoreMedia, VideoToolbox, Metal, ImageIO, libcurl, libcompression e libz são da Sistemas.

1. **`swift_retain` / `swift_release` / `swift_allocObject` / `swift_deallocObject`.** Tirar esses quatro (e os `_n`) de `sys_zerados.c` e usar o runtime já escrito. Teste de host: `swift_allocObject` com um descritor mínimo; `swift_retain`/`swift_release` sobem e liberam, no estilo de `tools/libswift_check.sh`.
2. **`swift_once`, `swift_getTypeByMangledNameInContext` e `swift_conformsToProtocol`.** Teste de host: o mangled name de `Swift.Int` devolve o mesmo metadado duas vezes; conformidade ausente devolve nulo.
3. **Tabelas Unicode de `swiftrt.c`.** NFC de `a` + acento combinante igual a `á`. Teste de host: `_swift_stdlib_getComposition` desse par deixa de ser `0xFFFFFFFF`.
4. **`dispatch_block_create` / `wait` / `cancel`.** Teste de host: o bloco roda numa fila, `wait` só volta depois, `cancel` antes do run não chama a função.
5. **`swift_task_isCancelled` e `swift_task_getMainExecutor`.** Dois símbolos do grupo `swift_*`, com teste de host: tarefa não cancelada devolve falso; o executor principal é o mesmo ponteiro nas duas chamadas.
6. **`swift_dynamicCast` de classe.** Teste de host: cast para a própria classe devolve o objeto; cast para uma classe sem relação devolve nulo, sem abortar.

`__cxa_throw` em `libcxx.c` continua abortando. Mexer nisso atravessa os símbolos de `exception` que outro agente já pegou, então fica de fora desta fila.

## Outros times
- **Sistemas:** Foundation, `CF*` / CoreFoundation, Security, CoreMedia, VideoToolbox, Metal, ImageIO, libcurl (bc-10a98d26), libcompression e libz. Os <!--n:ponteiros-->29<!--/n--> ponteiros `kCF*` também.
- **Kernel:** `kernel/stubs.c`, `mach_*`, `exception_*`, XPC, `kernel/arch/cpu.c`, `xpc.c`, `kernel/bt/btintel.c`.
- **Interface:** UIKit, PhotosUI, CoreGraphics, `swiftui_imports.c`, `chrome.c`, `vscode_media.c`, `cgdisplay.c`, `vscode_frameworks.c`.
- **Storage:** `getfsent` e xattr, AppleArchive, `kernel/lib/vfs.c`.
- **Frameworks Apple:** AVFoundation, CoreAudio, UniformTypeIdentifiers, CloudKit.
- **a definir:** ObjC sem módulo (<!--n:ObjC (sem módulo reconhecido)-->69<!--/n-->), `posixmore.c`, `compat_agentes.c`.

## Build e testes de host
Nesta rodada não rodei `tools/build.sh`, QEMU nem a suíte de host. O estado abaixo é leitura do fonte na main `0c9c75e`.

| Área | Estado |
|---|---|
| `operator new`, `__cxa_guard_*`, `std::string` em `libcxx.c` / `cxxabi.c` | Código com comportamento. Não reexecutado aqui. |
| `_Unwind_RaiseException` e `_Unwind_DeleteException` | Código em `cxx_unwind.c`. Não reexecutado aqui. |
| `compression_stream_*` e `crc32` | Código da zlib no #83. Não reexecutado aqui. |
| <!--n:total-sys-->5293<!--/n--> de `sys_zerados.c` e <!--n:total-swiftui-->918<!--/n--> de `swiftui_imports.c` | Stub. |
| `__cxa_throw` de `libcxx.c` | Stub que aborta. |
| Unicode em `swiftrt.c` | Stub (ASCII/Latin-1 de caixa é real; normalização não). |
| Exceção ObjC com libc++ no disco | Código em `objc_exc.c`. O harness citado é `tools/objc_eh_hosttest.c`. Não rodei. |
| ImageIO, CryptoKit, libcurl neste arquivo | Ausentes, não testados. |
