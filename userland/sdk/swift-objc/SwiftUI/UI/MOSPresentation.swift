// mOS — apresentação modal do SwiftUI.framework: sheet, alert (com Alert antigo), confirmationDialog e o
// DismissAction do ambiente, com as assinaturas da Apple (o upstream OpenSwiftUI não os tem). No mOS a folha e o
// alerta são desenhados na própria árvore por cima da view, com fundo escurecido; o DismissAction da folha fecha ela.
public import Foundation
public import OpenSwiftUICore

// MARK: - DismissAction

/// A ação que fecha a apresentação atual (folha) ou, fora dela, não faz nada.
@available(macOS 12.0, *)
public struct DismissAction {
    var action: (() -> Void)?
    public func callAsFunction() { action?() }
}

/// Fecha uma janela pelo id (o mOS só tem a janela do hosting; sem cena com esse id, não faz nada).
@available(macOS 14.0, *)
public struct DismissWindowAction {
    public func callAsFunction() {}
    public func callAsFunction(id: String) {}
    public func callAsFunction<D>(id: String, value: D) where D: Decodable, D: Encodable, D: Hashable {}
    public func callAsFunction<D>(value: D) where D: Decodable, D: Encodable, D: Hashable {}
}

private struct MOSDismissKey: EnvironmentKey {
    static var defaultValue: DismissAction { DismissAction(action: nil) }
}
private struct MOSIsPresentedKey: EnvironmentKey {
    static var defaultValue: Bool { false }
}

@available(macOS 12.0, *)
extension EnvironmentValues {
    public var dismiss: DismissAction {
        get { self[MOSDismissKey.self] }
        set { self[MOSDismissKey.self] = newValue }
    }
    public var isPresented: Bool {
        get { self[MOSIsPresentedKey.self] }
        set { self[MOSIsPresentedKey.self] = newValue }
    }
    @available(macOS 14.0, *)
    public var dismissWindow: DismissWindowAction { DismissWindowAction() }
}

// MARK: - camada modal

/// Desenha `panel` centralizado sobre `base`, com o resto escurecido, enquanto `isPresented`.
struct _MOSModalLayer<Panel: View>: ViewModifier {
    var isPresented: Binding<Bool>
    var onDismiss: (() -> Void)?
    var panel: () -> Panel

    func body(content: Content) -> some View {
        content.overlay {
            if isPresented.wrappedValue {
                ZStack {
                    Color.black.opacity(0.25)
                    panel()
                        .environment(\.dismiss, DismissAction { close() })
                        .environment(\.isPresented, true)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.white))
                        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.gray.opacity(0.35), lineWidth: 1))
                }
            }
        }
    }

    func close() {
        isPresented.wrappedValue = false
        onDismiss?()
    }
}

// MARK: - sheet

@available(macOS 10.15, *)
extension View {
    /// Apresenta uma folha enquanto `isPresented` for verdadeiro.
    public func sheet<Content>(isPresented: Binding<Bool>, onDismiss: (() -> Void)? = nil,
                               @ViewBuilder content: @escaping () -> Content) -> some View where Content: View {
        modifier(_MOSModalLayer(isPresented: isPresented, onDismiss: onDismiss, panel: content))
    }

    /// Apresenta uma folha para o item enquanto ele não for nil.
    public func sheet<Item, Content>(item: Binding<Item?>, onDismiss: (() -> Void)? = nil,
                                     @ViewBuilder content: @escaping (Item) -> Content) -> some View
        where Item: Identifiable, Content: View {
        let shown = Binding(get: { item.wrappedValue != nil }, set: { if !$0 { item.wrappedValue = nil } })
        return modifier(_MOSModalLayer(isPresented: shown, onDismiss: onDismiss) { item.wrappedValue.map(content) })
    }

    /// Impede que o usuário feche a folha por fora (no mOS a folha só fecha pelo dismiss ou pelo binding).
    @available(macOS 12.0, *)
    public func interactiveDismissDisabled(_ isDisabled: Bool = true) -> some View { self }
}

// MARK: - alert

/// O cartão de alerta do macOS: título, mensagem e os botões das ações (padrão "OK" quando não há nenhuma).
struct _MOSAlertCard<Actions: View, Message: View>: View {
    var title: Text
    var actions: Actions
    var message: Message
    var close: () -> Void

    var body: some View {
        VStack(spacing: 10) {
            title.font(.headline)
            message.font(.callout)
            HStack(spacing: 8) { actions }
                .buttonStyle(.plain)
                .simultaneousGesture(TapGesture().onEnded { close() })
        }
        .multilineTextAlignment(.center)
        .padding(16)
        .frame(width: 260)
    }
}

