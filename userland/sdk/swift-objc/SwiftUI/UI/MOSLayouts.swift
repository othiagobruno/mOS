// mOS — pilhas e grades "lazy" e o Grid do SwiftUI.framework, com as assinaturas da Apple (o upstream OpenSwiftUI
// não as tem). No mOS o layout é o mesmo das pilhas comuns: a diferença da Apple é só criar as views sob demanda.
// As grades são Layouts próprios com a regra de colunas da Apple (fixed, flexible e adaptive).
public import Foundation
public import OpenSwiftUICore

// MARK: - PinnedScrollableViews

/// Tipos de view que ficam presos às bordas da área de rolagem.
@available(macOS 11.0, *)
public struct PinnedScrollableViews: OptionSet, Sendable {
    public let rawValue: UInt32
    public init(rawValue: UInt32) { self.rawValue = rawValue }
    public static let sectionHeaders = PinnedScrollableViews(rawValue: 1 << 0)
    public static let sectionFooters = PinnedScrollableViews(rawValue: 1 << 1)
}

// MARK: - LazyVStack / LazyHStack

@available(macOS 11.0, *)
public struct LazyVStack<Content>: View where Content: View {
    var alignment: HorizontalAlignment
    var spacing: CGFloat?
    var pinnedViews: PinnedScrollableViews
    var content: Content

    public init(alignment: HorizontalAlignment = .center, spacing: CGFloat? = nil,
                pinnedViews: PinnedScrollableViews = .init(), @ViewBuilder content: () -> Content) {
        self.alignment = alignment
        self.spacing = spacing
        self.pinnedViews = pinnedViews
        self.content = content()
    }

    public var body: some View {
        VStack(alignment: alignment, spacing: spacing) { content }
    }
}

@available(macOS 11.0, *)
public struct LazyHStack<Content>: View where Content: View {
    var alignment: VerticalAlignment
    var spacing: CGFloat?
    var pinnedViews: PinnedScrollableViews
    var content: Content

    public init(alignment: VerticalAlignment = .center, spacing: CGFloat? = nil,
                pinnedViews: PinnedScrollableViews = .init(), @ViewBuilder content: () -> Content) {
        self.alignment = alignment
        self.spacing = spacing
        self.pinnedViews = pinnedViews
        self.content = content()
    }

    public var body: some View {
        HStack(alignment: alignment, spacing: spacing) { content }
    }
}

// MARK: - GridItem

/// A descrição de uma linha ou coluna de uma grade lazy.
@available(macOS 11.0, *)
public struct GridItem: Sendable {
    public enum Size: Sendable {
        case fixed(CGFloat)
        case flexible(minimum: CGFloat = 10, maximum: CGFloat = .infinity)
        case adaptive(minimum: CGFloat, maximum: CGFloat = .infinity)
    }
    public var size: GridItem.Size
    public var spacing: CGFloat?
    public var alignment: Alignment?

    public init(_ size: GridItem.Size = .flexible(), spacing: CGFloat? = nil, alignment: Alignment? = nil) {
        self.size = size
        self.spacing = spacing
        self.alignment = alignment
    }
}

/// Espaço padrão entre células de grade (o mesmo das pilhas da Apple no macOS).
let _mosGridDefaultSpacing: CGFloat = 8

