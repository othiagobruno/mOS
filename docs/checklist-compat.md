# Checklist de compatibilidade com apps do macOS: onde o mOS está

O documento "Compatibilidade com Apps macOS e Electron no mOS" (roadmap genérico, fases 0 a 7) conferido contra o código em
6 nov. 2026. A meta concreta, com os sete apps, está em [compat-apps.md](compat-apps.md).

Legenda: **feito** = existe e tem teste; **parcial** = existe e falta parte; **falta** = não existe.

## Loader e runtime

| Item | Estado | Onde / observação |
|---|---|---|
| Parser Mach-O 64 + fat | **feito** | `kernel/compat/macho.c` (`0xCAFEBABE/BF`, slice x86_64). Mach-O 32 bits: fora de escopo, o macOS também não roda desde o 10.15 |
| Mapeamento de segmentos, `LC_MAIN`/`LC_UNIXTHREAD` | **feito** | `macho.c` |
| dyld (dylibs, `@rpath`, chained fixups, reexports, `dlopen`) | **feito** | `macho.c` + `userland/libsystem/dlfcn.c`, `dyldimg.c`. Sem `dyld_shared_cache`, porque o libSystem do mOS faz o papel das bibliotecas do sistema |
| libobjc moderno | **feito** | `objc.c` (classes, categorias, `objc_msgSend`, ARC, weak) + **`objcrt.c` (novo)**: protocolos, objetos associados, `class_copy*List`, propriedades, codificação de tipos, `imp_implementationWithBlock` (pool de 4096 trampolins em `objc_tramp.S`). Teste `tools/build-objcrttest.ps1`: 25/25 |
| Blocks | **feito** | `objc.c` (`_Block_copy`, byref, `_NSConcrete*Block`) |
| Exceções | **parcial** | C++: `libcxx.c` (`__cxa_throw`, `__gxx_personality_v0`); ObjC: `objc_exception_throw` + `OBJC_EHTYPE_id` (novo), mas `@catch` ainda não captura (`__objc_personality_v0` sempre continua o desenrolar). `_objc_msgForward` só faz o caminho rápido (`forwardingTargetForSelector:`) |

## Frameworks para CLI

| Item | Estado | Onde |
|---|---|---|
| libSystem (libc, pthread, libm, kqueue, sockets) | **feito** | `libc.c`, `stdio.c`, `darwin.c`, `kqueue.c`, `sockets*.c`. O Codex CLI oficial roda |
| CoreFoundation | **parcial** | `cfcollections.c`, `cfdata.c`, `cfrunloop.c`, `cli.c` (CFURL/CFError/CFPreferences...) |
| Foundation | **parcial** | `foundation.c`, `foundationsys.c`, `nsurl.c` e o Foundation do Swift (sessão OpenSwiftUI) |
| Security | **feito** | `security.c` (Keychain real) + **`securetransport.c` (novo)**: TLS de verdade pelo motor do kernel e raízes do sistema. Teste `tools/build-tlstest-app.ps1`: 16/16 no QEMU |

## GUI

| Item | Estado | Onde |
|---|---|---|
| AppKit (NSApplication, NSWindow, NSView, eventos, nib) | **parcial** | `appkit.c`, `nibarchive.c`, `kernel/compat/guisys.c`. Os samples Cocoa abrem janela no window server do mOS |
| CoreGraphics | **parcial** | `coregraphics.c`, `cgdisplay.c` |
| QuartzCore / CALayer / CAContext | **parcial** | `quartzcore.c`, `iosurface.c`; animações reais (`CABasicAnimation`, keyframes, mola, grupos), `CAShapeLayer`, `CAGradientLayer`, `CATextLayer`, `CADisplayLink`, `NSValue` com structs em `coreanimation.c` (ver `docs/metal.md`) |
| Integração com o window server do mOS | **feito** | `guisys.c` (janelas do mOS ↔ NSWindow, eventos → NSEvent) |
| Metal | **parcial** | Metal por software (sem driver de GPU): `metal.c` (API, `CAMetalLayer`, `MTKView`), `metalexec.c` (rasterizador, compute, blit), `mtlshader.c`/`mtlexec.c` (compilador e interpretador de MSL). Shaders só do fonte, sem `.metallib` (AIR). Chromium/Electron continuam sem device. Ver `docs/metal.md` |

## Sistema

