# Core Graphics, Core Text, ImageIO, PDF e ColorSync — frente da Mariana

Dona: Mariana (slug `mariana`). Este doc é o estado honesto da frente: o que está pronto e com
qual teste, e o que falta. Atualizado a cada PR da frente.

## Levantamento de 15 nov. 2026 (antes do primeiro PR)

O `docs/compat-gaps.md` foi gerado antes de vários PRs: quase todos os nomes do topo dele
(`CGContextClip`, `CGContextSetLineDash`, transparency layers, `CTFontCreateWithGraphicsFont`,
`kCTFontURLAttribute`, `CTLineGetGlyphRuns`...) já existem no libSystem. O problema agora é o
que está por trás dos nomes:

| Área | Onde | Estado encontrado |
|---|---|---|
| ImageIO | `libsystem/grok_imageio.c`, `libsystem/openbot_gap.c`, `frameworks/ImageIO/ImageIO.m` | **Duas implementações.** O dylib `ImageIO.framework` ganha quando está no disco; sem ele vale o libSystem, cujo `CGImageSourceCreateImageAtIndex` devolve uma struct que **não é CGImage** (`CGImageGetWidth` dá 0). Só PNG e JPEG; uma imagem por arquivo; PNG gravado sem compressão; chaves criadas tarde (o app que lê `kCGImagePropertyPixelWidth` antes de chamar o ImageIO lê NULL). |
| CGImage | `libsystem/coregraphics.c` | Só 8 bits/componente e 32 ou 8 bpp; recusa bitmap acima de 32 MiB; `decode`, espaço de cor e intent ignorados; 16 bits, float, CMYK e cinza+alfa recusados. |
| CGBitmapContext | `coregraphics.c`, `coregraphics_bm.inc`, `cg_raster.c` | Path, traço, dash, recorte por máscara, gradientes, camadas e padrão existem. Só 8 bits RGBA/A8; sombra guardada e não desenhada; blend só normal/copy/multiply/screen/clear; `CGBitmapContextGetColorSpace` não existe. |
| CGColor/CGColorSpace | `coregraphics.c` | CGColor sempre 4 componentes sRGB; sem ICC (`CGColorSpaceCreateWithICCData`), sem conversão entre espaços. |
| PDF | `coregraphics_bm.inc`, `vscode_frameworks.c` | `CGPDFDocument` só conta páginas e lê MediaBox; não há parser de objetos nem desenho de página. `CGPDFContextCreate` é casca. PDFKit não existe. |
| Core Text | `libsystem/coretext.c` | Usa só a Inter do kernel; fonte de arquivo do app (`CTFontManagerRegisterFontsForURL`, `CTFontCreateWithGraphicsFont`) não tem parser sfnt próprio. CTLine sem bidi e sem grafema; CTFramesetter casca. |
| ColorSync | — | Não existe. |

## Plano (ordem de execução)

Cada passo fecha com teste de host em `tools/` e PR mergeado na `main`.

1. **ImageIO único no libSystem** (`libsystem/imageio.c` + codecs `iio_*.inc`). Remove o
   dylib `ImageIO.framework` e as duas metades antigas. CGImage de verdade, várias imagens por
   arquivo, chaves como constantes CFString estáticas. Codecs: PNG (todas as profundidades,
   Adam7, gravação com deflate da zlib), JPEG, GIF (animado, leitura e gravação), BMP, TIFF
   (none/LZW/PackBits/Deflate, predictor, multipágina, leitura e gravação). Propriedades
   `{TIFF}`, `{Exif}`, `{PNG}`, `{GIF}`, DPI, Orientation, Depth, ColorModel; miniatura com
   filtro de caixa e transformação EXIF. Teste: `tools/imageio_host.c` contra arquivos que o PIL gera.
2. **WebP** (VP8L sem perda e VP8 com perda, alfa, animação; gravação VP8L).
3. **HEIC/HEIF/AVIF** (contêiner ISOBMFF, grade de tiles, alfa auxiliar, EXIF; HEVC intra e AV1
   intra para os quadros). Combinar com a Isabela (VideoToolbox HEVC) para não haver dois decodificadores.
4. **CGImage completo**: 1/2/4/8/16 bits, float, cinza, cinza+alfa, CMYK, `decode`, máscara
   (`CGImageMaskCreate`, `CGImageCreateWithMask`, `CGImageCreateWithMaskingColors`), sem teto de 32 MiB.
5. **CGContext completo**: sombra desenhada (blur gaussiano), os 28 blend modes, interpolação
   na `CGContextDrawImage` com transformação, `CGBitmapContextGetColorSpace`, contextos 16 bits/float,
   `CGContextDrawTiledImage`, `CGContextClipToMask`, glifos e modos de texto.
6. **ColorSync e espaços de cor**: perfis ICC (leitura, `CGColorSpaceCreateWithICCData`,
   `CGColorSpaceCopyICCData`), matriz/TRC e LUT, conversão sRGB/P3/Gray/CMYK, `CGColorConverter`,
   `ColorSyncProfile*`/`ColorSyncTransform*`.
7. **PDF**: parser (xref, xref stream, object stream, Flate/LZW/ASCII85/ASCIIHex/RunLength/DCT),
   `CGPDFDocument/Page/Dictionary/Array/Stream/String/Scanner/OperatorTable`, desenho de página
   (paths, texto com fontes embutidas Type1/TrueType/CFF, imagens, sombreamentos), criptografia RC4/AES;
   `CGPDFContext` gravando PDF de verdade; **PDFKit** (`PDFDocument`, `PDFPage`, `PDFView`,
   `PDFSelection`, `PDFAnnotation`, busca de texto).
