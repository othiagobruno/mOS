// mOS SDK — CoreText mínimo para o OpenSwiftUI (Swift sem ObjC, sobre o CoreFoundation em C do mOS).
//
// CTFont, CTFontDescriptor, CTGlyphInfo e CTAdaptiveImageGlyph são tipos CF de verdade, criados em ctfont.c
// (módulo clang _MOSCoreTextC) pelo CFRuntime do mOS. Aqui estão os traços, os estilos de texto do macOS, as
// métricas da fonte do sistema e a SPI privada que o OpenSwiftUI declara por @_silgen_name
// (CoreText+Private.swift); essa SPI é definida com o mesmo nome de símbolo e um nome Swift _mos_....
//
// Sem ObjC não existe bridging com `as CFDictionary`/`as CFString`/`as CFNumber`: os overlays do OpenSwiftUI
// (OpenSwiftUIMOS/Replace) usam os helpers _mosCF, _mosDict, _mosStringDict, _mosCFNumber e _mosCFString.
@_exported import Foundation
@_exported import _MOSCoreTextC

// MARK: - tipos de valor

public struct CTFontSymbolicTraits: OptionSet, Hashable, Sendable {
    public let rawValue: UInt32
    public init(rawValue: UInt32) { self.rawValue = rawValue }
    public static let traitItalic = CTFontSymbolicTraits(rawValue: 1 << 0)
    public static let traitBold = CTFontSymbolicTraits(rawValue: 1 << 1)
    public static let traitExpanded = CTFontSymbolicTraits(rawValue: 1 << 5)
    public static let traitCondensed = CTFontSymbolicTraits(rawValue: 1 << 6)
    public static let traitMonoSpace = CTFontSymbolicTraits(rawValue: 1 << 10)
    public static let traitVertical = CTFontSymbolicTraits(rawValue: 1 << 11)
    public static let traitUIOptimized = CTFontSymbolicTraits(rawValue: 1 << 12)
    public static let traitColorGlyphs = CTFontSymbolicTraits(rawValue: 1 << 13)
    public static let traitComposite = CTFontSymbolicTraits(rawValue: 1 << 14)
    public static let traitClassMask = CTFontSymbolicTraits(rawValue: 15 << 28)
    public static let italicTrait = traitItalic
    public static let boldTrait = traitBold
    public static let expandedTrait = traitExpanded
    public static let condensedTrait = traitCondensed
    public static let monoSpaceTrait = traitMonoSpace
    public static let verticalTrait = traitVertical
    public static let uiOptimizedTrait = traitUIOptimized
    public static let colorGlyphsTrait = traitColorGlyphs
    public static let compositeTrait = traitComposite
    public static let classMaskTrait = traitClassMask
}
public struct CTFontStylisticClass: OptionSet, Hashable, Sendable {
    public let rawValue: UInt32
    public init(rawValue: UInt32) { self.rawValue = rawValue }
    public static let unknownClass = CTFontStylisticClass([])
    public static let oldStyleSerifsClass = CTFontStylisticClass(rawValue: 1 << 28)
    public static let transitionalSerifsClass = CTFontStylisticClass(rawValue: 2 << 28)
    public static let modernSerifsClass = CTFontStylisticClass(rawValue: 3 << 28)
    public static let clarendonSerifsClass = CTFontStylisticClass(rawValue: 4 << 28)
    public static let slabSerifsClass = CTFontStylisticClass(rawValue: 5 << 28)
    public static let freeformSerifsClass = CTFontStylisticClass(rawValue: 7 << 28)
    public static let sansSerifClass = CTFontStylisticClass(rawValue: 8 << 28)
    public static let ornamentalsClass = CTFontStylisticClass(rawValue: 9 << 28)
    public static let scriptsClass = CTFontStylisticClass(rawValue: 10 << 28)
    public static let symbolicClass = CTFontStylisticClass(rawValue: 12 << 28)
}
public struct CTFontUIFontType: RawRepresentable, Hashable, Sendable {
    public var rawValue: UInt32
    public init(rawValue: UInt32) { self.rawValue = rawValue }
    public init(_ v: UInt32) { rawValue = v }
    public static let none = CTFontUIFontType(rawValue: 0xFFFF_FFFF)
    public static let user = CTFontUIFontType(rawValue: 0)
    public static let userFixedPitch = CTFontUIFontType(rawValue: 1)
    public static let system = CTFontUIFontType(rawValue: 2)
    public static let emphasizedSystem = CTFontUIFontType(rawValue: 3)
    public static let smallSystem = CTFontUIFontType(rawValue: 4)
    public static let smallEmphasizedSystem = CTFontUIFontType(rawValue: 5)
    public static let label = CTFontUIFontType(rawValue: 10)
    public static let menuItem = CTFontUIFontType(rawValue: 12)
    public static let controlContent = CTFontUIFontType(rawValue: 26)
}
public struct CTFontDescriptorOptions: OptionSet, Sendable {
    public let rawValue: UInt32
    public init(rawValue: UInt32) { self.rawValue = rawValue }
}
/// Peso de legibilidade (SPI; o OpenSwiftUI guarda no atributo kCTFontLegibilityWeightAttribute).
public enum CTFontLegibilityWeight: Int, Sendable {
    case regular = 0
    case bold = 1
}

