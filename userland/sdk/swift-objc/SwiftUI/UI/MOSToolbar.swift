// mOS — toolbar do SwiftUI.framework: ToolbarContent, ToolbarItem, ToolbarItemGroup, ToolbarItemPlacement,
// ToolbarContentBuilder e `.toolbar`, com as assinaturas da Apple (o upstream OpenSwiftUI não tem toolbar).
// Os requisitos `_makeContent`/`_makeToolbar` existem para a ABI; no mOS os itens são listados por `_mosToolbarItems`
// (requisito novo com implementação padrão: não quebra as conformidades compiladas contra o SwiftUI da Apple) e
// desenhados numa barra no topo da view que declarou o toolbar: navegação à esquerda, principal no meio, o resto à direita.
public import Foundation
public import OpenSwiftUICore

// MARK: - tipos da ABI

/// Entradas do grafo para montar a lista de itens (ABI da Apple; sem uso no mOS).
public struct _ToolbarItemList {}
public struct _ToolbarInputs {}
public struct _ToolbarOutputs {}

// MARK: - colocação

/// Onde um item do toolbar fica.
@available(macOS 11.0, *)
public struct ToolbarItemPlacement: Hashable, Sendable {
    let raw: Int
    public static let automatic = ToolbarItemPlacement(raw: 0)
    public static let principal = ToolbarItemPlacement(raw: 1)
    public static let navigation = ToolbarItemPlacement(raw: 2)
    public static let primaryAction = ToolbarItemPlacement(raw: 3)
    public static let secondaryAction = ToolbarItemPlacement(raw: 4)
    public static let status = ToolbarItemPlacement(raw: 5)
    public static let confirmationAction = ToolbarItemPlacement(raw: 6)
    public static let cancellationAction = ToolbarItemPlacement(raw: 7)
    public static let destructiveAction = ToolbarItemPlacement(raw: 8)
    public static let keyboard = ToolbarItemPlacement(raw: 9)
}

/// Um item já resolvido: onde fica e o que desenha.
public struct _MOSToolbarItem {
    var placement: ToolbarItemPlacement
    var view: AnyView
}

// MARK: - ToolbarContent

/// Conteúdo de um toolbar.
@available(macOS 11.0, *)
public protocol ToolbarContent {
    associatedtype Body: ToolbarContent
    @ToolbarContentBuilder var body: Body { get }
    static func _makeContent(content: _GraphValue<Self>, inputs: _GraphInputs, resolved: inout _ToolbarItemList)
    static func _makeToolbar(content: _GraphValue<Self>, inputs: _ToolbarInputs) -> _ToolbarOutputs
    /// mOS: os itens deste conteúdo, na ordem
    var _mosToolbarItems: [_MOSToolbarItem] { get }
}

@available(macOS 11.0, *)
extension ToolbarContent {
    public static func _makeContent(content: _GraphValue<Self>, inputs: _GraphInputs, resolved: inout _ToolbarItemList) {}
    public static func _makeToolbar(content: _GraphValue<Self>, inputs: _ToolbarInputs) -> _ToolbarOutputs { _ToolbarOutputs() }
    public var _mosToolbarItems: [_MOSToolbarItem] { body._mosToolbarItems }
}

/// Toolbar que o usuário pode personalizar (no mOS, igual ao comum).
@available(macOS 11.0, *)
public protocol CustomizableToolbarContent: ToolbarContent where Body: CustomizableToolbarContent {}

@available(macOS 11.0, *)
extension Never: ToolbarContent, CustomizableToolbarContent {
    public var _mosToolbarItems: [_MOSToolbarItem] { [] }
}

@available(macOS 11.0, *)
extension Optional: ToolbarContent where Wrapped: ToolbarContent {
    public var body: Never { return fatalError() }
    public var _mosToolbarItems: [_MOSToolbarItem] { self?._mosToolbarItems ?? [] }
}

@available(macOS 11.0, *)
extension _ConditionalContent: ToolbarContent where TrueContent: ToolbarContent, FalseContent: ToolbarContent {
    public var body: Never { return fatalError() }
    public var _mosToolbarItems: [_MOSToolbarItem] {
        switch storage {
        case let .trueContent(c): c._mosToolbarItems
        case let .falseContent(c): c._mosToolbarItems
        }
    }
}

// MARK: - itens

/// Um item do toolbar.
@available(macOS 11.0, *)
public struct ToolbarItem<ID, Content>: ToolbarContent where Content: View {
    var id: ID
    var placement: ToolbarItemPlacement
    var content: Content
    public var body: Never { return fatalError() }
    public var _mosToolbarItems: [_MOSToolbarItem] { [_MOSToolbarItem(placement: placement, view: AnyView(content))] }
}

@available(macOS 11.0, *)
extension ToolbarItem where ID == () {
    public init(placement: ToolbarItemPlacement = .automatic, @ViewBuilder content: () -> Content) {
        id = ()
        self.placement = placement
        self.content = content()
    }
}

@available(macOS 11.0, *)
extension ToolbarItem: CustomizableToolbarContent where ID == String {
    public init(id: String, placement: ToolbarItemPlacement = .automatic, @ViewBuilder content: () -> Content) {
        self.id = id
        self.placement = placement
        self.content = content()
    }
}

