# O que ainda falta para rodar todos os apps de macOS nativamente

Estado em 8 nov. 2026. A ordem de trabalho acordada foi **SwiftUI → WebKit → UDP + AVFoundation → `.metallib` (AIR) →
arm64 → x86_64**. Este documento resume o que está pronto, o que está em andamento e o que falta, por frente.

## Resumo

| Frente | Estado | Verificação |
|---|---|---|
| WebKit / WKWebView | ✅ **completo** | 55/55 no QEMU ([webkit.md](webkit.md)) |
| UDP para os apps | ✅ **completo** | 34/34 no QEMU |
| CoreAudio + AVFoundation (áudio) | ✅ **completo** | 42/42 no QEMU + WAV conferido no host |
| `.metallib` (AIR) | 🟡 **parser pronto, lowering em andamento** | 14/14 .metallib reais lidos ([metallib-air.md](metallib-air.md)) |
| arm64 → x86_64 | 🟡 **em andamento (outra sessão)** | runtime e libSystem arm64 em curso |
| SwiftUI | 🟡 **em andamento (outra sessão)** | para no AppKit (Postico) |

As barreiras estruturais e os frameworks ausentes continuam valendo como em [compat-apps.md](compat-apps.md) e
[checklist-compat.md](checklist-compat.md); abaixo só o que falta de cada frente desta ordem.

## 1. WebKit — completo

`WKWebView` sobre o motor web do sistema (SYS_WEB, `kernel/web/webview.c`), `userland/frameworks/WebKit/WebKit.m`,
no `build.ps1`. Testes: `tools/build-webkittest.ps1` (55/55). **Falta (fora de escopo imediato):** iframes, vídeo/
áudio no `<video>`/`<audio>`, WebGL/canvas 3D, SVG, WebP/AVIF, PDF/impressão; diálogos JS não bloqueiam a página;
esquemas próprios só servem a navegação principal. Ver [webkit.md](webkit.md).

## 2. UDP — completo

`kernel/net/userudp.c` + `userland/libsystem/sockets_udp.inc`. IPv4/IPv6 dual-stack, laço local, connect, poll/kqueue,
truncamento, SO_RCVTIMEO, DNS real. Teste: `tools/build-udptest.ps1` (34/34). **Falta (fora de escopo):** UDP multicast
de entrada por grupo (join), e o microfone (entrada de áudio) para os apps.

## 3. AVFoundation + CoreAudio — completo (áudio)

Streams de áudio por processo no mixer (`kernel/dev/sound.c`, DEV_AUDIO_*), `userland/libsystem/coreaudio.c` e
`avfaudio.c`: HAL, AudioUnit, AudioQueue, AudioConverter, AudioFile/ExtAudioFile (WAV/AIFF/CAF), AudioServices,
AVAudioPlayer/Engine/Session, NSSound, AVPlayer (áudio local). Teste: `tools/build-audiotest.ps1` (42/42) + WAV da
saída do QEMU conferido no host. **Falta:**
- **decodificadores comprimidos**: MP3, AAC, ALAC, Opus (hoje devolvem `kAudioFileUnsupportedDataFormatError`);
- **microfone** para os apps (captura);
- **vídeo**: AVFoundation de vídeo (AVPlayer com imagem), AVCaptureSession real, VideoToolbox (decodificação) —
  tudo ainda stub.

## 4. `.metallib` (AIR) — parser pronto, lowering em andamento

Objetivo: executar os `.metallib` pré-compilados (AIR = bitcode LLVM) no Metal por software, já que muitos apps não
trazem o fonte `.metal`. Ver [metallib-air.md](metallib-air.md) e a memória [[metallib-air-parser]].

**Pronto e validado (host, sem VM):**
- parser do contêiner MTLB (`userland/libsystem/mtlair.c`);
- parser do módulo de bitcode LLVM (`mtlair_bc.inc` bitstream + `mtlair_mod.inc` tipos/funções/metadados/corpos);
- base do lowering (`mtlair_lower.inc`): modelo de valores SSA, mapa de tipos LLVM→`mtlint.h`, tabela de intrínsecos
  `air.*`, passo de cobertura;