// MARK: - conversões Swift <-> CF (sem bridging `as`)

/// Valor Swift que vira objeto CF (CFNumber, CFString, CFBoolean, CFArray, CFDictionary).
public protocol _MOSCFConvertible { var _mosCFValue: CFTypeRef { get } }
/// Chave de dicionário que vira CFString.
public protocol _MOSCFKey { var _mosCFKey: CFString { get } }

extension CFString: _MOSCFKey { public var _mosCFKey: CFString { self } }
extension String: _MOSCFKey, _MOSCFConvertible {
    public var _mosCFKey: CFString { _mosCFString(self) }
    public var _mosCFValue: CFTypeRef { _mosCFString(self) }
}
extension Int: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { _mosCFNumber(self) } }
extension Int32: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { _mosCFNumber(Int(self)) } }
extension UInt32: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { _mosCFNumber(Int(self)) } }
extension Double: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { _mosCFNumber(self) } }
extension Float: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { _mosCFNumber(Double(self)) } }
extension CGFloat: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { _mosCFNumber(Double(self)) } }
extension Bool: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { self ? kCFBooleanTrue : kCFBooleanFalse } }
extension CTFontLegibilityWeight: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { _mosCFNumber(rawValue) } }
extension Array: _MOSCFConvertible {
    public var _mosCFValue: CFTypeRef {
        let objs: [CFTypeRef] = compactMap { _mosToCF($0) }
        var ptrs: [UnsafeRawPointer?] = objs.map { UnsafeRawPointer(Unmanaged.passUnretained($0).toOpaque()) }
        return withExtendedLifetime(objs) {
            ptrs.withUnsafeMutableBufferPointer { CFArrayCreate(nil, $0.baseAddress, $0.count, [kCFTypeArrayCallBacks]) }
        }
    }
}
extension Dictionary: _MOSCFConvertible where Key: _MOSCFKey {
    public var _mosCFValue: CFTypeRef { _mosCF }
    /// `[kCTFontTraitsAttribute: [kCTFontWeightTrait: 0.3]]._mosCF` — no lugar de `as CFDictionary`.
    public var _mosCF: CFDictionary {
        var keys: [CFTypeRef] = [], vals: [CFTypeRef] = []
        for (k, v) in self {
            guard let cv = _mosToCF(v) else { continue }
            keys.append(k._mosCFKey); vals.append(cv)
        }
        var kp: [UnsafeRawPointer?] = keys.map { UnsafeRawPointer(Unmanaged.passUnretained($0).toOpaque()) }
        var vp: [UnsafeRawPointer?] = vals.map { UnsafeRawPointer(Unmanaged.passUnretained($0).toOpaque()) }
        return withExtendedLifetime((keys, vals)) {
            kp.withUnsafeMutableBufferPointer { k in
                vp.withUnsafeMutableBufferPointer { v in
                    CFDictionaryCreate(nil, k.baseAddress, v.baseAddress, k.count, [kCFTypeDictionaryKeyCallBacks], [kCFTypeDictionaryValueCallBacks])
                }
            }
        }
    }
}

