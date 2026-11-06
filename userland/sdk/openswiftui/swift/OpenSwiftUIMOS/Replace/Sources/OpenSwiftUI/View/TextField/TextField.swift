//
//  TextField.swift
//  OpenSwiftUI
//
//  Audited for 6.5.4
//  Status: Stub
//
//  mOS: o upstream é um stub (corpo e inicializador de formato em _openSwiftUIUnimplementedFailure). Aqui fica
//  um TextField básico, desenhado e editado pelo próprio OpenSwiftUI (sem NSTextField): clique para focar,
//  digitação ao fim do texto, Delete apaga, Return confirma (onCommit/onSubmit do formato), Esc e clique fora
//  soltam o foco, Tab passa ao próximo campo. O foco é do gerenciador do mOS (Core/MOSFocus.swift); as teclas
//  chegam pelo roteador de eventos (App/MOSEvents.swift). Ainda sem seleção, cursor no meio, copiar/colar.

public import Foundation
public import OpenSwiftUICore

// MARK: - TextField

@available(OpenSwiftUI_v1_0, *)
public struct TextField<Label>: View where Label: View {
    var text: Binding<String>
    var label: Label
    var prompt: Text?
    var onEditingChanged: (Bool) -> Void = { _ in }
    var onCommit: () -> Void = {}

    public var body: some View {
        MOSTextFieldBody(
            text: text,
            label: label,
            prompt: prompt,
            onEditingChanged: onEditingChanged,
            onCommit: onCommit
        )
    }
}

@available(*, unavailable)
extension TextField: Sendable {}

// MARK: - TextField + rótulo genérico

@available(OpenSwiftUI_v3_0, *)
extension TextField {
    /// Campo de texto com rótulo próprio e texto de instrução opcional.
    nonisolated public init(
        text: Binding<String>,
        prompt: Text? = nil,
        @ViewBuilder label: () -> Label
    ) {
        self.text = text
        self.label = label()
        self.prompt = prompt
    }
}

// MARK: - TextField + Text

@available(OpenSwiftUI_v1_0, *)
extension TextField where Label == Text {
    nonisolated public init(_ titleKey: LocalizedStringKey, text: Binding<String>) {
        self.text = text
        self.label = Text(titleKey)
        self.prompt = nil
    }

    @_disfavoredOverload
    nonisolated public init<S>(_ title: S, text: Binding<String>) where S: StringProtocol {
        self.text = text
        self.label = Text(title)
        self.prompt = nil
    }

    @available(OpenSwiftUI_v3_0, *)
    nonisolated public init(_ titleKey: LocalizedStringKey, text: Binding<String>, prompt: Text?) {
        self.text = text
        self.label = Text(titleKey)
        self.prompt = prompt
    }

    @available(OpenSwiftUI_v3_0, *)
    @_disfavoredOverload
    nonisolated public init<S>(_ title: S, text: Binding<String>, prompt: Text?) where S: StringProtocol {
        self.text = text
        self.label = Text(title)
        self.prompt = prompt
    }

    /// Forma antiga (onEditingChanged/onCommit); continua comum em código existente.
    nonisolated public init(
        _ titleKey: LocalizedStringKey,
        text: Binding<String>,
        onEditingChanged: @escaping (Bool) -> Void,
        onCommit: @escaping () -> Void = {}
    ) {
        self.text = text
        self.label = Text(titleKey)
        self.prompt = nil
        self.onEditingChanged = onEditingChanged
        self.onCommit = onCommit
    }

    @_disfavoredOverload
    nonisolated public init<S>(
        _ title: S,
        text: Binding<String>,
        onEditingChanged: @escaping (Bool) -> Void,
        onCommit: @escaping () -> Void = {}
    ) where S: StringProtocol {
        self.text = text
        self.label = Text(title)
        self.prompt = nil
        self.onEditingChanged = onEditingChanged
        self.onCommit = onCommit
    }
}

// MARK: - TextField + Format

