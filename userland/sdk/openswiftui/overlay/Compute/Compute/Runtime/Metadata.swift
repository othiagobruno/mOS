// mOS — versão do mOS deste arquivo do Compute (github.com/OpenSwiftUIProject/Compute, MIT).
// O C++ e os cabeçalhos do Compute seguem o caminho Darwin (TARGET_OS_MAC) no mOS, mas sem ObjC não há
// s CFString/autorelease: as descrições vão por String.cfString (+1, sem pool) e String(_: CFString).
import ComputeCxx

@_silgen_name("IAGTypeApplyFields")
func IAGTypeApplyFields(_ type: Metadata, body: (UnsafePointer<CChar>, Int, Metadata) -> Void)

public func forEachField(of type: Any.Type, do body: (UnsafePointer<Int8>, Int, Any.Type) -> Void) {
    IAGTypeApplyFields(Metadata(type)) { fieldName, fieldOffset, fieldType in
        body(fieldName, fieldOffset, fieldType.type)
    }
}

@_silgen_name("IAGTypeApplyFields2")
func IAGTypeApplyFields2(
    _ type: Metadata,
    options: Metadata.ApplyOptions,
    body: @escaping (UnsafePointer<CChar>, Int, Metadata) -> Bool
) -> Bool

extension Metadata {

    public init(_ type: Any.Type) {
        self.init(rawValue: unsafeBitCast(type, to: UnsafePointer<_Metadata>.self))
    }

    public var type: Any.Type {
        return unsafeBitCast(rawValue, to: Any.Type.self)
    }

    public func forEachField(
        options: Metadata.ApplyOptions,
        do body: (UnsafePointer<CChar>, Int, Any.Type) -> Bool
    )
        -> Bool
    {
        return withoutActuallyEscaping(body) { escapingBody in
            let apply: (UnsafePointer<CChar>, Int, Metadata) -> Bool = { fieldName, fieldOffset, fieldType in
                return escapingBody(fieldName, fieldOffset, fieldType.type)
            }
            return IAGTypeApplyFields2(self, options: options, body: apply)
        }
    }

}

extension Metadata: @retroactive CustomStringConvertible {

    public var description: String {
        #if true // mOS: API Darwin dos cabeçalhos
        return String(__IAGTypeDescription(self))
        #else
        return String(__IAGTypeCopyDescription(self))
        #endif
    }

}

extension Metadata: @retroactive Equatable {}

extension Metadata: @retroactive Hashable {}
