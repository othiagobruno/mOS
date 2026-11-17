// mOS — ListStyle e os estilos de lista do macOS (automatic, plain, sidebar, inset, bordered), com as assinaturas da
// Apple. O estilo vai pelo ambiente e a List do mOS (MOSContainers.swift) o lê: margem, fundo de barra lateral e
// linhas alternadas.
public import Foundation
public import OpenSwiftUICore

/// Aparência e interação de uma List.
@available(macOS 10.15, *)
public protocol ListStyle {
    /// mOS: como a List desenha com este estilo
    var _mosInfo: _MOSListStyleInfo { get }
}

public struct _MOSListStyleInfo: Sendable {
    var inset = false
    var sidebar = false
    var alternates = false
}

@available(macOS 10.15, *)
public struct DefaultListStyle: ListStyle {
    public init() {}
    public var _mosInfo: _MOSListStyleInfo { .init() }
}
@available(macOS 10.15, *)
public struct PlainListStyle: ListStyle {
    public init() {}
    public var _mosInfo: _MOSListStyleInfo { .init() }
}
@available(macOS 10.15, *)
public struct SidebarListStyle: ListStyle {
    public init() {}
    public var _mosInfo: _MOSListStyleInfo { .init(inset: true, sidebar: true) }
}
@available(macOS 11.0, *)
public struct InsetListStyle: ListStyle {
    var alternates: Bool
    public init() { alternates = false }
    @available(macOS 12.0, *)
    public init(alternatesRowBackgrounds: Bool) { alternates = alternatesRowBackgrounds }
    public var _mosInfo: _MOSListStyleInfo { .init(inset: true, alternates: alternates) }
}
@available(macOS 12.0, *)
public struct BorderedListStyle: ListStyle {
    var alternates: Bool
    public init() { alternates = false }
    public init(alternatesRowBackgrounds: Bool) { alternates = alternatesRowBackgrounds }
    public var _mosInfo: _MOSListStyleInfo { .init(inset: true, alternates: alternates) }
}

@available(macOS 10.15, *)
extension ListStyle where Self == DefaultListStyle { public static var automatic: DefaultListStyle { .init() } }
@available(macOS 10.15, *)
extension ListStyle where Self == PlainListStyle { public static var plain: PlainListStyle { .init() } }
@available(macOS 10.15, *)
extension ListStyle where Self == SidebarListStyle { public static var sidebar: SidebarListStyle { .init() } }
@available(macOS 11.0, *)
extension ListStyle where Self == InsetListStyle {
    public static var inset: InsetListStyle { .init() }
    @available(macOS 12.0, *)
    public static func inset(alternatesRowBackgrounds: Bool) -> InsetListStyle { .init(alternatesRowBackgrounds: alternatesRowBackgrounds) }
}
@available(macOS 12.0, *)
extension ListStyle where Self == BorderedListStyle {
    public static var bordered: BorderedListStyle { .init() }
    public static func bordered(alternatesRowBackgrounds: Bool) -> BorderedListStyle { .init(alternatesRowBackgrounds: alternatesRowBackgrounds) }
}

private struct MOSListStyleKey: EnvironmentKey { static var defaultValue: _MOSListStyleInfo { .init() } }

extension EnvironmentValues {
    var _mosListStyle: _MOSListStyleInfo {
        get { self[MOSListStyleKey.self] }
        set { self[MOSListStyleKey.self] = newValue }
    }
}

@available(macOS 10.15, *)
extension View {
    /// Define o estilo das List dentro desta view.
    public func listStyle<S>(_ style: S) -> some View where S: ListStyle {
        environment(\._mosListStyle, style._mosInfo)
    }
}
