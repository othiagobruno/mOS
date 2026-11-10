// mOS — contêineres do SwiftUI que o OpenSwiftUI não declara e que os binários da Apple importam.
// As assinaturas públicas já existentes (ScrollView, TextEditor, List, TabView, tabItem, popover) ficam.
// O que muda é o corpo: rolagem com recorte, abas, popover na árvore e glassEffect com vidro chapado.
// O vidro não amostra o fundo (mos_sui_glass_fallback: fallback=1, samples_backdrop=0).
#if !OPENSWIFTUI_SWIFTUI_RENDERER
public import OpenSwiftUICore
import CoreGraphics
import Foundation
import MOSBridge

// MARK: - Rolagem

final class MOSScrollSlot {
    var frame = CGRect.zero
    var viewport = CGSize.zero
    var content = CGSize.zero
    var offset = CGPoint.zero
    var vertical = true
    var horizontal = false
    var showsIndicators = true
}

private final class MOSScrollLink {
    weak var slot: MOSScrollSlot?
    init(_ slot: MOSScrollSlot) { self.slot = slot }
}

enum MOSScrollHub {
    nonisolated(unsafe) private static var links: [MOSScrollLink] = []
    nonisolated(unsafe) private static var installed = false

    static func register(_ slot: MOSScrollSlot) {
        let id = ObjectIdentifier(slot)
        links.removeAll { $0.slot == nil || ($0.slot.map { ObjectIdentifier($0) == id } ?? false) }
        links.append(MOSScrollLink(slot))
        guard !installed else { return }
        installed = true
        MOSScrollInput.handlers.append { event in
            links.removeAll { $0.slot == nil }
            let alive = links.compactMap(\.slot)
            let hits = alive.filter { mosScrollContains($0.frame, event.location) }
            let target = hits.min { mosScrollArea($0.frame) < mosScrollArea($1.frame) }
                ?? (alive.count == 1 ? alive[0] : nil)
            guard let slot = target else { return false }
            var next = slot.offset
            if slot.vertical {
                next.y = mosScrollClamp(slot.offset.y, -event.delta.height, slot.viewport.height, slot.content.height)
            }
            if slot.horizontal {
                next.x = mosScrollClamp(slot.offset.x, -event.delta.width, slot.viewport.width, slot.content.width)
            }
            if next == slot.offset { return true }
            slot.offset = next
            _MOSHostingState.renderAll()
            return true
        }
    }
}

private func mosScrollContains(_ r: CGRect, _ p: CGPoint) -> Bool {
    r.width > 0 && r.height > 0 && p.x >= r.minX && p.x < r.maxX && p.y >= r.minY && p.y < r.maxY
}

private func mosScrollArea(_ r: CGRect) -> CGFloat { max(r.width, 0) * max(r.height, 0) }

private func mosScrollClamp(_ offset: CGFloat, _ delta: CGFloat, _ viewport: CGFloat, _ content: CGFloat) -> CGFloat {
    CGFloat(mos_sui_clamp_offset(Double(offset), Double(delta), Double(viewport), Double(content)))
}

/// Lê o tamanho ideal do conteúdo (o background recebe o tamanho da view com fixedSize).
private struct MOSScrollMeasure: View {
    var slot: MOSScrollSlot
    var viewport: CGSize
    var frame: CGRect
    var vertical: Bool
    var horizontal: Bool
    var showsIndicators: Bool

    var body: some View {
        GeometryReader { inner in
            MOSScrollNote(slot: slot, content: inner.size, viewport: viewport, frame: frame,
                          vertical: vertical, horizontal: horizontal, showsIndicators: showsIndicators)
        }
    }
}

private struct MOSScrollNote: View {
    init(slot: MOSScrollSlot, content: CGSize, viewport: CGSize, frame: CGRect, vertical: Bool, horizontal: Bool, showsIndicators: Bool) {
        slot.content = content
        slot.viewport = viewport
        slot.frame = frame
        slot.vertical = vertical
        slot.horizontal = horizontal
        slot.showsIndicators = showsIndicators
        MOSScrollHub.register(slot)
    }

    var body: some View { Color.clear }
}

/// Polegar parado no viewport (não acompanha o offset do conteúdo). A conta é a mesma do teste C.
private func mosScrollThumbRect(_ slot: MOSScrollSlot, _ viewport: CGSize, _ vertical: Bool) -> CGRect? {
    let span = vertical ? viewport.height : viewport.width
    let content = vertical ? slot.content.height : slot.content.width
    let offset = vertical ? slot.offset.y : slot.offset.x
    var x = 0.0, y = 0.0, w = 0.0, h = 0.0
    let on = mos_sui_indicator(
        Double(offset), Double(span), Double(content), Int32(slot.showsIndicators ? 1 : 0),
        0, 0, Double(viewport.width), Double(viewport.height), Int32(vertical ? 1 : 0),
        &x, &y, &w, &h)
    if on == 0 || w < 1 || h < 1 { return nil }
    return CGRect(x: x, y: y, width: w, height: h)
}

