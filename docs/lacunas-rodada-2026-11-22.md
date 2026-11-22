# Rodada de lacunas do corpus — 22 nov. 2026

Ponto de partida: `docs/lacunas-por-dono.md` (libSystem 4c39ade2, 294 apps medidos). Fonte da contagem: as tabelas de
`docs/lacunas/*.md`, cruzadas com os símbolos que o `libSystem.mos` e os frameworks do mOS **compilados nesta rodada**
exportam de verdade (`llvm-nm` nos objetos), não com `grep` no fonte.

## O que a medição de 21/11 contava errado

- `WKUserScript`, `WebView`, `WKUserContentController`, `WebPreferences` (100+ apps), os símbolos `SwiftUI` (69), a API do
  ICU (`libicucore`) e do expat apareciam como ausentes porque o disco da varredura não tinha `WebKit.framework`,
  `libicucore.A.dylib` nem `libexpat.1.dylib`. O WebKit do mOS exporta essas classes; o build põe o ICU4C e o expat em
  `/usr/lib`. Não é lacuna do mOS: a próxima varredura precisa do `build/esp/mOS` completo como disco.
- `swprintf`/`vswprintf` (88 apps) **eram** lacuna de verdade: o beffd626 tirou a definição do `libc_corpus3.c` achando
  que `swiftsupport.c`/`vscode_libc.c` tinham as suas, e o 547c8faf tirou aquelas no mesmo dia. Sobrou nenhuma.

Descontado isso, os símbolos C/ObjC reais pedidos por 5+ apps eram **460** (969 por 3+, 496 por exatamente 2). Depois
desta rodada: **0** pedidos por 5+; por 3+ sobram **73**, todos de Tcl/Tk (o `tkinter` do Python embutido); por 2 sobram
**59**, todos de bibliotecas que o próprio app traz e não achou no disco da varredura (`libmimerapi`, `libX11`). Total de
símbolos reais ausentes (qualquer número de apps): 6747 → 5000.

## O que entrou

