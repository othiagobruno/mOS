// mOS — atributos de texto do OpenSwiftUI → estilos do MOSTextLayout, e a NSAttributedString.boundingRect/draw
// que o UIFoundation dá no macOS (entra no módulo OpenSwiftUICore; usada por Text+StringDrawingContext.swift).
//
// Os atributos são os que o OpenSwiftUI grava ao resolver o Text (Text+NSAttributedString.swift):
//   .kitFont            CTFont do CoreText do mOS (tamanho, peso, traço de negrito/monoespaçada)
//   .kitForegroundColor cor da plataforma (NDColor fora da Apple) ou Color.Resolved
//   .kitUnderlineStyle / .kitStrikethroughStyle  NSNumber (≠ 0 liga)
//   .kitBaselineOffset, .kitKern, .kitTracking  CGFloat
//   .kitParagraphStyle  NSParagraphStyle (alinhamento, quebra/truncamento, espaçamento entre linhas)
#if !canImport(Darwin)
package import Foundation
package import OpenCoreGraphicsShims
#if canImport(CoreText)
package import CoreText   // mesmo nível de acesso dos outros arquivos do OpenSwiftUICore
#endif

/// Opções de desenho (o NSString.DrawingOptions do UIFoundation). O mOS sempre faz o layout completo
/// a partir da origem do retângulo; as opções ficam pela compatibilidade de código com o caminho Darwin.
package struct MOSStringDrawingOptions: OptionSet {
    package let rawValue: Int
    package init(rawValue: Int) { self.rawValue = rawValue }
    package static let usesLineFragmentOrigin = MOSStringDrawingOptions(rawValue: 1 << 0)
    package static let usesFontLeading = MOSStringDrawingOptions(rawValue: 1 << 1)
    package static let truncatesLastVisibleLine = MOSStringDrawingOptions(rawValue: 1 << 5)
    /// Pede o layout completo (no UIFoundation desliga o atalho de quebra rápida).
    package static let requiresFullTextLayout = MOSStringDrawingOptions(rawValue: 1 << 20)
}

#if canImport(CoreText)
/// Valor de atributo → CTFont sem cast dinâmico: sem ObjC, `as?`/`as!` para um tipo CF sempre falha no
/// runtime do Swift (como no Linux). Confere o CFTypeID e faz o bitcast.
package func mosCTFont(_ value: Any?) -> CTFont? {
    guard let value else { return nil }
    let object = value as AnyObject
    guard CFGetTypeID(object) == CTFontGetTypeID() else { return nil }
    return unsafeBitCast(object, to: CTFont.self)
}
#endif

// MARK: - atributos → estilo

enum MOSTextAttributes {
    /// Executa `body` com a opacidade dos efeitos aplicada ao texto desenhado.
    static func withOpacity(_ opacity: Double, _ body: () -> Void) {
        let saved = MOSTextLayout.inheritedOpacity
        MOSTextLayout.inheritedOpacity = saved * opacity
        defer { MOSTextLayout.inheritedOpacity = saved }
        body()
    }

    /// Componentes RGBA (sRGB) de uma cor gravada nos atributos.
    static func rgba(_ value: Any?) -> (Double, Double, Double, Double)? {
        guard let value else { return nil }
        if let c = value as? Color.Resolved {
            return (Double(c.red), Double(c.green), Double(c.blue), Double(c.opacity))
        }
        #if !canImport(CoreGraphics)
        if let c = value as? NDColor {
            return (Double(c.red), Double(c.green), Double(c.blue), Double(c.alpha))
        }
        #endif
        return nil
    }

    static func number(_ value: Any?) -> CGFloat? {
        switch value {
        case let v as CGFloat: return v
        case let v as Double: return CGFloat(v)
        case let v as Float: return CGFloat(v)
        case let v as Int: return CGFloat(v)
        case let v as NSNumber: return CGFloat(v.doubleValue)
        default: return nil
        }
    }

    /// Tamanho e peso (formato do mos_font) de uma fonte do CoreText.
    static func fontStyle(_ value: Any?) -> (CGFloat, Int32)? {
        #if canImport(CoreText)
        // só funções CT*: o CTFont é tipo CF de verdade no CoreText do mOS
        guard let font = mosCTFont(value) else { return nil }
        let traits = font.symbolicTraits
        // Font.Design.monospaced: o CoreText do mOS marca traitMonoSpace nesses descritores
        if traits.contains(.traitMonoSpace) { return (font.pointSize, 3) }
        let w = font.weight
        let weight: Int32 = (traits.contains(.traitBold) || w >= 0.35) ? 2 : (w >= 0.2 ? 1 : 0)
        return (font.pointSize, weight)
        #else
        return nil
        #endif
    }

    static func style(_ attributes: [NSAttributedString.Key: Any]) -> MOSTextLayout.Style {
        var st = MOSTextLayout.Style()
        if let (size, weight) = fontStyle(attributes[.kitFont]) { st.size = size; st.weight = weight }
        if let (r, g, b, a) = rgba(attributes[.kitForegroundColor]) { st.red = r; st.green = g; st.blue = b; st.alpha = a }
        st.underline = (number(attributes[.kitUnderlineStyle]) ?? 0) != 0
        st.strikethrough = (number(attributes[.kitStrikethroughStyle]) ?? 0) != 0
        st.baselineOffset = number(attributes[.kitBaselineOffset]) ?? 0
        st.kern = (number(attributes[.kitKern]) ?? 0) + (number(attributes[.kitTracking]) ?? 0)
        return st
    }