- ferramenta de host `tools/airdump.c` (`-h` histograma de opcodes, `-i` despeja instruções, `-c` cobertura);
- lê **os 14 `.metallib` reais do WhatsApp** sem erro, com nomes, tipos, metadados e corpos (2–220 instruções).

**Conjunto de opcodes medido** (17 distintos; as `CALL` são os intrínsecos `air.*`): BINOP, CALL, EXTRACTELT,
INSERTELT, GEP, BR, PHI, RET, LOAD, SHUFFLEVEC, CMP2, CAST, EXTRACTVAL, STORE, VSELECT, INSERTVAL, ALLOCA.

**Falta:**
- **emissão dos nós**: traduzir cada um dos 17 opcodes (com controle de fluxo BR/PHI) e cada intrínseco `air.*`
  (amostragem de textura, conversões, acesso a `[[buffer]]`/`[[texture]]` via metadados) para a árvore de nós que
  `mtlexec.c` executa. É um arquivo do tamanho do `mtlshader.c` (~2000 linhas);
- **binding dos argumentos**: ligar os `[[buffer(n)]]`/`[[texture(n)]]`/`[[stage_in]]` do AIR (descritos nos
  metadados `air.*`) ao `msl_ctx_t` do interpretador;
- **ligação no `metal.c`**: `lib_from_bytes`/`is_air` hoje devolvem erro; passar a chamar o lowering e só cair no erro
  quando algo não for traduzível;
- **teste de execução**: rodar um `.metallib` real e comparar a saída com o mesmo shader compilado do fonte.

## 5. arm64 → x86_64 — em andamento (outra sessão)

Tradutor binário (nível Rosetta 2) para rodar os apps só-arm64, que são cada vez mais. É um projeto grande, dividido
em frentes paralelas por outra(s) sessão(ões) — ver as memórias [[tradutor-arm64]] (processo todo arm64, contrato em
`userland/a64/a64.h`, 7 frentes), [[tradutor-arm64-runtime]] (`a64rt.mos` + libSystem arm64; testes no machotest e no
QEMU), [[libsystem-arm64-triplo-elf]] (triplo `arm64-apple-macos11-elf`, darwinpcs/variádicos) e
[[qemu-windows-fma-bug]] (FMA do mingw erra 1 ulp).

**Falta (em alto nível):** cobrir o conjunto de instruções arm64 inteiro (inclusive NEON/SIMD), o modelo de memória
(ordenação arm64 vs x86), código gerado em tempo de execução (JIT dos apps), e amarrar tudo ao carregador Mach-O e ao
libSystem arm64. Não se conclui numa sessão.

## 6. SwiftUI — em andamento (outra sessão)

O `SwiftUI.framework` com interop ObjC está sendo construído por outra sessão (`tools/build-swiftui-objc.ps1`), sobre
o runtime Swift + Foundation + Combine + UIKit/Catalyst que já existem. Ver a memória [[swiftui-objc-framework]] e
[swift-objc-runtime.md](swift-objc-runtime.md) (fase 6).

**Falta:** o próprio SwiftUI de ponta a ponta — hoje o Postico sobe mas **para no AppKit**; `NSHostingView`/
`NSHostingController` como subclasses reais de `NSView`/`NSViewController`, e os controles (`ScrollView`, `List`,
`TabView`, `popover`, `glassEffect`, `Color(nsColor:)`, `Image(_:bundle:)`...). É a barreira que destrava a maioria
dos apps nativos modernos; o WhatsApp ainda tem milhares de símbolos SwiftUI ausentes.

## Além desta ordem (contexto)

Barreiras que seguem valendo para "todos os apps", fora das seis frentes acima:
- **WebKit só carrega áudio/web, não vídeo**; **sem CoreML/Vision com modelos da Apple**; **AppKit/CoreText/CoreGraphics
  parciais**; **assinatura de código sempre "não assinada"**.
- **iCloud/CloudKit, APNs, Sign in with Apple nativo, App Store** dependem de servidores e atestação da Apple — não
  implementáveis localmente.