/// Tamanhos das trilhas (colunas de uma LazyVGrid, linhas de uma LazyHGrid) para um comprimento disponível.
/// Regra da Apple: fixed primeiro; adaptive cabe quantas trilhas de `minimum` couberem no espaço dela;
/// flexible divide o resto em partes iguais, presas entre minimum e maximum.
func _mosGridTracks(_ items: [GridItem], length: CGFloat?) -> [(size: CGFloat, spacing: CGFloat, alignment: Alignment?)] {
    var expanded: [(size: CGFloat?, min: CGFloat, max: CGFloat, spacing: CGFloat, alignment: Alignment?, adaptive: Bool)] = []
    for item in items {
        let sp = item.spacing ?? _mosGridDefaultSpacing
        switch item.size {
        case .fixed(let w): expanded.append((w, w, w, sp, item.alignment, false))
        case .flexible(let mn, let mx): expanded.append((nil, mn, mx, sp, item.alignment, false))
        case .adaptive(let mn, let mx): expanded.append((nil, mn, mx, sp, item.alignment, true))
        }
    }
    guard !expanded.isEmpty else { return [] }
    let total = length ?? expanded.reduce(0) { $0 + ($1.size ?? $1.min) } + expanded.dropLast().reduce(0) { $0 + $1.spacing }
    let fixed = expanded.compactMap(\.size).reduce(0, +)
    let gaps = expanded.dropLast().reduce(0) { $0 + $1.spacing }
    let flexCount = CGFloat(expanded.filter { $0.size == nil }.count)
    let share = flexCount > 0 ? max(0, (total - fixed - gaps) / flexCount) : 0
    var out: [(CGFloat, CGFloat, Alignment?)] = []
    for e in expanded {
        if let s = e.size {
            out.append((s, e.spacing, e.alignment))
        } else if e.adaptive {
            // quantas trilhas de `min` cabem na parte desta, cada uma com o espaçamento dela
            let n = max(1, Int((share + e.spacing) / (e.min + e.spacing)))
            let each = min(e.max, (share - CGFloat(n - 1) * e.spacing) / CGFloat(n))
            for _ in 0..<n { out.append((max(each, 0), e.spacing, e.alignment)) }
        } else {
            out.append((min(max(share, e.min), e.max), e.spacing, e.alignment))
        }
    }
    return out.map { (size: $0.0, spacing: $0.1, alignment: $0.2) }
}

/// Grade com trilhas no eixo cruzado (`vertical`: colunas, cresce para baixo; senão linhas, cresce para a direita).
struct _MOSLazyGridLayout: Layout {
    var items: [GridItem]
    var vertical: Bool
    var alignment: Alignment
    var spacing: CGFloat?

    private func arrange(_ proposal: ProposedViewSize, _ subviews: Subviews)
        -> (tracks: [(size: CGFloat, spacing: CGFloat, alignment: Alignment?)], lines: [CGFloat], size: CGSize) {
        let tracks = _mosGridTracks(items, length: vertical ? proposal.width : proposal.height)
        guard !tracks.isEmpty else { return ([], [], .zero) }
        let lineSpacing = spacing ?? _mosGridDefaultSpacing
        var lines: [CGFloat] = []
        var i = 0
        while i < subviews.count {
            var line: CGFloat = 0
            for t in tracks where i < subviews.count {
                let s = subviews[i].sizeThatFits(vertical ? ProposedViewSize(width: t.size, height: nil)
                                                          : ProposedViewSize(width: nil, height: t.size))
                line = max(line, vertical ? s.height : s.width)
                i += 1
            }
            lines.append(line)
        }
        let cross = tracks.reduce(0) { $0 + $1.size } + tracks.dropLast().reduce(0) { $0 + $1.spacing }
        let main = lines.reduce(0, +) + CGFloat(max(lines.count - 1, 0)) * lineSpacing
        return (tracks, lines, vertical ? CGSize(width: cross, height: main) : CGSize(width: main, height: cross))
    }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        arrange(proposal, subviews).size
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        let (tracks, lines, size) = arrange(proposal, subviews)
        guard !tracks.isEmpty else { return }
        let lineSpacing = spacing ?? _mosGridDefaultSpacing
        // o bloco de trilhas é alinhado no eixo cruzado conforme `alignment`
        let crossFree = vertical ? bounds.width - size.width : bounds.height - size.height
        let crossStart: CGFloat
        if vertical {
            switch alignment.horizontal { case .leading: crossStart = 0; case .trailing: crossStart = crossFree; default: crossStart = crossFree / 2 }
        } else {
            switch alignment.vertical { case .top: crossStart = 0; case .bottom: crossStart = crossFree; default: crossStart = crossFree / 2 }
        }
        var i = 0
        var mainPos: CGFloat = 0
        for line in lines {
            var crossPos = crossStart
            for t in tracks where i < subviews.count {
                let cellAlign = t.alignment ?? .center
                let p = vertical ? ProposedViewSize(width: t.size, height: line) : ProposedViewSize(width: line, height: t.size)
                let s = subviews[i].sizeThatFits(p)
                let cw = vertical ? t.size : line, ch = vertical ? line : t.size
                let ox: CGFloat, oy: CGFloat
                switch cellAlign.horizontal { case .leading: ox = 0; case .trailing: ox = cw - s.width; default: ox = (cw - s.width) / 2 }
                switch cellAlign.vertical { case .top: oy = 0; case .bottom: oy = ch - s.height; default: oy = (ch - s.height) / 2 }
                let x = bounds.minX + (vertical ? crossPos : mainPos) + ox
                let y = bounds.minY + (vertical ? mainPos : crossPos) + oy
                subviews[i].place(at: CGPoint(x: x, y: y), anchor: .topLeading, proposal: p)
                crossPos += t.size + t.spacing
                i += 1
            }
            mainPos += line + lineSpacing
        }
    }
}

