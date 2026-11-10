# Inventário: Open Bot e Usagebar (10 nov. 2026)

Inventário de compatibilidade, sem código de produção. O método é o de [compat-apps.md](compat-apps.md):
`tools/machocheck.c` contra o `libSystem.mos` montado dos fontes atuais. "Ausente" é um import que não está nos
exports do próprio bundle, nem no `libSystem.mos`, e não é weak. O binário dos apps não entra no repositório.

FairPlay e qualquer outro DRM ficam só como limitação. Nada disso foi contornado, e não se buscou cópia pirata.

## Open Bot: origem

O pedido não trazia link. [compat-apps.md](compat-apps.md) deixa o nome "Open Bot" sem origem fechada. O pacote
medido aqui é o app de mesa cujo nome publicado é **OpenBot** (grafia junta), porque é o que tem instalador Intel
gratuito no site do fabricante. Se o nome do pedido for outro produto, estas contagens não se aplicam.

| | |
|---|---|
| Site | https://openbot.run/ (download Intel: https://openbot.run/download/macos) |
| Código | https://github.com/nightly-labs/openbot (Synthetify Labs) |
| Versão medida | 0.66.0 (`CFBundleIdentifier` `app.openbot.desktop`, `LSMinimumSystemVersion` 13.0) |
| Motor | Electron 44.9.3 (Chrome/152.0.7977.167) |
| Licença do app | PolyForm Noncommercial 1.0.0. O site diz que o app custa US$ 0 para uso não comercial |

O botão "Download for Intel Mac" responde HTTP 302 para
`https://github.com/nightly-labs/openbot/releases/latest/download/OpenBot-0.66.0-x64.dmg`.
A mesma release publica o zip do `.app`. Foi esse zip que se mediu, porque abre sem leitor de DMG. O DMG não foi
montado. Os dois checksums estão em
`https://github.com/nightly-labs/openbot/releases/download/v0.66.0/SHA256SUMS-macos-x64.txt`.

```
473091aac28ccab16be5921c9477f6deef8f2d9523d6c87962c99581821f817f  OpenBot-0.66.0-x64.zip
cee482aa7694a601800dfb8f1a0f561189427ab4c7826a965b9d2c1b5d3bf4e3  OpenBot-0.66.0-x64.dmg
```

O `sha256sum` do zip baixado bate com a primeira linha. O binário não foi modificado.

Homônimos que **não** foram medidos, porque não são esse app: o botão "Open Bot" da Clavue abre uma página web
(`https://www.clavue.com/bot/`); `github.com/CopilotKit/openbot` é um serviço em Docker; OpenClaw é outro produto.

Fuses do Electron (depois de `dL7pKGdnNz796PbbjQWNKmHXBZaB9tsX` no Electron Framework): `010011001`, a mesma sequência
do Slack em [compat-apps.md](compat-apps.md). O `Info.plist` traz `ElectronAsarIntegrity`.

## Arquitetura e FairPlay

Leitura dos cabeçalhos Mach-O dos 25 arquivos reais (sem seguir symlink):

| | |
|---|---|
| Finos x86_64 | 24 |
| Universal | 1: `Contents/Resources/cua-driver/darwin/x64/cua-driver` (x86_64 e arm64) |
| Só arm64 | 0 |
| `LC_ENCRYPTION_INFO_64` com cryptid diferente de zero | 0 |

O pacote medido é o DMG/zip notarizado da release no GitHub, não um pacote da Mac App Store. Não há texto cifrado
FairPlay nesses slices. O slice arm64 do `cua-driver` não entrou no `machocheck` (a ferramenta só resolve o slice
x86_64, `cputype` `0x01000007`). Não se compilou o `libSystem` arm64.

## Como o número foi obtido

