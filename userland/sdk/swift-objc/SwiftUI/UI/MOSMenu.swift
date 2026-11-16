// mOS — Menu, MenuStyle e contextMenu do SwiftUI.framework, com as assinaturas da Apple (o upstream não os tem).
// O menu aberto é um cartão desenhado na própria árvore SwiftUI, com os itens do `content` (Buttons, Dividers...):
// tocar num item roda a ação dele e fecha o menu. O contextMenu abre com o clique secundário (o botão direito chega
// do AppKit do mOS como MouseEvent .secondary, MOSEvents.swift), no ponto do clique.
public import Foundation
public import OpenSwiftUICore

// MARK: - clique secundário

/// Toque com o botão secundário do mouse; o valor é o ponto do clique no espaço local.
struct _MOSSecondaryTapGesture: Gesture {
    var body: some Gesture<CGPoint> {
        EventListener<MouseEvent>()
            .discrete()
            .gated(by: EventListener<TappableEvent>().duration(maximum: tapDurationThreshold))
            .eventFilter(forType: MouseEvent.self) { event in event.button == .secondary }
            .map { $0.location }
    }
    typealias Value = CGPoint
}

// MARK: - cartão do menu

/// A lista de itens de um menu aberto. Qualquer toque num item fecha o menu depois da ação dele.
struct _MOSMenuCard<Items: View>: View {
    var items: Items
    var close: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 2) { items }
            .buttonStyle(.plain)
            .padding(.vertical, 5).padding(.horizontal, 8)
            .frame(minWidth: 120, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 6).fill(Color.white))
            .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.gray.opacity(0.35), lineWidth: 1))
            .simultaneousGesture(TapGesture().onEnded { close() })
    }
}

// MARK: - MenuStyle

/// A aparência e a interação de um Menu.
@available(macOS 11.0, *)
public protocol MenuStyle {
    associatedtype Body: View
    @ViewBuilder func makeBody(configuration: Configuration) -> Body
    typealias Configuration = MenuStyleConfiguration
}

/// O que um MenuStyle recebe: o rótulo e o conteúdo do Menu.
@available(macOS 11.0, *)
public struct MenuStyleConfiguration {
    public struct Label: View {
        var base: AnyView
        public var body: some View { base }
    }
    public struct Content: View {
        var base: AnyView
        public var body: some View { base }
    }
    let label: Label
    let content: Content
    let primaryAction: (() -> Void)?
    let bordered: Bool
}

/// O Menu do sistema: rótulo com seta; tocar abre o cartão com os itens (ou roda a ação primária, se houver).
struct _MOSMenuControl: View {
    var configuration: MenuStyleConfiguration
    var bordered: Bool
    @State private var open = false

    var body: some View {
        Button {
            if let action = configuration.primaryAction { action() } else { open.toggle() }
        } label: {
            HStack(spacing: 4) {
                configuration.label
                Text("⌄")
            }
            .padding(.horizontal, bordered ? 8 : 0).padding(.vertical, bordered ? 2 : 0)
            .overlay(RoundedRectangle(cornerRadius: 5).stroke(Color.gray.opacity(bordered ? 0.5 : 0), lineWidth: 1))
        }
        .buttonStyle(.plain)
        .overlay(alignment: .topLeading) {
            if open {
                _MOSMenuCard(items: configuration.content) { open = false }
                    .fixedSize()
                    .offset(y: 22)
                    .zIndex(1)
            }
        }
    }
}

@available(macOS 11.0, *)
public struct DefaultMenuStyle: MenuStyle {
    public init() {}
    public func makeBody(configuration: Configuration) -> some View { _MOSMenuControl(configuration: configuration, bordered: true) }
}
@available(macOS 11.0, *)
public struct BorderlessButtonMenuStyle: MenuStyle {
    public init() {}
    public func makeBody(configuration: Configuration) -> some View { _MOSMenuControl(configuration: configuration, bordered: false) }
}
@available(macOS 12.0, *)
public struct BorderedButtonMenuStyle: MenuStyle {
    public init() {}
    public func makeBody(configuration: Configuration) -> some View { _MOSMenuControl(configuration: configuration, bordered: true) }
}
@available(macOS 14.0, *)
public struct ButtonMenuStyle: MenuStyle {
    public init() {}
    public func makeBody(configuration: Configuration) -> some View { _MOSMenuControl(configuration: configuration, bordered: true) }
}

@available(macOS 11.0, *)
extension MenuStyle where Self == DefaultMenuStyle {
    public static var automatic: DefaultMenuStyle { .init() }
}
@available(macOS 11.0, *)
extension MenuStyle where Self == BorderlessButtonMenuStyle {
    public static var borderlessButton: BorderlessButtonMenuStyle { .init() }
}
@available(macOS 14.0, *)
extension MenuStyle where Self == ButtonMenuStyle {
    public static var button: ButtonMenuStyle { .init() }
}

struct _MOSAnyMenuStyle {
    let make: (MenuStyleConfiguration) -> AnyView
    init<S: MenuStyle>(_ s: S) { make = { AnyView(s.makeBody(configuration: $0)) } }
}

