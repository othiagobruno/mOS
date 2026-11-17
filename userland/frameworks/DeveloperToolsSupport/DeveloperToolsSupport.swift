// mOS — DeveloperToolsSupport: os recursos de catálogo que o Xcode gera para os apps (GeneratedAssetSymbols:
// ImageResource.minhaImagem, ColorResource.minhaCor). AppKit (NSImage/NSColor(resource:)) e SwiftUI
// (Image/Color(_:)) leem o nome e o bundle daqui.

import Foundation

/// Uma imagem de um catálogo de recursos, pelo nome e pelo bundle.
public struct ImageResource: Hashable, @unchecked Sendable {
    public let name: String
    public let bundle: Bundle
    public init(name: String, bundle: Bundle) {
        self.name = name
        self.bundle = bundle
    }
}

/// Uma cor de um catálogo de recursos, pelo nome e pelo bundle.
public struct ColorResource: Hashable, @unchecked Sendable {
    public let name: String
    public let bundle: Bundle
    public init(name: String, bundle: Bundle) {
        self.name = name
        self.bundle = bundle
    }
}