/// Valor qualquer -> objeto CF (objetos CF passam direto; Optional vazio vira nil).
public func _mosToCF(_ v: Any) -> CFTypeRef? {
    if let c = v as? _MOSCFConvertible { return c._mosCFValue }
    if let o = v as? _MOSOptional { return o._mosWrapped.flatMap { _mosToCF($0) } }
    // (Mirror não serve: ele derruba o app com objetos CF, cuja metadata não é de classe Swift comum)
    if type(of: v) is AnyClass { return v as AnyObject }
    return nil   // struct/enum sem conversão: fica de fora (como no CF, que não guarda tipos Swift)
}
public protocol _MOSOptional { var _mosWrapped: Any? { get } }
extension Optional: _MOSOptional { public var _mosWrapped: Any? { self.map { $0 as Any } } }
public func _mosCFNumber(_ v: Int) -> CFNumber {
    var x = Int64(v)
    return CFNumberCreate(nil, .sInt64Type, &x)!
}
public func _mosCFNumber(_ v: Double) -> CFNumber {
    var x = v
    return CFNumberCreate(nil, .float64Type, &x)!
}
public func _mosCFNumber(_ v: CGFloat) -> CFNumber { _mosCFNumber(Double(v)) }
public func _mosCFString(_ s: String) -> CFString {
    CFStringCreateWithCString(nil, s, CFStringBuiltInEncodings.UTF8.rawValue)!
}
public func _mosString(_ s: CFString) -> String {
    if let p = CFStringGetCStringPtr(s, CFStringBuiltInEncodings.UTF8.rawValue) { return String(cString: p) }
    let n = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), CFStringBuiltInEncodings.UTF8.rawValue) + 1
    var buf = [CChar](repeating: 0, count: n)
    _ = CFStringGetCString(s, &buf, n, CFStringBuiltInEncodings.UTF8.rawValue)
    return String(cString: buf)
}
/// CFDictionary -> [CFString: Any] (os valores continuam objetos CF; as chaves que não são CFString ficam de fora).
public func _mosSwiftDict(_ d: CFDictionary) -> [CFString: Any] {
    let n = CFDictionaryGetCount(d)
    var keys = [UnsafeRawPointer?](repeating: nil, count: n), vals = [UnsafeRawPointer?](repeating: nil, count: n)
    keys.withUnsafeMutableBufferPointer { k in vals.withUnsafeMutableBufferPointer { v in CFDictionaryGetKeysAndValues(d, k.baseAddress, v.baseAddress) } }
    var out: [CFString: Any] = [:]
    for i in 0..<n {
        guard let kp = keys[i], let vp = vals[i] else { continue }
        let k = Unmanaged<AnyObject>.fromOpaque(kp).takeUnretainedValue()
        guard CFGetTypeID(k) == CFStringGetTypeID() else { continue }
        out[unsafeBitCast(k, to: CFString.self)] = Unmanaged<AnyObject>.fromOpaque(vp).takeUnretainedValue()
    }
    return out
}
@inline(__always) func _isDict(_ o: AnyObject) -> Bool { CFGetTypeID(o) == CFDictionaryGetTypeID() }

extension CFDictionary {
    /// No lugar de `cfDict as? [CFString: Any]`.
    public var _mosDict: [CFString: Any]? { _mosSwiftDict(self) }
    /// No lugar de `cfDict as? [String: Any]`.
    public var _mosStringDict: [String: Any]? {
        var out: [String: Any] = [:]
        for (k, v) in _mosSwiftDict(self) { out[_mosString(k)] = v }
        return out
    }
}
extension Optional where Wrapped == AnyObject {
    /// No lugar de `CTFontDescriptorCopyAttribute(...) as? [CFString: Any]`.
    public var _mosDict: [CFString: Any]? {
        guard let o = self, _isDict(o) else { return nil }
        return _mosSwiftDict(unsafeBitCast(o, to: CFDictionary.self))
    }
    public var _mosStringDict: [String: Any]? {
        guard let o = self, _isDict(o) else { return nil }
        return unsafeBitCast(o, to: CFDictionary.self)._mosStringDict
    }
}