@available(macOS 11.0, *)
public struct LazyVGrid<Content>: View where Content: View {
    var columns: [GridItem]
    var alignment: HorizontalAlignment
    var spacing: CGFloat?
    var pinnedViews: PinnedScrollableViews
    var content: Content

    public init(columns: [GridItem], alignment: HorizontalAlignment = .center, spacing: CGFloat? = nil,
                pinnedViews: PinnedScrollableViews = .init(), @ViewBuilder content: () -> Content) {
        self.columns = columns
        self.alignment = alignment
        self.spacing = spacing
        self.pinnedViews = pinnedViews
        self.content = content()
    }

    public var body: some View {
        _MOSLazyGridLayout(items: columns, vertical: true, alignment: Alignment(horizontal: alignment, vertical: .top),
                           spacing: spacing) { content }
    }
}

@available(macOS 11.0, *)
public struct LazyHGrid<Content>: View where Content: View {
    var rows: [GridItem]
    var alignment: VerticalAlignment
    var spacing: CGFloat?
    var pinnedViews: PinnedScrollableViews
    var content: Content

    public init(rows: [GridItem], alignment: VerticalAlignment = .center, spacing: CGFloat? = nil,
                pinnedViews: PinnedScrollableViews = .init(), @ViewBuilder content: () -> Content) {
        self.rows = rows
        self.alignment = alignment
        self.spacing = spacing
        self.pinnedViews = pinnedViews
        self.content = content()
    }

    public var body: some View {
        _MOSLazyGridLayout(items: rows, vertical: false, alignment: Alignment(horizontal: .leading, vertical: alignment),
                           spacing: spacing) { content }
    }
}

// MARK: - Grid / GridRow (macOS 13)

/// Marca as células de um mesmo GridRow (o objeto muda de uma linha para outra).
final class _MOSGridRowMark {}

struct _MOSGridRowKey: LayoutValueKey {
    static var defaultValue: _MOSGridRowMark? { nil }
}

struct _MOSGridColumnsKey: LayoutValueKey {
    static var defaultValue: Int { 1 }
}

struct _MOSGridRowRoot: _VariadicView_MultiViewRoot {
    let mark: _MOSGridRowMark
    func body(children: _VariadicView.Children) -> some View {
        ForEach(children) { child in child.layoutValue(key: _MOSGridRowKey.self, value: mark) }
    }
}

/// Uma linha de um Grid.
@available(macOS 13.0, *)
public struct GridRow<Content>: View where Content: View {
    var alignment: VerticalAlignment?
    var content: Content

    public init(alignment: VerticalAlignment? = nil, @ViewBuilder content: () -> Content) {
        self.alignment = alignment
        self.content = content()
    }

    public var body: some View {
        _VariadicView.Tree(_MOSGridRowRoot(mark: _MOSGridRowMark())) { content }
    }
}

@available(macOS 13.0, *)
extension View {
    /// Quantas colunas esta célula ocupa num Grid.
    public func gridCellColumns(_ count: Int) -> some View {
        layoutValue(key: _MOSGridColumnsKey.self, value: max(1, count))
    }
}

