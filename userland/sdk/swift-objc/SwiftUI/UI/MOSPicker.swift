// mOS — Picker do SwiftUI.framework, com as assinaturas da Apple (o upstream OpenSwiftUI não tem Picker).
// As opções são os filhos do `content`, cada um com `.tag(_:)` (ou o `id` de um ForEach) do tipo da seleção; o
// _VariadicView entrega os filhos com os traços de tag. Estilos do macOS: menu (o padrão, botão que abre a lista),
// segmentado, grupo de rádio e inline. O estilo escolhido por `.pickerStyle(_:)` vai pelo ambiente.
public import Foundation
public import OpenSwiftUICore

// MARK: - estilos

/// Um tipo que define a aparência e o comportamento de um Picker.
@available(macOS 10.15, *)
public protocol PickerStyle {
    /// mOS: qual das aparências do sistema este estilo usa
    var _mosKind: _MOSPickerKind { get }
}

public enum _MOSPickerKind: Hashable, Sendable { case menu, segmented, radioGroup, inline }

@available(macOS 10.15, *)
extension PickerStyle {
    public var _mosKind: _MOSPickerKind { .menu }
}

@available(macOS 10.15, *)
public struct DefaultPickerStyle: PickerStyle {
    public init() {}
    public var _mosKind: _MOSPickerKind { .menu }
}
@available(macOS 11.0, *)
public struct MenuPickerStyle: PickerStyle {
    public init() {}
    public var _mosKind: _MOSPickerKind { .menu }
}
@available(macOS 10.15, *)
public struct PopUpButtonPickerStyle: PickerStyle {
    public init() {}
    public var _mosKind: _MOSPickerKind { .menu }
}
@available(macOS 10.15, *)
public struct SegmentedPickerStyle: PickerStyle {
    public init() {}
    public var _mosKind: _MOSPickerKind { .segmented }
}
@available(macOS 10.15, *)
public struct RadioGroupPickerStyle: PickerStyle {
    public init() {}
    public var _mosKind: _MOSPickerKind { .radioGroup }
}
@available(macOS 11.0, *)
public struct InlinePickerStyle: PickerStyle {
    public init() {}
    public var _mosKind: _MOSPickerKind { .inline }
}

@available(macOS 10.15, *)
extension PickerStyle where Self == DefaultPickerStyle {
    public static var automatic: DefaultPickerStyle { .init() }
}
@available(macOS 11.0, *)
extension PickerStyle where Self == MenuPickerStyle {
    public static var menu: MenuPickerStyle { .init() }
}
@available(macOS 10.15, *)
extension PickerStyle where Self == SegmentedPickerStyle {
    public static var segmented: SegmentedPickerStyle { .init() }
}
@available(macOS 10.15, *)
extension PickerStyle where Self == RadioGroupPickerStyle {
    public static var radioGroup: RadioGroupPickerStyle { .init() }
}
@available(macOS 11.0, *)
extension PickerStyle where Self == InlinePickerStyle {
    public static var inline: InlinePickerStyle { .init() }
}

private struct MOSPickerKindKey: EnvironmentKey {
    static var defaultValue: _MOSPickerKind { .menu }
}

extension EnvironmentValues {
    var _mosPickerKind: _MOSPickerKind {
        get { self[MOSPickerKindKey.self] }
        set { self[MOSPickerKindKey.self] = newValue }
    }
}

@available(macOS 10.15, *)
extension View {
    /// Define o estilo dos pickers dentro desta view.
    public func pickerStyle<S>(_ style: S) -> some View where S: PickerStyle {
        environment(\._mosPickerKind, style._mosKind)
    }
}

// MARK: - Picker

/// Um controle para escolher entre valores mutuamente exclusivos.
@available(macOS 10.15, *)
public struct Picker<Label, SelectionValue, Content>: View where Label: View, SelectionValue: Hashable, Content: View {
    var selection: Binding<SelectionValue>
    var label: Label
    var content: Content

    public init(selection: Binding<SelectionValue>, @ViewBuilder content: () -> Content, @ViewBuilder label: () -> Label) {
        self.selection = selection
        self.content = content()
        self.label = label()
    }

    @available(macOS, deprecated: 100000.0, renamed: "Picker(selection:content:label:)")
    public init(selection: Binding<SelectionValue>, label: Label, @ViewBuilder content: () -> Content) {
        self.selection = selection
        self.label = label
        self.content = content()
    }

    @Environment(\._mosPickerKind) private var kind

    public var body: some View {
        _VariadicView.Tree(_MOSPickerRoot(selection: selection, label: label, kind: kind)) { content }
    }
}