private struct MOSScrollThumbBar: View {
    var slot: MOSScrollSlot
    var viewport: CGSize
    var vertical: Bool

    var body: some View {
        let rect = mosScrollThumbRect(slot, viewport, vertical)
        if let rect {
            RoundedRectangle(cornerRadius: 2.5)
                .fill(Color.gray.opacity(0.9))
                .frame(width: rect.width, height: rect.height)
                .offset(x: rect.minX, y: rect.minY)
        }
    }
}

private struct MOSScrollThumb: View {
    var slot: MOSScrollSlot
    var viewport: CGSize

    var body: some View {
        ZStack(alignment: .topLeading) {
            if slot.vertical { MOSScrollThumbBar(slot: slot, viewport: viewport, vertical: true) }
            if slot.horizontal { MOSScrollThumbBar(slot: slot, viewport: viewport, vertical: false) }
        }
    }
}

@available(macOS 10.15, *)
public struct ScrollView<Content>: View where Content: View {
    public var content: Content
    public var axes: Axis.Set
    public var showsIndicators: Bool
    @State private var slot = MOSScrollSlot()

    public init(_ axes: Axis.Set = .vertical, showsIndicators: Bool = true, @ViewBuilder content: () -> Content) {
        self.axes = axes
        self.showsIndicators = showsIndicators
        self.content = content()
    }

    public var body: some View {
        let vertical = axes.contains(.vertical)
        let horizontal = axes.contains(.horizontal)
        GeometryReader { proxy in
            ZStack(alignment: .topLeading) {
                content
                    .fixedSize(horizontal: horizontal, vertical: vertical)
                    .background(
                        MOSScrollMeasure(slot: slot, viewport: proxy.size, frame: proxy.frame(in: .global),
                                         vertical: vertical, horizontal: horizontal, showsIndicators: showsIndicators)
                    )
                    .offset(x: horizontal ? -slot.offset.x : 0, y: vertical ? -slot.offset.y : 0)
                MOSScrollThumb(slot: slot, viewport: proxy.size)
            }
        }
        .clipped()
    }
}

// MARK: - TextEditor
// O corpo (caret, seleção, digitação) está em MOSTextEditor.swift.

// MARK: - List

@available(macOS 10.15, *)
public struct List<SelectionValue, Content>: View where SelectionValue: Hashable, Content: View {
    var content: Content

    public var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 0) { content }
        }
    }
}

@available(macOS 10.15, *)
extension List where SelectionValue == Never {
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
}

// MARK: - TabView

private struct MOSTabSelectionKey: EnvironmentKey {
    static var defaultValue: Int { -1 }
}

extension EnvironmentValues {
    var mosTabSelection: Int {
        get { self[MOSTabSelectionKey.self] }
        set { self[MOSTabSelectionKey.self] = newValue }
    }
}

final class MOSTabBuild {
    nonisolated(unsafe) static var stack: [MOSTabBuild] = []
    var next = 0
    var labels: [AnyView] = []
    func take() -> Int {
        let i = next
        next += 1
        return i
    }
}

private struct MOSTabPage<Content: View>: View {
    var index: Int
    var content: Content
    @Environment(\.mosTabSelection) private var selection: Int

    var body: some View {
        if index < 0 || selection < 0 || selection == index {
            content
        }
    }
}

private struct MOSTabBar: View {
    var labels: [AnyView]
    var index: Int
    var current: Int
    var select: (Int) -> Void

    var body: some View {
        if index < labels.count {
            HStack(spacing: 8) {
                labels[index]
                    .padding(.horizontal, 8)
                    .background(index == current ? Color.gray.opacity(0.25) : Color.white.opacity(0))
                    .contentShape(Rectangle())
                    .onTapGesture { select(index) }
                MOSTabBar(labels: labels, index: index + 1, current: current, select: select)
            }
        }
    }
}

@available(macOS 10.15, *)
public struct TabView<SelectionValue, Content>: View where SelectionValue: Hashable, Content: View {
    var content: Content
    var labels: [AnyView]
    var selection: Binding<SelectionValue>?
    @State private var local = 0

    private var current: Int {
        if let selection, let value = selection.wrappedValue as? Int { return value }
        return local
    }

    private func select(_ i: Int) {
        if var selection, let value = i as? SelectionValue {
            selection.wrappedValue = value
        } else {
            local = i
        }
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if !labels.isEmpty {
                MOSTabBar(labels: labels, index: 0, current: current, select: select)
            }
            content.environment(\.mosTabSelection, current)
        }
    }
}

@available(macOS 10.15, *)
extension TabView where SelectionValue == Int {
    public init(@ViewBuilder content: () -> Content) {
        let build = MOSTabBuild()
        MOSTabBuild.stack.append(build)
        self.content = content()
        MOSTabBuild.stack.removeLast()
        self.labels = build.labels
        self.selection = nil
    }

    public init(selection: Binding<Int>, @ViewBuilder content: () -> Content) {
        let build = MOSTabBuild()
        MOSTabBuild.stack.append(build)
        self.content = content()
        MOSTabBuild.stack.removeLast()
        self.labels = build.labels
        self.selection = selection
    }
}