`libSystem.mos`: zig 0.17.1 para Linux (`3581863e3cbcfd0fce0efc682ce7e9e93da96668a977791d356a26d2a7e9b17a`, o mesmo
0.17.1 de [auto-hospedagem.md](auto-hospedagem.md)), com as flags de `tools/build-libsystem.ps1` (libunwind do zig +
`zig cc -target x86_64-freestanding-none` dos `userland/libsystem/*.{c,S}` desta árvore). Não se rodou o script: ele
reescreve `kernel/compat/libsystem_blob.S`. O `.eh_frame` também não entra nessa conta.

O link dos fontes atuais para antes de terminar: `userspace_services.c` referencia `SKPaymentQueue_c` e
`UNUserNotificationCenter_c`, e `vscode_fwclasses.c` publica esses objetos só com o nome
`OBJC_CLASS_$_SKPaymentQueue` e `OBJC_CLASS_$_UNUserNotificationCenter`. Para o ELF existir, o link deste inventário
acrescentou dois símbolos locais com o nome C, fora do repositório. O app importa o nome `OBJC_CLASS_$_*`, que já
sai de `vscode_fwclasses.c`. O ELF exporta **6977** símbolos.

```
gcc -O2 -o machocheck tools/machocheck.c
./machocheck libSystem.mos OpenBot.app relatorio.txt
```

No `.app` extraído do zip, esse comando imprime **43** imagens, **29399** imports, **1352** ausentes e **844**
símbolos distintos. Quatorze caminhos são symlink (`Versions/Current` e o nome no topo de Electron, Mantle,
ReactiveObjC e Squirrel). O `stat` do `machocheck` segue esses links, então o Electron Framework (0 ausentes) entra
três vezes e o Squirrel (4 ausentes) também. A diferença de ausentes, 1352 − 1344 = 8, é o Squirrel contado três
vezes em vez de uma.

A tabela abaixo é o mesmo `machocheck`, no mesmo `libSystem.mos`, sobre uma cópia só de arquivos regulares (os
symlinks de diretório não foram descidos). Cada arquivo uma vez.

## OpenBot 0.66.0: cada arquivo uma vez

**16495** imports, **15151** resolvidos, **1344** sem implementação, **844** símbolos distintos.
Exports do bundle: 55374. Imagens x86_64: 25.

A tabela por biblioteca que o `machocheck` imprime pula o que está em `@rpath` (`bundled`). A soma dessa tabela é
15566 imports e os 1344 ausentes. Os outros 929 imports (16495 − 15566) são de bibliotecas do próprio bundle e estão
todos resolvidos.

Os 1344 ausentes estão em sete imagens. O executável, os quatro helpers, o Electron Framework, libffmpeg,
libvk_swiftshader, Mantle, ReactiveObjC, ShipIt e os binários do Sunshine que não são o `Sunshine` em si fecham com
0 ausentes.

| Imagem | MiB | Imports | Resolvidos | Ausentes |
|---|---:|---:|---:|---:|
| `Contents/MacOS/OpenBot` | 0,0 | 10 | 10 | 0 |
| `Resources/cua-driver/darwin/x64/cua-driver` | 36,1 | 995 | 585 | 410 |
| `Resources/remote-desktop-runtime/.../Sunshine` | 24,0 | 3092 | 2745 | 347 |
| `.../libcrypto.3.dylib` | 5,0 | 157 | 157 | 0 |
| `.../libssl.3.dylib` | 0,9 | 613 | 613 | 0 |
| `.../libminiupnpc.21.dylib` | 0,1 | 54 | 54 | 0 |
| `.../streamer` | 19,1 | 228 | 228 | 0 |
| `.../web-server` | 18,3 | 232 | 232 | 0 |
| `app.asar.unpacked/.../koffi.node` | 1,5 | 264 | 253 | 11 |
| `Resources/voice/runtime/libsherpa-onnx-c-api.dylib` | 4,4 | 751 | 552 | 199 |
| `Resources/voice/runtime/sherpa-onnx.node` | 0,6 | 272 | 245 | 27 |
| `Resources/voice/runtime/libonnxruntime.dylib` | 30,8 | 3003 | 2657 | 346 |
| Electron Framework | 210,2 | 5175 | 5175 | 0 |
| `app_shim_loader` | 0,0 | 22 | 22 | 0 |
| `chrome_crashpad_handler` | 1,2 | 347 | 347 | 0 |
| `libffmpeg.dylib` | 2,5 | 104 | 104 | 0 |
| `libvk_swiftshader.dylib` | 4,4 | 158 | 158 | 0 |
| Mantle | 0,1 | 110 | 110 | 0 |
| OpenBot Helper (GPU) | 0,2 | 93 | 93 | 0 |
| ReactiveObjC | 0,4 | 186 | 186 | 0 |
| OpenBot Helper | 0,2 | 93 | 93 | 0 |
| Squirrel | 0,2 | 210 | 206 | 4 |
| ShipIt | 0,1 | 140 | 140 | 0 |
| OpenBot Helper (Renderer) | 0,2 | 93 | 93 | 0 |
| OpenBot Helper (Plugin) | 0,2 | 93 | 93 | 0 |

