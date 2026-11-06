// mOS — medição e desenho de texto do OpenSwiftUI fora da Apple (entra no módulo OpenSwiftUICore).
// No macOS isso vem do UIFoundation (NSStringDrawing, Objective-C); aqui é feito sobre a medição de texto
// do AppKit do mOS (mos_measure) e o desenho no canvas (mos_draw_text/mos_draw_rect).
//
// O layout aceita trechos com estilos diferentes (tamanho, peso, cor, sublinhado, riscado, deslocamento da
// linha de base, kern), quebra por palavras (palavras longas são cortadas por caractere), alinhamento,
// espaçamento entre linhas, limite de linhas e de altura com reticências no fim, e redução de escala até
// o minimumScaleFactor. As métricas verticais são as do CoreText do mOS (ascent 0,952 e descent 0,241
// do tamanho), para baterem com as linhas de base que o OpenSwiftUI calcula pelas fontes.
#if !canImport(Darwin)
package import Foundation
package import OpenCoreGraphicsShims
import MOSBridge

package enum MOSTextLayout {
    package struct Line { package var text: String; package var width: CGFloat }

    // MARK: - medição básica

    /// Largura e altura de uma linha de texto na fonte do sistema do mOS.
    package static func measure(_ s: String, size: CGFloat, weight: Int32 = 0) -> CGSize {
        var w = 0.0, h = 0.0
        s.withCString { mos_measure($0, Double(size), weight, &w, &h) }
        return CGSize(width: CGFloat(w), height: CGFloat(h))
    }

    private struct MeasureKey: Hashable { var text: String; var size: CGFloat; var weight: Int32 }
    nonisolated(unsafe) private static var advanceCache: [MeasureKey: CGFloat] = [:]
    nonisolated(unsafe) private static var paddingCache: [MeasureKey: CGFloat] = [:]

    /// Avanço horizontal do texto (sem a margem interna que o NSTextField do mos_measure soma).
    package static func advance(_ s: String, size: CGFloat, weight: Int32) -> CGFloat {
        if s.isEmpty { return 0 }
        let key = MeasureKey(text: s, size: size, weight: weight)
        if let w = advanceCache[key] { return w }
        let padKey = MeasureKey(text: "", size: size, weight: weight)
        let pad: CGFloat
        if let p = paddingCache[padKey] { pad = p } else {
            pad = measure("", size: size, weight: weight).width
            paddingCache[padKey] = pad
        }
        let w = max(0, measure(s, size: size, weight: weight).width - pad)
        if advanceCache.count > 4096 { advanceCache.removeAll(keepingCapacity: true) }
        advanceCache[key] = w
        return w
    }

    // MARK: - API simples (uma fonte e uma cor)

    /// Quebra o texto em linhas que caibam em `width` (palavra a palavra; palavras longas são cortadas).
    package static func lines(_ s: String, width: CGFloat, size: CGFloat, weight: Int32 = 0) -> [Line] {
        var style = Style(); style.size = size; style.weight = weight
        let r = layout([Span(text: s, style: style)], width: width)
        return r.lines.map { Line(text: $0.fragments.map(\.text).joined(), width: $0.width) }
    }

    /// Retângulo ocupado pelo texto quebrado em `maxSize` (o que o boundingRect do UIFoundation devolve).
    package static func boundingRect(_ s: String, maxSize: CGSize, size: CGFloat, weight: Int32 = 0) -> CGRect {
        var style = Style(); style.size = size; style.weight = weight
        let r = layout([Span(text: s, style: style)], width: maxSize.width, height: maxSize.height)
        return CGRect(origin: .zero, size: CGSize(width: ceil(r.size.width), height: ceil(r.size.height)))
    }

    /// Desenha o texto quebrado no retângulo (dentro do canvas do mOS; origem no topo).
    package static func draw(_ s: String, in rect: CGRect, size: CGFloat, weight: Int32 = 0,
                             red: Double, green: Double, blue: Double, alpha: Double) {
        var style = Style(); style.size = size; style.weight = weight
        style.red = red; style.green = green; style.blue = blue; style.alpha = alpha
        let r = layout([Span(text: s, style: style)], width: rect.width, height: rect.height + 1)
        draw(r, in: rect)
    }

    // MARK: - texto com estilos

    /// Estilo de um trecho de texto.
    package struct Style: Equatable {
        package var size: CGFloat = 13
        /// Peso no formato do mos_font: 0 normal, 1 semibold, 2 bold, 3 monoespaçada.
        package var weight: Int32 = 0
        package var red = 0.0, green = 0.0, blue = 0.0, alpha = 0.85
        package var underline = false
        package var strikethrough = false
        package var baselineOffset: CGFloat = 0
        /// Espaço extra depois de cada caractere (kern/tracking).
        package var kern: CGFloat = 0
        package init() {}

        package var ascent: CGFloat { size * 0.952 }
        package var descent: CGFloat { size * 0.241 }

        func scaled(_ s: CGFloat) -> Style {
            guard s != 1 else { return self }
            var c = self
            c.size = max(1, (size * s * 4).rounded() / 4)
            c.kern *= s
            c.baselineOffset *= s
            return c
        }
    }

    package struct Span {
        package var text: String
        package var style: Style
        package init(text: String, style: Style) { self.text = text; self.style = style }
    }

    package enum Alignment { case leading, center, trailing }
    package enum Truncation { case tail, head, middle, clip }

    /// Propriedades de parágrafo (do NSParagraphStyle).
    package struct Paragraph {
        package var alignment: Alignment = .leading
        package var truncation: Truncation = .tail
        package var lineSpacing: CGFloat = 0
        package var lineHeightMultiple: CGFloat = 0
        package var minimumLineHeight: CGFloat = 0
        package var maximumLineHeight: CGFloat = 0
        package var firstLineHeadIndent: CGFloat = 0
        package init() {}
    }

    /// Pedaço de uma linha já posicionado (x relativo ao início da linha).
    package struct Fragment {
        package var text: String
        package var style: Style
        package var x: CGFloat
        package var width: CGFloat
    }

    package struct LaidLine {
        package var fragments: [Fragment] = []
        /// Largura sem os espaços do fim.
        package var width: CGFloat = 0
        package var ascent: CGFloat = 0
        package var descent: CGFloat = 0
        package var top: CGFloat = 0
        package var height: CGFloat { ascent + descent }
        package var baseline: CGFloat { top + ascent }
        package var indent: CGFloat = 0
    }

    package struct Result {
        package var lines: [LaidLine] = []
        package var size: CGSize = .zero
        package var firstBaseline: CGFloat = 0
        package var lastBaseline: CGFloat = 0
        package var truncated = false
        package var scale: CGFloat = 1
        package var paragraph = Paragraph()
    }

    /// Unidade de quebra: palavra seguida dos espaços dela (pode atravessar trechos de estilos diferentes).
    private struct Word {
        var pieces: [(String, Style)] = []
        var width: CGFloat = 0
        var trailing: CGFloat = 0   // largura dos espaços do fim
    }

    private static func width(_ s: String, _ st: Style) -> CGFloat {
        advance(s, size: st.size, weight: st.weight) + st.kern * CGFloat(s.count)
    }

    /// Faz o layout dos trechos em `width` (infinito = sem quebra) e `height` (infinito = sem limite).
    /// `maxLines` 0 = sem limite de linhas; `minimumScaleFactor` < 1 reduz a fonte até caber.
    package static func layout(_ spans: [Span], paragraph: Paragraph = Paragraph(), width: CGFloat = .infinity,
                               height: CGFloat = .infinity, maxLines: Int = 0,
                               scale: CGFloat = 1, minimumScaleFactor: CGFloat = 1) -> Result {
        var r = layoutOnce(spans, paragraph, width, height, maxLines, scale)
        guard r.truncated, minimumScaleFactor > 0, minimumScaleFactor < 1 else { return r }
        var s: CGFloat = 0.95
        while s > minimumScaleFactor {
            let t = layoutOnce(spans, paragraph, width, height, maxLines, scale * s)
            if !t.truncated { return t }
            s -= 0.05
        }
        r = layoutOnce(spans, paragraph, width, height, maxLines, scale * minimumScaleFactor)
        return r
    }

    private static func layoutOnce(_ spans: [Span], _ paragraph: Paragraph, _ maxWidth: CGFloat, _ maxHeight: CGFloat,
                                   _ maxLines: Int, _ scale: CGFloat) -> Result {
        let wraps = maxWidth.isFinite && maxWidth > 0
        let defaultStyle = (spans.first?.style ?? Style()).scaled(scale)

        // parágrafos (separados por \n), cada um uma lista de pedaços com estilo
        var paragraphs: [[(String, Style)]] = [[]]
        for span in spans {
            let st = span.style.scaled(scale)
            var first = true
            for part in span.text.split(omittingEmptySubsequences: false, whereSeparator: { $0 == "\n" || $0 == "\r" || $0 == "\u{2028}" || $0 == "\u{2029}" }) {
                if !first { paragraphs.append([]) }
                first = false
                if !part.isEmpty { paragraphs[paragraphs.count - 1].append((String(part), st)) }
            }
        }

        var lines: [LaidLine] = []
        var paragraphStarts: Set<Int> = []
        for pieces in paragraphs {
            paragraphStarts.insert(lines.count)
            // palavras
            var words: [Word] = []
            var current = Word()
            var inSpaces = false
            for (text, st) in pieces {
                var run = ""
                func flushRun() {
                    guard !run.isEmpty else { return }
                    let w = width(run, st)
                    current.pieces.append((run, st))
                    current.width += w
                    if inSpaces { current.trailing += w }
                    run = ""
                }
                for ch in text {
                    let isSpace = ch == " " || ch == "\t"
                    if isSpace != inSpaces {
                        flushRun()
                        if !isSpace && inSpaces {   // nova palavra depois dos espaços
                            words.append(current); current = Word()
                        }
                        inSpaces = isSpace
                    }
                    run.append(ch == "\t" ? " " : ch)
                }
                flushRun()
            }
            if !current.pieces.isEmpty { words.append(current) }

            var line = LaidLine()
            line.indent = paragraph.firstLineHeadIndent   // só a primeira linha do parágrafo; finish() zera
            var x: CGFloat = 0
            func finish() {
                line.width = max(0, line.width)
                if line.fragments.isEmpty { line.ascent = defaultStyle.ascent; line.descent = defaultStyle.descent }
                lines.append(line)
                line = LaidLine()
                x = 0
            }
            func append(_ text: String, _ st: Style, _ w: CGFloat) {
                if var last = line.fragments.last, last.style == st {
                    last.text += text; last.width += w
                    line.fragments[line.fragments.count - 1] = last
                } else {
                    line.fragments.append(Fragment(text: text, style: st, x: x, width: w))
                }
                x += w
                line.ascent = max(line.ascent, st.ascent + max(0, st.baselineOffset))
                line.descent = max(line.descent, st.descent + max(0, -st.baselineOffset))
            }
            let available = { (indent: CGFloat) in maxWidth - indent }
            for word in words {
                let body = word.width - word.trailing
                if wraps && !line.fragments.isEmpty && x + body > available(line.indent) {
                    finish()
                }
                if wraps && line.fragments.isEmpty && body > available(line.indent) {
                    // palavra maior que a linha: corta por caractere
                    for (text, st) in word.pieces {
                        for ch in text {
                            let s = String(ch)
                            let w = width(s, st)
                            if !line.fragments.isEmpty && x + w > available(line.indent) && ch != " " {
                                line.width = x
                                finish()
                            }
                            append(s, st, w)
                        }
                    }
                    line.width = x - word.trailing
                    continue
                }
                for (text, st) in word.pieces { append(text, st, width(text, st)) }
                line.width = x - word.trailing
            }
            finish()
        }

        // alturas das linhas
        var top: CGFloat = 0
        for i in lines.indices {
            var h = lines[i].height
            if paragraph.lineHeightMultiple > 0 { h *= paragraph.lineHeightMultiple }
            if paragraph.minimumLineHeight > 0 { h = max(h, paragraph.minimumLineHeight) }
            if paragraph.maximumLineHeight > 0 { h = min(h, paragraph.maximumLineHeight) }
            let extra = h - lines[i].height
            lines[i].ascent += extra   // a sobra fica acima do texto, como no TextKit
            lines[i].top = top
            top += h + paragraph.lineSpacing
        }

        // limite de linhas (maxLines e altura)
        var limit = maxLines > 0 ? maxLines : Int.max
        if maxHeight.isFinite && maxHeight > 0 {
            var fit = 0
            for l in lines where l.top + l.height <= maxHeight + 0.5 { fit += 1 }
            limit = min(limit, max(1, fit))
        }
        var truncated = false
        if lines.count > limit {
            truncated = true
            let kept = Array(lines[..<limit])
            let rest = lines[(limit - 1)...]
            var last = kept[limit - 1]
            // junta o resto do texto na última linha e corta com reticências
            var pieces: [(String, Style)] = []
            for (n, l) in rest.enumerated() {
                if n > 0, paragraphStarts.contains(limit - 1 + n), let st = pieces.last?.1 { pieces.append((" ", st)) }
                for f in l.fragments { pieces.append((f.text, f.style)) }
            }
            last = truncate(pieces, line: last, width: wraps ? maxWidth - last.indent : .infinity, mode: paragraph.truncation)
            lines = Array(kept[..<(limit - 1)]) + [last]
        }

        var r = Result()
        r.lines = lines
        r.truncated = truncated
        r.scale = scale
        r.paragraph = paragraph
        let w = lines.map { $0.width + $0.indent }.max() ?? 0
        let h = lines.last.map { $0.top + $0.height } ?? 0
        r.size = CGSize(width: w, height: h)
        r.firstBaseline = lines.first?.baseline ?? 0
        r.lastBaseline = lines.last?.baseline ?? 0
        return r
    }

    /// Corta `pieces` para caber em `width` com "…" (no fim, no começo ou no meio).
    private static func truncate(_ pieces: [(String, Style)], line: LaidLine, width maxW: CGFloat, mode: Truncation) -> LaidLine {
        var chars: [(Character, Style)] = []
        for (t, st) in pieces { for c in t { chars.append((c, st)) } }
        while let l = chars.last, l.0 == " " { chars.removeLast() }
        func total(_ cs: ArraySlice<(Character, Style)>) -> CGFloat { cs.reduce(0) { $0 + width(String($1.0), $1.1) } }
        var keepHead = chars.count, keepTail = 0
        let ellipsisStyle = chars.last?.1 ?? Style()
        let ell = mode == .clip ? 0 : width("…", ellipsisStyle)
        if maxW.isFinite {
            switch mode {
            case .tail, .clip:
                while keepHead > 0 && total(chars[..<keepHead]) + ell > maxW { keepHead -= 1 }
            case .head:
                keepHead = 0; keepTail = chars.count
                while keepTail > 0 && total(chars[(chars.count - keepTail)...]) + ell > maxW { keepTail -= 1 }
            case .middle:
                keepHead = chars.count / 2; keepTail = chars.count - keepHead
                while keepHead + keepTail > 0 && total(chars[..<keepHead]) + total(chars[(chars.count - keepTail)...]) + ell > maxW {
                    if keepHead >= keepTail { keepHead -= 1 } else { keepTail -= 1 }
                }
            }
        }
        var out = LaidLine()
        out.top = line.top; out.indent = line.indent
        out.ascent = line.ascent; out.descent = line.descent
        var x: CGFloat = 0
        func add(_ s: String, _ st: Style) {
            let w = width(s, st)
            if var last = out.fragments.last, last.style == st {
                last.text += s; last.width += w
                out.fragments[out.fragments.count - 1] = last
            } else {
                out.fragments.append(Fragment(text: s, style: st, x: x, width: w))
            }
            x += w
        }
        for (c, st) in chars[..<keepHead] { add(String(c), st) }
        if mode != .clip { add("…", ellipsisStyle) }
        if keepTail > 0 { for (c, st) in chars[(chars.count - keepTail)...] { add(String(c), st) } }
        out.width = x
        return out
    }

    /// Opacidade herdada dos efeitos do DisplayList (multiplica o alfa de cada trecho ao desenhar).
    nonisolated(unsafe) package static var inheritedOpacity: Double = 1

    /// Desenha o layout no retângulo (canvas do mOS; origem no topo). O alinhamento usa `rect.width`.
    package static func draw(_ r: Result, in rect: CGRect) {
        let opacity = inheritedOpacity
        let boxWidth = rect.width.isFinite && rect.width > 0 ? rect.width : r.size.width
        for line in r.lines {
            let free = boxWidth - line.indent - line.width
            let dx: CGFloat
            switch r.paragraph.alignment {
            case .leading: dx = 0
            case .center: dx = max(0, free) / 2
            case .trailing: dx = max(0, free)
            }
            for f in line.fragments {
                var st = f.style
                st.alpha *= opacity
                let x = rect.minX + line.indent + dx + f.x
                let baseline = rect.minY + line.baseline - st.baselineOffset
                let y = baseline - st.ascent
                if st.kern == 0 {
                    f.text.withCString { mos_draw_text($0, Double(x), Double(y), Double(st.size), st.weight, st.red, st.green, st.blue, st.alpha) }
                } else {
                    var cx = x
                    for c in f.text {
                        let s = String(c)
                        s.withCString { mos_draw_text($0, Double(cx), Double(y), Double(st.size), st.weight, st.red, st.green, st.blue, st.alpha) }
                        cx += width(s, st)
                    }
                }
                let thickness = Double(max(1, st.size / 14))
                if st.underline {
                    mos_draw_rect(Double(x), Double(baseline) + Double(st.size) * 0.1, Double(f.width), thickness, 0,
                                  st.red, st.green, st.blue, st.alpha, 1, 1)
                }
                if st.strikethrough {
                    mos_draw_rect(Double(x), Double(baseline) - Double(st.size) * 0.3, Double(f.width), thickness, 0,
                                  st.red, st.green, st.blue, st.alpha, 1, 1)
                }
            }
        }
    }
}
#endif
