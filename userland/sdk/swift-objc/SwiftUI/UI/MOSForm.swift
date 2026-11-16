// mOS — Form, FormStyle e os estilos do macOS (automatic, columns, grouped), com as assinaturas da Apple
// (o upstream OpenSwiftUI não tem Form). No macOS o padrão é `columns`: as linhas uma abaixo da outra, alinhadas à
// esquerda; `grouped` põe cada Section num cartão arredondado, rolável, como os Ajustes do Sistema.
public import Foundation
public import OpenSwiftUICore

/// A aparência e o comportamento de um Form.
@available(macOS 13.0, *)
public protocol FormStyle {
    associatedtype Body: View
    @ViewBuilder func makeBody(configuration: Configuration) -> Body
    typealias Configuration = FormStyleConfiguration
}

/// O que um FormStyle recebe: o conteúdo do Form.
@available(macOS 13.0, *)
public struct FormStyleConfiguration {
    /// O conteúdo do Form, apagado de tipo.
    public struct Content: View {
        var base: AnyView
        public var body: some View { base }
    }
    public let content: FormStyleConfiguration.Content
}

@available(macOS 13.0, *)
public struct AutomaticFormStyle: FormStyle {
    public init() {}
    public func makeBody(configuration: Configuration) -> some View { ColumnsFormStyle().makeBody(configuration: configuration) }
}

@available(macOS 13.0, *)
public struct ColumnsFormStyle: FormStyle {
    public init() {}
    public func makeBody(configuration: Configuration) -> some View {
        VStack(alignment: .leading, spacing: 8) { configuration.content }
            .padding(20)
    }
}

@available(macOS 13.0, *)
public struct GroupedFormStyle: FormStyle {
    public init() {}
    public func makeBody(configuration: Configuration) -> some View {
        ScrollView {
            _VariadicView.Tree(_MOSGroupedFormRoot()) { configuration.content }
                .padding(20)
        }
    }
}

/// Cada filho de nível de cima (normalmente uma Section) vira um cartão.
struct _MOSGroupedFormRoot: _VariadicView_MultiViewRoot {
    func body(children: _VariadicView.Children) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            ForEach(children) { child in
                child
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(10)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Color.gray.opacity(0.12)))
            }
        }
    }
}

@available(macOS 13.0, *)
extension FormStyle where Self == AutomaticFormStyle {
    public static var automatic: AutomaticFormStyle { .init() }
}
@available(macOS 13.0, *)
extension FormStyle where Self == ColumnsFormStyle {
    public static var columns: ColumnsFormStyle { .init() }
}
@available(macOS 13.0, *)
extension FormStyle where Self == GroupedFormStyle {
    public static var grouped: GroupedFormStyle { .init() }
}

/// O estilo escolhido, apagado de tipo, para passar pelo ambiente.
struct _MOSAnyFormStyle {
    let make: (FormStyleConfiguration) -> AnyView
    init<S: FormStyle>(_ s: S) { make = { AnyView(s.makeBody(configuration: $0)) } }
}

private struct MOSFormStyleKey: EnvironmentKey {
    static var defaultValue: _MOSAnyFormStyle? { nil }
}

extension EnvironmentValues {
    var _mosFormStyle: _MOSAnyFormStyle? {
        get { self[MOSFormStyleKey.self] }
        set { self[MOSFormStyleKey.self] = newValue }
    }
}

@available(macOS 13.0, *)
extension View {
    /// Define o estilo dos Forms dentro desta view.
    public func formStyle<S>(_ style: S) -> some View where S: FormStyle {
        environment(\._mosFormStyle, _MOSAnyFormStyle(style))
    }
}

/// Um contêiner para agrupar controles de entrada de dados.
@available(macOS 10.15, *)
public struct Form<Content>: View where Content: View {
    var content: Content
    @Environment(\._mosFormStyle) private var style

    public init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    public var body: some View {
        let configuration = FormStyleConfiguration(content: .init(base: AnyView(content)))
        if let style { style.make(configuration) } else { ColumnsFormStyle().makeBody(configuration: configuration) }
    }
}