@available(macOS 12.0, *)
extension View {
    public func alert<A>(_ titleKey: LocalizedStringKey, isPresented: Binding<Bool>, @ViewBuilder actions: () -> A) -> some View where A: View {
        alert(Text(titleKey), isPresented: isPresented, actions: actions) { EmptyView() }
    }

    @_disfavoredOverload
    public func alert<S, A>(_ title: S, isPresented: Binding<Bool>, @ViewBuilder actions: () -> A) -> some View
        where S: StringProtocol, A: View {
        alert(Text(title), isPresented: isPresented, actions: actions) { EmptyView() }
    }

    public func alert<A, M>(_ title: Text, isPresented: Binding<Bool>, @ViewBuilder actions: () -> A,
                            @ViewBuilder message: () -> M) -> some View where A: View, M: View {
        let a = actions(), m = message()
        return modifier(_MOSModalLayer(isPresented: isPresented, onDismiss: nil) {
            _MOSAlertCard(title: title, actions: a, message: m) { isPresented.wrappedValue = false }
        })
    }

    public func alert<A, M>(_ titleKey: LocalizedStringKey, isPresented: Binding<Bool>, @ViewBuilder actions: () -> A,
                            @ViewBuilder message: () -> M) -> some View where A: View, M: View {
        alert(Text(titleKey), isPresented: isPresented, actions: actions, message: message)
    }

    @_disfavoredOverload
    public func alert<S, A, M>(_ title: S, isPresented: Binding<Bool>, @ViewBuilder actions: () -> A,
                               @ViewBuilder message: () -> M) -> some View where S: StringProtocol, A: View, M: View {
        alert(Text(title), isPresented: isPresented, actions: actions, message: message)
    }

    public func alert<A, M, T>(_ title: Text, isPresented: Binding<Bool>, presenting data: T?,
                               @ViewBuilder actions: (T) -> A, @ViewBuilder message: (T) -> M) -> some View where A: View, M: View {
        let a = data.map(actions), m = data.map(message)
        return alert(title, isPresented: isPresented, actions: { a }, message: { m })
    }

    @_disfavoredOverload
    public func alert<S, A, M, T>(_ title: S, isPresented: Binding<Bool>, presenting data: T?,
                                  @ViewBuilder actions: (T) -> A, @ViewBuilder message: (T) -> M) -> some View
        where S: StringProtocol, A: View, M: View {
        alert(Text(title), isPresented: isPresented, presenting: data, actions: actions, message: message)
    }

    public func alert<E, A>(isPresented: Binding<Bool>, error: E?, @ViewBuilder actions: () -> A) -> some View
        where E: LocalizedError, A: View {
        alert(Text(error?.errorDescription ?? ""), isPresented: isPresented, actions: actions) {
            if let r = error?.recoverySuggestion { Text(r) }
        }
    }

    public func alert<E, A, M>(isPresented: Binding<Bool>, error: E?, @ViewBuilder actions: (E) -> A,
                               @ViewBuilder message: (E) -> M) -> some View where E: LocalizedError, A: View, M: View {
        alert(Text(error?.errorDescription ?? ""), isPresented: isPresented, presenting: error, actions: actions, message: message)
    }
}

// MARK: - Alert (API do macOS 10.15)

/// Um alerta descrito por título, mensagem e botões (API antiga).
@available(macOS, introduced: 10.15, deprecated: 100000.0)
public struct Alert {
    public struct Button {
        var label: Text
        var action: (() -> Void)?
        var role: Int   // 0 padrão, 1 cancelar, 2 destrutivo

        public static func `default`(_ label: Text, action: (() -> Void)? = {}) -> Alert.Button { .init(label: label, action: action, role: 0) }
        public static func cancel(_ label: Text, action: (() -> Void)? = {}) -> Alert.Button { .init(label: label, action: action, role: 1) }
        public static func cancel(_ action: (() -> Void)? = {}) -> Alert.Button { .init(label: Text("Cancelar"), action: action, role: 1) }
        public static func destructive(_ label: Text, action: (() -> Void)? = {}) -> Alert.Button { .init(label: label, action: action, role: 2) }
    }

    var title: Text
    var message: Text?
    var buttons: [Alert.Button]

    public init(title: Text, message: Text? = nil, dismissButton: Alert.Button? = nil) {
        self.title = title
        self.message = message
        buttons = [dismissButton ?? .default(Text("OK"))]
    }

    public init(title: Text, message: Text? = nil, primaryButton: Alert.Button, secondaryButton: Alert.Button) {
        self.title = title
        self.message = message
        buttons = [secondaryButton, primaryButton]
    }

    func card(close: @escaping () -> Void) -> some View {
        _MOSAlertCard(title: title, actions: ForEach(buttons.indices, id: \.self) { i in
            SwiftUIAlertButton(button: buttons[i])
        }, message: message, close: close)
    }
}