    static func paragraph(_ value: Any?) -> MOSTextLayout.Paragraph {
        var p = MOSTextLayout.Paragraph()
        guard let style = value as? NSParagraphStyle else { return p }
        let rtl = style.baseWritingDirection == .rightToLeft
        switch style.horizontalAlignment {
        case .left: p.alignment = .leading
        case .right: p.alignment = .trailing
        case .center: p.alignment = .center
        case .natural: p.alignment = rtl ? .trailing : .leading
        }
        switch style.lineBreakMode {
        case .byTruncatingHead: p.truncation = .head
        case .byTruncatingMiddle: p.truncation = .middle
        case .byClipping: p.truncation = .clip
        default: p.truncation = .tail
        }
        p.lineSpacing = style.lineSpacing
        p.lineHeightMultiple = style.lineHeightMultiple
        p.minimumLineHeight = style.minimumLineHeight
        p.maximumLineHeight = style.maximumLineHeight
        p.firstLineHeadIndent = style.firstLineHeadIndent
        return p
    }
}

extension NSAttributedString {
    /// Trechos com estilo (um por faixa de atributos iguais).
    package var mosSpans: [MOSTextLayout.Span] {
        guard length > 0 else { return [] }
        var spans: [MOSTextLayout.Span] = []
        enumerateAttributes(in: NSRange(location: 0, length: length), options: []) { attributes, range, _ in
            let text = attributedSubstring(from: range).string
            let st = MOSTextAttributes.style(attributes)
            if let i = spans.indices.last, spans[i].style == st {
                spans[i].text += text
            } else {
                spans.append(MOSTextLayout.Span(text: text, style: st))
            }
        }
        return spans
    }

    /// Parágrafo (alinhamento, truncamento, espaçamento) do primeiro caractere, como o UIFoundation.
    package var mosParagraph: MOSTextLayout.Paragraph {
        guard length > 0 else { return MOSTextLayout.Paragraph() }
        return MOSTextAttributes.paragraph(attribute(.kitParagraphStyle, at: 0, effectiveRange: nil))
    }

    /// Estilo do primeiro caractere (tamanho, peso e cor do bloco).
    package var mosFirstStyle: MOSTextLayout.Style {
        guard length > 0 else { return MOSTextLayout.Style() }
        return MOSTextAttributes.style(attributes(at: 0, effectiveRange: nil))
    }

    private func mosLayout(size: CGSize, context: NSStringDrawingContext?) -> MOSTextLayout.Result {
        MOSTextLayout.layout(
            mosSpans,
            paragraph: mosParagraph,
            width: size.width >= .greatestFiniteMagnitude ? .infinity : size.width,
            height: size.height >= .greatestFiniteMagnitude ? .infinity : size.height,
            maxLines: context?.maximumNumberOfLines ?? 0,
            scale: context?.mosFontScale ?? 1,
            minimumScaleFactor: context.map { $0.minimumScaleFactor > 0 ? $0.minimumScaleFactor : 1 } ?? 1
        )
    }

    /// Mede o texto quebrado em `size` e preenche as saídas do contexto (linhas de base, número de linhas,
    /// escala aplicada, truncamento) — a NSAttributedString.boundingRect(with:options:context:) do UIFoundation.
    /// (internal: o NSStringDrawingContext do shim é interno ao OpenSwiftUICore)
    func boundingRect(
        with size: CGSize,
        options: MOSStringDrawingOptions = [],
        context: NSStringDrawingContext?
    ) -> CGRect {
        let r = mosLayout(size: size, context: context)
        if let context {
            let base = context.mosFontScale
            context.actualScaleFactor = base > 0 ? r.scale / base : r.scale
            context.firstBaselineOffset = r.firstBaseline
            context.baselineOffset = r.lastBaseline
            context.numberOfLineFragments = r.lines.count
            context.hasTruncatedRanges = r.truncated
            if context.actualScaleFactor < 1 {
                // altura e linha de base finais já na escala reduzida
                context.scaledLineHeight = r.size.height
                context.scaledBaselineOffset = r.lastBaseline
            }
            context.totalBounds = CGRect(origin: .zero, size: r.size)
        }
        return CGRect(origin: .zero, size: r.size)
    }

    /// Desenha o texto quebrado em `rect` no canvas atual do mOS (origem no topo).
    func draw(
        with rect: CGRect,
        options: MOSStringDrawingOptions = [],
        context: NSStringDrawingContext?
    ) {
        // meio ponto de folga na altura para o arredondamento da medição não cortar a última linha
        let r = mosLayout(size: CGSize(width: rect.width, height: rect.height + 0.5), context: context)
        MOSTextLayout.draw(r, in: rect)
        if let context {
            context.numberOfLineFragments = r.lines.count
            context.hasTruncatedRanges = r.truncated
        }
    }

    /// Desenha a partir de um ponto, sem quebra (NSAttributedString.draw(at:)).
    package func draw(at point: CGPoint) {
        let r = MOSTextLayout.layout(mosSpans, paragraph: mosParagraph)
        MOSTextLayout.draw(r, in: CGRect(origin: point, size: r.size))
    }

    /// Tamanho em uma linha (NSAttributedString.size()).
    package func size() -> CGSize {
        MOSTextLayout.layout(mosSpans, paragraph: mosParagraph).size
    }
}
#endif