// MARK: - leitura de atributos

/// Valor numérico de um objeto CF (CFNumber ou CFBoolean) ou de um valor Swift.
func _num(_ v: Any?) -> CGFloat? {
    guard let v else { return nil }
    switch v {
    case let x as CGFloat: return x
    case let x as Double: return CGFloat(x)
    case let x as Int: return CGFloat(x)
    default: break
    }
    guard type(of: v) is AnyClass else { return nil }
    let o = v as AnyObject
    if CFGetTypeID(o) == CFNumberGetTypeID() {
        var d = 0.0
        _ = CFNumberGetValue(unsafeBitCast(o, to: CFNumber.self), .float64Type, &d)
        return CGFloat(d)
    }
    if CFGetTypeID(o) == CFBooleanGetTypeID() { return CFBooleanGetValue(unsafeBitCast(o, to: CFBoolean.self)) ? 1 : 0 }
    return nil
}
extension CTFontDescriptor {
    var _attrs: [CFString: Any] { _mosSwiftDict(CTFontDescriptorCopyAttributes(self)) }
    var _traits: [CFString: Any] { CTFontDescriptorCopyAttribute(self, kCTFontTraitsAttribute)._mosDict ?? [:] }
}
/// Tamanho (pt) e peso padrão de cada estilo de texto do macOS.
func _textStyleSize(_ style: String) -> (CGFloat, CGFloat) {
    let table: [(String, CGFloat, CGFloat)] = [
        ("ExtraLargeTitle2", 28, 0.4), ("ExtraLargeTitle", 36, 0.4),
        ("Title0", 26, 0), ("Title1", 22, 0), ("Title2", 17, 0), ("Title3", 15, 0),
        ("Headline", 13, 0.3), ("Subhead", 11, 0), ("Callout", 12, 0), ("Footnote2", 10, 0),
        ("Footnote", 10, 0), ("Caption1", 10, 0), ("Caption2", 10, 0), ("Caption3", 10, 0),
    ]
    for (suffix, size, weight) in table where style.hasSuffix(suffix) { return (size, weight) }
    return (13, 0)   // Body
}
/// O macOS usa tamanhos fixos; as categorias de acessibilidade aumentam o texto.
func _sizeCategoryScale(_ c: String) -> CGFloat {
    let table: [(String, CGFloat)] = [("AccessibilityXXXL", 2.0), ("AccessibilityXXL", 1.8), ("AccessibilityXL", 1.6),
                                      ("AccessibilityL", 1.45), ("AccessibilityM", 1.3)]
    for (suffix, scale) in table where c.hasSuffix(suffix) { return scale }
    return 1.0
}

// MARK: - API pública do CoreText (a parte em C está em MOSCoreTextC.h)

