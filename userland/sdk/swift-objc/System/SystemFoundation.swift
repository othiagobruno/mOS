// mOS — módulo _System_Foundation: a ponte entre FilePath (System) e URL (Foundation), como o overlay cruzado da
// Apple. Compilado por tools/build-swift-foundation-objc.sh depois do Foundation e do System.

@_exported import System
import Foundation

extension FilePath {
    /// O caminho de uma URL de arquivo (file:), já sem o percent-encoding; nil para outros esquemas.
    public init?(_ url: URL) {
        guard url.isFileURL else { return nil }
        let p = url.path
        guard !p.isEmpty else { return nil }
        self.init(p)
    }
}

extension URL {
    /// URL de arquivo para o caminho; nil se o caminho estiver vazio.
    public init?(_ path: FilePath) {
        let s = path.string
        guard !s.isEmpty else { return nil }
        self.init(fileURLWithPath: s)
    }
}
