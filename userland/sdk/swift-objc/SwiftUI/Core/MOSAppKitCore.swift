// mOS — partes do SwiftUI.framework (variante com interop ObjC) que têm de morar no módulo OpenSwiftUICore para o nome
// mangled sair como o da Apple: membros de tipos do Core declarados em outro módulo ganham contexto de extensão
// ($s7SwiftUI5ColorVAAE02nsC0... em vez de $s7SwiftUI5ColorV02nsC0...).
#if !OPENSWIFTUI_SWIFTUI_RENDERER
public import AppKit
public import os

// MARK: - Color <-> NSColor

@available(macOS 10.15, *)
extension Color {
    /// Uma cor a partir de um NSColor (componentes sRGB).
    public init(nsColor: NSColor) {
        let c = nsColor.usingColorSpaceName("NSCalibratedRGBColorSpace") ?? nsColor
        self.init(.sRGB, red: Double(c.redComponent), green: Double(c.greenComponent), blue: Double(c.blueComponent),
                  opacity: Double(c.alphaComponent))
    }

    @available(*, deprecated, renamed: "init(nsColor:)")
    public init(_ color: NSColor) { self.init(nsColor: color) }
}

// MARK: - Log.runtimeIssuesLog (o os_log das "runtime issues" que o código inlinável dos apps usa)

private let _mosRuntimeIssuesLog: OS_os_log = os_log_create("com.apple.runtime-issues", "SwiftUI")

extension Log {
    public static var runtimeIssuesLog: OS_os_log { _mosRuntimeIssuesLog }
}
#endif