public func CTFontDescriptorCreateWithNameAndSize(_ name: CFString, _ size: CGFloat) -> CTFontDescriptor {
    let attrs: [CFString: Any] = [kCTFontNameAttribute: name, kCTFontSizeAttribute: size]
    return CTFontDescriptorCreateWithAttributes(attrs._mosCF)
}
public func CTFontDescriptorCreateCopyWithAttributes(_ original: CTFontDescriptor, _ attributes: CFDictionary) -> CTFontDescriptor {
    var a = original._attrs
    for (k, v) in _mosSwiftDict(attributes) {
        // traços são mesclados (como no CoreText); o resto é substituído
        if CFEqual(k, kCTFontTraitsAttribute) {
            var t = original._traits
            if _isDict(v as AnyObject) { for (tk, tv) in _mosSwiftDict(unsafeBitCast(v as AnyObject, to: CFDictionary.self)) { t[tk] = tv } }
            a[k] = t
        } else {
            a[k] = v
        }
    }
    return CTFontDescriptorCreateWithAttributes(a._mosCF)
}
public func CTFontDescriptorCreateCopyWithSymbolicTraits(_ original: CTFontDescriptor, _ value: CTFontSymbolicTraits, _ mask: CTFontSymbolicTraits) -> CTFontDescriptor? {
    var a = original._attrs
    var traits = original._traits
    let old = UInt32(_num(traits[kCTFontSymbolicTrait]) ?? 0)
    traits[kCTFontSymbolicTrait] = Int((old & ~mask.rawValue) | (value.rawValue & mask.rawValue))
    a[kCTFontTraitsAttribute] = traits
    return CTFontDescriptorCreateWithAttributes(a._mosCF)
}
public func CTFontDescriptorCreateCopyWithFeature(_ original: CTFontDescriptor, _ featureType: CFNumber, _ featureSelector: CFNumber) -> CTFontDescriptor { original }
public func CTFontDescriptorCreateCopyWithVariation(_ original: CTFontDescriptor, _ variationIdentifier: CFNumber, _ variationValue: CGFloat) -> CTFontDescriptor { original }
public func CTFontDescriptorCreateWithAttributesAndOptions(_ attributes: CFDictionary, _ options: CTFontDescriptorOptions) -> CTFontDescriptor {
    CTFontDescriptorCreateWithAttributes(attributes)
}
public func CTFontCreateWithFontDescriptor(_ descriptor: CTFontDescriptor, _ size: CGFloat, _ matrix: UnsafeRawPointer?) -> CTFont {
    let s = size > 0 ? size : (_num(CTFontDescriptorCopyAttribute(descriptor, kCTFontSizeAttribute)) ?? 13)
    return _MOSCTFontCreate(descriptor, Double(s))
}
public func CTFontCreateWithName(_ name: CFString, _ size: CGFloat, _ matrix: UnsafeRawPointer?) -> CTFont {
    _MOSCTFontCreate(CTFontDescriptorCreateWithNameAndSize(name, size), Double(size > 0 ? size : 12))
}
public func CTFontCreateUIFontForLanguage(_ uiType: CTFontUIFontType, _ size: CGFloat, _ language: CFString?) -> CTFont? {
    CTFontCreateWithFontDescriptor(_mos_CTFontDescriptorCreateForUIType(uiType, size, language), 0, nil)
}
public func CTFontGetSize(_ font: CTFont) -> CGFloat { CGFloat(_MOSCTFontGetSize(font)) }
public func CTFontGetSymbolicTraits(_ font: CTFont) -> CTFontSymbolicTraits { _mos_CTFontDescriptorGetSymbolicTraits(CTFontCopyFontDescriptor(font)) }
// métricas da fonte do sistema do mOS (proporções da fonte de interface)
public func CTFontGetAscent(_ font: CTFont) -> CGFloat { CTFontGetSize(font) * 0.952 }
public func CTFontGetDescent(_ font: CTFont) -> CGFloat { CTFontGetSize(font) * 0.241 }
public func CTFontGetLeading(_ font: CTFont) -> CGFloat { 0 }
public func CTFontGetCapHeight(_ font: CTFont) -> CGFloat { CTFontGetSize(font) * 0.705 }
public func CTFontGetXHeight(_ font: CTFont) -> CGFloat { CTFontGetSize(font) * 0.53 }
public func CTFontGetUnderlinePosition(_ font: CTFont) -> CGFloat { -CTFontGetSize(font) * 0.1 }
public func CTFontGetUnderlineThickness(_ font: CTFont) -> CGFloat { max(1, CTFontGetSize(font) / 14) }

// MARK: - SPI privada (mesmos símbolos que o OpenSwiftUI declara em CoreText+Private.swift)

