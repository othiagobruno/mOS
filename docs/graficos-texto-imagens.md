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

### Passo 5a — PDF: leitor e API CGPDF (PR de 16 nov. 2026)

- **Núcleo** (`pdf_core.c`/`pdf_core.h`, ISO 32000-1/-2): léxico completo, xref clássica, xref stream, híbrida
  (`/XRefStm`), cadeia `/Prev` (atualização incremental, a seção mais nova ganha), reparo por varredura quando a
  xref não confere ou não existe, object streams, `/Length` indireto ou errado, filtros FlateDecode e LZWDecode com
  Predictor 2 e PNG, ASCIIHex, ASCII85, RunLength, Crypt/Identity; security handler padrão R2–R6 (RC4 40/128,
  AES-128 V4, AES-256 V5 R5/R6 com SHA-256/384/512), senha de usuário e de dono, strings e streams decifrados por
  objeto; árvore de páginas com proteção contra ciclos e atributos herdáveis; imagem inline no léxico.
- **API** (`cgpdf.c`): `CGPDFDocument` (Provider/URL, páginas, criptografia, versão, catálogo, Info, ID, permissões,
  outline com títulos, destinos e filhos — destinos nomeados e árvore `/Names`), `CGPDFPage` (caixas com as regras
  de padrão e recorte pela MediaBox, rotação, dicionário, `GetDrawingTransform` com o `/Rotate`), `CGPDFObject`,
  `Dictionary`, `Array` (com `ApplyFunction`/`ApplyBlock`), `Stream` (`CopyData` com formato JPEG/JPEG 2000),
  `String` (`CopyTextString` com PDFDocEncoding, UTF-16BE e UTF-8; `CopyDate` com fuso), `CGPDFScanner`,
  `CGPDFContentStream` (recursos herdados dos pais) e `CGPDFOperatorTable`.
- O leitor antigo do `coregraphics_bm.inc` (só contava páginas) saiu.

Testes: `tools/pdf_host.py` 92/92 — PDFs gerados no teste (xref clássica, stream, incremental, xref quebrada e
ausente, cada filtro, R2/R3/R4/R6 com senha de usuário, de dono, errada e vazia, cifra feita em Python com a
biblioteca `cryptography`) e PDFs do Ghostscript 10.06 (1.4, 1.7 com object streams, RC4 128); vetores de
MD5/SHA-2/AES (FIPS-197)/RC4 em `pdf_host hash`. `tools/cg_context_test.c` 83/83 com a API nova (título UTF-16,
data com fuso, arrays, scanner com imagem inline, outline).
Ainda não: PDFKit.

