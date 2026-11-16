# Lacunas de Mariana (CG, Core Text, ImageIO, PDFKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Mariana (CG, Core Text, ImageIO, PDFKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**20** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libswiftCoreGraphics.dylib | `_$sSo7CGPointV12CoreGraphicsE16debugDescriptionSSvg` | 2 | alt-tab, rectangle |
| CoreGraphics.framework | `_CGContextGetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGContextSetBaseCTM` | 1 | firefox |
| CoreGraphics.framework | `_CGContextSetCTM` | 1 | firefox |
| CoreGraphics.framework | `_CGContextSetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGPDFContextAddDestinationAtPoint` | 1 | firefox |
| CoreGraphics.framework | `_CGPDFContextSetDestinationForRect` | 1 | firefox |
| CoreGraphics.framework | `_CGPDFContextSetURLForRect` | 1 | firefox |
| CoreGraphics.framework | `_kCGColorWhite` | 1 | mos |
| CoreText.framework | `_CTFontManagerRegisterFontDescriptors` | 1 | iterm2 |
| CoreText.framework | `_CTFontManagerRegisterFontURLs` | 1 | firefox |
| CoreText.framework | `_CTFontManagerUnregisterFontsForURL` | 1 | iterm2 |
| CoreText.framework | `_CTRunGetImageBounds` | 1 | alt-tab |
| CoreText.framework | `_kCTFontMatrixAttribute` | 1 | iterm2 |
| CoreText.framework | `_kCTLigatureAttributeName` | 1 | iterm2 |
| CoreText.framework | `_kCTWritingDirectionAttributeName` | 1 | firefox |
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 1 | iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 1 | iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 1 | rectangle |
| libswiftCoreGraphics.dylib | `_$sSo6CGSizeV12CoreGraphicsE16debugDescriptionSSvg` | 1 | rectangle |
