// mOS — Image e Color de um recurso de catálogo gerado pelo Xcode (ImageResource/ColorResource do módulo
// DeveloperToolsSupport, userland/frameworks/DeveloperToolsSupport, da Beatriz). O recurso é só nome + bundle: carrega
// como Image(_:bundle:) e Color(_:bundle:).
public import DeveloperToolsSupport
public import Foundation
public import OpenSwiftUICore

@available(macOS 14.0, *)
extension Image {
    public init(_ resource: ImageResource) {
        self.init(resource.name, bundle: resource.bundle)
    }
}

@available(macOS 14.0, *)
extension Color {
    public init(_ resource: ColorResource) {
        self.init(resource.name, bundle: resource.bundle)
    }
}
