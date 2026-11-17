// mOS — APIs do SwiftUI.framework pedidas pelos apps do corpus mais perto de rodar (docs/lacunas/ricardo.md):
// modificadores de acessibilidade, List com seleção e estilos de lista, Color de asset, VSplitView/HSplitView,
// scrollDisabled/scrollContentBackground, drawingGroup, horizontalRadioGroupLayout, dialogSeverity, janelas
// (WindowGroup com id, windowResizability) e searchable. Assinaturas e forma de declaração (extensão restrita ou
// `where` no membro) iguais às da Apple, que é o que decide o nome mangled.
public import AppKit
public import Foundation
public import OpenSwiftUICore

// MARK: - acessibilidade (API do macOS 10.15 e 11)

@available(macOS 10.15, *)
extension View {
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityLabel(_:)")
    public func accessibility(label: Text) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibilityLabel(label)
    }
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityHint(_:)")
    public func accessibility(hint: Text) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibilityHint(hint)
    }
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityValue(_:)")
    public func accessibility(value: Text) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibilityValue(value)
    }
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityIdentifier(_:)")
    public func accessibility(identifier: String) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibilityIdentifier(identifier)
    }
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityHidden(_:)")
    public func accessibility(hidden: Bool) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibility(AccessibilityProperties.VisibilityKey.self, AccessibilityVisibilityStorage(adding: .hidden), isEnabled: hidden)
    }

    @available(macOS 11.0, *)
    public func accessibilityLabel(_ label: Text) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibility(AccessibilityProperties.LabelKey.self, AccessibilityLabelStorage(texts: [label]))
    }
    @available(macOS 11.0, *)
    public func accessibilityLabel(_ labelKey: LocalizedStringKey) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibilityLabel(Text(labelKey))
    }
    @available(macOS 11.0, *)
    @_disfavoredOverload
    public func accessibilityLabel<S>(_ label: S) -> ModifiedContent<Self, AccessibilityAttachmentModifier> where S: StringProtocol {
        accessibilityLabel(Text(label))
    }
    @available(macOS 11.0, *)
    public func accessibilityHint(_ hint: Text) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibility(AccessibilityProperties.HintsKey.self, [hint])
    }
    @available(macOS 11.0, *)
    public func accessibilityHint(_ hintKey: LocalizedStringKey) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibilityHint(Text(hintKey))
    }
    @available(macOS 11.0, *)
    @_disfavoredOverload
    public func accessibilityHint<S>(_ hint: S) -> ModifiedContent<Self, AccessibilityAttachmentModifier> where S: StringProtocol {
        accessibilityHint(Text(hint))
    }
    @available(macOS 11.0, *)
    public func accessibilityValue(_ valueDescription: Text) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibility(AccessibilityProperties.ValueKey.self, AccessibilityValueStorage(description: valueDescription))
    }
    @available(macOS 11.0, *)
    public func accessibilityValue(_ valueKey: LocalizedStringKey) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibilityValue(Text(valueKey))
    }
    @available(macOS 11.0, *)
    @_disfavoredOverload
    public func accessibilityValue<S>(_ value: S) -> ModifiedContent<Self, AccessibilityAttachmentModifier> where S: StringProtocol {
        accessibilityValue(Text(value))
    }
    @available(macOS 11.0, *)
    public func accessibilityIdentifier(_ identifier: String) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibility(AccessibilityProperties.IdentifierKey.self, AccessibilityIdentifierStorage(identifier))
    }
    @available(macOS 11.0, *)
    public func accessibilitySortPriority(_ sortPriority: Double) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibility(AccessibilityProperties.SortPriorityKey.self, sortPriority)
    }
    @available(macOS 11.0, *)
    public func accessibilityHeading(_ level: AccessibilityHeadingLevel) -> ModifiedContent<Self, AccessibilityAttachmentModifier> {
        accessibility(AccessibilityProperties.TextHeadingLevelKey.self, level)
    }
}

@available(macOS 10.15, *)
extension ModifiedContent where Modifier == AccessibilityAttachmentModifier {
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityLabel(_:)")
    public func accessibility(label: Text) -> ModifiedContent<Content, Modifier> { accessibilityLabel(label) }
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityHint(_:)")
    public func accessibility(hint: Text) -> ModifiedContent<Content, Modifier> { accessibilityHint(hint) }
    @available(macOS, deprecated: 100000.0, renamed: "accessibilityValue(_:)")
    public func accessibility(value: Text) -> ModifiedContent<Content, Modifier> { accessibilityValue(value) }

