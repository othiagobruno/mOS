// mOS — ExtensionFoundation.framework (Swift, macOS 13): os protocolos de uma extensão de app.
// Uma extensão declara `@main struct X: AppExtension` com uma `configuration` que aceita as conexões XPC do app que a
// hospeda. O ponto de entrada (AppExtension.main) vem do ExtensionKit, para a configuração de cenas.
// tools/build-frameworks-objc.sh

@_exported import ExtensionFoundation

public protocol AppExtensionConfiguration {
    /// o app hospedeiro abriu uma conexão; true = aceita (a configuração já deu resume nela ou vai dar)
    func accept(connection: NSXPCConnection) -> Bool
}

public protocol AppExtension {
    associatedtype Configuration: AppExtensionConfiguration
    var configuration: Configuration { get }
    init()
}

/// O ouvinte de serviço XPC da extensão: cada conexão nova vai para a configuração. O hospedeiro (pluginkit/
/// ExtensionKit da Apple) não existe no mOS; quem abre conexões com o serviço da extensão é o launchd/XPC do mOS.
public final class _MOSAppExtensionListener: NSObject, NSXPCListenerDelegate {
    private let accept: (NSXPCConnection) -> Bool
    public init(_ accept: @escaping (NSXPCConnection) -> Bool) { self.accept = accept }
    public func listener(_ listener: NSXPCListener, shouldAcceptNewConnection c: NSXPCConnection) -> Bool { accept(c) }

    /// liga o ouvinte de serviço e não volta (o -resume do serviceListener entra no laço principal)
    public static func run(_ accept: @escaping (NSXPCConnection) -> Bool) -> Never {
        let d = _MOSAppExtensionListener(accept)
        let l = NSXPCListener.service()
        l.delegate = d
        l.resume()
        withExtendedLifetime(d) {}
        fatalError("ExtensionFoundation[mOS]: o ouvinte de serviço XPC voltou do resume")
    }
}

extension AppExtension {
    /// ponto de entrada genérico: a configuração recebe as conexões do serviço XPC da extensão
    public static func main() throws {
        let ext = Self()
        let cfg = ext.configuration
        _MOSAppExtensionListener.run { cfg.accept(connection: $0) }
    }
}
