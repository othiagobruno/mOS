// mOS — navegação do SwiftUI.framework (NavigationStack, NavigationPath, NavigationLink, navigationDestination,
// NavigationSplitView e o NavigationView antigo), com as assinaturas da Apple; o upstream OpenSwiftUI não a tem.
// Cada contêiner publica no ambiente um contexto observável (Observation): a pilha de telas (views ou valores) e os
// destinos registrados por tipo. NavigationLink empurra na pilha mais próxima; numa NavigationSplitView, um link da
// barra lateral troca a coluna de detalhe, como no macOS.
public import Foundation
public import OpenSwiftUICore
import Observation

// MARK: - NavigationPath

/// Uma lista de valores de tipos variados que representa a pilha de uma NavigationStack.
@available(macOS 13.0, *)
public struct NavigationPath: Equatable {
    var elements: [AnyHashable]

    public init() { elements = [] }
    public init<S>(_ elements: S) where S: Sequence, S.Element: Hashable { self.elements = elements.map { AnyHashable($0) } }

    public var count: Int { elements.count }
    public var isEmpty: Bool { elements.isEmpty }
    public mutating func append<V>(_ value: V) where V: Hashable { elements.append(AnyHashable(value)) }
    public mutating func removeLast(_ k: Int = 1) { elements.removeLast(k) }

    public static func == (lhs: NavigationPath, rhs: NavigationPath) -> Bool { lhs.elements == rhs.elements }
}

// MARK: - contexto

/// Uma tela empilhada: uma view pronta (NavigationLink(destination:)) ou um valor resolvido por navigationDestination.
enum _MOSNavEntry {
    case view(AnyView)
    case value(AnyHashable)
}

/// A pilha de uma NavigationStack (ou a coluna de detalhe de uma NavigationSplitView).
final class _MOSNavContext: Observation.Observable {
    private let registrar = ObservationRegistrar()
    private var _entries: [_MOSNavEntry] = []
    /// pilha de valores ligada a um Binding externo (path:)
    var path: Binding<[AnyHashable]>?
    /// destinos por tipo de valor, registrados pelo navigationDestination durante o corpo
    private var destinations: [ObjectIdentifier: (AnyHashable) -> AnyView] = [:]

    /// registra o destino de um tipo; a primeira vez invalida quem já procurou um destino
    func register(_ type: ObjectIdentifier, _ make: @escaping (AnyHashable) -> AnyView) {
        let isNew = destinations[type] == nil
        destinations[type] = make
        // o registro acontece durante a atualização do grafo, quando uma mutação não invalida nada: o aviso vai para
        // depois dela (fim da atualização), e o topo da pilha é refeito com o destino já conhecido
        if isNew {
            Update.enqueueAction(reason: nil) { [weak self] in
                guard let self else { return }
                self.registrar.withMutation(of: self, keyPath: \.entries) {}
            }
        }
    }
    /// numa split view: os links trocam o detalhe em vez de empilhar
    let replacesDetail: Bool

    init(replacesDetail: Bool) { self.replacesDetail = replacesDetail }

    var entries: [_MOSNavEntry] {
        get {
            registrar.access(self, keyPath: \.entries)
            if let path { return path.wrappedValue.map { .value($0) } }
            return _entries
        }
        set {
            registrar.withMutation(of: self, keyPath: \.entries) {
                if let path {
                    path.wrappedValue = newValue.compactMap { if case let .value(v) = $0 { v } else { nil } }
                } else {
                    _entries = newValue
                }
            }
        }
    }

    func push(_ entry: _MOSNavEntry) {
        if replacesDetail { entries = [entry] } else { entries.append(entry) }
    }

    func pop() { if !entries.isEmpty { entries.removeLast() } }

    func view(for entry: _MOSNavEntry) -> AnyView? {
        switch entry {
        case let .view(v): return v
        case let .value(v):
            registrar.access(self, keyPath: \.entries)
            return destinations[ObjectIdentifier(type(of: v.base))]?(v)
        }
    }
}

private struct MOSNavContextKey: EnvironmentKey {
    static var defaultValue: _MOSNavContext? { nil }
}

extension EnvironmentValues {
    var _mosNavContext: _MOSNavContext? {
        get { self[MOSNavContextKey.self] }
        set { self[MOSNavContextKey.self] = newValue }
    }
}

/// Barra de voltar de uma tela empilhada.
struct _MOSNavBackBar: View {
    let context: _MOSNavContext
    var body: some View {
        HStack {
            Button { context.pop() } label: { Text("‹ Voltar") }.buttonStyle(.plain)
            Spacer()
        }
        .padding(.horizontal, 10).padding(.vertical, 6)
    }
}

