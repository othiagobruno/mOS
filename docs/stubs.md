# Stubs exportados do mOS
Auditoria somente de leitura do que ainda liga e não faz o trabalho. Contagem conferida em `userland/libsystem/sys_zerados.c`: **5346** funções no formato `void nome(void) {}`. O recado de «cerca de 5404» não bate com o arquivo nesta main (`0c9c75e`). A lista símbolo a símbolo está em [stubs-full.txt](stubs-full.txt).
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

ImageIO, CryptoKit e libcurl **não aparecem** entre as 5346. Quando surgirem: ImageIO e CryptoKit são da Sistemas (bc-315c5f63, `cursor/grok-imageio-cryptokit-6729`); libcurl é da Sistemas (bc-10a98d26).

## sys_zerados.c por biblioteca
Dono da libswift (todo símbolo `$s…` e `swift_*`): Runtime C/C++ e Swift. O arquivo também tem símbolos C. Esses são o lado compartilhado com a Sistemas (bc-315c5f63), salvo mach/xpc (Kernel) e filesystem (Storage).

| Quantidade | Grupo | Dono | Primeiro símbolo |
|---:|---|---|---|
| 2907 | Swift (stdlib) | Runtime C/C++ e Swift | `$s10_ErrorType10Foundation01_A12CodeProtocolPTl` |
| 1070 | Foundation | Runtime C/C++ e Swift | `$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg` |
| 248 | UIKit | Runtime C/C++ e Swift | `$s5UIKit01_A23NumericRawRepresentablePAAE1poiyxx_0C5ValueQztFZ` |
| 174 | swift_* (runtime C) | Runtime C/C++ e Swift | `swift_allocBox` |
| 154 | Network | Runtime C/C++ e Swift | `$s7Network10NWEndpointO4HostO13stringLiteralAESS_tcfC` |
| 116 | Dispatch | Runtime C/C++ e Swift | `$s8Dispatch0A11SpecificKeyCACyxGycfC` |
| 69 | ObjC (sem módulo reconhecido) | Runtime C/C++ e Swift | `$sSo10CKDatabaseC8CloudKitE17modifyRecordZones6saving8deletingSDySo14CKRecordZoneIDCs6ResultOySo0iJ0Cs5Error_pGG11saveResults_SDyAhJyytsAM_pGG06deleteO0tSayALG_SayAHGtYaKF` |
| 65 | CoreMedia | Runtime C/C++ e Swift | `$s9CoreMedia15CMTIME_IS_VALIDySbSo6CMTimeaF` |
| 62 | libc/libm (C) | Sistemas (bc-315c5f63), lado compartilhado de sys_zerados.c | `NXGetArchInfoFromCpuType` |
| 60 | CoreGraphics | Runtime C/C++ e Swift | `$s12CoreGraphics14CGPathFillRuleO7evenOddyA2CmFWC` |
| 47 | UniformTypeIdentifiers | Runtime C/C++ e Swift | `$s22UniformTypeIdentifiers10UTTagClassV04mimeB0ACvgZ` |
| 34 | AVFoundation | Runtime C/C++ e Swift | `$s12AVFoundation15AVAsyncPropertyC6StatusO11descriptionSSvg` |
| 32 | System | Runtime C/C++ e Swift | `$s6System14FileDescriptorV11OpenOptionsVMn` |
| 29 | Darwin | Runtime C/C++ e Swift | `$s6Darwin0A7BooleanVMn` |
| 27 | Foundation (C, NS*) | Sistemas (bc-315c5f63), lado compartilhado de sys_zerados.c | `NSLocaleCalendar` |
| 27 | PhotosUI | Runtime C/C++ e Swift | `$s8PhotosUI14PHPickerFilterV6imagesACvgZ` |
| 27 | os | Runtime C/C++ e Swift | `$s2os0A4_log_3dso0B04type_ys12StaticStringV_SVSgSo03OS_a1_B0CSo0a1_b1_D2_tas7CVarArg_pdtF` |
| 25 | Network (C, nw_*) | Sistemas (bc-315c5f63), lado compartilhado de sys_zerados.c | `nw_connection_cancel` |
| 21 | ObjectiveC | Runtime C/C++ e Swift | `$s10ObjectiveC8ObjCBoolVMn` |
| 21 | _StringProcessing | Runtime C/C++ e Swift | `$s17_StringProcessing13_RegexFactoryV10accumulateyAA0C0VyxGq__q0_tAA0C9ComponentR_AaHR0_r1_lF` |
| 18 | RegexBuilder | Runtime C/C++ e Swift | `$s12RegexBuilder0a9ComponentB0O15buildExpressionyxx17_StringProcessing0aC0RzlFZ` |
| 15 | XPC | Runtime C/C++ e Swift | `$s3XPC0A10_TYPE_BOOLs13OpaquePointerVvg` |
| 13 | CoreAudio | Runtime C/C++ e Swift | `$s9CoreAudio013UnsafeMutableB17BufferListPointerV06unsafeG0SPySo0beF0VGvg` |
| 11 | CommonCrypto (C) | Sistemas (bc-315c5f63), lado compartilhado de sys_zerados.c | `CCCalibratePBKDF` |
| 10 | AppleArchive | Runtime C/C++ e Swift | `$s12AppleArchive0B10ByteStreamC015withCompressionD05using9writingTo9blockSize5flags11threadCount_xAA0bF0V_ACSiAA0B5FlagsVSixACKXEtKlFZ` |
| 9 | Compression | Runtime C/C++ e Swift | `$s11Compression12OutputFilterC5writeyyxSgK10Foundation12DataProtocolRzlFTj` |
| 9 | Observation | Runtime C/C++ e Swift | `$s11Observation04withA8Tracking_8onChangexxyXE_yyYbcyXKtlF` |
| 7 | Accelerate | Runtime C/C++ e Swift | `$s10Accelerate6vImageO7OptionsV7noFlagsAEvgZ` |
| 7 | libdispatch (C) | Runtime C/C++ e Swift | `dispatch_apply` |
| 6 | filesystem (C) | Storage | `endfsent` |
| 5 | Combine | Runtime C/C++ e Swift | `$sSo12NSURLSessionC10FoundationE17DataTaskPublisherV7Combine0E0ACMc` |
| 5 | CoreFoundation | Runtime C/C++ e Swift | `$s14CoreFoundation9_CFObjectMp` |
| 5 | mach (C) | Kernel | `mach_port_get_attributes` |
| 4 | pthread (C) | Runtime C/C++ e Swift | `pthread_fchdir_np` |
| 3 | mach exception (C) | Kernel | `exception_raise` |
| 2 | Security (C) | Sistemas (bc-315c5f63), lado compartilhado de sys_zerados.c | `SecTaskCopyTeamIdentifier` |
| 2 | xpc (C) | Kernel | `xpc_dictionary_get_audit_token` |