/// Um botão de Alert como Button do SwiftUI.
struct SwiftUIAlertButton: View {
    let button: Alert.Button
    var body: some View {
        Button { button.action?() } label: {
            button.label.foregroundColor(button.role == 2 ? .red : .primary)
        }
    }
}

@available(macOS, introduced: 10.15, deprecated: 100000.0)
extension View {
    public func alert(isPresented: Binding<Bool>, content: () -> Alert) -> some View {
        let a = content()
        return modifier(_MOSModalLayer(isPresented: isPresented, onDismiss: nil) { a.card { isPresented.wrappedValue = false } })
    }

    public func alert<Item>(item: Binding<Item?>, content: (Item) -> Alert) -> some View where Item: Identifiable {
        let a = item.wrappedValue.map(content)
        let shown = Binding(get: { item.wrappedValue != nil }, set: { if !$0 { item.wrappedValue = nil } })
        return modifier(_MOSModalLayer(isPresented: shown, onDismiss: nil) { a?.card { item.wrappedValue = nil } })
    }
}

// MARK: - confirmationDialog

@available(macOS 12.0, *)
extension View {
    public func confirmationDialog<A>(_ titleKey: LocalizedStringKey, isPresented: Binding<Bool>,
                                      titleVisibility: Visibility = .automatic, @ViewBuilder actions: () -> A) -> some View where A: View {
        confirmationDialog(Text(titleKey), isPresented: isPresented, titleVisibility: titleVisibility, actions: actions) { EmptyView() }
    }

    @_disfavoredOverload
    public func confirmationDialog<S, A>(_ title: S, isPresented: Binding<Bool>, titleVisibility: Visibility = .automatic,
                                         @ViewBuilder actions: () -> A) -> some View where S: StringProtocol, A: View {
        confirmationDialog(Text(title), isPresented: isPresented, titleVisibility: titleVisibility, actions: actions) { EmptyView() }
    }

    public func confirmationDialog<A, M>(_ title: Text, isPresented: Binding<Bool>, titleVisibility: Visibility = .automatic,
                                         @ViewBuilder actions: () -> A, @ViewBuilder message: () -> M) -> some View
        where A: View, M: View {
        let a = actions(), m = message()
        return modifier(_MOSModalLayer(isPresented: isPresented, onDismiss: nil) {
            _MOSAlertCard(title: titleVisibility == .hidden ? Text("") : title, actions: a, message: m) { isPresented.wrappedValue = false }
        })
    }

    public func confirmationDialog<A, M>(_ titleKey: LocalizedStringKey, isPresented: Binding<Bool>, titleVisibility: Visibility = .automatic,
                                         @ViewBuilder actions: () -> A, @ViewBuilder message: () -> M) -> some View
        where A: View, M: View {
        confirmationDialog(Text(titleKey), isPresented: isPresented, titleVisibility: titleVisibility, actions: actions, message: message)
    }

    @_disfavoredOverload
    public func confirmationDialog<S, A, M>(_ title: S, isPresented: Binding<Bool>, titleVisibility: Visibility = .automatic,
                                            @ViewBuilder actions: () -> A, @ViewBuilder message: () -> M) -> some View
        where S: StringProtocol, A: View, M: View {
        confirmationDialog(Text(title), isPresented: isPresented, titleVisibility: titleVisibility, actions: actions, message: message)
    }

    public func confirmationDialog<A, M, T>(_ title: Text, isPresented: Binding<Bool>, titleVisibility: Visibility = .automatic,
                                            presenting data: T?, @ViewBuilder actions: (T) -> A,
                                            @ViewBuilder message: (T) -> M) -> some View where A: View, M: View {
        let a = data.map(actions), m = data.map(message)
        return confirmationDialog(title, isPresented: isPresented, titleVisibility: titleVisibility, actions: { a }, message: { m })
    }

    public func confirmationDialog<A, M, T>(_ titleKey: LocalizedStringKey, isPresented: Binding<Bool>,
                                            titleVisibility: Visibility = .automatic, presenting data: T?,
                                            @ViewBuilder actions: (T) -> A, @ViewBuilder message: (T) -> M) -> some View
        where A: View, M: View {
        confirmationDialog(Text(titleKey), isPresented: isPresented, titleVisibility: titleVisibility, presenting: data,
                           actions: actions, message: message)
    }

    @_disfavoredOverload
    public func confirmationDialog<S, A, M, T>(_ title: S, isPresented: Binding<Bool>, titleVisibility: Visibility = .automatic,
                                               presenting data: T?, @ViewBuilder actions: (T) -> A,
                                               @ViewBuilder message: (T) -> M) -> some View where S: StringProtocol, A: View, M: View {
        confirmationDialog(Text(title), isPresented: isPresented, titleVisibility: titleVisibility, presenting: data,
                           actions: actions, message: message)
    }
}