8. **Fontes**: parser sfnt próprio (TrueType `glyf`, CFF/CFF2, TTC, variações `fvar`/`gvar`),
   rasterizador, `CGFontCreateWithDataProvider`, `CTFontManagerRegister*`/`CreateFontDescriptorsFromURL`,
   cmap 4/12/14, `kern` e GPOS/GSUB (ligaduras, kerning), métricas reais.
9. **Core Text layout**: `CTLine`/`CTRun`/`CTTypesetter` com bidi (UAX #9), quebra de linha
   (UAX #14), grafemas, `CTFramesetter`/`CTFrame` com parágrafos, `CTParagraphStyle`, fallback de fonte.

## Pronto

### Passo 1 — ImageIO único no libSystem (PR de 15 nov. 2026)

- **Uma implementação só**: `userland/libsystem/imageio.c` (codecs `iio_*.inc`, casca CF em
  `imageio_cf.inc`). Saíram `frameworks/ImageIO/ImageIO.m`, `libsystem/grok_imageio.c`, o PNG/destino
  de `openbot_gap.c` e o `photos_imageio.c` vazio. Sem o dylib no disco, o `macho.c` resolve
  `ImageIO.framework` no libSystem (`framework_has_libsystem_fallback`).
- **CGImageSource**: `CreateWithData/URL/DataProvider`, `CreateIncremental` + `UpdateData/UpdateDataProvider`
  (status ReadingHeader/Incomplete/Complete/UnknownType/InvalidData), `GetType`, `GetCount` (quadros e
  páginas), `CopyProperties` (FileSize, ImageCount, `{GIF}`/`{PNG}` LoopCount e tela), `CopyPropertiesAtIndex`
  (PixelWidth/Height, Depth, ColorModel, HasAlpha, IsIndexed, Orientation, DPI, ProfileName lido do ICC,
  `{TIFF}`, `{Exif}`, `{JFIF}`, `{PNG}`, `{GIF}` com DelayTime/UnclampedDelayTime), `CreateImageAtIndex`
  (com `kCGImageSourceSubsampleFactor`), `CreateThumbnailAtIndex` (Always/IfAbsent, MaxPixelSize com filtro
  de caixa, WithTransform aplica a orientação EXIF), `GetTypeID`, `CopyTypeIdentifiers`, `RemoveCacheAtIndex`.
  Devolve **CGImage de verdade** (antes o caminho do libSystem devolvia uma struct que não era CGImage).
- **CGImageDestination**: `CreateWithData/URL/DataConsumer`, `SetProperties`, `AddImage`,
  `AddImageFromSource` (leva atraso, orientação e DPI da fonte), `Finalize`, `CopyTypeIdentifiers`; opções
  LossyCompressionQuality, Orientation, DPI, BackgroundColor (achata alfa no JPEG), ImageMaxPixelSize,
  `{GIF}`/`{PNG}` DelayTime e LoopCount, `{TIFF}` Compression. Aceita UTI e MIME.
- **Codecs** — leitura: PNG (todas as profundidades e tipos, tRNS, Adam7, APNG com disposição/mistura,
  pHYs, gAMA, sRGB, iCCP), JPEG (baseline e progressivo; cinza, YCbCr, CMYK/YCCK Adobe; JFIF, EXIF, ICC),
  GIF (animado, entrelaçado, disposição 0..3, NETSCAPE loop), TIFF (multipágina, faixas e tiles,
  none/LZW/Deflate/PackBits, predictor, gray/RGB/paleta/CMYK, 1..16 bits e float, alfa associado),
  BMP (1..32 bits, RLE4/8, bitfields) e ICO/CUR. Gravação: PNG (deflate zlib, filtro por linha, APNG,
  pHYs, eXIf), JPEG (4:4:4, JFIF com DPI, APP1 com Orientation), GIF (animado, corte mediano), TIFF
  (multipágina, none/LZW/Deflate/PackBits), BMP (24 ou 32 bits). Teto de 128 Mpx por imagem.
- **CGDataConsumer** real (`cg_consumer.c`): callbacks, CFMutableData e URL; antes devolvia NULL.
- **CGImage/CGBitmapContext**: teto subiu de 32 MiB para 1 GiB.
- Corrigido no gravador JPEG herdado: escala da DCT (8× maior), DQT fora do zigue-zague e quantização sem arredondamento.

Testes de host (todos saem 0):
- `tools/imageio_host.py` (binário de `tools/imageio_host.c`): 41 casos, o mOS lê o que o PIL grava e o PIL
  lê o que o mOS grava; sem perda bate pixel a pixel; JPEG com erro médio ≤ 5 (4:2:0) e ≤ 2 (4:4:4).
- `tools/imageio_cf_host.c`: 35 verificações da casca CF com uma CF mínima de host.
- Os antigos continuam passando com o novo caminho: `tools/test-imageio.c`, `tools/photostest.c`,
  `tools/test-coreimage.c`, `tools/imageio_exif_test.c`.

Não verificado: nada disso rodou dentro do mOS (build proibido até o fim da frente). Limites conhecidos:
JPEG lê crominância 4:2:0 por vizinho mais próximo (não interpolado); JPEG aritmético, sem perdas e 12 bits
recusados; TIFF com JPEG dentro (compressão 6/7) recusado; `CGImageMetadata*` ainda não existe;
WebP/HEIC/AVIF são os passos 2 e 3.