@available(macOS 10.15, *)
extension Picker where Label == Text {
    public init(_ titleKey: LocalizedStringKey, selection: Binding<SelectionValue>, @ViewBuilder content: () -> Content) {
        self.init(selection: selection, content: content) { Text(titleKey) }
    }

    @_disfavoredOverload
    public init<S>(_ title: S, selection: Binding<SelectionValue>, @ViewBuilder content: () -> Content) where S: StringProtocol {
        self.init(selection: selection, content: content) { Text(title) }
    }
}

@available(macOS 15.0, *)
extension Picker {
    /// Picker com um rótulo para o valor atual (no macOS o rótulo do valor só aparece no menu fechado).
    public init<C>(selection: Binding<SelectionValue>, @ViewBuilder content: () -> Content, @ViewBuilder label: () -> Label,
                   @ViewBuilder currentValueLabel: () -> C) where C: View {
        self.init(selection: selection, content: content, label: label)
    }
}

/// Monta o controle a partir dos filhos do conteúdo (cada filho é uma opção com sua tag).
struct _MOSPickerRoot<Label: View, SelectionValue: Hashable>: _VariadicView_MultiViewRoot {
    var selection: Binding<SelectionValue>
    var label: Label
    var kind: _MOSPickerKind

    func body(children: _VariadicView.Children) -> some View {
        let options = children.compactMap { child -> (_VariadicView_Children.Element, SelectionValue)? in
            if let t = child.traits.tag(for: SelectionValue.self) { return (child, t) }
            if let id = child.id.base as? SelectionValue { return (child, id) }
            return nil
        }
        return _MOSPickerControl(selection: selection, label: label, kind: kind, options: options)
    }
}

struct _MOSPickerControl<Label: View, SelectionValue: Hashable>: View {
    var selection: Binding<SelectionValue>
    var label: Label
    var kind: _MOSPickerKind
    var options: [(_VariadicView_Children.Element, SelectionValue)]
    @State private var open = false

    var body: some View {
        switch kind {
        case .segmented:
            HStack(spacing: 8) {
                label
                HStack(spacing: 0) {
                    ForEach(options.indices, id: \.self) { i in
                        let (view, tag) = options[i]
                        Button { selection.wrappedValue = tag } label: {
                            view.padding(.horizontal, 8).padding(.vertical, 2)
                                .background(tag == selection.wrappedValue ? Color.accentColor.opacity(0.85) : Color.clear)
                        }
                        .buttonStyle(.plain)
                    }
                }
                .overlay(RoundedRectangle(cornerRadius: 5).stroke(Color.gray.opacity(0.5), lineWidth: 1))
            }
        case .radioGroup:
            HStack(alignment: .top, spacing: 8) {
                label
                VStack(alignment: .leading, spacing: 4) {
                    ForEach(options.indices, id: \.self) { i in
                        let (view, tag) = options[i]
                        Button { selection.wrappedValue = tag } label: {
                            HStack(spacing: 6) {
                                Circle().stroke(Color.gray, lineWidth: 1).frame(width: 14, height: 14)
                                    .overlay(Circle().fill(tag == selection.wrappedValue ? Color.accentColor : Color.clear)
                                        .frame(width: 8, height: 8))
                                view
                            }
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
        case .inline:
            VStack(alignment: .leading, spacing: 4) {
                label
                ForEach(options.indices, id: \.self) { i in
                    let (view, tag) = options[i]
                    Button { selection.wrappedValue = tag } label: {
                        HStack(spacing: 6) {
                            Text(tag == selection.wrappedValue ? "✓" : " ").frame(width: 14)
                            view
                        }
                    }
                    .buttonStyle(.plain)
                }
            }
        case .menu:
            HStack(spacing: 8) {
                label
                Button { open.toggle() } label: {
                    HStack(spacing: 6) {
                        if let cur = options.first(where: { $0.1 == selection.wrappedValue }) { cur.0 } else { Text(" ") }
                        Text("⌄")
                    }
                    .padding(.horizontal, 8).padding(.vertical, 2)
                    .overlay(RoundedRectangle(cornerRadius: 5).stroke(Color.gray.opacity(0.5), lineWidth: 1))
                }
                .buttonStyle(.plain)
                .popover(isPresented: $open, arrowEdge: .bottom) {
                    VStack(alignment: .leading, spacing: 2) {
                        ForEach(options.indices, id: \.self) { i in
                            let (view, tag) = options[i]
                            Button { selection.wrappedValue = tag; open = false } label: {
                                HStack(spacing: 6) {
                                    Text(tag == selection.wrappedValue ? "✓" : " ").frame(width: 14)
                                    view
                                }
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    .padding(6)
                }
            }
        }
    }
}