### Imports por biblioteca de sistema

"Resolvidos" = total − ausentes, na linha que o `machocheck` imprimiu. A marca `(biblioteca inexistente no mOS)`
dessa ferramenta usa a lista curta `sys_ok` em `tools/machocheck.c`, menor que `libsystem_dylib_ok` em
`kernel/compat/macho.c`. Security, Metal, CFNetwork e a libc++ já estão na lista do kernel. A coluna de ausentes é a
que conta símbolo, e não depende dessa marca.

| Biblioteca | Imports | Resolvidos | Ausentes |
|---|---:|---:|---:|
| libc++.1.dylib | 3888 | 3029 | 859 |
| (flat/self) | 4791 | 4789 | 2 |
| libSystem.B.dylib | 3434 | 3424 | 10 |
| CoreFoundation.framework | 557 | 479 | 78 |
| AppKit.framework | 406 | 406 | 0 |
| libobjc.A.dylib | 312 | 312 | 0 |
| Foundation.framework | 230 | 210 | 20 |
| CoreGraphics.framework | 223 | 217 | 6 |
| Security.framework | 173 | 171 | 2 |
| CoreVideo.framework | 163 | 148 | 15 |
| libswiftCore.dylib | 162 | 0 | 162 |
| CoreMedia.framework | 127 | 80 | 47 |
| ApplicationServices.framework | 90 | 89 | 1 |
| VideoToolbox.framework | 83 | 61 | 22 |
| IOKit.framework | 82 | 82 | 0 |
| IOSurface.framework | 78 | 63 | 15 |
| CoreText.framework | 75 | 75 | 0 |
| libcups.2.dylib | 51 | 51 | 0 |
| AVFoundation.framework | 49 | 42 | 7 |
| ScreenCaptureKit.framework | 49 | 41 | 8 |
| libswiftDispatch.dylib | 38 | 0 | 38 |
| Metal.framework | 36 | 36 | 0 |
| SystemConfiguration.framework | 31 | 31 | 0 |
| CoreAudio.framework | 31 | 29 | 2 |
| AudioToolbox.framework | 24 | 24 | 0 |
| CoreServices.framework | 24 | 24 | 0 |
| Carbon.framework | 23 | 23 | 0 |
| CFNetwork.framework | 22 | 22 | 0 |
| CoreMIDI.framework | 22 | 22 | 0 |
| QuartzCore.framework | 20 | 20 | 0 |
| Vision.framework | 20 | 20 | 0 |
| AuthenticationServices.framework | 17 | 17 | 0 |
| CoreML.framework | 17 | 16 | 1 |
| Accelerate.framework | 17 | 17 | 0 |
| UserNotifications.framework | 12 | 12 | 0 |
| libswift_Concurrency.dylib | 11 | 0 | 11 |
| AVFAudio.framework | 11 | 10 | 1 |
| MediaPlayer.framework | 11 | 11 | 0 |
| MediaAccessibility.framework | 1 | 1 | 0 |
| libcurl.4.dylib | 10 | 0 | 10 |
| UniformTypeIdentifiers.framework | 10 | 10 | 0 |
| CoreBluetooth.framework | 10 | 10 | 0 |
| libsandbox.1.dylib | 9 | 9 | 0 |
| CoreHaptics.framework | 9 | 9 | 0 |
| ForceFeedback.framework | 9 | 9 | 0 |
| libswiftUniformTypeIdentifiers.dylib | 8 | 0 | 8 |
| IOBluetooth.framework | 8 | 8 | 0 |
| CoreImage.framework | 7 | 7 | 0 |
| libswiftCoreFoundation.dylib | 6 | 0 | 6 |
| Network.framework | 6 | 6 | 0 |
| ServiceManagement.framework | 6 | 6 | 0 |
| libswiftCoreGraphics.dylib | 3 | 0 | 3 |
| libcompression.dylib | 3 | 0 | 3 |
| StoreKit.framework | 3 | 3 | 0 |
| libresolv.9.dylib | 3 | 3 | 0 |
| Accessibility.framework | 3 | 3 | 0 |
| CoreLocation.framework | 3 | 3 | 0 |
| libbz2.1.0.dylib | 3 | 3 | 0 |
| ImageIO.framework | 4 | 0 | 4 |
| OpenDirectory.framework | 4 | 4 | 0 |
| OpenGL.framework | 2 | 2 | 0 |
| QuickLookThumbnailing.framework | 2 | 2 | 0 |
| SecurityInterface.framework | 2 | 2 | 0 |
| libbsm.0.dylib | 2 | 2 | 0 |
| GameController.framework | 12 | 12 | 0 |
| libswiftMetal.dylib | 1 | 0 | 1 |
| libswiftObjectiveC.dylib | 1 | 0 | 1 |
| libz.1.dylib | 1 | 0 | 1 |
| QuickLookUI.framework | 1 | 1 | 0 |
| LocalAuthentication.framework | 1 | 1 | 0 |
| CoreMediaIO.framework | 1 | 1 | 0 |
| CoreWLAN.framework | 1 | 1 | 0 |
| CryptoTokenKit.framework | 1 | 1 | 0 |