    @available(macOS 11.0, *)
    public func accessibilityLabel(_ label: Text) -> ModifiedContent<Content, Modifier> {
        update(AccessibilityProperties.LabelKey.self, replacing: AccessibilityLabelStorage(texts: [label]))
    }
    @available(macOS 11.0, *)
    public func accessibilityLabel(_ labelKey: LocalizedStringKey) -> ModifiedContent<Content, Modifier> { accessibilityLabel(Text(labelKey)) }
    @available(macOS 11.0, *)
    @_disfavoredOverload
    public func accessibilityLabel<S>(_ label: S) -> ModifiedContent<Content, Modifier> where S: StringProtocol { accessibilityLabel(Text(label)) }
    @available(macOS 11.0, *)
    public func accessibilityHint(_ hint: Text) -> ModifiedContent<Content, Modifier> {
        update(AccessibilityProperties.HintsKey.self, replacing: [hint])
    }
    @available(macOS 11.0, *)
    public func accessibilityHint(_ hintKey: LocalizedStringKey) -> ModifiedContent<Content, Modifier> { accessibilityHint(Text(hintKey)) }
    @available(macOS 11.0, *)
    @_disfavoredOverload
    public func accessibilityHint<S>(_ hint: S) -> ModifiedContent<Content, Modifier> where S: StringProtocol { accessibilityHint(Text(hint)) }
    @available(macOS 11.0, *)
    public func accessibilityValue(_ valueDescription: Text) -> ModifiedContent<Content, Modifier> {
        update(AccessibilityProperties.ValueKey.self, replacing: AccessibilityValueStorage(description: valueDescription))
    }
    @available(macOS 11.0, *)
    public func accessibilityValue(_ valueKey: LocalizedStringKey) -> ModifiedContent<Content, Modifier> { accessibilityValue(Text(valueKey)) }
    @available(macOS 11.0, *)
    @_disfavoredOverload
    public func accessibilityValue<S>(_ value: S) -> ModifiedContent<Content, Modifier> where S: StringProtocol { accessibilityValue(Text(value)) }
    @available(macOS 11.0, *)
    public func accessibilityIdentifier(_ identifier: String) -> ModifiedContent<Content, Modifier> {
        update(AccessibilityProperties.IdentifierKey.self, replacing: AccessibilityIdentifierStorage(identifier))
    }
    @available(macOS 11.0, *)
    public func accessibilityHeading(_ level: AccessibilityHeadingLevel) -> ModifiedContent<Content, Modifier> {
        update(AccessibilityProperties.TextHeadingLevelKey.self, replacing: level)
    }
}

// MARK: - Color de asset

@available(macOS 10.15, *)
extension Color {
    /// A cor de nome dado no catálogo do bundle (NSColor(named:bundle:) do AppKit do mOS); sem a cor, transparente.
    public init(_ name: String, bundle: Bundle? = nil) {
        if let c = NSColor(named: name, bundle: bundle) {
            self.init(nsColor: c)
        } else {
            self = .clear
        }
    }
}

// MARK: - split views

/// Views empilhadas na vertical com divisórias entre elas.
@available(macOS 10.15, *)
public struct VSplitView<Content>: View where Content: View {
    var content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public var body: some View {
        _VariadicView.Tree(_MOSSplitRoot(vertical: true)) { content }
    }
}

/// Views lado a lado com divisórias entre elas.
@available(macOS 10.15, *)
public struct HSplitView<Content>: View where Content: View {
    var content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public var body: some View {
        _VariadicView.Tree(_MOSSplitRoot(vertical: false)) { content }
    }
}