`tools/libsystem-symcheck.sh`: compila os .c do libSystem em objetos e acusa símbolo forte duplicado (o que quebrou o
link no PR #367) e indefinido nos arquivos dados. Rodar dentro da trava do time antes de cada PR.

### Passo 5b — PDF: desenho da página (PR de 16 nov. 2026)

`CGContextDrawPDFPage` (`cgpdf_draw.c`, sobre o próprio CoreGraphics; o stub do `vscode_frameworks.c` saiu):
- **Conteúdo**: estado gráfico completo (`q/Q/cm/w/J/j/M/d/i/gs`), paths, `W/W*` aplicado depois da pintura, os
  10 operadores de pintura, cores em todos os espaços (`pdf_color.inc`: Device*, Cal*, Lab, ICCBased, Indexed,
  Separation/DeviceN com função de tinta, Pattern com e sem cor), ExtGState (LW, LC, LJ, ML, D, FL, CA, ca, BM com
  os 16 modos, Font, SMask de luminosidade/alfa com BC e TR, desenhada num bitmap e aplicada com `ClipToMask`).
- **Padrões**: ladrilho desenhado célula a célula no espaço do padrão (Matrix exata, PaintType 1 e 2); shading
  pattern. **Shadings** (`pdf_shade.inc`): 2 e 3 viram `CGShading` com `CGFunction`; 1 e 4–7 passam por um raster
  próprio por pixel (Gouraud, sem fendas; patches de Coons e tensor subdivididos). **Funções** (`pdf_func.inc`):
  tipos 0 (amostrada, interpolação multilinear), 2, 3 e 4 (calculadora PostScript compilada).
- **Imagens** (`pdf_image.inc`): todos os bpc, Decode, ImageMask com a cor/padrão de preenchimento, Mask por cor e
  por stream, SMask, Matte, imagem inline com abreviações, DCT pelo ImageIO, CCITT G3/G4 (`pdf_ccitt.inc`, T.4/T.6).
- **Texto** (`pdf_font.inc`, `pdf_glyph.inc`, `pdf_enc.inc`): Type0/CID (CMaps embutidos, Identity, usecmap, W/W2,
  DW2, escrita vertical), Type1 (eexec, Subrs, flex, seac), Type1C/CFF (Type 2 charstrings, CID-keyed com FDSelect),
  TrueType (glyf simples e composto, cmap 3/1, 3/0, 1/0, post), Type3 (CharProcs com d0/d1), fonte do sistema
  quando não embutida; Differences e as codificações Standard/WinAnsi/MacRoman/Symbol; os 8 modos de `Tr`
  (recorte acumulado até `ET`), Tc/Tw/Tz/TL/Ts, TJ.
- **Form XObject** com Matrix/BBox e grupo de transparência (camada), conteúdo opcional (`BDC /OC`, OCMD, `/D /OFF`),
  aparência `/N` das anotações (sem Hidden/NoView/Popup), recorte pela CropBox.
- `cx_pdf_page_glyphs`: percorre a página e entrega cada glifo (Unicode + caixa na página) — base do texto do PDFKit.
- Correção no CoreGraphics achada aqui: `CGImageCreate` com `kCGBitmapByteOrder32Little` lia os componentes na ordem
  errada (afetava ImageIO, captura de tela e todo app que cria imagem BGRA).

Testes: `tools/cgpdf_draw_host.py` 17/17 — o mOS desenha cada PDF num bitmap e compara pixel a pixel com o poppler
(`pdftoppm`) ou o Ghostscript: paths/traços/tracejado, recortes, CMYK/Indexed, imagens 8 e 16 bits, máscaras
(estêncil, SMask, SMask com Matte, por cor, inline), CCITT G4 do PIL, shadings 2/3/4/6, ladrilho, form + grupo + blend + conteúdo
opcional, máscara suave do ExtGState, Type3, anotação, texto CFF e TrueType do Ghostscript e Type 1 (Nimbus da URW)
embutido à mão. `tools/cg_context_test.c` 83/83.
Limites: JPX (JPEG 2000) e JBIG2 não decodificam (a imagem não aparece); SMask do ExtGState só em contexto de bitmap.
Fonte não embutida usa a do sistema com contornos reais (passo 5c).

### Passo 5c — CGPDFContext e contornos vetoriais das fontes (PR de 16 nov. 2026)

**Contornos** (`ct_outline.c`): o kernel só entrega o raster dos glifos; agora as tabelas da face (`GUI_FONT`/
`GF_TABLE`) são lidas uma vez e os contornos saem dos leitores do PDF (TrueType glyf/loca com compostos, CFF/OpenType
com CID). `CTFontCreatePathForGlyph` devolve o contorno de verdade (com a matriz do chamador) — antes era a caixa do
glifo. Também: avanço em milésimos de em e Unicode do glifo (cmap 3/10, 3/1 ou 0/x invertida segmento a segmento).

**CGPDFContext** (`coregraphics_pdf.inc`): `CGPDFContextCreate`/`CreateWithURL`, `BeginPage` (caixas Media/Crop/Bleed/
Trim/Art por página), `EndPage`, `CGContextBeginPage`/`EndPage`, `Close`, `SetURLForRect`, `SetDestinationForRect`,
`AddDestinationAtPoint`, `SetOutline`, `AddDocumentMetadata`, `BeginTag`/`EndTag` (marked content + árvore de
estrutura com ParentTree, Alt/ActualText/T/Lang), as chaves `kCGPDFContext*`/`kCGPDFOutline*`/`kCGPDFTagProperty*`.
O contexto é um bitmap do tamanho da página com um gravador: paths e traços saem vetoriais (o traço como contorno já
calculado), recortes como `W n`/`W* n`, máscara como SMask de luminosidade, imagens como XObject com SMask, gradientes e
`CGShading` como shading 2/3 com função amostrada, camadas como form com grupo de transparência, alfa e os 16 blend
modes do PDF no ExtGState, texto do CTFont em fonte Type3 com os contornos da face e ToUnicode (selecionável). O que o
PDF não expressa (sombra, padrão, gradiente cônico, blend Porter-Duff) é pintado no bitmap e sai como imagem no lugar
certo da ordem. Info em UTF-16, PDF/A-2B (XMP + OutputIntent sRGB do nosso ICC), cifra padrão RC4 40/128 (R2/R3) com
senha de usuário/dono e as permissões (`AllowsPrinting`/`AllowsCopying`/`AccessPermissions`).
Correção achada no caminho: `CGContextFillRect`/`ClearRect` com CTM girada pintavam a caixa envolvente.

Testes: `tools/cgpdf_write_host.py` 21/21 — a mesma cena em PDF e em bitmap, o PDF rasterizado pelo poppler bate com
o bitmap (erro médio 1,7/255); texto extraído pelo `pdftotext`, metadados/tags/páginas pelo `pdfinfo`, links,
destinos, outline, cifra (usuário abre, sem senha não abre, dono decifra, página cifrada = sem cifra) e o PDF relido
pelo nosso `CGContextDrawPDFPage` (erro médio 1,8). `tools/ct_outline_host.py` 19/19 glifos (DejaVu, Inter, JetBrains
Mono, Noto Sans JP CFF, Noto Arabic) contra o FreeType. `cg_context_test` 83/83, `cgpdf_draw_host` 17/17.
Limites: o texto em PDF sai em Type3 (contornos), não com a fonte TrueType embutida; o raster de reserva é a 72 dpi.

**Base do PDFKit** (mesmo PR): `pdf_write.c` regrava um PDF novo com páginas copiadas de documentos abertos
(ordem, junção, rotação, caixas, anotações de origem e novas com destino na página nova, outline, Info), com os
objetos alcançáveis copiados uma vez e referência à árvore antiga virando null; e `cx_pdf_page_text` (cgpdf_draw.c)
dá o texto da página com a caixa de cada caractere (espaços e quebras inferidos; `Tc` não vira espaço).
Testes: `tools/pdf_write_host.py` 12/12 (cada página regravada = a de origem no poppler; texto CFF e Type3 preservados;
PDF cifrado destravado sai sem cifra) e o texto de 4 PDFs igual ao do `pdftotext` (no `cgpdf_draw_host.py`).

### Passo 6 — PDFKit (PR de 16 nov. 2026)

`PDFKit.framework` sem dylib cai no libSystem (`kernel/compat/macho.c`); as classes ficam em `pdfkit.c` (modelo) e
`pdfkit_view.c` (views), registradas por `cx_pdfkit_init` (userspace_services.c).
- **PDFDocument**: `initWithURL:`/`initWithData:`/`init`, páginas (inserir, remover, trocar, `indexForPage:`), versão,
  cifra (`isEncrypted`, `isLocked`, `unlockWithPassword:`, as 8 permissões, `permissionsStatus`), `documentAttributes`
  (Info em UTF-16, Keywords como array, datas como NSDate), `outlineRoot` (do PDF ou novo), `string`, busca
  (`findString:withOptions:` com caixa, acentos e para trás; `fromSelection:`, `beginFindString(s):` e as notificações),
  seleções por caractere e por ponto, `dataRepresentation`/`writeTo…` (sem mudança devolve os bytes originais; com
  mudança, regrava por `pdf_write.c`: ordem, rotação, caixas, anotações, Info e outline).
- **PDFPage**: `initWithImage:` (PDF de uma página pelo CGPDFContext), caixas, rotação, `label` (/PageLabels: decimal,
  romano, letras, prefixo), anotações (as do PDF lidas na primeira vez), `drawWithBox:toContext:` (conteúdo + anotações),
  `transformContext:forBox:`, `thumbnailOfSize:forBox:`, texto (`string`, `numberOfCharacters`,
  `characterBoundsAtIndex:`, `characterIndexAtPoint:`) e seleção (intervalo, retângulo, dois pontos, palavra, linha).
- **PDFSelection** (várias páginas e intervalos, `boundsForPage:`, `selectionsByLine`, estender, cor, desenho),
  **PDFAnnotation** (tipo, caixa, conteúdo, cores, borda, URL/ação/destino, QuadPoints, Ink, chaves `/…`, aparência das
  novas: Highlight, Underline, StrikeOut, Square, Circle, Line, Ink, Text, FreeText/Widget/Stamp; as de origem sem
  mudança usam o /AP do PDF), **PDFOutline**, **PDFDestination**, **PDFAction** URL/GoTo/Named/ResetForm/RemoteGoTo,
  **PDFBorder** e as constantes (atributos, opções, notificações, subtipos e chaves de anotação).
- **PDFView**: as páginas renderizadas pelo Core Graphics na escala da view (cache de 8), os 4 modos de exibição (livro,
  RTL), escala/zoom/ajuste automático, navegação com histórico, destinos e seleção, seleção com o mouse (arrastar,
  duplo e triplo clique), links (delegate `PDFViewWillClickOnLink:withURL:` ou NSWorkspace), rolagem, teclado,
  copiar (respeitando a permissão), conversões de ponto/retângulo e as notificações. **PDFThumbnailView**.
- Renderizador: `cx_pdf_draw_page_content` (página sem anotações) e `cx_pdf_draw_annotation` (uma aparência);
  `cx_pdf_dest_array` resolve destino nomeado com coordenadas.

Testes: `tools/pdfkit_host.py` 43 + 4 — `tools/pdfkit_host.c` traz um runtime ObjC mínimo (objc_msgSend em assembly,
classes com superclasse, retain/release com dealloc) e a Foundation que o PDFKit usa, sobre o Core Graphics real:
leitura, atributos, texto, busca (e notificações), palavra/linha/índice, anotações e outline do arquivo, edição
completa e regravação relida pelo PDFKit e pelo poppler, cifra, página de imagem e miniatura.
Limite honesto: PDFView e PDFThumbnailView só foram compilados (x86_64 e arm64); não há AppKit no host para rodá-los.

**Lacunas do corpus** (docs/lacunas/mariana.md, mesmo PR): `CTFontCopyNameForGlyph` (tabela post, senão AGL),
`CTFontManagerRegisterFontURLs`/`RegisterFontDescriptors`/`UnregisterFontsForURL`/`UnregisterFontURLs`,
`CTRunGetImageBounds`/`CTRunGetTypeID`, oito constantes kCT*, `kCGColorWhite`, `CGContextSetCTM`/`SetBaseCTM`/
`Set|GetFontSmoothingStyle`, `ColorSyncDeviceCopyDeviceInfo`/`ColorSyncIterateDeviceProfiles` (o monitor com o perfil
sRGB gravado em ~/Library/ColorSync/Profiles) e 21 constantes kColorSync* de dispositivo. Os de display/eventos/CGS
da lista são da Camila (avisado ao Igor).

### Passo 7 — Core Text: quebra, bidi, parágrafo, frame e fontes (PR de 16 nov. 2026)

- `ct_layout.c` (C puro): oportunidades de quebra UAX #14 (obrigatórias e permitidas), grafemas UAX #29 e bidi UAX #9
  completo (embeddings, isolates, números) com a reordenação L1/L2 da linha, pela ICU da libicucore do mOS (carregador
  próprio de `ubidi`/`ubrk`, sem mexer na tabela da Foundation); sem ICU, regras simplificadas.
- Typesetter: o texto e as análises guardados; `CTTypesetterSuggestLineBreak/ClusterBreak` pelas oportunidades UAX #14
  e pelos grafemas (espaço do fim "pendura"); a ordem visual da linha pelos níveis da UAX #9; opções
  `ForcedEmbeddingLevel` e `DisableBidiProcessing`.
- `coretext_frame.inc`: `CTParagraphStyle` (18 especificadores; o `NSParagraphStyle` do AppKit também vale),
  `CTTextTab`, `CTRunDelegate` (largura e métricas do anexo), `CTGlyphInfo` (glifo no lugar do da cmap),
  `CTRubyAnnotation` (dados), 40 constantes de atributo/frame/typesetter, e o framesetter de verdade no lugar do falso
  do `vscode_frameworks.c`: recuos, alinhamento (natural pela direção, justificado menos a última linha), tabulações
  (esquerda, direita, centro; padrão 28 pt), múltiplo/mínimo/máximo de altura, espaçamento, espaço antes/depois,
  modos de quebra e truncagem, origens relativas à caixa do caminho, faixa visível, `SuggestFrameSizeWithConstraints`,
  `CreateWithTypesetter`, `GetTypesetter`, `GetFrameAttributes`. Linha: `GetStringIndexForPosition`,
  `GetPenOffsetForFlush`, `EnumerateCaretOffsets`; typesetter com offset.
- `coretext_fonts.inc`: `CTFontCopyName` pela chave (tabela `name`, 18 chaves), `CopyDisplayName`, idiomas pela OS/2,
  `HasTable`, carets de ligadura pelo GDEF, limites ópticos, cópias por família/plataforma/opções, descritores
  derivados (família, recurso, traços, variação), casamento com progresso, coleções (atributos, cópia, ordenação),
  gerenciador (URLs e nomes disponíveis, registro/remoção em lote, escopo, suporte, ativação automática, pedidos),
  `CTRunGetAdvancesPtr` e `GetBaseAdvancesAndOrigins`.

Testes: `tools/ct_layout_host.c` (com e sem ICU), `tools/coretext_host.c` 32/32 (framesetter com contas exatas,
estilo, tabulação, truncagem, run delegate, nomes/idiomas de uma fonte real registrada), e os antigos
`ctrun_host`/`ctapps_host` seguem verdes. Limite: o texto de ruby é guardado mas não desenhado.