`(flat/self)` é o nome curto que o `machocheck` imprime para o bind flat `(flat/self)`, não uma biblioteca chamada
`self`. Os dois símbolos ausentes aí são `__ZTISt12length_error` e `__ZTISt12out_of_range` (RTTI da libc++abi).

Os 844 símbolos distintos não se repartem por essa tabela: o relatório grava cada nome uma vez, na primeira
biblioteca que o importou. Os maiores grupos nessa atribuição: libc++ 364, libswiftCore 162, CoreFoundation 78,
CoreMedia 43, libswiftDispatch 38, VideoToolbox 22, Foundation 20.

Nomes nos grupos pequenos (primeira biblioteca):

| Biblioteca | Símbolos |
|---|---|
| libSystem.B.dylib | `_dispatch_async_and_wait`, `_DNSServiceRegister`, `_pthread_condattr_destroy`, `_pthread_condattr_setpshared`, `_pthread_mutexattr_setpshared`, `___muldc3`, `___mulsc3`, `_llroundl`, `_nftw$INODE64`, `_vsnprintf_l` |
| libcurl.4.dylib | `_curl_easy_cleanup`, `_curl_easy_escape`, `_curl_easy_init`, `_curl_easy_perform`, `_curl_easy_setopt`, `_curl_free`, `_curl_url`, `_curl_url_cleanup`, `_curl_url_get`, `_curl_url_set` |
| CoreGraphics.framework | `_CGContextSetBlendMode`, `_CGDisplayModeGetPixelWidth`, `_CGDisplayModeGetPixelHeight`, `_CGEventCreateScrollWheelEvent`, `_CGEventSetDoubleValueField` |
| ImageIO.framework | `_CGImageDestinationAddImage`, `_CGImageDestinationCreateWithURL`, `_CGImageDestinationFinalize`, `_kCGImageDestinationLossyCompressionQuality` |
| libcompression.dylib | `_compression_stream_destroy`, `_compression_stream_process`, `_compression_stream_init` |
| Security.framework | `_SSLSetSessionTicketsEnabled`, `_kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly` |
| CoreAudio.framework | `_AudioHardwareCreateProcessTap`, `_AudioHardwareDestroyProcessTap` |
| ApplicationServices.framework | `__AXUIElementCreateWithRemoteToken` |
| AVFAudio.framework | `_AVNumberOfChannelsKey` |
| CoreML.framework | `_OBJC_CLASS_$_MLDictionaryFeatureProvider` |
| libz.1.dylib | `_crc32` |
| libswiftMetal.dylib | `_$sSo9MTLBufferP5MetalE14didModifyRangeyySnySiGF` |
| libswiftObjectiveC.dylib | `_$sSo8NSObjectC10ObjectiveCE9hashValueSivg` |