| Item | Estado | Onde |
|---|---|---|
| Mach IPC (ports, direitos, mensagens) | **feito** | `kernel/proc/machipc.c` + `mach.c` (teste `userland/machtest.c`) |
| XPC | **feito (sem serviços)** | **`xpc.c` (novo)**: todos os tipos de objeto, e conexões que se comportam como num Mac sem o serviço (`XPC_ERROR_CONNECTION_INVALID`). Teste `tools/build-xpctest.ps1`: 24/24. Falta um launchd com serviços XPC registrados |
| Memória virtual (`mmap`, `fork`, COW) | **feito** | `kernel/proc/process.c` + `kernel/mm/lazy.c`: PML4 por processo; `mmap` anônimo sob demanda (página zerada na primeira falta); `mmap` de arquivo `MAP_PRIVATE`/só leitura sob demanda (cada página lida do arquivo na falta, `SYS_MMAP` b = 4); `fork` (`SYS_FORK`) duplica o espaço em cópia na escrita com contagem atômica de donos por página física, copia fds, pasta atual, grupo/sessão e só a thread que chamou; `vfork` = `fork` e `posix_spawn` intacto (`extra.c`). Teste `userland/teste.c` (fork + COW no heap, na memória sob demanda e no mmap de arquivo; `wait`/`waitpid`). Falta: `MAP_SHARED` gravável de arquivo (ainda é cópia, sem gravar de volta), bits de proteção no PTE (NX desligado) e paginação para disco |
| Syscalls Darwin | **parcial** | o libSystem traduz para as syscalls do mOS (`int 0x80`, `kernel/include/syscall.h`) |
| Launch Services | **parcial** | `LSOpen*`/`LSCopy*` em `chrome.c`. O mOS abre `.app` pelo Finder/Dock |
| Bundle loading (`.app`, `Info.plist`, `NSBundle`) | **feito** | `kernel/compat/bundle.c`, `plist.c` |
| Rede (TCP/DNS/TLS para os apps) | **feito** | `kernel/net/usersock.c`, `sockets_inet.inc`, `securetransport.c` |

## Electron

| Item | Estado | Onde |
|---|---|---|
| Tudo acima | **parcial** | o caminho está em [porte-vscode.md](porte-vscode.md) e [porte-chrome.md](porte-chrome.md) |
| JIT do V8 | **parcial** | no x86_64 o V8 só precisa de páginas RWX (`MAP_JIT` é exigência do Hardened Runtime/arm64) |
| Private APIs comuns do Chromium (CGS*, CAContext, IOSurface) | **parcial** | `quartzcore.c`, `iosurface.c`, `chrome.c` |
| Glifos (Skia → `CTFontDrawGlyphs` num `CGBitmapContext`) | **feito** | `coretext.c`: raster da fonte do kernel (`GUI_FONT`/`GF_RASTER`) |
| Área de transferência (`NSPasteboard`, `NSPasteboardItem`) | **feito** | `appkit.c`: pasteboards com nome, `changeCount`, vários tipos (dados/texto/plist), dono que fornece sob demanda; o texto do geral é compartilhado entre apps por `~/.mOS-pasteboard` |
| Teclado e texto (`interpretKeyEvents:`, `NSTextInputContext`, `inputContext`) | **feito** | `appkit_input.c`: hover; teclas de edição/navegação viram `doCommandBySelector:`, texto vira `insertText:replacementRange:`; o window server agora entrega também "tecla solta" (`keyUp:`). **IME CJK** (`appkit_ime.c`): ⌃Espaço troca a fonte de entrada (global, no kernel); japonês romaji→kana→kanji (dicionário embutido + `ja.dict`/SKK), chinês pinyin com candidatos e escolha parcial, coreano 2-Set com jamo compostos; composição como marked text (`setMarkedText:selectedRange:replacementRange:`), janelinha de candidatos desenhada pelo window server em `firstRectForCharacterRange:`; campos nativos mostram a composição na janelinha. Fonte CJK de fallback carregada de `/System/Library/Fonts/mOS-CJK.ttf` (TrueType glyf) se existir — sem ela o kernel desenha quadrados |
| Arrastar e soltar (`NSDraggingItem`/`Session`, `registerForDraggedTypes:`) | **feito** | kernel (`guisys_dragime.inc`): sessão de arrasto global, o window server segue o cursor e entrega ENTER/MOVE/EXIT/DROP à janela de qualquer app sob ele; conteúdo do pasteboard serializado (texto, dados, plist) e lido pelo destino (`DG_DATA`); cursor "copiar" quando o destino aceita. `appkit_input.c`: `NSDraggingInfo`, `draggingEntered:`/`Updated:`/`Exited:`, `prepare`/`perform`/`concludeDragOperation:`, `draggingSession:movedToPoint:`/`endedAtPoint:operation:` na fonte |
| Frameworks de nicho | **feito** | `gamecontroller.c`: GameController (controles do kernel `dev/gamepad.c`: HID USB genérico, DualShock 4/DualSense, Xbox 360 e Xbox One/Series; `GCExtendedGamepad`/`GCMicroGamepad`/`GCPhysicalInputProfile` com todos os handlers, `GCKeyboard`, `GCMouse`, vibração) e CoreHaptics (motor, padrões e AHAP, players simples e avançados; vibra o controle; eventos de áudio no mixer pelo `DEV_AUDIO_PLAY`). `corebluetooth.c`: CoreBluetooth central com cliente GATT completo sobre o canal ATT cru do kernel (`ble.c`) e papel periférico (`CBPeripheralManager` anuncia pelo `DEV_BT_ADVERTISE` e serve GATT: serviços mutáveis, leituras/escritas pelo delegate, CCCD e notificações — `corebluetooth_server.inc`); uma conexão LE por vez. `vision.c`: QR Code (v1–40, Reed-Solomon, inclusive o modo kanji pela tabela Shift-JIS de `tools/gen-sjis-table.ps1`), EAN-13/UPC-A/EAN-8, Code 128 (A/B/C), retângulos (Sobel + quadriláteros, com minimumSize/AspectRatio/quadratureTolerance) e feature print; faces/texto/classificação rodam sem resultados (sem os modelos de ML da Apple). `cryptokit.c` + módulo Swift `CryptoKit` (`tools/build-cryptokit.ps1`) para os apps Swift do mOS: SHA-2, Insecure, HMAC, HKDF, AES-GCM (128/192/256), AES-KW, ChaChaPoly, Curve25519 (X25519/Ed25519), P-256/384/521 (ECDSA com DER/PEM, ECDH). Falta o `CryptoKit.framework` no ABI da Apple: espera o overlay Swift do Foundation com interop (fase 5 de `docs/swift-objc-runtime.md`); o mesmo fonte serve. Testes: `userland/samples/{Entrada,Jogo,Sensor,Farol}`, `userland/tests-cryptokit`, `userland/samples-swiftui/Cripto` |
| Símbolos importados pelo VS Code (`machocheck`) | **parcial** | 2166 → 690 imports sem implementação. **Com 0 ausentes**: Electron Framework, os 4 Code Helper, libffmpeg, Squirrel/Mantle/ReactiveObjC e os módulos nativos pty, spdlog, sqlite3, keymapping, policy-watcher, fs-copyfile, watchdog, os-proxy-resolver e kerberos. Entraram: o libSystem.B.dylib inteiro (`vscode_libc.c`), CommonCrypto (`commoncrypto2.c`, conferido com vetores da RFC 6070/FIPS-197), CoreFoundation (`vscode_cf.c`), constantes com os valores do macOS (`vscode_consts*.c`, geradas), `NSConstantArray`/`NSConstantDictionary` (`constobjs.c`), Foundation (`foundation_extra.c`: NSMutableData, NSTimeZone, NSCalendar, NSOrderedSet, NSInvocation...), AppKit (`appkit_extra.c`: painéis, barra de status, NSAnimation, NSParagraphStyle, NSTrackingArea...), frameworks do sistema (`vscode_frameworks.c`, `vscode_media.c` com vDSP conferido, `vscode_network.c`, `vscode_fwclasses.c`). A libc++ dos módulos nativos agora é a do LLVM de verdade: `tools/build-libcxx.ps1` compila a libc++ e a libc++abi do código-fonte do zig como Mach-O e o `build.ps1` instala em `/usr/lib/libc++.1.dylib` (o carregador carrega dylib de caminho absoluto do disco); as 164 importações dela existem no libSystem e ela cobre os 183 imports de libc++ do `@parcel/watcher` e do `vsda` (o `machocheck` do app ainda as conta porque não olha `/usr/lib`). Faltam as bibliotecas opcionais do Copilot (MSAL/OneAuth); GameController, CoreHaptics, CoreBluetooth e Vision agora são implementações de verdade (linha "Frameworks de nicho") |
| Desempenho de renderização | **falta** | sem GPU. O compositor é por CPU multinúcleo |