| Arquivo | Conteúdo | Teste |
|---|---|---|
| `libc_corpus3.c` | `swprintf`/`vswprintf` de volta | `tools/libc_corpus3_hosttest.sh` (11/11) e `corpusteste` |
| `stdio.c` | `%ls`/`%S`/`%lc`/`%C` no printf (antes `%ls` lia o `wchar_t *` como `char *`) | `corpusteste` no mOS |
| `libc_corpus4.c` | `_res`, `hash_*`, ndbm em arquivo, `fgetws`/`swscanf`/`wcsto*_l`, `*printf_l`, `strfmon`, `open_memstream`, `fmemopen`, `daemon`, `inet_net_pton`, NIS/RPC sem ypbind, dyld legado (`NSLookupAndBindSymbol`...), `__udivmodti4`, `__truncsfhf2`, inversas do simd, complexos trigonométricos; `matrix_identity_*` viraram dados (eram funções vazias em `sys_zerados.c`) | `tools/libc_corpus4_hosttest.sh` (34/34) |
| `cg_corpus.c` | `CGDisplayMode` como objeto CF, lista/troca de modo, captura, transações de configuração, gamma, cursor, estado do mouse, CGS de Spaces e lista de janelas, IOKit de tela e HID, IOPowerSources | `corpusteste` no mOS |
| `ffi_mos.c` + `ffi_x64.S`/`ffi_arm64.S` | libffi: `ffi_prep_cif(_var)`, `ffi_call`, closures, `ffi_type_*` (SysV x86_64 e PCS arm64 da Apple) | `tools/ffi_mos_test.sh` (17/17 nativo no arm64) e `corpusteste` (x86_64 dentro do mOS) |
| `foundation_corpus.c` | `NSPort`/`NSSocketPort`/`NSConnection`/`NSDistantObject`/`NSProtocolChecker`, `NSArchiver`/`NSUnarchiver`, `NSURLDownload`, `NSScriptSuiteRegistry`, `NSPointerFunctions`, `NSDistributedLock`, CFUserNotification, CFURLCreatePropertyFromResource; `NSMachPort : NSPort` | `corpusteste` |
| `classes_corpus.c` | ~300 classes de fachada (NetworkExtension, ImageCaptureCore, SpriteKit, SceneKit, MapKit, MPS, FSKit...) com a superclasse da Apple, marcadas com `cx_mark_stub_class` (o limite subiu de 256 para 1024) | `corpusteste` (subclasse de `NEPacketTunnelProvider`) |
| `services_corpus.c` | OpenCL sem plataforma, vmnet/Hypervisor sem suporte, PC/SC, pcap, xar, `nw_framer`, DNS-SD extra e TXT records reais, GLU, `vis`/`strunvis`, `wordexp`, `fmtcheck`, `gcvt`/`fcvt`, `tempnam`, `errc`, `carg`/`cabs` | `tools/services_corpus_hosttest.sh` (18/18) |
| `readline_mos.c`, `cups_corpus.c` | API readline do libedit (histórico em arquivo, completion, callback); CUPS sem impressora com opções de verdade | `tools/readline_cups_hosttest.sh` (25/25) |
| `accelerate_corpus.c` | vImage (RGBA8888, espelho vertical, half, NV12, ARGB→YpCbCr, histograma, dilatação, matriz), vDSP, vBigNum, `posv`/`gels` | `tools/accelerate_corpus_test.sh` (16/16) |
| `accelerate_vimage.c` | **correção**: o croma do YpCbCr→ARGB era multiplicado por 2 (o teste antigo só via cores que saturam) | caso de tom médio novo em `tools/accelerate_vimage_test.c` |
| `resolv_ns.c` | `ns_initparse`/`ns_parserr`/`ns_name_uncompress`, `dns_open`/`dns_search` | `tools/resolv_ns_test.sh` (10/10) |
| `libarchive_mos.c` | tar ustar/pax (escrita e leitura), gzip/bzip2/xz, extração | `tools/libarchive_mos_test.sh` (12/12, contra o `tar` do host) |
| `curses_mos.c` | ncurses e panel (xterm-256color): janelas, `doupdate` por diferença em ANSI, entrada cbreak/raw com teclas especiais, cores, ACS, terminfo/termcap embutidos com `tparm`, painéis | `corpusteste` |
| `fdn_coll.c` | **correção**: `NSArray`, `NSDictionary` e `NSData` não tinham `isEqual:`/`isEqualToArray:`... (caíam no do `NSObject`, que compara ponteiro) | `corpusteste` |
| `misc_corpus.c` e vizinhos | AddressBook, SystemConfiguration (`SCNetworkSet`), VideoToolbox (rotação de pixel buffer real), CSSM (digest, geração de chave, módulos, OIDs, GUIDs), Kerberos/GSS sem credencial, ForceFeedback, constantes de AVFoundation/ImageIO/UTType/CoreServices | compilação e link |
| `corpus2.c` | **lacunas de 2 apps**: `link_ntoa`, `lfind`, `sys_siglist`, `swap_*` do Mach-O, `statx_np`/`filesec_*`, `clock_get_attributes`, `DNSServiceConstructFullName`, RFC 3394 (`CCSymmetricKeyWrap`/`Unwrap`), `CFTree` inteiro, `NSZoneName` e chaves do `NSMetadata`, regiões e modos de tela do CGS, IOKit de I2C/HID/kexts, libpcap (`pcap_open_dead`, arquivos `.pcap`, nomes de DLT, filtro vazio), OpenCL, IOReport e Multitouch sem canais/dispositivos, libsasl2/libpam sem mecanismo, libxslt sem processador, `class_setVersion` e a classe `Protocol`, `BZ2_bzopen`/`bzclose`, constantes de CoreMedia/AVFoundation/VideoToolbox/QuartzCore/SystemConfiguration/CoreText/ImageIO/FileProvider | `corpusteste` |
| `bzip2_enc.c` | **compressor bzip2** (o mOS só descomprimia: `BZ2_bzWrite`, `BZ2_bzCompress` e a libarchive com bzip2 davam `BZ_CONFIG_ERROR` dentro do mOS). BWT por dobra de prefixos, MTF, RUNA/RUNB, Huffman canônico; API de buffer, de fluxo (com `BZ_FLUSH`) e de arquivo | `tools/bzip2_enc_hosttest.sh` (31/31: o libbz2 do host e o `bzip2 -t` leem a saída) e `corpusteste` |
| `accelerate_corpus2.c` | vImage: rotação livre e de 90°, espelhos, cisalhamento, warp afim, composição pré-multiplicada (normal, screen, multiply, lighten, darken), min/max/erosão/dilatação/convolução ARGB8888, conversões RGB888/Planar16F/ARGB16U, `vImageBuffer_InitWithCVPixelBuffer`/`CopyToCVPixelBuffer` e os conversores CV↔CG (32 bits); vDSP double e FFT complexa (`fft_zip`/`zop`/`zopD`/`zropD`); `ARGB8888To422CbYpCrYp8` no `accelerate_corpus.c` | `tools/accelerate_corpus2_test.sh` (24/24) |
| `glu_mos.c` | GLU: `gluPerspective`, `gluLookAt`, `gluProject`, `gluErrorString`, quádricas (esfera, cilindro, disco) e o tesselador (furos ligados por ponte + corte de orelhas, `BOUNDARY_ONLY`, callbacks `_DATA`) | `tools/glu_mos_test.sh` (8/8) |
| frameworks | CoreData (`NSManagedObjectID` e descrições), CoreImage (`CISampler`, `CIBlendKernel`, `CIImageProcessorKernel`), NaturalLanguage (`NLTokenizer`, `NLTagger`, os 36 idiomas que faltavam), Virtualization, WebKit (`WebDownload`), JSC (`JSStringCreateWithCFString`); `PDFView` passou a exportar o metaclass | `tools/naturallanguage_host_test.sh` (7/7) |