/// O layout do Grid: colunas com a largura da maior célula, linhas com a altura da maior célula.
struct _MOSGridLayout: Layout {
    var alignment: Alignment
    var hSpacing: CGFloat
    var vSpacing: CGFloat

    /// linhas de índices de subview; uma view fora de GridRow ocupa a linha inteira
    private func rows(_ subviews: Subviews) -> [[Int]] {
        var out: [[Int]] = []
        var last: _MOSGridRowMark?
        for i in subviews.indices {
            let m = subviews[i][_MOSGridRowKey.self]
            if let m, m === last, !out.isEmpty { out[out.count - 1].append(i) } else { out.append([i]) }
            last = m
        }
        return out
    }

    private func measure(_ subviews: Subviews) -> (rows: [[Int]], cols: [CGFloat], heights: [CGFloat]) {
        let rs = rows(subviews)
        var cols: [CGFloat] = []
        var heights: [CGFloat] = []
        for r in rs {
            var c = 0
            var h: CGFloat = 0
            let full = r.count == 1 && subviews[r[0]][_MOSGridRowKey.self] == nil
            for i in r {
                let s = subviews[i].sizeThatFits(.unspecified)
                let span = full ? 0 : subviews[i][_MOSGridColumnsKey.self]
                if span == 1 {
                    while cols.count <= c { cols.append(0) }
                    cols[c] = max(cols[c], s.width)
                }
                c += max(span, 1)
                h = max(h, s.height)
            }
            heights.append(h)
        }
        return (rs, cols, heights)
    }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let (_, cols, heights) = measure(subviews)
        let w = cols.reduce(0, +) + CGFloat(max(cols.count - 1, 0)) * hSpacing
        let h = heights.reduce(0, +) + CGFloat(max(heights.count - 1, 0)) * vSpacing
        return CGSize(width: w, height: h)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        let (rs, cols, heights) = measure(subviews)
        let width = cols.reduce(0, +) + CGFloat(max(cols.count - 1, 0)) * hSpacing
        var y = bounds.minY
        for (ri, r) in rs.enumerated() {
            let full = r.count == 1 && subviews[r[0]][_MOSGridRowKey.self] == nil
            var c = 0
            var x = bounds.minX
            for i in r {
                let span = full ? cols.count : max(subviews[i][_MOSGridColumnsKey.self], 1)
                let end = min(c + span, cols.count)
                let cw = c < end ? cols[c..<end].reduce(0, +) + CGFloat(end - c - 1) * hSpacing : (full ? width : 0)
                let ch = heights[ri]
                let s = subviews[i].sizeThatFits(ProposedViewSize(width: cw, height: ch))
                let ox: CGFloat, oy: CGFloat
                switch alignment.horizontal { case .leading: ox = 0; case .trailing: ox = cw - s.width; default: ox = (cw - s.width) / 2 }
                switch alignment.vertical { case .top: oy = 0; case .bottom: oy = ch - s.height; default: oy = (ch - s.height) / 2 }
                subviews[i].place(at: CGPoint(x: x + ox, y: y + oy), anchor: .topLeading, proposal: ProposedViewSize(width: cw, height: ch))
                x += cw + hSpacing
                c = end
            }
            y += heights[ri] + vSpacing
        }
    }
}

/// Um contêiner que arruma as views numa grade bidimensional.
@available(macOS 13.0, *)
public struct Grid<Content>: View where Content: View {
    var alignment: Alignment
    var horizontalSpacing: CGFloat?
    var verticalSpacing: CGFloat?
    var content: Content

    public init(alignment: Alignment = .center, horizontalSpacing: CGFloat? = nil, verticalSpacing: CGFloat? = nil,
                @ViewBuilder content: () -> Content) {
        self.alignment = alignment
        self.horizontalSpacing = horizontalSpacing
        self.verticalSpacing = verticalSpacing
        self.content = content()
    }

    public var body: some View {
        _MOSGridLayout(alignment: alignment, hSpacing: horizontalSpacing ?? _mosGridDefaultSpacing,
                       vSpacing: verticalSpacing ?? _mosGridDefaultSpacing) { content }
    }
}