A libc++ (859 imports, 364 nomes) e as `libswift*` (230 nomes, todos ausentes: Core 162, Dispatch 38, Concurrency
11, UniformTypeIdentifiers 8, CoreFoundation 6, CoreGraphics 3, Metal 1, ObjectiveC 1) dominam o buraco desta
rodada. O `machocheck` de então só enxergava a libc++ e as `libswift*` quando o arquivo estava dentro da pasta
varrida. Nesta rodada não estava: `tools/build-libcxx.ps1` não foi executado, e as dylibs Swift do mOS não foram
copiadas para a pasta. A releitura com `--disco` está na seção seguinte.

### Dylibs de sistema que o bundle pede

O `machocheck` listou **96** caminhos absolutos. A lista `libsystem_dylib_ok` do kernel aceita a maior parte
(AppKit, Foundation, Security, Metal, CFNetwork, ScreenCaptureKit, AVFoundation, a libc++, `libbsm`, `libcups`,
`libsandbox`, `libresolv`, `libbz2`, `libiconv`, `libpmsample`, `libpmenergy`, entre outras). Não aparecem nessa
lista, lida em `kernel/compat/macho.c` nesta data:

- `CoreData`, `CloudKit`, `ImageIO`, `QuickLookUI`, `StoreKit`
- `/usr/lib/libcompression.dylib`
- as `libswift*` (o carregador ainda tenta `/usr/lib/swift/libswiftFoundation.dylib` no disco, se o arquivo existir)
- `/usr/lib/libz.1.dylib` e `/usr/lib/libcurl.4.dylib`: não estão na lista; `link_abs_dep` troca o caminho pela
  fórmula Homebrew correspondente, se esse arquivo existir no disco

## stage-electron

Comando, PowerShell 7.5.4, depois de criar só o diretório vazio `build-openbot-inv/esp/mOS` para passar da primeira
checagem:

```
pwsh -NoProfile -File tools/stage-electron.ps1 -App OpenBot.app -OutDir build-openbot-inv -Autostart -Diagnostico
```

Não produz imagem. Três motivos, nesta ordem:

1. Sem um `build.ps1 -OutDir` anterior, a linha que testa `$OutDir\esp\mOS` sai com "rode antes: .\build.ps1". O
   inventário não compila o sistema. O diretório vazio só serviu para ver a falha seguinte.