## Segurança e assinatura

| Item | Estado | Onde |
|---|---|---|
| Assinatura de código | **parcial** | tudo é "não assinado" (`errSecCSUnsigned`), como o macOS trata código sem assinatura. O rendezvous do Chromium aceita |
| Sandbox (`sandbox_init`, `sandbox_apply`, `sandbox-exec`) | **parcial** | `darwin.c`: `sandbox_init`, `sandbox_compile_string`/`sandbox_apply` (libsandbox dos helpers do Electron), parâmetros e extensões respondem "sucesso" sem aplicar política, e `sandbox_check` diz "em sandbox" depois. `sandbox-exec -p/-f ... comando` (Codex) é tratado no `execve`/`posix_spawn` (`extra.c`): roda o comando direto |
| Entitlements, Gatekeeper, notarização | **falta** | ignorados de propósito, como o roadmap recomenda no começo |

## Referências

O mOS é MIT, e isso decide como cada referência pode ser usada:
- **Cocotron** (github.com/cjwl/cocotron, MIT): dá para aproveitar código no AppKit/Foundation, com atribuição.
- **Apple Open Source** (opensource.apple.com, APSL 2.0, copyleft por arquivo): referência de comportamento de dyld, objc4,
  Security, libdispatch e xnu. Um arquivo copiado continua APSL, então é melhor reescrever.
- **GNUstep** (gnustep.org, LGPL): só referência. Copiar código traria a LGPL para dentro do libSystem.
- **Apportable Foundation** (github.com/apportable/Foundation): derivada do CoreFoundation da Apple (APSL), então vale a regra
  da Apple.
- **Darling**: a referência de arquitetura (darlingserver ≈ o `machipc.c` do mOS).