/// Vários itens com a mesma colocação.
@available(macOS 11.0, *)
public struct ToolbarItemGroup<Content>: ToolbarContent where Content: View {
    var placement: ToolbarItemPlacement
    var content: Content

    public init(placement: ToolbarItemPlacement = .automatic, @ViewBuilder content: () -> Content) {
        self.placement = placement
        self.content = content()
    }

    public var body: Never { return fatalError() }
    public var _mosToolbarItems: [_MOSToolbarItem] { [_MOSToolbarItem(placement: placement, view: AnyView(content))] }
}

/// Vários conteúdos de toolbar em sequência (o que o ToolbarContentBuilder monta).
@available(macOS 11.0, *)
public struct TupleToolbarContent<T>: ToolbarContent {
    var value: T

    public init(_ value: T) { self.value = value }

    public var body: Never { return fatalError() }
    public var _mosToolbarItems: [_MOSToolbarItem] {
        Mirror(reflecting: value).children.flatMap { ($0.value as? any ToolbarContent)?._mosToolbarItems ?? [] }
    }
}

@available(macOS 11.0, *)
extension TupleToolbarContent: CustomizableToolbarContent {}

// MARK: - construtor

@available(macOS 11.0, *)
@resultBuilder
public struct ToolbarContentBuilder {
    public static func buildExpression<Content>(_ content: Content) -> Content where Content: ToolbarContent { content }
    public static func buildBlock<Content>(_ content: Content) -> some ToolbarContent where Content: ToolbarContent { content }
    public static func buildBlock<each Content>(_ content: repeat each Content) -> TupleToolbarContent<(repeat each Content)>
        where repeat each Content: ToolbarContent {
        TupleToolbarContent((repeat each content))
    }
    public static func buildIf<Content>(_ content: Content?) -> Content? where Content: ToolbarContent { content }
    public static func buildEither<T, F>(first: T) -> _ConditionalContent<T, F> where T: ToolbarContent, F: ToolbarContent {
        _ConditionalContent(_storage: .trueContent(first))
    }
    public static func buildEither<T, F>(second: F) -> _ConditionalContent<T, F> where T: ToolbarContent, F: ToolbarContent {
        _ConditionalContent(_storage: .falseContent(second))
    }
    public static func buildLimitedAvailability<Content>(_ content: Content) -> Content where Content: ToolbarContent { content }
}

// MARK: - barra

/// A barra com os itens: navegação à esquerda, principal no centro, os outros à direita.
struct _MOSToolbarBar: View {
    var items: [_MOSToolbarItem]

    var body: some View {
        let leading = items.filter { $0.placement == .navigation || $0.placement == .cancellationAction }
        let center = items.filter { $0.placement == .principal }
        let trailing = items.filter { ![.navigation, .cancellationAction, .principal].contains($0.placement) }
        ZStack {
            HStack(spacing: 8) {
                ForEach(leading.indices, id: \.self) { leading[$0].view }
                Spacer(minLength: 0)
                ForEach(trailing.indices, id: \.self) { trailing[$0].view }
            }
            HStack(spacing: 8) { ForEach(center.indices, id: \.self) { center[$0].view } }
        }
        .buttonStyle(.plain)
        .padding(.horizontal, 10)
        .frame(height: 38)
        .background(Color.gray.opacity(0.08))
    }
}

struct _MOSToolbarModifier: ViewModifier {
    var items: [_MOSToolbarItem]
    func body(content: Content) -> some View {
        VStack(spacing: 0) {
            if !items.isEmpty { _MOSToolbarBar(items: items) }
            content.frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }
}

/// Itens padrão do toolbar que podem ser removidos.
@available(macOS 14.0, *)
public struct ToolbarDefaultItemKind: Sendable {
    let raw: Int
    public static let sidebarToggle = ToolbarDefaultItemKind(raw: 0)
    public static let title = ToolbarDefaultItemKind(raw: 1)
}

@available(macOS 11.0, *)
extension View {
    /// Põe os itens no toolbar desta view.
    public func toolbar<Content>(@ToolbarContentBuilder content: () -> Content) -> some View where Content: ToolbarContent {
        modifier(_MOSToolbarModifier(items: content()._mosToolbarItems))
    }

    /// Views como itens de colocação automática.
    public func toolbar<Content>(@ViewBuilder content: () -> Content) -> some View where Content: View {
        modifier(_MOSToolbarModifier(items: [_MOSToolbarItem(placement: .automatic, view: AnyView(content()))]))
    }

    /// Toolbar personalizável com um identificador (no mOS, igual ao comum).
    public func toolbar<Content>(id: String, @ToolbarContentBuilder content: () -> Content) -> some View
        where Content: CustomizableToolbarContent {
        modifier(_MOSToolbarModifier(items: content()._mosToolbarItems))
    }

    /// Remove um item padrão (o mOS não põe itens padrão: não há o que remover).
    @available(macOS 14.0, *)
    public func toolbar(removing defaultItemKind: ToolbarDefaultItemKind?) -> some View { self }
}