Soma: **5346**. Há ainda 29 `void *kCF…` / `kCFURL…` sem inicializador (ponteiro nulo), listados no anexo. Não são função.

### O que o grupo deveria fazer
- **Swift (stdlib), 2859.** Metadados, witness tables e entradas da stdlib. Corpo vazio: o binário liga e qualquer chamada não materializa tipo, não compara e não formata. Apps Swift que passam de `swift_retain` e caem num símbolo `$ss…` quebram em silêncio.
- **Foundation (libswift), 1070.** `URLRequest`, `JSONDecoder`, `Data`, `Date`, erros POSIX/Cocoa. Sem isso o overlay não decodifica JSON nem monta pedido HTTP.
- **UIKit, 248; PhotosUI, 27; AVFoundation, 34; CoreGraphics, 60; CoreMedia, 65.** Superfície Swift desses frameworks. Dono do símbolo neste arquivo é Runtime (é libswift). O desenho na tela continua com a Interface.
- **swift_*, 174.** `swift_retain`/`release`, `swift_allocObject`, cast, key path, task e actor. São o núcleo. A implementação de referência já está em `tools/vscode-macho-stubs/libswift-runtime.c` (#82/#94), não nestes corpos vazios.
- **Network (libswift) 154 e nw_* 26.** `NWConnection` não abre socket. O C `nw_*` é da Sistemas no recorte compartilhado.
- **Dispatch 116 e libdispatch C.** Fila e `dispatch_block_*` vazios. Jobs Swift não entram na fila.
- **libc/libm, CommonCrypto, CF e NS* (C).** Nomes que o machocheck pedia e ninguém implementou aqui. CommonCrypto vazio: hash e PBKDF não rodam por este símbolo (há outra implementação de CC em `commoncrypto.c` se o link escolher a de lá).
- **mach / exception / xpc (10).** Portas e exceção Mach. Dono Kernel.
- **filesystem (6).** `getfsent`/`setfsent`/`endfsent`, `xattr_name_with_flags`, `nftw$INODE64`, `open_dprotected_np`. Dono Storage.

## Outros corpos vazios ou que só devolvem 0/NULL
Uma linha, fora de `third_party` e fora de `sys_zerados.c`. Métodos de amostra em `userland/samples` não entram.

| Quantidade | Arquivo | Dono | Impacto |
|---:|---|---|---|
| 976 | `userland/libsystem/swiftui_imports.c` | Runtime C/C++ e Swift | SwiftUI importado por Claude, ChatGPT e WhatsApp liga e não desenha. Lista no anexo. |
| 31 | `kernel/stubs.c` | Kernel | Áudio, rede, ping e DHCP fracos (`weak`): o subsistema real, se existir, substitui. |
| 6 | `userland/libsystem/vscode_frameworks.c` | Runtime C/C++ e Swift | Símbolos de framework do VS Code que só retornam vazio. |
| 5 | `userland/libsystem/posixmore.c` | Runtime C/C++ e Swift | `endservent`, `getservent` (NULL), `sync`, `gethostid` (0), utmpx. |
| 4 | `userland/libsystem/chrome.c` | Runtime C/C++ e Swift | Kill ring do AppKit (`_NSInitializeKillRing` e vizinhos). |
| 4 | `kernel/arch/cpu.c` | Kernel | Entradas de CPU vazias ou que retornam 0. |
| 2 | `userland/libsystem/vscode_media.c` | Runtime C/C++ e Swift | `MIDIGetNumberOfSources` e `MIDIGetNumberOfDestinations` devolvem 0. |
| 2 | `userland/libsystem/cgdisplay.c` | Runtime C/C++ e Swift | Display que devolve 0. |
| 2 | `userland/libsystem/xpc.c` | Runtime C/C++ e Swift | XPC que devolve 0. |
| 2 | `userland/libsystem/openbot_swift.c` | Runtime C/C++ e Swift | Símbolo Swift do OpenBot vazio. |
| 2 | `userland/libsystem/compat_agentes.c` | Runtime C/C++ e Swift | Compat de agentes. |
| 1 | `userland/libsystem/security.c:946` `SecTransformCreateGroupTransform` | Runtime C/C++ e Swift | Devolve NULL. O resto de Security no mesmo arquivo não é este caso. |
| 1 | `userland/frameworks/CloudKit/CloudKit.c` | Runtime C/C++ e Swift | Uma entrada vazia. |
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
| `frameworks/libcompression/compression.c:20` | `compression_encode_buffer` / `decode_buffer` | LZFSE, LZ4, LZMA, zlib | Devolve 0 e avisa no stderr. O fluxo `compression_stream_*` de zlib já está em `compression_z.c`. |

## Próximos PRs do Runtime (um comportamento por PR)
Base dos `swift_*`: `tools/vscode-macho-stubs/libswift-runtime.c` dos PRs #82 e #94. Não reimplementar `_Unwind_DeleteException`, `crc32` nem `compression_stream_*`. Não mexer no unwind de `-O1` do #77.

1. **`swift_retain` / `swift_release` / `swift_allocObject` / `swift_deallocObject`.** Tirar esses quatro (e os `_n`) de `sys_zerados.c` e apontar para o runtime já escrito. Teste de host: um programa C que chama `swift_allocObject` com um descritor mínimo e confere que `swift_retain`/`swift_release` zera e libera, no estilo de `tools/libswift_check.sh`.
2. **`swift_once`, `swift_getTypeByMangledNameInContext` e `swift_conformsToProtocol`.** Sem isso genérico e protocolo não resolvem. Teste de host: mangled name de `Swift.Int` devolve o mesmo metadado duas vezes; conformidade ausente devolve nulo de verdade, não «ligou».
3. **`__cxa_throw` do libSystem passa a chamar `_Unwind_RaiseException` de `cxx_unwind.c`.** Hoje aborta. Teste de host: `tools/cxx_unwind_host.cpp` (já no repo) com um `try/catch` de `std::runtime_error` que imprime a mensagem e segue, sem `abort`.
4. **Personalidade e `type_info` de `catch` por base em `libcxx.c`.** Uma personalidade só. Teste de host: `catch (const std::exception&)` pega `std::runtime_error` lançado pelo teste 3.
5. **Tabelas Unicode de `swiftrt.c`.** NFC de `a` + acento combinante igual a `á`. Teste de host: comparar as duas sequências pela função que a stdlib chama (`_swift_stdlib_getComposition` deixa de ser `0xFFFFFFFF` nesse par).
6. **`URLRequest` do grupo Foundation em `sys_zerados.c` (25 símbolos em volta de `URLRequest`, mais o bridge para `NSURLRequest`).** Teste de host: criar o pedido, ler `httpMethod` e `url`, e ver o objeto ObjC com o mesmo URL. O resto dos 1070 fica para outro PR.
7. **`dispatch_block_create` / `wait` / `cancel`.** Teste de host: o bloco roda numa fila, `wait` só volta depois, `cancel` antes do run não chama a função.
8. **SwiftUI: um tipo com corpo, não os 976.** Começar por `Text`/`View` se ainda estiverem vazios em `swiftui_imports.c`, com teste de host que instancia e lê a string. O restante do arquivo continua stub até o PR seguinte.

## Outros times
- **Kernel:** `kernel/stubs.c` (rede e áudio fracos), `mach_*` e `exception_*` de `sys_zerados.c`, XPC, `kernel/arch/cpu.c`.
- **Interface:** `swiftui_imports.c` é símbolo de Runtime, mas a janela é da Interface. `chrome.c` (kill ring), `vscode_media.c` (MIDI), `cgdisplay.c`, `vscode_frameworks.c`.
- **Sistemas (bc-315c5f63):** lado C de `sys_zerados.c` (CF, NS*, `nw_*`, CommonCrypto, Security, libc/libm), `posixmore.c`, CloudKit, ImageIO e CryptoKit quando existirem (`cursor/grok-imageio-cryptokit-6729`).
- **Sistemas (bc-10a98d26):** libcurl. Zero símbolos neste arquivo.
- **Storage:** `getfsent` e xattr em `sys_zerados.c`, a entrada de `kernel/lib/vfs.c`.

## Build e testes de host
Nesta rodada não rodei `tools/build.sh`, QEMU nem a suíte de host. O estado abaixo é leitura do fonte na main `0c9c75e`.

| Área | Estado |
|---|---|
| `operator new`, `__cxa_guard_*`, `std::string` em `libcxx.c` / `cxxabi.c` | Código com comportamento. Não reexecutado aqui. |
| `_Unwind_RaiseException` e `_Unwind_DeleteException` | Código em `cxx_unwind.c`. Não reexecutado aqui. |
| `compression_stream_*` e `crc32` | Código da zlib no #83. Não reexecutado aqui. |
| 5346 de `sys_zerados.c` e 976 de `swiftui_imports.c` | Stub. |
| `__cxa_throw` de `libcxx.c` | Stub que aborta. |
| Unicode em `swiftrt.c` | Stub (ASCII/Latin-1 de caixa é real; normalização não). |
| Exceção ObjC com libc++ no disco | Código em `objc_exc.c`. O harness citado é `tools/objc_eh_hosttest.c`. Não rodei. |
| ImageIO, CryptoKit, libcurl neste arquivo | Ausentes, não testados. |