2. `Copy-Bin` usa o prefixo de caminho do Windows `\\?\`. No PowerShell 7.5.4 em Linux, `ReadAllText` procura
   `Contents\Info.plist` e cada `Copy` falha com "Could not find a part of the path" em `...\\?\...`. Nenhum Mach-O
   foi copiado. O que passou foi `Copy-Item` de recursos (20 arquivos, 158,7 MiB: `app.asar`, `.pak`, `icudtl.dat`,
   `v8_context_snapshot.x86_64.bin`, plists dos helpers).
3. O script termina chamando `$OutDir\mkimg.exe`. Esse executável não existe sem o `build.ps1`. A última linha foi
   `mkimg FAILED`.

Mesmo num Windows com imagem pronta, o `stage-electron.ps1` não copia `Resources/cua-driver`,
`remote-desktop-runtime` nem `voice`: só entra o que o script lista (executável, `Versions/A`, helpers, recursos do
Electron, `app.asar` e `app.asar.unpacked`). Quase todos os 1344 ausentes estão justamente nessas pastas que o stage
deixa de fora. A contagem deste documento é a do bundle inteiro.

## Releitura com `--disco` (10 nov. 2026)

O `machocheck` agora aceita `--disco DIR` (repetível; também aceita o arquivo). O caminho é o build ou a imagem.
A varredura fica em `libc++.1.dylib`, `libc++abi.dylib`, `libswift*.dylib` e na libunwind (`libunwind*.o`,
`Unwind*.o`, `libunwind.a` ou `libunwind.dylib`). Os exports entram na resolução, como o carregador lê
`/usr/lib/libc++.1.dylib` e `/usr/lib/swift/libswift*.dylib` do disco. O relatório agrupa os ausentes e a última
linha da saída é a conta:

```
# ausentes sistema: N
biblioteca	simbolo
# ausentes libc++: N
# ausentes Swift: N
# ausentes unwind: N
Ausentes por origem (simbolos distintos): sistema N, libc++ N, Swift N, unwind N
```

O balde libc++ inclui o bind flat de RTTI Itanium (`__Z…`). No OpenBot são 366, não 364: 364 de
`libc++.1.dylib` e os dois que o inventário acima já listava em `(flat/self)`, `__ZTISt12length_error` e
`__ZTISt12out_of_range`.

### O que havia para apontar

Compilado nesta VM, fora do repositório (não se rodou `build.ps1` nem `patch-ehframe.ps1`, e
`kernel/compat/libsystem_blob.S` não foi reescrito).

| Peça | Origem | O que o `--disco` leu |
|---|---|---|
| `libc++.1.dylib` | zig 0.17.1 (`https://ziglang.org/download/0.17.1/zig-x86_64-linux-0.17.1.tar.xz`), flags de `tools/build-libcxx.ps1`, 0 falhas de compilação | 1 arquivo Mach-O x86_64, **2356** símbolos |
| libunwind | os cinco objetos que `tools/build-libsystem.ps1` compila do LLVM que vem com o zig | 5 arquivos, **102** símbolos |
| `libswift*.dylib` | não estão no repositório, no zig nem na imagem. O compilador Swift não está nesta VM | **0** arquivos |

O binário do zig usado aqui é `58959f01285c05a74276f09327b551d42ed6006bf325a78dc661a024e314987b`. Não é o
`3581863e…` citado no inventário de cima. `userland/libsystem/fi_swift.c` não entrou no ELF: o protótipo local
`cx_cstr(void *)` conflita com `cx_cstr(id)` de `foundation.h`. O `libSystem.mos` com a libunwind exporta **7188**
símbolos globais; sem os cinco objetos, **7144**. Sem o `patch-ehframe.ps1`, `unw_getcontext` sai `WEAK` e o
`machocheck` não o indexa no ELF (o script promove `unw_*` / `_Unwind_*` / `__unw_*` de `WEAK` para `GLOBAL`).
Promovi só esse símbolo, do mesmo jeito, numa cópia. Com isso o balde unwind do ELF completo é 0.

Não há `libswift*` para ler. O balde Swift não muda.

### OpenBot de novo

O mesmo zip, o mesmo `sha256` `473091aa…`. `gcc -O2 -Wall -Wextra -o machocheck tools/machocheck.c`.

Antes, ELF com libunwind (7188 símbolos), sem `--disco`:

```
./machocheck libSystem-com-unwind-prom.mos OpenBot.app relatorio.txt
```

```
Imagens Mach-O x86_64 no bundle: 43 (exports do bundle: 55374)
libc++.1.dylib                              3888 /     859
Total: 29399 imports, 1352 sem implementacao (844 simbolos distintos)
Ausentes por origem (simbolos distintos): sistema 248, libc++ 366, Swift 230, unwind 0
```

É o mesmo 29399 / 1352 / 844 da rodada que segue symlink, agora separado. Os 366 são 364 + 2.

Depois, apontando a `libc++.1.dylib` do build:

