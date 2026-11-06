// mOS SDK — Foundation para o Swift completo (sem interoperabilidade ObjC): o núcleo que as bibliotecas
// do OpenSwiftUI usam. NSObject aqui é uma classe Swift nativa (como no swift-corelibs-foundation do
// Linux): igualdade por isEqual(_:), hash por `hash`, descrição por `description`.

@_exported import FoundationEssentials   // swift-foundation: Date, Calendar, Locale, URL, Data, UUID, AttributedString, JSON...
@_exported import CoreFoundation
@_exported import Dispatch          // como o Foundation da Apple (DispatchQueue sem import próprio)
@_exported import CoreGraphics      // CGFloat (módulo Swift CoreGraphics do mOS)
@_exported import MOSLibm

/// Zona de memória do ObjC: no mOS não existe, é só um ponteiro opaco (sempre nil na prática).
public typealias NSZone = OpaquePointer

public protocol NSCopying {
    func copy(with zone: NSZone?) -> Any
}
public protocol NSMutableCopying {
    func mutableCopy(with zone: NSZone?) -> Any
}

public protocol NSObjectProtocol: AnyObject {
    func isEqual(_ object: Any?) -> Bool
    var hash: Int { get }
    var description: String { get }
}

open class NSObject: NSObjectProtocol, Hashable, CustomStringConvertible, CustomDebugStringConvertible {
    public init() {}

    open func isEqual(_ object: Any?) -> Bool {
        guard let other = object as? NSObject else { return false }
        return self === other
    }
    open var hash: Int { ObjectIdentifier(self).hashValue }
    open var description: String { "<\(String(describing: type(of: self))): \(ObjectIdentifier(self))>" }
    open var debugDescription: String { description }
    open func copy() -> Any { self }

    public static func == (lhs: NSObject, rhs: NSObject) -> Bool { lhs.isEqual(rhs) }
    public func hash(into hasher: inout Hasher) { hasher.combine(hash) }
}
