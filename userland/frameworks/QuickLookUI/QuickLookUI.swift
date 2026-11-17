// mOS — overlay Swift do QuickLookUI (libswiftQuickLookUI.dylib): os inicializadores Swift do QLPreviewReply para as
// extensões de pré-visualização (QLPreviewProvider). O QLPreviewReply em si é o do libSystem (quicklook_fw.c).

import Foundation
import UniformTypeIdentifiers

extension QLPreviewReply {
    /// Uma resposta com dados de um tipo, gerados quando o Quick Look pedir.
    public convenience init(dataOfContentType contentType: UTType, contentSize: CGSize,
                            createDataUsing dataCreationBlock: @escaping (QLPreviewReply) throws -> Data) {
        self.init(__dataOfContentType: contentType.identifier as NSString, contentSize: contentSize, dataCreationBlock: { reply, errorOut in
            do {
                return try dataCreationBlock(reply) as NSData
            } catch {
                errorOut?.pointee = error as NSError
                return nil
            }
        })
    }

    /// A codificação do texto da resposta (para HTML e texto puro).
    public var stringEncoding: String.Encoding {
        get { String.Encoding(rawValue: __stringEncoding) }
        set { __stringEncoding = newValue.rawValue }
    }
}
