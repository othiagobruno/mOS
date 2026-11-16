// mOS — conversões entre Color e NSColor do SwiftUI.framework, com as assinaturas da Apple. O upstream as tem em
// Integration/Graphic/AppKit/AppKitColorConversions.swift, mas lá dependem de NSAppearance e de NSColor dinâmica, que
// o AppKit do mOS ainda não oferece; aqui a cor é resolvida no ambiente padrão (aparência clara) e vai como sRGB.
public import AppKit
public import Foundation
public import OpenSwiftUICore

@available(macOS 11.0, *)
extension NSColor {
    /// Cria a NSColor sRGB da cor do SwiftUI.
    public convenience init(_ color: Color) {
        let r = color.resolve(in: EnvironmentValues())
        self.init(srgbRed: CGFloat(r.red), green: CGFloat(r.green), blue: CGFloat(r.blue), alpha: CGFloat(r.opacity))
    }
}

@available(macOS 12.0, *)
extension Color {
    /// A cor do SwiftUI com os componentes sRGB da NSColor.
    public init(nsColor: NSColor) {
        let c = nsColor.usingColorSpaceName("NSCalibratedRGBColorSpace") ?? nsColor
        self.init(.sRGB, red: Double(c.redComponent), green: Double(c.greenComponent), blue: Double(c.blueComponent),
                  opacity: Double(c.alphaComponent))
    }

    @_disfavoredOverload
    public init(_ color: NSColor) {
        self.init(nsColor: color)
    }
}