```
./machocheck --disco libc++.1.dylib libSystem-com-unwind-prom.mos OpenBot.app relatorio.txt
```

```
Disco: /tmp/mos-build/libcxx
  libc++: 1 arquivo(s), 2356 simbolos
  Swift: 0 arquivo(s), 0 simbolos
  unwind: 0 arquivo(s), 0 simbolos
libc++.1.dylib                              3888 /       0
Total: 29399 imports, 491 sem implementacao (478 simbolos distintos)
Ausentes por origem (simbolos distintos): sistema 248, libc++ 0, Swift 230, unwind 0
```

`clang -O2 -Wall -Wextra` gerou o mesmo relatório (`diff` vazio contra o do gcc). Os 366 da libc++ saíram.
844 − 366 = 478. Sobram 248 de sistema e 230 de Swift.

Sem a libunwind no ELF (7144 símbolos) o mesmo app marca unwind **13** (857 distintos, 1517 imports sem
implementação). Com `--disco` nos cinco `.o`, os 13 saem e a conta volta a 844 / 1352, com unwind 0. A libc++
continua 366 enquanto o `--disco` não aponta a dylib.

### VS Code 1.145.2

O zip `https://update.code.visualstudio.com/1.145.2/darwin/stable` (`VSCode-darwin.zip`, 342 MiB). O
`Info.plist` diz `CFBundleShortVersionString` 1.145.2, `com.microsoft.VSCode`. 70 imagens x86_64, 31362 imports,
31908 exports do bundle. Não havia esse binário na VM antes do download.

Sem a libunwind no ELF, que é o caso em que o backlog contava 330 ausentes:

```
./machocheck libSystem-sem-unwind.mos "Visual Studio Code.app" relatorio.txt
```

```
libc++.1.dylib                              3097 /     836
Total: 31362 imports, 1313 sem implementacao (330 simbolos distintos)
Ausentes por origem (simbolos distintos): sistema 64, libc++ 194, Swift 53, unwind 19
```

Dos 194 da libc++, **189** são `libc++.1.dylib` e 5 são RTTI flat: `__ZTISt11range_error`,
`__ZTISt12domain_error`, `__ZTISt14overflow_error`, `__ZTISt15underflow_error`, `__ZTISt16invalid_argument`.
Swift **53** e unwind **19** (`__Unwind_Resume`, `__Unwind_Backtrace`, `__Unwind_DeleteException`,
`__Unwind_GetCFA`, `__Unwind_GetDataRelBase`, `__Unwind_GetIP`, `__Unwind_GetIPInfo`,
`__Unwind_GetLanguageSpecificData`, `__Unwind_GetRegionStart`, `__Unwind_GetTextRelBase`,
`__Unwind_RaiseException`, `__Unwind_SetGR`, `__Unwind_SetIP`, `_unw_get_reg`, `_unw_getcontext`,
`_unw_init_local`, `_unw_is_signal_frame`, `_unw_set_reg`, `_unw_step`). Sistema 64. 64 + 189 + 5 + 53 + 19 = 330.

Depois, `--disco` na dylib e nos cinco objetos:

```
./machocheck --disco libc++.1.dylib --disco libunwind-x86 libSystem-sem-unwind.mos "Visual Studio Code.app" relatorio.txt
```

```
Disco: /tmp/mos-build/libcxx/libc++.1.dylib /tmp/mos-build/libunwind-x86
  libc++: 1 arquivo(s), 2356 simbolos
  Swift: 0 arquivo(s), 0 simbolos
  unwind: 5 arquivo(s), 102 simbolos
libc++.1.dylib                              3097 /       0
Total: 31362 imports, 131 sem implementacao (117 simbolos distintos)
Ausentes por origem (simbolos distintos): sistema 64, libc++ 0, Swift 53, unwind 0
```

330 − 194 − 19 = 117, que é 64 de sistema + 53 de Swift. Com a libunwind já no ELF e `unw_getcontext` promovido,
o antes já é 311 (unwind 0) e o depois só da libc++ também fecha em 117.