extension TextField {
    /// Campo ligado a um valor por um estilo de formato: o texto é formatado na leitura e interpretado a
    /// cada edição (uma entrada que ainda não se interpreta não muda o valor).
    @available(OpenSwiftUI_v3_0, *)
    nonisolated public init<F>(
        value: Binding<F.FormatInput>,
        format: F,
        prompt: Text? = nil,
        @ViewBuilder label: () -> Label
    ) where F: ParseableFormatStyle, F.FormatOutput == String {
        self.text = Binding(
            get: { format.format(value.wrappedValue) },
            set: { newText in
                if let parsed = try? format.parseStrategy.parse(newText) {
                    value.wrappedValue = parsed
                }
            }
        )
        self.label = label()
        self.prompt = prompt
    }
}

@available(OpenSwiftUI_v3_0, *)
extension TextField where Label == Text {
    nonisolated public init<F>(
        _ titleKey: LocalizedStringKey,
        value: Binding<F.FormatInput>,
        format: F,
        prompt: Text? = nil
    ) where F: ParseableFormatStyle, F.FormatOutput == String {
        self.init(value: value, format: format, prompt: prompt) { Text(titleKey) }
    }
}

// MARK: - MOSTextFieldBody

/// mOS: aparência e edição do campo (visual do macOS Tahoe: caixa clara arredondada, anel de foco na cor
/// de acento, texto de instrução em cinza enquanto vazio).
private struct MOSTextFieldBody<Label>: View where Label: View {
    var text: Binding<String>
    var label: Label
    var prompt: Text?
    var onEditingChanged: (Bool) -> Void
    var onCommit: () -> Void

    /// Identidade estável do campo para o gerenciador de foco do mOS.
    @State private var target = MOSFocusTarget()
    @State private var isFocused = false

    @Environment(\.isEnabled)
    private var isEnabled: Bool

    var body: some View {
        configureTarget()
        let current = text.wrappedValue
        return ZStack(alignment: .leading) {
            RoundedRectangle(cornerRadius: 6)
                .fill(Color.white)
            RoundedRectangle(cornerRadius: 6)
                .strokeBorder(
                    isFocused ? Color.accentColor : Color.gray.opacity(0.45),
                    lineWidth: isFocused ? 2 : 1
                )
            HStack(spacing: 0) {
                if current.isEmpty {
                    if isFocused {
                        caret
                    }
                    placeholder
                        .foregroundColor(Color.gray)
                } else {
                    Text(verbatim: current)
                        .foregroundColor(isEnabled ? Color.primary : Color.gray)
                        .lineLimit(1)
                    if isFocused {
                        caret
                    }
                }
            }
            .padding(.horizontal, 6)
        }
        .frame(minHeight: 22)
        .contentShape(Rectangle())
        .onTapGesture {
            MOSFocusManager.shared.focus(target, byClick: true)
        }
        .onDisappear {
            MOSFocusManager.shared.unregister(target)
        }
    }

    private var caret: some View {
        Rectangle()
            .fill(Color.accentColor)
            .frame(width: 1.5, height: 15)
    }

    @ViewBuilder
    private var placeholder: some View {
        if let prompt {
            prompt
        } else {
            label
        }
    }

    /// Liga o alvo de foco a este campo (closures com o binding e callbacks atuais).
    private func configureTarget() {
        let target = target
        let text = text
        let focused = $isFocused
        let onEditingChanged = onEditingChanged
        let onCommit = onCommit
        target.canFocus = isEnabled
        target.focusDidChange = { isFocused in
            focused.wrappedValue = isFocused
            onEditingChanged(isFocused)
        }
        target.handleKey = { key in
            switch key.keyCode {
            case MOSKeyInput.returnKey, MOSKeyInput.enterKey:
                onCommit()
                return true
            case MOSKeyInput.deleteKey:
                var value = text.wrappedValue
                if !value.isEmpty {
                    value.removeLast()
                    text.wrappedValue = value
                }
                return true
            case MOSKeyInput.tabKey, MOSKeyInput.escapeKey:
                return false   // navegação/soltura ficam com o gerenciador
            default:
                let insert = key.insertableText
                guard !insert.isEmpty else { return false }
                text.wrappedValue += insert
                return true
            }
        }
        MOSFocusManager.shared.register(target)
    }
}