/// Mostra o topo da pilha (ou a raiz) de um contexto.
struct _MOSNavStackBody<Root: View>: View {
    let context: _MOSNavContext
    let root: Root
    var body: some View {
        let entries = context.entries
        let top = entries.last.flatMap { context.view(for: $0) }
        ZStack(alignment: .topLeading) {
            // a raiz continua viva por baixo (é nela que ficam os navigationDestination), escondida sob a pilha
            root.frame(maxWidth: .infinity, maxHeight: .infinity)
                .opacity(top == nil ? 1 : 0)
                .allowsHitTesting(top == nil)
            if let top {
                VStack(spacing: 0) {
                    if !context.replacesDetail { _MOSNavBackBar(context: context) }
                    top.frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
        }
        .environment(\._mosNavContext, context)
    }
}

// MARK: - NavigationStack

/// Uma view que mostra uma raiz e permite empilhar telas sobre ela.
@available(macOS 13.0, *)
public struct NavigationStack<Data, Root>: View where Root: View {
    var root: Root
    var path: Binding<[AnyHashable]>?
    @State private var context = _MOSNavContext(replacesDetail: false)

    public var body: some View {
        let _ = { context.path = path }()
        _MOSNavStackBody(context: context, root: root)
    }
}

@available(macOS 13.0, *)
extension NavigationStack {
    // ABI da Apple: o `where` fica em cada init
    public init(@ViewBuilder root: () -> Root) where Data == NavigationPath {
        self.root = root()
    }

    public init(path: Binding<NavigationPath>, @ViewBuilder root: () -> Root) where Data == NavigationPath {
        self.root = root()
        self.path = Binding(get: { path.wrappedValue.elements }, set: { path.wrappedValue.elements = $0 })
    }
}

@available(macOS 13.0, *)
extension NavigationStack {
    public init(path: Binding<Data>, @ViewBuilder root: () -> Root)
        where Data: MutableCollection, Data: RandomAccessCollection, Data: RangeReplaceableCollection, Data.Element: Hashable {
        self.root = root()
        self.path = Binding(get: { path.wrappedValue.map { AnyHashable($0) } },
                            set: { path.wrappedValue = Data($0.compactMap { $0.base as? Data.Element }) })
    }
}

// MARK: - navigationDestination

struct _MOSNavDestinationModifier<D: Hashable, Dest: View>: ViewModifier {
    let destination: (D) -> Dest
    @Environment(\._mosNavContext) private var context
    func body(content: Content) -> some View {
        let _ = context.map { c in
            c.register(ObjectIdentifier(D.self)) { v in AnyView(destination(v.base as! D)) }
        }
        content
    }
}

struct _MOSNavPresentedModifier<Dest: View>: ViewModifier {
    let isPresented: Binding<Bool>
    let destination: Dest
    @Environment(\._mosNavContext) private var context
    func body(content: Content) -> some View {
        content.onChange(of: isPresented.wrappedValue) { _, presented in
            guard let context else { return }
            if presented { context.push(.view(AnyView(destination))) } else { context.pop() }
        }
    }
}

@available(macOS 13.0, *)
extension View {
    /// Associa um destino aos valores do tipo dado empilhados por NavigationLink(value:) ou pelo path.
    public func navigationDestination<D, C>(for data: D.Type, @ViewBuilder destination: @escaping (D) -> C) -> some View
        where D: Hashable, C: View {
        modifier(_MOSNavDestinationModifier(destination: destination))
    }

    /// Empilha o destino enquanto `isPresented` for verdadeiro.
    public func navigationDestination<V>(isPresented: Binding<Bool>, @ViewBuilder destination: () -> V) -> some View where V: View {
        modifier(_MOSNavPresentedModifier(isPresented: isPresented, destination: destination()))
    }
}

// MARK: - NavigationLink

/// Um botão que empilha uma tela na navegação mais próxima.
@available(macOS 10.15, *)
public struct NavigationLink<Label, Destination>: View where Label: View, Destination: View {
    var label: Label
    var entry: () -> _MOSNavEntry
    var isActive: Binding<Bool>?
    @Environment(\._mosNavContext) private var context

    public init(@ViewBuilder destination: () -> Destination, @ViewBuilder label: () -> Label) {
        let d = destination()
        self.label = label()
        entry = { .view(AnyView(d)) }
    }

    @available(macOS, deprecated: 100000.0)
    public init(destination: Destination, @ViewBuilder label: () -> Label) {
        self.label = label()
        entry = { .view(AnyView(destination)) }
    }

    @available(macOS, deprecated: 13.0)
    public init(destination: Destination, isActive: Binding<Bool>, @ViewBuilder label: () -> Label) {
        self.label = label()
        entry = { .view(AnyView(destination)) }
        self.isActive = isActive
    }

    @available(macOS, deprecated: 13.0)
    public init<V>(destination: Destination, tag: V, selection: Binding<V?>, @ViewBuilder label: () -> Label) where V: Hashable {
        self.label = label()
        entry = { .view(AnyView(destination)) }
        isActive = Binding(get: { selection.wrappedValue == tag }, set: { selection.wrappedValue = $0 ? tag : nil })
    }

    public var body: some View {
        Button {
            isActive?.wrappedValue = true
            context?.push(entry())
        } label: { label }
        .buttonStyle(.plain)
    }
}

@available(macOS 13.0, *)
extension NavigationLink where Destination == Never {
    /// Empilha um valor; o destino vem do navigationDestination(for:) do tipo dele.
    public init<P>(value: P?, @ViewBuilder label: () -> Label) where P: Hashable {
        self.label = label()
        entry = { value.map { .value(AnyHashable($0)) } ?? .view(AnyView(EmptyView())) }
    }
}

@available(macOS 13.0, *)
extension NavigationLink where Label == Text, Destination == Never {
    public init<P>(_ titleKey: LocalizedStringKey, value: P?) where P: Hashable {
        self.init(value: value) { Text(titleKey) }
    }
    @_disfavoredOverload
    public init<S, P>(_ title: S, value: P?) where S: StringProtocol, P: Hashable {
        self.init(value: value) { Text(title) }
    }
}

@available(macOS 10.15, *)
extension NavigationLink where Label == Text {
    public init(_ titleKey: LocalizedStringKey, @ViewBuilder destination: () -> Destination) {
        self.init(destination: destination) { Text(titleKey) }
    }
    @_disfavoredOverload
    public init<S>(_ title: S, @ViewBuilder destination: () -> Destination) where S: StringProtocol {
        self.init(destination: destination) { Text(title) }
    }
}

// MARK: - NavigationSplitView

/// Quais colunas de uma NavigationSplitView ficam visíveis.
@available(macOS 13.0, *)
public struct NavigationSplitViewVisibility: Equatable, Codable, Sendable {
    let raw: Int
    public static var detailOnly: NavigationSplitViewVisibility { .init(raw: 1) }
    public static var doubleColumn: NavigationSplitViewVisibility { .init(raw: 2) }
    public static var all: NavigationSplitViewVisibility { .init(raw: 3) }
    public static var automatic: NavigationSplitViewVisibility { .init(raw: 0) }
}

/// A aparência de uma NavigationSplitView.
@available(macOS 13.0, *)
public protocol NavigationSplitViewStyle {
    /// mOS: se a coluna de detalhe tem prioridade (as laterais ficam por cima dela)
    var _mosProminentDetail: Bool { get }
}

@available(macOS 13.0, *)
extension NavigationSplitViewStyle {
    public var _mosProminentDetail: Bool { false }
}

@available(macOS 13.0, *)
public struct AutomaticNavigationSplitViewStyle: NavigationSplitViewStyle { public init() {} }
@available(macOS 13.0, *)
public struct BalancedNavigationSplitViewStyle: NavigationSplitViewStyle { public init() {} }
@available(macOS 13.0, *)
public struct ProminentDetailNavigationSplitViewStyle: NavigationSplitViewStyle {
    public init() {}
    public var _mosProminentDetail: Bool { true }
}

@available(macOS 13.0, *)
extension NavigationSplitViewStyle where Self == AutomaticNavigationSplitViewStyle {
    public static var automatic: AutomaticNavigationSplitViewStyle { .init() }
}
@available(macOS 13.0, *)
extension NavigationSplitViewStyle where Self == BalancedNavigationSplitViewStyle {
    public static var balanced: BalancedNavigationSplitViewStyle { .init() }
}
@available(macOS 13.0, *)
extension NavigationSplitViewStyle where Self == ProminentDetailNavigationSplitViewStyle {
    public static var prominentDetail: ProminentDetailNavigationSplitViewStyle { .init() }
}

private struct MOSSplitProminentKey: EnvironmentKey {
    static var defaultValue: Bool { false }
}

extension EnvironmentValues {
    var _mosSplitProminentDetail: Bool {
        get { self[MOSSplitProminentKey.self] }
        set { self[MOSSplitProminentKey.self] = newValue }
    }
}

@available(macOS 13.0, *)
extension View {
    public func navigationSplitViewStyle<S>(_ style: S) -> some View where S: NavigationSplitViewStyle {
        environment(\._mosSplitProminentDetail, style._mosProminentDetail)
    }

    /// Largura de uma coluna da NavigationSplitView.
    public func navigationSplitViewColumnWidth(_ width: CGFloat) -> some View {
        frame(width: width)
    }

    public func navigationSplitViewColumnWidth(min: CGFloat? = nil, ideal: CGFloat, max: CGFloat? = nil) -> some View {
        frame(minWidth: min, idealWidth: ideal, maxWidth: max)
    }
}

/// Barra lateral, conteúdo opcional e detalhe, lado a lado.
@available(macOS 13.0, *)
public struct NavigationSplitView<Sidebar, Content, Detail>: View where Sidebar: View, Content: View, Detail: View {
    var sidebar: Sidebar
    var content: Content?
    var detail: Detail
    var visibility: Binding<NavigationSplitViewVisibility>?
    @State private var detailContext = _MOSNavContext(replacesDetail: true)
    @State private var contentContext = _MOSNavContext(replacesDetail: true)
    @Environment(\._mosSplitProminentDetail) private var prominentDetail

    public init(columnVisibility: Binding<NavigationSplitViewVisibility>, @ViewBuilder sidebar: () -> Sidebar,
                @ViewBuilder content: () -> Content, @ViewBuilder detail: () -> Detail) {
        self.sidebar = sidebar()
        self.content = content()
        self.detail = detail()
        visibility = columnVisibility
    }

    public init(@ViewBuilder sidebar: () -> Sidebar, @ViewBuilder content: () -> Content, @ViewBuilder detail: () -> Detail) {
        self.sidebar = sidebar()
        self.content = content()
        self.detail = detail()
    }

    public var body: some View {
        let v = visibility?.wrappedValue ?? .automatic
        let showSidebar = v != .detailOnly && !(v == .doubleColumn && content != nil)
        let showContent = content != nil && v != .detailOnly
        HStack(spacing: 0) {
            if showSidebar {
                sidebar
                    .environment(\._mosNavContext, content != nil ? contentContext : detailContext)
                    .frame(minWidth: 180, idealWidth: 220, maxWidth: 260, maxHeight: .infinity, alignment: .topLeading)
                    .background(Color.gray.opacity(0.08))
                Rectangle().fill(Color.gray.opacity(0.3)).frame(width: 1)
            }
            if showContent, let content {
                _MOSNavStackBody(context: contentContext, root: content.environment(\._mosNavContext, detailContext))
                    .frame(minWidth: 200, idealWidth: 260, maxWidth: 320, maxHeight: .infinity, alignment: .topLeading)
                Rectangle().fill(Color.gray.opacity(0.3)).frame(width: 1)
            }
            _MOSNavStackBody(context: detailContext, root: detail)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }
}

@available(macOS 13.0, *)
extension NavigationSplitView {
    public init(columnVisibility: Binding<NavigationSplitViewVisibility>, @ViewBuilder sidebar: () -> Sidebar,
                @ViewBuilder detail: () -> Detail) where Content == EmptyView {
        self.sidebar = sidebar()
        self.content = nil
        self.detail = detail()
        visibility = columnVisibility
    }

    public init(@ViewBuilder sidebar: () -> Sidebar, @ViewBuilder detail: () -> Detail) where Content == EmptyView {
        self.sidebar = sidebar()
        self.content = nil
        self.detail = detail()
    }

    /// No macOS a coluna preferida no modo compacto não se aplica (a janela mostra as colunas lado a lado).
    @available(macOS 14.0, *)
    public init(columnVisibility: Binding<NavigationSplitViewVisibility>,
                preferredCompactColumn: Binding<NavigationSplitViewColumn>,
                @ViewBuilder sidebar: () -> Sidebar, @ViewBuilder detail: () -> Detail) where Content == EmptyView {
        self.init(columnVisibility: columnVisibility, sidebar: sidebar, detail: detail)
    }
}

/// Uma coluna de NavigationSplitView.
@available(macOS 14.0, *)
public struct NavigationSplitViewColumn: Hashable, Sendable {
    let raw: Int
    public static var sidebar: NavigationSplitViewColumn { .init(raw: 0) }
    public static var content: NavigationSplitViewColumn { .init(raw: 1) }
    public static var detail: NavigationSplitViewColumn { .init(raw: 2) }
}

// MARK: - NavigationView (macOS 10.15–12)

/// Na Apple, no macOS: a primeira view é a barra lateral e a segunda o detalhe.
@available(macOS, introduced: 10.15, deprecated: 100000.0)
public struct NavigationView<Content>: View where Content: View {
    var content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public var body: some View {
        _VariadicView.Tree(_MOSNavigationViewRoot()) { content }
    }
}

struct _MOSNavigationViewRoot: _VariadicView_MultiViewRoot {
    func body(children: _VariadicView.Children) -> some View {
        if children.count >= 2 {
            NavigationSplitView { children[0] } detail: { children[1] }
        } else {
            NavigationStack { ForEach(children) { $0 } }
        }
    }
}
