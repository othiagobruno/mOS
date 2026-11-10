# O que ainda falta para rodar todos os apps de macOS nativamente

Estado em 8 nov. 2026. A ordem de trabalho acordada foi **SwiftUI → WebKit → UDP + AVFoundation → `.metallib` (AIR) →
arm64 → x86_64**. Este documento resume o que está pronto, o que está em andamento e o que falta, por frente.

## Resumo

| Frente | Estado | Verificação |
|---|---|---|
| WebKit / WKWebView | ✅ **completo** | 55/55 no QEMU ([webkit.md](webkit.md)) |
| UDP para os apps | ✅ **completo** | 34/34 no QEMU |
| CoreAudio + AVFoundation (áudio) | 🟡 **LPCM completo; MP3/ALAC no host** | 42/42 no QEMU é resultado antigo, não reexecutado. MP3/ALAC: `tools/audiodec_test.c` |
| `.metallib` (AIR) | 🟡 **17 opcodes no host** | diferencial clang vs fonte (`tools/airlower_test.c`); sem `.metallib` de app nesta entrega |
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

## 3. AVFoundation + CoreAudio — áudio LPCM completo; comprimido e captura parciais

Streams de áudio por processo no mixer (`kernel/dev/sound.c`, DEV_AUDIO_*), `userland/libsystem/coreaudio.c` e
`avfaudio.c`: HAL, AudioUnit, AudioQueue, AudioConverter, AudioFile/ExtAudioFile (WAV/AIFF/CAF), AudioServices,
AVAudioPlayer/Engine/Session, NSSound, AVPlayer (áudio local). Teste antigo: `tools/build-audiotest.ps1` (42/42) + WAV da
saída do QEMU conferido no host. Esse teste de VM **não foi reexecutado** nesta entrega.

**Funciona (host, `tools/audiodec_test.c`):**
- MP3 (minimp3, CC0) e ALAC de 16 bits em CAF e M4A (decodificador de referência da Apple, Apache-2.0). O ALAC bate
  bit a bit com o ffmpeg. O MP3, depois do atraso do lame, teve correlação 1 no seno de teste. Ver
  [midia-licencas.md](midia-licencas.md). O CoreAudio troca o arquivo por esse PCM e o AVPlayer toca o áudio.

**Reconhecido, sem PCM (limitação):**
- AAC (ADTS e `mp4a`) e Opus (Ogg e CAF). Continuam `kAudioFileUnsupportedDataFormatError`. FAAD2 é GPL e não entra.
  fdk-aac (BSD) e libopus (BSD-3) não foram vendidos.

**Microfone:** `DEV_AUDIO_READ` lê `audio_capture_read` (48 kHz estéreo i16). `AudioQueueNewInput` e
`AudioUnitRender` no barramento de entrada só seguem se `cx_tcc_request("kTCCServiceMicrophone")` devolver 1.
Sem o símbolo, ou com negação, a API devolve erro e não captura. 0 quadros da captura viram silêncio marcado, não
um sinal inventado. Não houve teste em VM nem em hardware: o anel de captura não foi exercitado.

**Vídeo:** o AVPlayer não produz quadro. `AVCaptureSession` tem o portão de TCC (câmera/microfone) e lista de
dispositivos vazia; `startRunning` com câmera concedida fica `isRunning` sem quadro. Sem FairPlay.
`AVCaptureScreenInput` só muda o portão para `kTCCServiceScreenCapture`; não fabrica quadro.
A captura contínua (`SCStream`, `CGDisplayStream`) lê `DEV_SCREEN_CAPTURE` depois de `cx_tcc_preflight` e
embrulha o ARGB em IOSurface, CVPixelBuffer e CMSampleBuffer. Codificar e decodificar H.264/HEVC no
VideoToolbox continua -12906/-12908. `VTPixelTransferSession` copia o buffer quando formato e tamanho coincidem.

## 4. `.metallib` (AIR) — opcodes emitidos e conferidos no host contra o fonte

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

**Feito nesta entrega (host, sem VM):** emissão dos 17 opcodes, intrínsecos `air.*`/`llvm.*` conhecidos, binds
explícitos e o fallback de ponteiro para `[[buffer]]`, e `metal.c` chamando `mtlair_lower`. O teste
`tools/airlower_test.c` compara o bitcode do clang com o shader compilado do fonte (0 falhas). Detalhe em
[metallib-air.md](metallib-air.md).

**Ainda não conferido:** um `.metallib` real de app. Função com `double`, `icmp` vetorial sem sinal, atômico,
`fcmp` não ordenado ou pack/unpack é omitida. Textura/sampler e `thread_position_in_grid` dependem de metadado ou
de bind explícito; sem isso o tid fica 0.

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