@_silgen_name("CTFontDescriptorGetWeight")
public func _mos_CTFontDescriptorGetWeight(_ descriptor: CTFontDescriptor) -> CGFloat {
    _num(descriptor._traits[kCTFontWeightTrait]) ?? 0
}
@_silgen_name("CTFontDescriptorGetSymbolicTraits")
public func _mos_CTFontDescriptorGetSymbolicTraits(_ descriptor: CTFontDescriptor) -> CTFontSymbolicTraits {
    let traits = descriptor._traits
    var t = CTFontSymbolicTraits(rawValue: UInt32(_num(traits[kCTFontSymbolicTrait]) ?? 0))
    if (_num(traits[kCTFontWeightTrait]) ?? 0) >= 0.4 { t.insert(.traitBold) }
    // Font.Design.monospaced: o mOS escolhe a fonte mono pelo traço (mos_font peso 3)
    if let design = traits[_kCTFontUIFontDesignTrait], CFEqual(design as AnyObject, _kCTFontUIFontDesignMonospaced) { t.insert(.traitMonoSpace) }
    return t
}
@_silgen_name("kCTFontUIFontDesignTrait") let _kCTFontUIFontDesignTrait: CFString
@_silgen_name("kCTFontUIFontDesignMonospaced") let _kCTFontUIFontDesignMonospaced: CFString
@_silgen_name("CTFontDescriptorGetTextStyleSize")
public func _mos_CTFontDescriptorGetTextStyleSize(_ textStyle: CFString, _ sizeCategory: CFString, _ platform: Int32,
                                                  _ weight: UnsafePointer<CGFloat>?, _ size: UnsafePointer<CGFloat>?) -> CGFloat {
    let (s, w) = _textStyleSize(_mosString(textStyle))
    let scaled = (s * _sizeCategoryScale(_mosString(sizeCategory))).rounded()
    if let weight { UnsafeMutablePointer(mutating: weight).pointee = w }
    if let size { UnsafeMutablePointer(mutating: size).pointee = scaled }
    return scaled
}
@_silgen_name("CTFontDescriptorCreateWithTextStyleAndAttributes")
public func _mos_CTFontDescriptorCreateWithTextStyleAndAttributes(_ textStyle: CFString, _ sizeCategory: CFString, _ attributes: CFDictionary) -> CTFontDescriptor {
    let (base, w) = _textStyleSize(_mosString(textStyle))
    let size = (base * _sizeCategoryScale(_mosString(sizeCategory))).rounded()
    var a = _mosSwiftDict(attributes)
    if a[kCTFontSizeAttribute] == nil { a[kCTFontSizeAttribute] = size }
    var traits = (a[kCTFontTraitsAttribute] as AnyObject?)._mosDict ?? [:]
    if traits[kCTFontWeightTrait] == nil { traits[kCTFontWeightTrait] = w }
    a[kCTFontTraitsAttribute] = traits
    a[_mosCFString("NSCTFontUIUsageAttribute")] = textStyle
    return CTFontDescriptorCreateWithAttributes(a._mosCF)
}
@_silgen_name("CTFontDescriptorIsSystemUIFont")
public func _mos_CTFontDescriptorIsSystemUIFont(_ descriptor: CTFontDescriptor) -> Bool {
    CTFontDescriptorCopyAttribute(descriptor, kCTFontNameAttribute) == nil && CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute) == nil
}
@_silgen_name("CTFontDescriptorCreateForUIType")
public func _mos_CTFontDescriptorCreateForUIType(_ uiType: CTFontUIFontType, _ size: CGFloat, _ language: CFString?) -> CTFontDescriptor {
    let small = uiType == .smallSystem || uiType == .smallEmphasizedSystem
    let bold: CGFloat = (uiType == .emphasizedSystem || uiType == .smallEmphasizedSystem) ? 0.4 : 0
    let attrs: [CFString: Any] = [kCTFontSizeAttribute: size > 0 ? size : (small ? 11 : 13),
                                  kCTFontTraitsAttribute: [kCTFontWeightTrait: bold]]
    return CTFontDescriptorCreateWithAttributes(attrs._mosCF)
}
@_silgen_name("CTFontCopySystemUIFontExcessiveLineHeightCharacterSet")
public func _mos_CTFontCopySystemUIFontExcessiveLineHeightCharacterSet() -> CFCharacterSet? { nil }
@_silgen_name("CTFontCopyTallestTextStyleLanguageForString")
public func _mos_CTFontCopyTallestTextStyleLanguageForString(_ string: CFString) -> CFString? { nil }
@_silgen_name("CTFontGetLanguageAwareOutsets")
public func _mos_CTFontGetLanguageAwareOutsets(_ font: CTFont, _ left: UnsafeMutablePointer<CGFloat>, _ top: UnsafeMutablePointer<CGFloat>,
                                               _ right: UnsafeMutablePointer<CGFloat>, _ bottom: UnsafeMutablePointer<CGFloat>) -> Bool { false }