As `libswift*` não estavam no disco. Os 53 do VS Code e os 230 do OpenBot continuam ausentes, e o relatório
marca `libswiftCore.dylib` como biblioteca inexistente no mOS.

## Usagebar: sem pacote

Site oficial: https://usagebar.com/. App de barra de menus para o uso do Claude Code. A página só oferece "Get for
Mac — $9", um pagamento único, no Stripe em modo live:

`https://buy.stripe.com/bJeaEY5kVbtZ1iG3zI8og09`

A página do Stripe identifica o vendedor como ViralBaby, LLC, o item como "Usagebar — Track Claude Code Usage from
your Menu Bar", total US$ 9,00, "Payment mode: Live mode". Não há DMG, zip, GitHub nem ficha da Mac App Store nessa
página, nem na de produto. A busca no índice de casks do Homebrew (`formulae.brew.sh`) não devolve `usagebar`.

Não existe pacote gratuito legítimo para medir. Não se pagou os US$ 9 e não se procurou cópia por fora. Por isso não
há contagem de símbolos, dylibs nem arquitetura do Usagebar.

FairPlay: sem o binário, não dá para ver `LC_ENCRYPTION_INFO`. A venda observada é direta, pelo Stripe, não pela
Mac App Store. Nada foi contornado.

Repositórios com nome parecido (`betoxf/Usagebar`, `claude-usage-bar` e outros) são outros programas. Não foram
medidos no lugar do app de usagebar.com.

## cua-driver, recontagem

O binário é o do zip oficial `OpenBot-0.66.0-x64.zip` (SHA-256
`473091aac28ccab16be5921c9477f6deef8f2d9523d6c87962c99581821f817f`, igual ao
`SHA256SUMS-macos-x64.txt` da release v0.66.0). O `machocheck` rodou só em
`Contents/Resources/cua-driver/darwin/x64/cua-driver`, slice x86_64.

Comando, nos dois momentos (o `libSystem.mos` é o ELF freestanding do zig 0.17.1, os
mesmos flags de `tools/build-libsystem.ps1`, sem rodar o script e sem regravar
`kernel/compat/libsystem_blob.S`):

```
gcc -O2 -o machocheck tools/machocheck.c
./machocheck libSystem.mos diretorio-do-cua-driver relatorio.txt
```

| | imports | ausentes |
|---|---:|---:|
| Antes (libSystem de `main`) | 995 | 410 |
| Depois | 995 | 399 |

Os 11 que passaram a resolver, todos desta frente: `OBJC_CLASS_$_SCStream`,
`OBJC_CLASS_$_SCRunningApplication`, `SCStreamFrameInfoStatus`,
`SCStreamFrameInfoDisplayTime`, `SCStreamFrameInfoScaleFactor`,
`SCStreamFrameInfoContentScale`, `SCStreamFrameInfoContentRect`,
`SCStreamFrameInfoDirtyRects`, `CGContextSetBlendMode`, `CGDisplayModeGetPixelWidth`,
`_AXUIElementCreateWithRemoteToken`. `CGWindowList*`, `CGEvent*` e AppKit já
resolviam no antes (AppKit 9/9, CoreGraphics ficou 35/0, ScreenCaptureKit 36/0).

Os 399 que continuam, por frente:

| Frente | Ausentes |
|---|---:|
| AppKit / CoreGraphics / ScreenCaptureKit / AX | 0 |
| Swift (`libswiftCore` 162, `libswiftDispatch` 38, `libswift_Concurrency` 11, `libswiftUniformTypeIdentifiers` 8, `libswiftCoreFoundation` 6, `libswiftCoreGraphics` 3, e mais 2 de Metal/ObjectiveC; overlay Foundation 20) | 250 |
| Sistemas e mídia (CoreFoundation 78, CoreMedia 41, IOSurface 15, AVFoundation 5, ImageIO 4, CoreVideo 3, Security 2, `dispatch_async_and_wait` 1) | 149 |

libc++ não importa nada neste binário. Nada de Swift nem de libc++ foi implementado aqui.
