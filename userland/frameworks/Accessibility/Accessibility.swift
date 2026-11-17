// mOS — Accessibility (Swift): os avisos de acessibilidade tipados do macOS 14
// (AccessibilityNotification.Announcement("...").post(), LayoutChanged, ScreenChanged, PageScrolled).
// O aviso vai aos observadores AX pelo AppKit do libSystem (cx_ax_announce e cx_ax_post_named, em cgdisplay.c).

/// Um aviso de acessibilidade que o app pode mandar com post().
public protocol _AccessibilityNotifications {
    func _mosPost()
}

extension _AccessibilityNotifications {
    /// Manda o aviso aos observadores de acessibilidade (leitores de tela e automação).
    public func post() { _mosPost() }
}

public enum AccessibilityNotification {
    /// Pede que um texto seja anunciado.
    public struct Announcement: _AccessibilityNotifications {
        internal var _text: String
        public init(_ announcement: String) { _text = announcement }
        public func _mosPost() { _text.withCString { cx_ax_announce($0, 90) } }
    }
    /// A disposição da tela mudou (um elemento apareceu, sumiu ou se moveu).
    public struct LayoutChanged: _AccessibilityNotifications {
        public init(_ element: Any? = nil) { _ = element }
        public func _mosPost() { cx_ax_post_named("AXLayoutChanged") }
    }
    /// A tela mudou por inteiro.
    public struct ScreenChanged: _AccessibilityNotifications {
        public init(_ element: Any? = nil) { _ = element }
        public func _mosPost() { cx_ax_post_named("AXScreenChanged") }
    }
    /// O conteúdo rolou; o texto descreve a nova posição.
    public struct PageScrolled: _AccessibilityNotifications {
        internal var _text: String
        public init(_ text: String) { _text = text }
        public func _mosPost() { _text.withCString { cx_ax_announce($0, 50) }; cx_ax_post_named("AXPageScrolled") }
    }
}