@_silgen_name("CTFontGetClippingMetrics")
public func _mos_CTFontGetClippingMetrics(_ font: CTFont, _ ascent: UnsafeMutablePointer<CGFloat>, _ descent: UnsafeMutablePointer<CGFloat>) -> Bool { false }
@_silgen_name("CTFontIsSystemUIFont")
public func _mos_CTFontIsSystemUIFont(_ font: CTFont) -> Bool { _mos_CTFontDescriptorIsSystemUIFont(CTFontCopyFontDescriptor(font)) }
@_silgen_name("CTFontGetWeight")
public func _mos_CTFontGetWeight(_ font: CTFont) -> CGFloat { _mos_CTFontDescriptorGetWeight(CTFontCopyFontDescriptor(font)) }
@_silgen_name("CTFontGetAccessibilityBoldWeightOfWeight")
public func _mos_CTFontGetAccessibilityBoldWeightOfWeight(_ weight: CGFloat) -> CGFloat { min(1, weight + 0.2) }

// MARK: - pesos públicos (double em ctfont.c, lidos como CGFloat)

@_silgen_name("kCTFontWeightUltraLight") public let kCTFontWeightUltraLight: CGFloat
@_silgen_name("kCTFontWeightThin") public let kCTFontWeightThin: CGFloat
@_silgen_name("kCTFontWeightLight") public let kCTFontWeightLight: CGFloat
@_silgen_name("kCTFontWeightMedium") public let kCTFontWeightMedium: CGFloat
@_silgen_name("kCTFontWeightSemibold") public let kCTFontWeightSemibold: CGFloat
@_silgen_name("kCTFontWeightBold") public let kCTFontWeightBold: CGFloat
@_silgen_name("kCTFontWeightBlack") public let kCTFontWeightBlack: CGFloat

// MARK: - Hashable e tipos que o OpenSwiftUI espera do CoreText da Apple

extension CTFont: @retroactive Hashable {
    public static func == (lhs: CTFont, rhs: CTFont) -> Bool { CFEqual(lhs, rhs) }
    public func hash(into hasher: inout Hasher) { hasher.combine(CFHash(self)) }
}
extension CTGlyphInfo: @retroactive Hashable {
    public static func == (lhs: CTGlyphInfo, rhs: CTGlyphInfo) -> Bool { CFEqual(lhs, rhs) }
    public func hash(into hasher: inout Hasher) { hasher.combine(CFHash(self)) }
}

public class CTLine: Hashable, @unchecked Sendable {
    public init() {}
    public static func == (lhs: CTLine, rhs: CTLine) -> Bool { lhs === rhs }
    public func hash(into hasher: inout Hasher) { hasher.combine(ObjectIdentifier(self)) }
}

public struct CTCompositionLanguage: RawRepresentable, Hashable, Codable, Sendable {
    public var rawValue: Int32
    public init(rawValue: Int32) { self.rawValue = rawValue }
    public static let unset = CTCompositionLanguage(rawValue: 0)
}

public func CTParagraphStyleGetCompositionLanguageForLanguage(_ language: CFString?) -> CTCompositionLanguage { .unset }
