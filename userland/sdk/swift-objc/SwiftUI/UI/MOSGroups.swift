// mOS — DisclosureGroup, GroupBox, ContentUnavailableView e ControlGroup do SwiftUI.framework, com as assinaturas da
// Apple (o upstream OpenSwiftUI não os tem). Aparência do macOS: triângulo de abrir/fechar, caixa arredondada com
// título, estado vazio centralizado e grupo de controles lado a lado.
public import Foundation
public import OpenSwiftUICore

// MARK: - DisclosureGroup

/// O que um DisclosureGroupStyle recebe.
@available(macOS 13.0, *)
public struct DisclosureGroupStyleConfiguration {
    public struct Label: View {
        var base: AnyView
        public var body: some View { base }
    }
    public struct Content: View {
        var base: AnyView
        public var body: some View { base }
    }
    public let label: Label
    public let content: Content
    @Binding public var isExpanded: Bool
}

/// Um grupo que mostra ou esconde o conteúdo ao tocar no rótulo.
@available(macOS 11.0, *)
public struct DisclosureGroup<Label, Content>: View where Label: View, Content: View {
    var label: Label
    var content: Content
    var external: Binding<Bool>?
    @State private var expanded = false

    public init(@ViewBuilder content: @escaping () -> Content, @ViewBuilder label: () -> Label) {
        self.content = content()
        self.label = label()
    }

    public init(isExpanded: Binding<Bool>, @ViewBuilder content: @escaping () -> Content, @ViewBuilder label: () -> Label) {
        self.content = content()
        self.label = label()
        external = isExpanded
    }

    public var body: some View {
        let open = external ?? $expanded
        VStack(alignment: .leading, spacing: 4) {
            Button { open.wrappedValue.toggle() } label: {
                HStack(spacing: 4) {
                    Text(open.wrappedValue ? "▾" : "▸").frame(width: 12)
                    label
                }
            }
            .buttonStyle(.plain)
            if open.wrappedValue {
                VStack(alignment: .leading, spacing: 4) { content }.padding(.leading, 16)
            }
        }
    }
}

@available(macOS 11.0, *)
extension DisclosureGroup where Label == Text {
    public init(_ titleKey: LocalizedStringKey, @ViewBuilder content: @escaping () -> Content) {
        self.init(content: content) { Text(titleKey) }
    }
    public init(_ titleKey: LocalizedStringKey, isExpanded: Binding<Bool>, @ViewBuilder content: @escaping () -> Content) {
        self.init(isExpanded: isExpanded, content: content) { Text(titleKey) }
    }
    @_disfavoredOverload
    public init<S>(_ label: S, @ViewBuilder content: @escaping () -> Content) where S: StringProtocol {
        self.init(content: content) { Text(label) }
    }
    @_disfavoredOverload
    public init<S>(_ label: S, isExpanded: Binding<Bool>, @ViewBuilder content: @escaping () -> Content) where S: StringProtocol {
        self.init(isExpanded: isExpanded, content: content) { Text(label) }
    }
}

// MARK: - GroupBox

/// O que um GroupBoxStyle recebe.
@available(macOS 11.0, *)
public struct GroupBoxStyleConfiguration {
    public struct Label: View {
        var base: AnyView
        public var body: some View { base }
    }
    public struct Content: View {
        var base: AnyView
        public var body: some View { base }
    }
    public let label: Label
    public let content: Content
}

/// Uma caixa com título opcional que agrupa views relacionadas.
@available(macOS 10.15, *)
public struct GroupBox<Label, Content>: View where Label: View, Content: View {
    var label: Label
    var content: Content

    @available(macOS 11.0, *)
    public init(@ViewBuilder content: () -> Content, @ViewBuilder label: () -> Label) {
        self.content = content()
        self.label = label()
    }

    @available(macOS, deprecated: 100000.0, renamed: "GroupBox(content:label:)")
    public init(label: Label, @ViewBuilder content: () -> Content) {
        self.label = label
        self.content = content()
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            label.font(.headline)
            VStack(alignment: .leading, spacing: 6) { content }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(10)
                .background(RoundedRectangle(cornerRadius: 6).fill(Color.gray.opacity(0.12)))
        }
    }
}

@available(macOS 10.15, *)
extension GroupBox where Label == EmptyView {
    public init(@ViewBuilder content: () -> Content) {
        self.label = EmptyView()
        self.content = content()
    }
}

@available(macOS 11.0, *)
extension GroupBox where Label == Text {
    public init(_ titleKey: LocalizedStringKey, @ViewBuilder content: () -> Content) {
        self.init(content: content) { Text(titleKey) }
    }
    @_disfavoredOverload
    public init<S>(_ title: S, @ViewBuilder content: () -> Content) where S: StringProtocol {
        self.init(content: content) { Text(title) }
    }
}

// MARK: - ContentUnavailableView

/// A view do estado vazio: ícone e título, descrição e ações, centralizados.
@available(macOS 14.0, *)
public struct ContentUnavailableView<Label, Description, Actions>: View where Label: View, Description: View, Actions: View {
    var label: Label
    var description: Description
    var actions: Actions

    public init(@ViewBuilder label: () -> Label, @ViewBuilder description: () -> Description = { EmptyView() },
                @ViewBuilder actions: () -> Actions = { EmptyView() }) {
        self.label = label()
        self.description = description()
        self.actions = actions()
    }

    public var body: some View {
        VStack(spacing: 8) {
            label.font(.title2)
            description.foregroundColor(.secondary)
            actions
        }
        .multilineTextAlignment(.center)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

@available(macOS 14.0, *)
extension ContentUnavailableView where Label == OpenSwiftUI.Label<Text, Image>, Description == Text?, Actions == EmptyView {
    public init(_ title: LocalizedStringKey, systemImage name: String, description: Text? = nil) {
        self.init(label: { OpenSwiftUI.Label(title, systemImage: name) }, description: { description })
    }
    @_disfavoredOverload
    public init<S>(_ title: S, systemImage name: String, description: Text? = nil) where S: StringProtocol {
        self.init(label: { OpenSwiftUI.Label(title, systemImage: name) }, description: { description })
    }
}

// MARK: - ControlGroup

/// Conteúdo de um ControlGroup com rótulo.
@available(macOS 13.0, *)
public struct LabeledControlGroupContent<Content, Label>: View where Content: View, Label: View {
    var content: Content
    var label: Label
    public var body: some View {
        HStack(spacing: 6) { label; HStack(spacing: 0) { content } }
    }
}

/// Controles relacionados, lado a lado, numa moldura comum.
@available(macOS 12.0, *)
public struct ControlGroup<Content>: View where Content: View {
    var content: Content

    public init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    public var body: some View {
        HStack(spacing: 0) { content }
            .padding(.horizontal, 4)
            .overlay(RoundedRectangle(cornerRadius: 5).stroke(Color.gray.opacity(0.5), lineWidth: 1))
    }
}

@available(macOS 13.0, *)
extension ControlGroup {
    public init<C, L>(@ViewBuilder content: () -> C, @ViewBuilder label: () -> L)
        where Content == LabeledControlGroupContent<C, L>, C: View, L: View {
        self.content = LabeledControlGroupContent(content: content(), label: label())
    }
}
