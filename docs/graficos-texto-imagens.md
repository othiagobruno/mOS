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
2. ✅ **WebP** (leitura; o ImageIO da Apple também não grava WebP). Ver "Passo 2" abaixo.
3. **HEIC/HEIF/AVIF** (contêiner ISOBMFF, grade de tiles, alfa auxiliar, EXIF; HEVC intra e AV1
   intra para os quadros). Combinar com a Isabela (VideoToolbox HEVC) para não haver dois decodificadores.
4. ✅ **CGImage completo** (passo 3 abaixo): 1/2/4/8/16 bits, float, cinza, cinza+alfa, CMYK, `decode`, máscara
   (`CGImageMaskCreate`, `CGImageCreateWithMask`, `CGImageCreateWithMaskingColors`), sem teto de 32 MiB.
5. ✅ **CGContext completo** (passo 3 abaixo): sombra desenhada (blur gaussiano), os 28 blend modes, interpolação
   na `CGContextDrawImage` com transformação, `CGBitmapContextGetColorSpace`, contextos 16 bits/float,
   `CGContextDrawTiledImage`, `CGContextClipToMask`, glifos e modos de texto.
6. ✅ **ColorSync e espaços de cor** (passo 4 abaixo): perfis ICC (leitura, `CGColorSpaceCreateWithICCData`,
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
HEIC/AVIF são o passo 3.

### Passo 2 — WebP (PR de 15 nov. 2026)

- `userland/libsystem/iio_webp.c` compila **sem mudança** o decodificador do motor web
  (`kernel/web/webp.c`, frente do Fabio: VP8L, VP8 com filtro de laço, ALPH, VP8X). `kshim/web.h` e
  `kshim/gfx.h` dão só tipos, malloc e `surface_t`. Unidade separada porque o `webp.c` tem estáticos com os
  nomes dos codecs do `imageio.c`.
- Animação feita aqui, por fora do `webp.c` (que entrega só o primeiro quadro): cada `ANMF` vira um WebP
  avulso (VP8X + sub-blocos), composto na tela com mistura e disposição do quadro; ANIM dá as repetições;
  ICCP vira o perfil da imagem.
- `CGImageSource`: tipo `org.webmproject.webp`, quadros, `{WebP}` com DelayTime/UnclampedDelayTime por quadro
  e LoopCount/CanvasPixelWidth/Height no arquivo.
- Teste: `tools/imageio_host.py` ganhou 5 casos contra o PIL (libwebp): sem perda RGBA e RGB pixel a pixel,
  com perda e com alfa erro médio ≤ 2, animado de 3 quadros pixel a pixel com loop 4 (46 casos no total).
  `tools/imageio_cf_host.c`: 35 verificações (8 tipos de leitura).

### Passo 3 — CGContext, CGImage e CGFont completos (PR de 15 nov. 2026)

Régua: a lista da API pública C de CoreGraphics/Core Text/ImageIO/ColorSync (711 funções). Antes deste passo
318 não existiam no libSystem; depois, 238 — nenhuma de CGContext, CGImage, CGBitmapContext, CGDataProvider,
CGFunction, CGShading, CGLayer ou CGFont. As que sobram são de ColorSync/ICC, PDF, Core Text, CGPath booleanas e
CGImageMetadata (passos seguintes).

- **Blend modes**: os 28 de `CGBlendMode`, com a numeração da Apple (`cg_blend.h`). Antes 1 era tratado como
  Copy (na Apple é Multiply), 2 como Multiply e 3 como Screen; o resto caía em Normal.
- **Formatos** (`cg_pixel.inc`): `CGImageCreate` aceita 1/2/4/5/8/16 bits e float 32, cinza/RGB/CMYK/indexado,
  todas as combinações de alfa, ordem 16/32 little/big e `decode`. `CGBitmapContextCreate` aceita só-alfa, cinza 8
  e 16, cinza+alfa, RGB 32 bpp nas 8 combinações, RGB 555, RGBA 16 bpc, RGBA float e CMYK 8. Antes: só 8 bits
  RGBA/A8, a ordem de bytes era ignorada (PremultipliedFirst sem `32Little` saía BGRA) e contexto cinza voltava NULL.
  O CoreImage (`CoreImage.m`) e o UIKit (`UIKitAnimation.m`) passavam `info` sem `kCGBitmapByteOrder32Little`
  contando com o erro; as duas linhas ganharam `| 0x2000`, que é o layout que gravam.
- **Sombra desenhada**: a operação vai para um buffer, a sombra (alfa borrado por 3 caixas ≈ gaussiana de sigma
  blur/2, deslocada no espaço base) e depois o conteúdo são compostos. Vale para path, retângulo, imagem, gradiente,
  shading, glifo e camada de transparência (que agora é sempre ARGB pré-multiplicado, também em contexto sem alfa).
- **Máscaras**: `CGContextClipToMask` (image mask ou imagem cinza), `CGImageMaskCreate`, `CGImageCreateWithMask`,
  `CGImageCreateWithMaskingColors`; `CGContextClipToRects`.
- **Imagem**: interpolação bilinear (salvo `kCGInterpolationNone` ou `shouldInterpolate` falso),
  `CGContextDrawTiledImage` limitado ao recorte, `CGImageCreateWithPNG/JPEGDataProvider`,
  `CGImageCreateCopyWithColorSpace`, `IsMask`, `GetDecode`, `GetShouldInterpolate`, `GetByteOrderInfo`,
  `GetPixelFormatInfo`, `GetUTType` (posto pelo ImageIO).
- **Path e cor**: `AddLines`, `AddRects`, `AddArcToPoint`, `IsPathEmpty`, `GetPathCurrentPoint`,
  `GetPathBoundingBox`, `PathContainsPoint`, `EOFillPath` (antes preenchia com a regra não nula), `DrawPathDirect`,
  `SetFillColor`/`SetStrokeColor` pelo espaço de cor, `SetStrokeColorSpace`, CMYK, `SetStrokePattern`,
  conversões de ponto e retângulo para o dispositivo e de volta.
- **Texto no CGContext**: `SetFont`, `SetFontSize`, `SetCharacterSpacing`, `SetTextPosition/GetTextPosition`,
  `ShowGlyphs`, `ShowGlyphsAtPoint`, `ShowGlyphsAtPositions`, `ShowGlyphsWithAdvances`, `SelectFont`, `ShowText`,
  `ShowTextAtPoint` (MacRoman) e os 8 modos de `SetTextDrawingMode` (traço e recorte pelos contornos dos glifos).
- **Shading**: `CGFunctionCreate`, `CGShadingCreateAxial/Radial`, `CGContextDrawShading`, `CGContextDrawConicGradient`.
- **DataProvider**: `CreateWithFilename`, `CreateSequential`, `CreateDirect`, `GetInfo`, `GetTypeID`.
- **CGFont**: `GetUnitsPerEm`, `Ascent/Descent/Leading`, `CapHeight/XHeight`, `FontBBox`, `ItalicAngle`, `StemV`
  (estimado pelo peso), `GetGlyphAdvances`, `GetGlyphBBoxes` (glyf/loca; CFF pelo contorno), `CopyTableTags`,
  `CopyFullName`, `CopyVariationAxes`, `CopyVariations`, `CreateCopyWithVariations`, `CanCreatePostScriptSubset`.

Teste: `tools/cg_context_test.c` passou de 26 para 58 verificações (blend modes, ordem de bytes, cinza/16 bits/
float, sombra dura e borrada, ClipToMask, image mask, 1 bit, ClipToRects, consultas ao path, shading, texto e modo
invisível), todas ok. Limites: `CGFontCreateCopyWithVariations` devolve os mesmos contornos (o rasterizador do
kernel desenha a instância padrão); `CGFontCanCreatePostScriptSubset` responde não; as métricas de CGFont não
têm teste de host (o harness do Core Text precisa de runtime ObjC completo); a intenção de renderização não muda
pixel (o mOS desenha em sRGB até o passo de ColorSync).

### Passo 4 — ColorSync, ICC e espaços de cor (PR de 16 nov. 2026)

- **Motor** (`colorsync.c`/`colorsync.h`): perfis ICC v2/v4 — matriz/TRC (`curv` gama e tabela, `para` 0..4),
  cinza (`kTRC`), LUTs `lut8`/`lut16` (`mft1`/`mft2`) e `lutAtoB`/`lutBtoA` (`mAB `/`mBA `), PCS XYZ ou Lab,
  intenção por tabela (A2B0/1/2), interpolação tetraédrica (3 entradas; 4 entradas com o primeiro eixo linear),
  Bradford. Perfis padrão gerados a partir das normas (IEC 61966-2-1, SMPTE EG 432-1, Adobe RGB (1998), ITU-R
  BT.709/2020/2100 PQ e HLG, ACEScg, ROMM, lineares, cinza 2.2, Lab, CMYK genérico), gravador ICC v2 com ID MD5.
- **ColorSync**: `ColorSyncProfileCreate/CreateWithName/CreateWithURL/CreateMutable/SetTag/RemoveTag/SetHeader/
  CopyData/CopyHeader/CopyTag/ContainsTag/CopyDescriptionString/GetMD5/GetURL/Verify/CreateDeviceProfile`,
  `ColorSyncTransformCreate/Convert` (8, 16, half, 32 bits e float; os 7 layouts de alfa; ordem de bytes),
  `ColorSyncCreateCodeFragment`, as constantes `kColorSync*`.
- **CGColorSpace**: `CreateWithICCData`, `CreateICCBased`, `CopyICCData` (gerado para os de nome), `DeviceCMYK`,
  `CreateLab`, `CreateIndexed` + `GetColorTable(Count)`, `CreateCalibratedRGB/Gray` de verdade (antes devolviam
  sRGB), `CreateWithPlatformColorSpace`, `Create/CopyPropertyList`, `IsHDR`, `CreateExtended/Linearized/
  ExtendedLinearized/CopyWithStandardRange`, modelos CMYK/Lab/XYZ pelo nome, `CGColorConversionInfoCreate*`.
- **CGColor em qualquer espaço**: subclasse do NSColor do mOS (o AppKit continua lendo r,g,b,a em sRGB) que guarda
  espaço e componentes originais — `CGColorCreate` respeita o espaço, `GetNumberOfComponents/GetComponents/
  GetColorSpace` devolvem os do espaço, `CreateGenericGray` (2 componentes), `CreateGenericCMYK`,
  `CreateGenericGrayGamma2_2`, `CreateWithPattern/GetPattern` (cor de padrão vale em `SetFill/StrokeColorWithColor`),
  `CreateCopyByMatchingToColorSpace` de verdade (antes devolvia a mesma cor).
- **Imagens**: pixel de espaço não sRGB (ICC embutido de PNG/JPEG/TIFF/WebP, P3, Adobe RGB, calibrado, Lab,
  indexado com base convertível) vira sRGB ao entrar no CGImage; `CGImageGetDataProvider` agora devolve o provedor
  original (antes, os pixels internos em ARGB, num layout que não batia com o bitmapInfo).

Testes: `tools/colorsync_host.py` (binário `tools/colorsync_host.c`) — 15 perfis contra o LittleCMS 2.17
(ghostscript e colord do sistema: sRGB, Adobe 98, ROMM, sGray, dois CMYK de LUT, FOGRA45, ColorMatch, e os 6 que o
mOS grava, lidos de volta pelo LittleCMS): pior diferença 1,23/255 (cores dentro do gamut, colorimétrico relativo,
LittleCMS sem device link otimizado); `colorsync_host self` 11/11; `tools/cg_context_test.c` 70/70.
Limites: o contexto de bitmap não converte para o próprio espaço (desenha em sRGB mesmo num contexto P3); fora do
gamut o recorte é por canal (sem mapeamento perceptual); sem compensação de ponto preto; perfil CMYK/cinza embutido
em JPEG não se aplica (o decodificador já entrega RGB).