## Verificação

- `libSystem.mos` x86_64 e arm64 ligam (link estático, sem símbolo indefinido; o arm64 pelo `tools/build-libsystem-arm64.sh`).
- Imagem mínima (kernel + libSystem + `userland/*.c` + testes Mach-O, sem os frameworks que dependem de fontes baixadas)
  bootada no QEMU 11 (q35, TCG, 4 CPUs) com o autoteste (`.autoteste`), contra a mesma imagem com o libSystem do `HEAD`:

  | imagem | ok | falhou |
  |---|---:|---|
  | libSystem do `HEAD` (b810bfc7) | 442 | `darwinobjcteste` (o `@try` precisa da libc++abi de disco, que a imagem mínima não tem), `corpusteste` (símbolos ausentes) |
  | libSystem desta rodada | 443 | `darwinobjcteste` (a mesma) |
| com as lacunas de 2 apps | 443 | `darwinobjcteste` (a mesma) |

  O `corpusteste` novo (`userland/tests-corpus`, Mach-O x86_64 pelo carregador do mOS) deu **35/35**, incluindo a libffi na
  ABI x86_64, que o teste de host não cobre (o host aqui é arm64), e o compressor bzip2 rodando dentro do mOS (200 KB →
  1150 bytes e de volta). O `smpteste: FALHOU` do log aparece igual na imagem do `HEAD`: no TCG os núcleos não rodam em
  paralelo.
- Achados no caminho, que os testes de host escondiam: o `printf` do libSystem lia `%ls` como `char *`; o `open` do
  libSystem recebe os `O_*` do macOS (o ndbm e a libarchive usavam os do Linux); `NSArray`/`NSDictionary`/`NSData` sem
  igualdade de conteúdo.

## O que falta

- Tcl/Tk (73 símbolos, 3-4 apps: o `tkinter`). Como função ausente vira stub só chamado na hora, esses apps carregam; só
  o `import tkinter` falharia. Fechar de verdade é portar o Tcl/Tk (fontes de fora).
- Os 4867 símbolos pedidos por um app só.
- O que ficou sem implementação de verdade e só devolve erro: processador XSLT, filtros BPF além do vazio, OpenCL,
  CGPSConverter (PostScript), SASL, Multitouch e IOReport.
- Uma varredura nova do corpus com o disco completo (`tools/corpus/varrer-tudo.sh` com `build/esp/mOS`), que precisa
  baixar os apps.
- Abrir os apps de verdade: símbolo resolvido não quer dizer que o app funciona.
