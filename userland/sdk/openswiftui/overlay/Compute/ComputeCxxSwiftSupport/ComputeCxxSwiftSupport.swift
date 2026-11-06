// mOS — substitui Sources/ComputeCxxSwiftSupport/ComputeCxxSwiftSupport.swift do Compute.
// IAGDispatchEquatable fica igual. IAGSetTypeForKey só é chamado por Graph.mm (Objective-C++, fora do build
// do mOS) com NSMutableDictionary/NSString: aqui fica um stub com o mesmo símbolo, sem tocar no dicionário.

@_silgen_name("IAGDispatchEquatable")
public func Equatable_isEqual_indirect<T: Equatable>(
    _ lhs: UnsafePointer<T>,
    _ rhs: UnsafePointer<T>
) -> Bool {
    return lhs.pointee == rhs.pointee
}

@_silgen_name("IAGSetTypeForKey")
public func setTypeForKey(
    _ dict: UnsafeMutableRawPointer,
    _ key: UnsafeRawPointer,
    _ type: Any.Type
) {}