private struct MOSMenuStyleKey: EnvironmentKey {
    static var defaultValue: _MOSAnyMenuStyle? { nil }
}

extension EnvironmentValues {
    var _mosMenuStyle: _MOSAnyMenuStyle? {
        get { self[MOSMenuStyleKey.self] }
        set { self[MOSMenuStyleKey.self] = newValue }
    }
}

@available(macOS 11.0, *)
extension View {
    /// Define o estilo dos Menus dentro desta view.
    public func menuStyle<S>(_ style: S) -> some View where S: MenuStyle {
        environment(\._mosMenuStyle, _MOSAnyMenuStyle(style))
    }
}

// MARK: - Menu

/// Um controle que mostra uma lista de ações ao ser tocado.
@available(macOS 11.0, *)
public struct Menu<Label, Content>: View where Label: View, Content: View {
    var label: Label
    var content: Content
    var primaryAction: (() -> Void)?
    @Environment(\._mosMenuStyle) private var style

    public init(@ViewBuilder content: () -> Content, @ViewBuilder label: () -> Label) {
        self.content = content()
        self.label = label()
    }

    @available(macOS 12.0, *)
    public init(@ViewBuilder content: () -> Content, @ViewBuilder label: () -> Label, primaryAction: @escaping () -> Void) {
        self.content = content()
        self.label = label()
        self.primaryAction = primaryAction
    }

    public var body: some View {
        let c = MenuStyleConfiguration(label: .init(base: AnyView(label)), content: .init(base: AnyView(content)),
                                       primaryAction: primaryAction, bordered: true)
        if let style { style.make(c) } else { DefaultMenuStyle().makeBody(configuration: c) }
    }
}

@available(macOS 11.0, *)
extension Menu where Label == Text {
    public init(_ titleKey: LocalizedStringKey, @ViewBuilder content: () -> Content) {
        self.init(content: content) { Text(titleKey) }
    }

    @_disfavoredOverload
    public init<S>(_ title: S, @ViewBuilder content: () -> Content) where S: StringProtocol {
        self.init(content: content) { Text(title) }
    }

    @available(macOS 12.0, *)
    public init(_ titleKey: LocalizedStringKey, @ViewBuilder content: () -> Content, primaryAction: @escaping () -> Void) {
        self.init(content: content, label: { Text(titleKey) }, primaryAction: primaryAction)
    }
}

@available(macOS 11.0, *)
extension Menu where Label == MenuStyleConfiguration.Label, Content == MenuStyleConfiguration.Content {
    /// Um Menu feito da configuração de um estilo (para estilos que embrulham o padrão).
    public init(_ configuration: MenuStyleConfiguration) {
        label = configuration.label
        content = configuration.content
        primaryAction = configuration.primaryAction
    }
}

// MARK: - contextMenu

/// Um menu de contexto (API antiga do macOS 10.15).
@available(macOS 10.15, *)
public struct ContextMenu<MenuContent> where MenuContent: View {
    var items: MenuContent
    public init(@ViewBuilder menuItems: () -> MenuContent) { items = menuItems() }
}

struct _MOSContextMenuModifier<Items: View>: ViewModifier {
    var items: Items
    @State private var at: CGPoint?

    func body(content: Content) -> some View {
        content
            .simultaneousGesture(_MOSSecondaryTapGesture().onEnded { at = $0 })
            .overlay(alignment: .topLeading) {
                if let p = at {
                    _MOSMenuCard(items: items) { at = nil }
                        .fixedSize()
                        .offset(x: p.x, y: p.y)
                        .zIndex(1)
                }
            }
    }
}

@available(macOS 10.15, *)
extension View {
    /// Mostra um menu de contexto com os itens dados no clique secundário.
    public func contextMenu<MenuItems>(@ViewBuilder menuItems: () -> MenuItems) -> some View where MenuItems: View {
        modifier(_MOSContextMenuModifier(items: menuItems()))
    }

    @available(macOS, deprecated: 100000.0)
    public func contextMenu<MenuItems>(_ contextMenu: ContextMenu<MenuItems>?) -> some View where MenuItems: View {
        modifier(_MOSContextMenuModifier(items: contextMenu?.items))
    }

    @available(macOS 13.0, *)
    public func contextMenu<M, P>(@ViewBuilder menuItems: () -> M, @ViewBuilder preview: () -> P) -> some View where M: View, P: View {
        modifier(_MOSContextMenuModifier(items: menuItems()))
    }
}

@available(macOS 13.0, *)
extension View {
    /// Menu de contexto de uma lista com seleção: o menu recebe o conjunto selecionado (no mOS, o vazio quando o
    /// clique não cai numa linha selecionada) e a ação primária roda no clique duplo.
    public func contextMenu<I, M>(forSelectionType itemType: I.Type = I.self, @ViewBuilder menu: @escaping (Set<I>) -> M,
                                  primaryAction: ((Set<I>) -> Void)? = nil) -> some View where I: Hashable, M: View {
        modifier(_MOSContextMenuModifier(items: menu([])))
            .simultaneousGesture(TapGesture(count: 2).onEnded { primaryAction?([]) })
    }
}
