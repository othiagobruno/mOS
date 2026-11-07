// mOS — overlay Swift "CoreFoundation" da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3), com os
// nomes do SDK da Apple. _CFObject: o importador faz cada tipo CF (CFString, CFArray...) aderir a ele, e a
// extensão dá Equatable/Hashable pelo CFEqual/CFHash do libSystem do mOS.
// (O CGFloat, que no SDK da Apple fica fisicamente na libswiftCoreFoundation com o nome mangled de
// CoreGraphics, está no overlay CoreGraphics; o carregador do mOS resolve o símbolo pela busca plana.)

@_exported import CoreFoundation

public protocol _CFObject: AnyObject, Hashable {}

extension _CFObject {
    public var hashValue: Int { Int(bitPattern: CFHash(self)) }
    public func hash(into hasher: inout Hasher) { hasher.combine(hashValue) }
    public static func == (lhs: Self, rhs: Self) -> Bool { CFEqual(lhs, rhs) }
}
