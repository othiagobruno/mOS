// mOS — foco de teclado do OpenSwiftUI no mOS (entra no módulo OpenSwiftUICore).
//
// No caminho não-Darwin o FocusBridge do upstream precisa de um host que seja view de plataforma
// (NSView/UIView) e de um sistema de foco da plataforma; o host do mOS (StdoutRendererHost) não é view.
// Este gerenciador faz o papel mínimo: guarda qual alvo tem o foco (campo de texto, controle focável),
// entrega o teclado a ele, navega com Tab/Shift-Tab e solta o foco num clique fora de qualquer alvo.
// O roteador de eventos (OpenSwiftUIMOS/App/MOSEvents.swift) consulta este gerenciador antes de mandar
// a tecla ao EventBindingManager.
#if !OPENSWIFTUI_SWIFTUI_RENDERER

/// Uma tecla vinda do canvas do mOS, já decodificada.
package struct MOSKeyInput: Equatable {
    /// Código de tecla estilo Mac (36 Return, 48 Tab, 51 Delete, 53 Esc, 117 Delete à frente, 123..126 setas).
    package var keyCode: Int
    /// Texto produzido pela tecla (UTF-8 do AppKit; setas e afins vêm na faixa privada 0xF700).
    package var characters: String
    package var modifiers: EventModifiers

    package init(keyCode: Int, characters: String, modifiers: EventModifiers) {
        self.keyCode = keyCode
        self.characters = characters
        self.modifiers = modifiers
    }

    package static let returnKey = 36
    package static let enterKey = 76
    package static let tabKey = 48
    package static let deleteKey = 51
    package static let forwardDeleteKey = 117
    package static let escapeKey = 53
    package static let leftArrowKey = 123
    package static let rightArrowKey = 124

    /// Texto imprimível da tecla (sem caracteres de controle nem os da faixa privada das teclas de função);
    /// vazio se a tecla não insere texto ou se vem com Command/Control.
    package var insertableText: String {
        if modifiers.contains(.command) || modifiers.contains(.control) { return "" }
        var out = ""
        for scalar in characters.unicodeScalars {
            let v = scalar.value
            if v < 0x20 || v == 0x7F || (v >= 0xF700 && v <= 0xF8FF) { continue }
            out.unicodeScalars.append(scalar)
        }
        return out
    }
}

/// Alvo de foco registrado por um controle (ex.: TextField). Classe para manter identidade estável
/// enquanto a view existir (guarde-a num @State).
package final class MOSFocusTarget {
    /// Avisado quando o alvo ganha (true) ou perde (false) o foco.
    package var focusDidChange: (Bool) -> Void = { _ in }
    /// Recebe a tecla enquanto o alvo tem foco; devolve true se tratou.
    package var handleKey: (MOSKeyInput) -> Bool = { _ in false }
    /// Respondedor do OpenSwiftUI associado (para KeyEvent/onKeyPress via EventBindingManager), se houver.
    package weak var responder: ResponderNode?
    /// Se pode receber foco agora (ex.: controle desabilitado não pode).
    package var canFocus: Bool = true

    package init() {}
}

package final class MOSFocusManager {
    nonisolated(unsafe) package static let shared = MOSFocusManager()

    private struct WeakTarget { weak var value: MOSFocusTarget? }
    private var order: [WeakTarget] = []
    package private(set) weak var focused: MOSFocusTarget?
    private var clickClaimedFocus = false

    /// Chamado depois de qualquer mudança de foco (o host do mOS pede um redesenho).
    package var onChange: (() -> Void)?

    package init() {}

    /// Registra o alvo na ordem de navegação por Tab (idempotente).
    package func register(_ target: MOSFocusTarget) {
        order.removeAll { $0.value == nil }
        if !order.contains(where: { $0.value === target }) {
            order.append(WeakTarget(value: target))
        }
    }

    package func unregister(_ target: MOSFocusTarget) {
        order.removeAll { $0.value == nil || $0.value === target }
        if focused === target { focus(nil) }
    }

    package var focusedResponder: ResponderNode? { focused?.responder }

    package func isFocused(_ target: MOSFocusTarget) -> Bool { focused === target }

    /// Move o foco (nil solta). `byClick` marca que o clique atual foi tratado por um alvo.
    package func focus(_ target: MOSFocusTarget?, byClick: Bool = false) {
        if byClick { clickClaimedFocus = true }
        if let target, !target.canFocus { return }
        guard focused !== target else { return }
        let old = focused
        focused = target
        if let target { register(target) }
        old?.focusDidChange(false)
        target?.focusDidChange(true)
        onChange?()
    }

    // MARK: cliques

    /// Início de um clique no canvas: se nenhum alvo pedir foco até o fim do clique, o foco é solto.
    package func mouseDown() { clickClaimedFocus = false }

    package func mouseUp() {
        if !clickClaimedFocus, focused != nil { focus(nil) }
        clickClaimedFocus = false
    }

    // MARK: teclado

    /// Oferece a tecla ao alvo focado; Tab/Shift-Tab navegam entre os alvos. Devolve true se consumiu.
    package func handleKey(_ key: MOSKeyInput) -> Bool {
        if key.keyCode == MOSKeyInput.tabKey, !key.modifiers.contains(.command), !key.modifiers.contains(.control) {
            return moveFocus(backward: key.modifiers.contains(.shift))
        }
        guard let focused else { return false }
        if focused.handleKey(key) { return true }
        if key.keyCode == MOSKeyInput.escapeKey {
            focus(nil)
            return true
        }
        return false
    }

    /// Próximo (ou anterior) alvo focável na ordem de registro, com volta ao início.
    @discardableResult
    package func moveFocus(backward: Bool) -> Bool {
        order.removeAll { $0.value == nil }
        let candidates = order.compactMap(\.value).filter(\.canFocus)
        guard !candidates.isEmpty else { return false }
        let next: MOSFocusTarget
        if let focused, let i = candidates.firstIndex(where: { $0 === focused }) {
            let n = candidates.count
            next = candidates[(i + (backward ? n - 1 : 1)) % n]
        } else {
            next = backward ? candidates[candidates.count - 1] : candidates[0]
        }
        focus(next)
        return true
    }
}
#endif