@available(macOS 10.15, *)
extension View {
    public func tabItem<V>(@ViewBuilder _ label: () -> V) -> some View where V: View {
        let index: Int
        if let build = MOSTabBuild.stack.last {
            index = build.take()
            build.labels.append(AnyView(label()))
        } else {
            index = -1
        }
        return MOSTabPage(index: index, content: self)
    }
}

// MARK: - popover

@available(macOS 10.15, *)
public enum PopoverAttachmentAnchor {
    case rect(Anchor<CGRect>.Source)
    case point(UnitPoint)
}

private func mosPopoverEdge(_ edge: Edge) -> Int {
    if edge == .bottom { return 1 }
    if edge == .leading { return 2 }
    if edge == .trailing { return 3 }
    return 0
}

private struct MOSPopoverCard<Pop: View>: View {
    @Binding var isPresented: Bool
    var edge: Edge
    var pop: () -> Pop

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            pop()
            Button("Fechar") { isPresented = false }
        }
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 10).fill(Color.white))
    }
}

@available(macOS 10.15, *)
extension View {
    public func popover<Content>(
        isPresented: Binding<Bool>,
        attachmentAnchor: PopoverAttachmentAnchor = .rect(.bounds),
        arrowEdge: Edge = .top,
        @ViewBuilder content: @escaping () -> Content
    ) -> some View where Content: View {
        MOSPopoverLayer(base: self, isPresented: isPresented, anchor: attachmentAnchor, edge: arrowEdge, pop: content)
    }
}

private struct MOSPopoverLayer<Base: View, Pop: View>: View {
    var base: Base
    @Binding var isPresented: Bool
    var anchor: PopoverAttachmentAnchor
    var edge: Edge
    var pop: () -> Pop

    var body: some View {
        let native = MOSPopoverHook.presenter
        let showCard = native?.replacesCard != true
        return ZStack(alignment: .topLeading) {
            base
            if isPresented.wrappedValue {
                if native != nil {
                    Color.white.opacity(0)
                        .onAppear { presentNative() }
                        .onDisappear { MOSPopoverHook.presenter?.dismiss() }
                }
                if showCard {
                    Color.white.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture { isPresented = false }
                    MOSPopoverCard(isPresented: $isPresented, edge: edge, pop: pop)
                }
            }
        }
    }

    private func presentNative() {
        let request = MOSPopoverRequest(anchor: .zero, edge: mosPopoverEdge(edge), dismiss: { isPresented = false })
        MOSPopoverHook.presenter?.present(request)
        _ = anchor
    }
}

// MARK: - glassEffect

@available(macOS 14.0, *)
public struct Glass {
    public enum Style {
        case regular
        case clear
        case identity
    }
    public var style: Style
    public var tint: Color?
    public var interactive: Bool

    public init(_ style: Style = .regular) {
        self.style = style
        tint = nil
        interactive = true
    }

    public func tint(_ color: Color) -> Glass {
        var copy = self
        copy.tint = color
        return copy
    }

    public func interactive(_ on: Bool = true) -> Glass {
        var copy = self
        copy.interactive = on
        return copy
    }

    public static var regular: Glass { Glass(.regular) }
    public static var clear: Glass { Glass(.clear) }
    public static var identity: Glass { Glass(.identity) }
}

private func mosGlassFill(clear: Bool) -> (Color, CGFloat) {
    var r = 0.0, g = 0.0, b = 0.0, a = 0.0, radius = 0.0
    var fallback: Int32 = 0, samples = 0
    mos_sui_glass_fallback(&r, &g, &b, &a, &radius, &fallback, &samples)
    let alpha = clear ? 0.28 : a
    return (Color.white.opacity(alpha), CGFloat(radius))
}

@available(macOS 14.0, *)
private struct MOSGlass<Content: View>: View {
    var glass: Glass
    var content: Content

    var body: some View {
        if glass.style == .identity {
            content
        } else {
            let fill = mosGlassFill(clear: glass.style == .clear)
            let color = glass.tint?.opacity(0.55) ?? fill.0
            content.background(RoundedRectangle(cornerRadius: fill.1).fill(color))
        }
    }
}

@available(macOS 14.0, *)
extension View {
    /// Vidro chapado. Não lê os pixels de trás: samples_backdrop fica 0.
    public func glassEffect(_ glass: Glass = .regular) -> some View {
        MOSGlass(glass: glass, content: self)
    }

    /// A forma é aceita para o binário linkar. O desenho continua o retângulo do fallback.
    public func glassEffect<S: Shape>(_ glass: Glass = .regular, in shape: S) -> some View {
        MOSGlass(glass: glass, content: self)
    }
}

@available(macOS 14.0, *)
public struct GlassEffectContainer<Content: View>: View {
    public var spacing: CGFloat
    public var content: Content

    public init(spacing: CGFloat = 0, @ViewBuilder content: () -> Content) {
        self.spacing = spacing
        self.content = content()
    }

    /// O espaçamento fica guardado. Não há amostragem compartilhada entre os filhos.
    public var body: some View { content }
}
#endif
