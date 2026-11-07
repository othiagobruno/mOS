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
| QuartzCore / CALayer / CAContext | **parcial** | `quartzcore.c`, `iosurface.c` |
| Integração com o window server do mOS | **feito** | `guisys.c` (janelas do mOS ↔ NSWindow, eventos → NSEvent) |
| Metal | **parcial** | Metal por software (sem driver de GPU): `metal.c` (API, `CAMetalLayer`, `MTKView`), `metalexec.c` (rasterizador, compute, blit), `mtlshader.c`/`mtlexec.c` (compilador e interpretador de MSL). Shaders só do fonte, sem `.metallib` (AIR). Chromium/Electron continuam sem device. Ver `docs/metal.md` |

## Sistema

| Item | Estado | Onde |
|---|---|---|
| Mach IPC (ports, direitos, mensagens) | **feito** | `kernel/proc/machipc.c` + `mach.c` (teste `userland/machtest.c`) |
| XPC | **feito (sem serviços)** | **`xpc.c` (novo)**: todos os tipos de objeto, e conexões que se comportam como num Mac sem o serviço (`XPC_ERROR_CONNECTION_INVALID`). Teste `tools/build-xpctest.ps1`: 24/24. Falta um launchd com serviços XPC registrados |
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
| Teclado e texto (`interpretKeyEvents:`, `NSTextInputContext`, `inputContext`) | **parcial** | `appkit_input.c`: teclas de edição/navegação viram `doCommandBySelector:`, texto vira `insertText:replacementRange:`. Sem composição de IME (teclas mortas, CJK) |
| Arrastar e soltar (`NSDraggingItem`/`Session`, `registerForDraggedTypes:`) | **parcial** | `appkit_input.c`: os itens vão ao pasteboard de arrasto e a sessão termina na hora (`NSDragOperationNone`); o servidor de janelas ainda não leva o arrasto a outra janela |
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
