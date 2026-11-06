// mOS SDK — classes Swift que dão identidade de classe aos objetos CF de libMOSCFRuntime.a
// (userland/sdk/openswiftui/src/cf_mos.cpp). Compilado SEM library evolution: a metadata destas classes é
// estática e o C++ a usa direto (o símbolo `...CN`).
//
// Layout = CFRuntimeBase do swift-corelibs com DEPLOYMENT_RUNTIME_SWIFT: {isa, contador do Swift, _cfinfoa}.
// Sendo instâncias de classe de verdade, objetos CF podem morar em Any/AnyObject, passar por type(of:), Mirror etc.

/// Classe de todo objeto CF criado em runtime (CFString, CFNumber..., Graph/Subgraph do Compute).
/// `_cfinfoa` guarda o índice do tipo CF; os bytes além da instância são o resto da struct CF.
public final class _MOSCFObject {
    public var _cfinfoa: UInt64 = 0
    private init() {}
    deinit { _mos_cf_finalize(Unmanaged.passUnretained(self).toOpaque()) }
}

/// Classe das constantes CFSTR("...") do CoreFoundation do swift-corelibs (DEPLOYMENT_RUNTIME_SWIFT + TARGET_OS_MAC):
/// o clang grava `isa = &$s15SwiftFoundation19_NSCFConstantStringCN` e um contador imortal. Nunca é instanciada.
public final class _NSCFConstantString {
    public var _cfinfoa: UInt64 = 0
    public var _ptr: UnsafePointer<UInt8>? = nil
    public var _length: Int = 0
    private init() {}
}

/// finalize da CFRuntimeClass do objeto (cf_mos.cpp); depois o Swift libera a memória.
@_silgen_name("mos_cf_finalize") func _mos_cf_finalize(_ object: UnsafeMutableRawPointer)