struct _MOSSplitRoot: _VariadicView_MultiViewRoot {
    var vertical: Bool
    func body(children: _VariadicView.Children) -> some View {
        let divider = Rectangle().fill(Color.gray.opacity(0.35))
        if vertical {
            VStack(spacing: 0) {
                ForEach(Array(children.enumerated()), id: \.offset) { i, child in
                    if i > 0 { divider.frame(height: 1) }
                    child.frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
        } else {
            HStack(spacing: 0) {
                ForEach(Array(children.enumerated()), id: \.offset) { i, child in
                    if i > 0 { divider.frame(width: 1) }
                    child.frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
        }
    }
}

// MARK: - rolagem e desenho

private struct MOSScrollDisabledKey: EnvironmentKey { static var defaultValue: Bool { false } }
private struct MOSScrollBackgroundKey: EnvironmentKey { static var defaultValue: Bool { true } }

extension EnvironmentValues {
    /// Se a rolagem das ScrollView desta hierarquia está desligada (o ScrollView do mOS lê esta chave).
    public var isScrollEnabled: Bool {
        get { !self[MOSScrollDisabledKey.self] }
        set { self[MOSScrollDisabledKey.self] = !newValue }
    }
    var _mosScrollBackgroundVisible: Bool {
        get { self[MOSScrollBackgroundKey.self] }
        set { self[MOSScrollBackgroundKey.self] = newValue }
    }
}

@available(macOS 13.0, *)
extension View {
    /// Desliga a rolagem das ScrollView (e List) dentro desta view.
    public func scrollDisabled(_ disabled: Bool) -> some View {
        transformEnvironment(\.isScrollEnabled) { if disabled { $0 = false } }
    }

    /// Mostra ou esconde o fundo padrão de List/Form rolável (no mOS o fundo padrão é transparente).
    public func scrollContentBackground(_ visibility: Visibility) -> some View {
        environment(\._mosScrollBackgroundVisible, visibility != .hidden)
    }
}

@available(macOS 10.15, *)
extension View {
    /// Na Apple, compõe a view numa camada fora da tela antes de mostrar. O renderizador do mOS desenha a lista de
    /// exibição inteira num só passo, então o resultado na tela é o mesmo sem a camada intermediária.
    public func drawingGroup(opaque: Bool = false, colorMode: ColorRenderingMode = .nonLinear) -> some View {
        compositingGroup()
    }

    /// Pickers de grupo de rádio na horizontal.
    public func horizontalRadioGroupLayout() -> some View {
        environment(\._mosRadioHorizontal, true)
    }
}

private struct MOSRadioHorizontalKey: EnvironmentKey { static var defaultValue: Bool { false } }

extension EnvironmentValues {
    var _mosRadioHorizontal: Bool {
        get { self[MOSRadioHorizontalKey.self] }
        set { self[MOSRadioHorizontalKey.self] = newValue }
    }
}

// MARK: - diálogos e janelas

/// A gravidade de um alerta ou diálogo de confirmação.
@available(macOS 14.0, *)
public struct DialogSeverity: Equatable, Sendable {
    let raw: Int
    public static let automatic = DialogSeverity(raw: 0)
    public static let critical = DialogSeverity(raw: 1)
    public static let standard = DialogSeverity(raw: 2)
}

private struct MOSDialogSeverityKey: EnvironmentKey { static var defaultValue: Int { 0 } }

extension EnvironmentValues {
    var _mosDialogSeverity: Int {
        get { self[MOSDialogSeverityKey.self] }
        set { self[MOSDialogSeverityKey.self] = newValue }
    }
}

@available(macOS 14.0, *)
extension View {
    /// Gravidade dos alertas desta hierarquia (o cartão de alerta crítico do mOS ganha o ícone de aviso).
    public func dialogSeverity(_ severity: DialogSeverity) -> some View {
        environment(\._mosDialogSeverity, severity.raw)
    }
}

/// Como a janela de uma cena pode ser redimensionada.
@available(macOS 13.0, *)
public struct WindowResizability: Sendable {
    let raw: Int
    public static var automatic: WindowResizability { .init(raw: 0) }
    public static var contentSize: WindowResizability { .init(raw: 1) }
    public static var contentMinSize: WindowResizability { .init(raw: 2) }
}

@available(macOS 13.0, *)
extension Scene {
    /// Limites de redimensionamento da janela (a janela do mOS usa o tamanho do conteúdo como mínimo).
    public func windowResizability(_ resizability: WindowResizability) -> some Scene {
        self
    }
}

@available(macOS 11.0, *)
extension WindowGroup {
    public init(id: String, @ViewBuilder content: () -> Content) {
        self.init(content: content)
        self.id = id
    }
    public init(_ title: Text, id: String, @ViewBuilder content: () -> Content) {
        self.init(content: content)
        self.id = id
        self.title = title
    }
    public init(_ titleKey: LocalizedStringKey, id: String, @ViewBuilder content: () -> Content) {
        self.init(Text(titleKey), id: id, content: content)
    }
    @_disfavoredOverload
    public init<S>(_ title: S, id: String, @ViewBuilder content: () -> Content) where S: StringProtocol {
        self.init(Text(title), id: id, content: content)
    }
    public init(_ title: Text, @ViewBuilder content: () -> Content) {
        self.init(content: content)
        self.title = title
    }
}

// MARK: - busca

/// Onde fica o campo de busca.
@available(macOS 12.0, *)
public struct SearchFieldPlacement: Sendable {
    let raw: Int
    public static let automatic = SearchFieldPlacement(raw: 0)
    public static let toolbar = SearchFieldPlacement(raw: 1)
    public static let sidebar = SearchFieldPlacement(raw: 2)
    @available(macOS 14.0, *)
    public static let toolbarPrincipal = SearchFieldPlacement(raw: 3)
}

/// O campo de busca do mOS: um TextField com a lupa, acima do conteúdo.
struct _MOSSearchable: ViewModifier {
    var text: Binding<String>
    var prompt: Text?
    func body(content: Content) -> some View {
        VStack(spacing: 0) {
            HStack(spacing: 4) {
                Text("⌕").foregroundColor(.secondary)
                TextField(text: text, prompt: prompt) { prompt ?? Text("Buscar") }
            }
            .padding(.horizontal, 8).padding(.vertical, 4)
            .background(RoundedRectangle(cornerRadius: 6).fill(Color.gray.opacity(0.12)))
            .padding(8)
            content.frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }
}

@available(macOS 12.0, *)
extension View {
    public func searchable(text: Binding<String>, placement: SearchFieldPlacement = .automatic, prompt: Text? = nil) -> some View {
        modifier(_MOSSearchable(text: text, prompt: prompt))
    }
    public func searchable(text: Binding<String>, placement: SearchFieldPlacement = .automatic, prompt: LocalizedStringKey) -> some View {
        modifier(_MOSSearchable(text: text, prompt: Text(prompt)))
    }
    @_disfavoredOverload
    public func searchable<S>(text: Binding<String>, placement: SearchFieldPlacement = .automatic, prompt: S) -> some View
        where S: StringProtocol {
        modifier(_MOSSearchable(text: text, prompt: Text(prompt)))
    }
    @available(macOS 14.0, *)
    public func searchable(text: Binding<String>, isPresented: Binding<Bool>, placement: SearchFieldPlacement = .automatic,
                           prompt: LocalizedStringKey) -> some View {
        modifier(_MOSSearchable(text: text, prompt: Text(prompt)))
    }
    @available(macOS 14.0, *)
    @_disfavoredOverload
    public func searchable<S>(text: Binding<String>, isPresented: Binding<Bool>, placement: SearchFieldPlacement = .automatic,
                              prompt: S) -> some View where S: StringProtocol {
        modifier(_MOSSearchable(text: text, prompt: Text(prompt)))
    }
}

// MARK: - TextField com Formatter

@available(macOS 10.15, *)
extension TextField where Label == Text {
    /// Campo que mostra o valor pelo formatter e grava de volta quando o texto vira um valor válido.
    public init<V>(_ titleKey: LocalizedStringKey, value: Binding<V>, formatter: Formatter,
                   onEditingChanged: @escaping (Bool) -> Void = { _ in }, onCommit: @escaping () -> Void = {}) {
        self.init(titleKey, text: _mosFormatted(value, formatter), onEditingChanged: onEditingChanged, onCommit: onCommit)
    }

    @_disfavoredOverload
    public init<S, V>(_ title: S, value: Binding<V>, formatter: Formatter,
                      onEditingChanged: @escaping (Bool) -> Void = { _ in }, onCommit: @escaping () -> Void = {}) where S: StringProtocol {
        self.init(title, text: _mosFormatted(value, formatter), onEditingChanged: onEditingChanged, onCommit: onCommit)
    }
}

@available(macOS 12.0, *)
extension TextField {
    public init<V>(value: Binding<V>, formatter: Formatter, prompt: Text? = nil, @ViewBuilder label: () -> Label) {
        self.init(text: _mosFormatted(value, formatter), prompt: prompt, label: label)
    }
}

/// O texto de um valor pelo formatter (NSFormatter); escrever um texto que o formatter aceita atualiza o valor.
func _mosFormatted<V>(_ value: Binding<V>, _ formatter: Formatter) -> Binding<String> {
    Binding(get: {
        formatter.string(for: value.wrappedValue as AnyObject).map { $0 as String } ?? ""
    }, set: { s in
        var obj: AnyObject?
        if formatter.getObjectValue(&obj, for: s, errorDescription: nil), let v = obj as? V {
            value.wrappedValue = v
        }
    })
}
