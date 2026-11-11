// mOS — ExtensionKit.framework (Swift, macOS 13): extensões de app com cenas (AppExtensionScene).
// A extensão monta uma AppExtensionSceneConfiguration com uma cena; cada PrimitiveAppExtensionScene tem um id, a
// view (SwiftUI) e o tratador das conexões XPC (onConnection). No macOS o hospedeiro abre uma conexão por cena; aqui a
// configuração entrega cada conexão do serviço XPC da extensão à cena primitiva (a de id pedido não chega: o mOS não
// tem o hospedeiro da Apple) e, se ela recusar, à configuração extra.
// tools/build-frameworks-objc.sh

@_exported import ExtensionKit
import ExtensionFoundation
import SwiftUI

public protocol AppExtensionScene {
    associatedtype Body: AppExtensionScene
    var body: Body { get }
    func _makeScene(with id: String) -> PrimitiveAppExtensionScene?
}

extension AppExtensionScene {
    public func _makeScene(with id: String) -> PrimitiveAppExtensionScene? { body._makeScene(with: id) }
}

extension Never: AppExtensionScene {
    public var body: Never { fatalError("Never não tem corpo") }
    public func _makeScene(with id: String) -> PrimitiveAppExtensionScene? { nil }
}

public struct PrimitiveAppExtensionScene: AppExtensionScene {
    public let id: String
    internal let _content: () -> Any
    internal let _onConnection: (NSXPCConnection) -> Bool

    public init<Content: View>(id: String, content: @escaping () -> Content, onConnection: @escaping (NSXPCConnection) -> Bool) {
        self.id = id
        _content = { content() }
        _onConnection = onConnection
    }

    public var body: Never { fatalError("PrimitiveAppExtensionScene não tem corpo") }

    /// a cena com esse id; id vazio (o mOS não sabe qual cena o hospedeiro pediu) = esta
    public func _makeScene(with id: String) -> PrimitiveAppExtensionScene? { id.isEmpty || id == self.id ? self : nil }
}

public struct AppExtensionSceneConfiguration: AppExtensionConfiguration {
    internal let _scene: () -> PrimitiveAppExtensionScene?
    internal let _extra: ((NSXPCConnection) -> Bool)?

    public init<E: AppExtensionScene, C: AppExtensionConfiguration>(_ scene: @autoclosure @escaping () -> E, configuration: C?) {
        _scene = { scene()._makeScene(with: "") }
        if let configuration { _extra = { configuration.accept(connection: $0) } } else { _extra = nil }
    }

    public func accept(connection: NSXPCConnection) -> Bool {
        if let s = _scene(), s._onConnection(connection) { return true }
        return _extra?(connection) ?? false
    }
}

extension AppExtension where Configuration == AppExtensionSceneConfiguration {
    /// ponto de entrada (@main) de uma extensão com cenas: serve as conexões XPC pela configuração
    public static func main() throws {
        let ext = Self()
        let cfg = ext.configuration
        _MOSAppExtensionListener.run { cfg.accept(connection: $0) }
    }
}
