// mOS — contêineres e modificadores do SwiftUI que o OpenSwiftUI ainda não tem e que os binários da Apple importam
// (Postico: ScrollView, TextEditor, List, TabView/tabItem, popover). Versões mínimas com as assinaturas e os nomes
// mangled da Apple ($s7SwiftUI10ScrollViewV_15showsIndicators7contentACyxGAA4AxisO3SetV_SbxyXEtcfC...): o conteúdo
// aparece, sem rolagem, abas, edição de texto nem janela de popover ainda.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
public import OpenSwiftUICore
public import Foundation

// MARK: - ScrollView

@available(macOS 10.15, *)
public struct ScrollView<Content>: View where Content: View {
    public var content: Content
    public var axes: Axis.Set
    public var showsIndicators: Bool

    public init(_ axes: Axis.Set = .vertical, showsIndicators: Bool = true, @ViewBuilder content: () -> Content) {
        self.axes = axes
        self.showsIndicators = showsIndicators
        self.content = content()
    }

    public var body: some View { content }
}

// MARK: - TextEditor

@available(macOS 11.0, *)
public struct TextEditor: View {
    var text: Binding<String>

    public init(text: Binding<String>) { self.text = text }

    public var body: some View { Text(verbatim: text.wrappedValue) }
}

// MARK: - List

@available(macOS 10.15, *)
public struct List<SelectionValue, Content>: View where SelectionValue: Hashable, Content: View {
    var content: Content

    public var body: some View {
        VStack(alignment: .leading, spacing: 0) { content }
    }
}

@available(macOS 10.15, *)
extension List where SelectionValue == Never {
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
}

// MARK: - TabView

@available(macOS 10.15, *)
public struct TabView<SelectionValue, Content>: View where SelectionValue: Hashable, Content: View {
    var content: Content

    public var body: some View {
        VStack(alignment: .leading, spacing: 0) { content }
    }
}

@available(macOS 10.15, *)
extension TabView where SelectionValue == Int {
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
}

@available(macOS 10.15, *)
extension View {
    /// O rótulo da aba (o TabView do mOS ainda não desenha a barra de abas).
    public func tabItem<V>(@ViewBuilder _ label: () -> V) -> some View where V: View { self }
}

// MARK: - popover

@available(macOS 10.15, *)
public enum PopoverAttachmentAnchor {
    case rect(Anchor<CGRect>.Source)
    case point(UnitPoint)
}

@available(macOS 10.15, *)
extension View {
    /// Mostra um popover (no mOS ainda sem janela: o conteúdo não aparece).
    public func popover<Content>(
        isPresented: Binding<Bool>,
        attachmentAnchor: PopoverAttachmentAnchor = .rect(.bounds),
        arrowEdge: Edge = .top,
        @ViewBuilder content: @escaping () -> Content
    ) -> some View where Content: View {
        self
    }
}
#endif
