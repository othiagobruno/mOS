// mOS — overlay Swift do UniformTypeIdentifiers (docs/swift-objc-runtime.md, fase 5), com os nomes e assinaturas da
// Apple: os binários Swift (Postico, o helper Computer Use do Grok Bot, módulos nativos do Codex) importam os nomes
// mangled daqui. O struct guarda o identificador; o resto (extensão, MIME, conformidade) pergunta à classe ObjC UTType
// do libSystem (vscode_media.c), que consulta o Launch Services do mOS.
@_exported import Foundation

// o importador desta variante não faz a ponte String <-> NSString sozinho nas APIs da classe ObjC
@inline(__always) internal func _ns(_ s: String) -> NSString { s._bridgeToObjectiveC() }
@inline(__always) internal func _sw(_ s: NSString) -> String { String._unconditionallyBridgeFromObjectiveC(s) }
@inline(__always) internal func _sw(_ s: NSString?) -> String? { s.map { String._unconditionallyBridgeFromObjectiveC($0) } }

public struct UTType: Hashable, Sendable, CustomStringConvertible, CustomDebugStringConvertible {
    public let identifier: String

    @usableFromInline internal init(_unchecked identifier: String) { self.identifier = identifier }

    internal var _objc: _UTTypeObjC? { _UTTypeObjC(identifier: _ns(identifier)) }

    /// init?(_:) — só tipos que o sistema conhece (declarados ou dinâmicos que ele aceita)
    public init?(_ identifier: String) {
        guard let t = _UTTypeObjC(identifier: _ns(identifier)) else { return nil }
        self.identifier = _sw(t.identifier)
    }
    public init(exportedAs identifier: String, conformingTo parentType: UTType? = nil) {
        self.identifier = identifier
    }
    public init(importedAs identifier: String, conformingTo parentType: UTType? = nil) {
        self.identifier = identifier
    }
    public init?(filenameExtension: String, conformingTo supertype: UTType = .data) {
        guard let s = _UTTypeObjC(identifier: _ns(supertype.identifier)),
              let t = _UTTypeObjC(filenameExtension: _ns(filenameExtension), conformingTo: s) else { return nil }
        self.identifier = _sw(t.identifier)
    }
    public init?(filenameExtension: String) { self.init(filenameExtension: filenameExtension, conformingTo: .data) }
    public init?(mimeType: String, conformingTo supertype: UTType = .data) {
        guard let s = _UTTypeObjC(identifier: _ns(supertype.identifier)),
              let t = _UTTypeObjC(mimeType: _ns(mimeType), conformingTo: s) else { return nil }
        self.identifier = _sw(t.identifier)
    }
    public init?(tag: String, tagClass: UTTagClass, conformingTo supertype: UTType?) {
        guard let t = UTType.types(tag: tag, tagClass: tagClass, conformingTo: supertype).first else { return nil }
        self = t
    }

    public static func types(tag: String, tagClass: UTTagClass, conformingTo supertype: UTType?) -> [UTType] {
        let s = supertype.flatMap { _UTTypeObjC(identifier: _ns($0.identifier)) }
        return _UTTypeObjC.types(withTag: _ns(tag), tagClass: _ns(tagClass.rawValue), conformingTo: s).map { UTType(_unchecked: _sw(($0 as! _UTTypeObjC).identifier)) }
    }

    public var preferredFilenameExtension: String? { _sw(_objc?.preferredFilenameExtension) }
    public var preferredMIMEType: String? { _sw(_objc?.preferredMIMEType) }
    public var localizedDescription: String? { _sw(_objc?.localizedDescription) }
    public var isDynamic: Bool { _objc?.isDynamic ?? identifier.hasPrefix("dyn.") }
    public var isDeclared: Bool { !isDynamic }
    public var isPublic: Bool { identifier.hasPrefix("public.") }
    public var tags: [UTTagClass: [String]] {
        var r: [UTTagClass: [String]] = [:]
        if let e = preferredFilenameExtension { r[.filenameExtension] = [e] }
        if let m = preferredMIMEType { r[.mimeType] = [m] }
        return r
    }

    public func conforms(to type: UTType) -> Bool {
        if type.identifier == identifier { return true }
        guard let a = _objc, let b = type._objc else { return false }
        return a.conforms(to: b)
    }
    public func isSubtype(of type: UTType) -> Bool { type != self && conforms(to: type) }
    public func isSupertype(of type: UTType) -> Bool { type != self && type.conforms(to: self) }

    public var description: String { identifier }
    public var debugDescription: String { identifier }

    public static func == (a: UTType, b: UTType) -> Bool { a.identifier == b.identifier }
    public func hash(into h: inout Hasher) { h.combine(identifier) }
}

// ponte com a classe ObjC (o Swift da Apple passa UTType para APIs ObjC como NSOpenPanel.allowedContentTypes)
extension UTType: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> _UTTypeObjC {
        _UTTypeObjC(identifier: _ns(identifier)) ?? _UTTypeObjC.exportedType(withIdentifier: _ns(identifier))
    }
    public static func _forceBridgeFromObjectiveC(_ source: _UTTypeObjC, result: inout UTType?) {
        result = UTType(_unchecked: _sw(source.identifier))
    }
    public static func _conditionallyBridgeFromObjectiveC(_ source: _UTTypeObjC, result: inout UTType?) -> Bool {
        result = UTType(_unchecked: _sw(source.identifier))
        return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: _UTTypeObjC?) -> UTType {
        UTType(_unchecked: source.map { _sw($0.identifier) } ?? "public.data")
    }
}

public struct UTTagClass: RawRepresentable, Hashable, Sendable {
    public var rawValue: String
    public init(rawValue: String) { self.rawValue = rawValue }
    public static var filenameExtension: UTTagClass { UTTagClass(rawValue: "public.filename-extension") }
    public static var mimeType: UTTagClass { UTTagClass(rawValue: "public.mime-type") }
}

// os tipos declarados pela Apple que os apps pedem (static var ...: UTType { get })
extension UTType {
    public static var item: UTType { UTType(_unchecked: "public.item") }
    public static var content: UTType { UTType(_unchecked: "public.content") }
    public static var compositeContent: UTType { UTType(_unchecked: "public.composite-content") }
    public static var data: UTType { UTType(_unchecked: "public.data") }
    public static var directory: UTType { UTType(_unchecked: "public.directory") }
    public static var folder: UTType { UTType(_unchecked: "public.folder") }
    public static var volume: UTType { UTType(_unchecked: "public.volume") }
    public static var package: UTType { UTType(_unchecked: "com.apple.package") }
    public static var bundle: UTType { UTType(_unchecked: "com.apple.bundle") }
    public static var application: UTType { UTType(_unchecked: "com.apple.application") }
    public static var applicationBundle: UTType { UTType(_unchecked: "com.apple.application-bundle") }
    public static var executable: UTType { UTType(_unchecked: "public.executable") }
    public static var unixExecutable: UTType { UTType(_unchecked: "public.unix-executable") }
    public static var symbolicLink: UTType { UTType(_unchecked: "public.symlink") }
    public static var aliasFile: UTType { UTType(_unchecked: "com.apple.alias-file") }
    public static var url: UTType { UTType(_unchecked: "public.url") }
    public static var fileURL: UTType { UTType(_unchecked: "public.file-url") }
    public static var text: UTType { UTType(_unchecked: "public.text") }
    public static var plainText: UTType { UTType(_unchecked: "public.plain-text") }
    public static var utf8PlainText: UTType { UTType(_unchecked: "public.utf8-plain-text") }
    public static var utf16ExternalPlainText: UTType { UTType(_unchecked: "public.utf16-external-plain-text") }
    public static var utf16PlainText: UTType { UTType(_unchecked: "public.utf16-plain-text") }
    public static var delimitedText: UTType { UTType(_unchecked: "public.delimited-values-text") }
    public static var commaSeparatedText: UTType { UTType(_unchecked: "public.comma-separated-values-text") }
    public static var tabSeparatedText: UTType { UTType(_unchecked: "public.tab-separated-values-text") }
    public static var utf8TabSeparatedText: UTType { UTType(_unchecked: "public.utf8-tab-separated-values-text") }
    public static var rtf: UTType { UTType(_unchecked: "public.rtf") }
    public static var html: UTType { UTType(_unchecked: "public.html") }
    public static var xml: UTType { UTType(_unchecked: "public.xml") }
    public static var json: UTType { UTType(_unchecked: "public.json") }
    public static var yaml: UTType { UTType(_unchecked: "public.yaml") }
    public static var sourceCode: UTType { UTType(_unchecked: "public.source-code") }
    public static var script: UTType { UTType(_unchecked: "public.script") }
    public static var shellScript: UTType { UTType(_unchecked: "public.shell-script") }
    public static var propertyList: UTType { UTType(_unchecked: "com.apple.property-list") }
    public static var image: UTType { UTType(_unchecked: "public.image") }
    public static var png: UTType { UTType(_unchecked: "public.png") }
    public static var jpeg: UTType { UTType(_unchecked: "public.jpeg") }
    public static var gif: UTType { UTType(_unchecked: "com.compuserve.gif") }
    public static var tiff: UTType { UTType(_unchecked: "public.tiff") }
    public static var bmp: UTType { UTType(_unchecked: "com.microsoft.bmp") }
    public static var heic: UTType { UTType(_unchecked: "public.heic") }
    public static var heif: UTType { UTType(_unchecked: "public.heif") }
    public static var webP: UTType { UTType(_unchecked: "org.webmproject.webp") }
    public static var svg: UTType { UTType(_unchecked: "public.svg-image") }
    public static var ico: UTType { UTType(_unchecked: "com.microsoft.ico") }
    public static var icns: UTType { UTType(_unchecked: "com.apple.icns") }
    public static var pdf: UTType { UTType(_unchecked: "com.adobe.pdf") }
    public static var audiovisualContent: UTType { UTType(_unchecked: "public.audiovisual-content") }
    public static var audio: UTType { UTType(_unchecked: "public.audio") }
    public static var mp3: UTType { UTType(_unchecked: "public.mp3") }
    public static var wav: UTType { UTType(_unchecked: "com.microsoft.waveform-audio") }
    public static var mpeg4Audio: UTType { UTType(_unchecked: "public.mpeg-4-audio") }
    public static var movie: UTType { UTType(_unchecked: "public.movie") }
    public static var video: UTType { UTType(_unchecked: "public.video") }
    public static var mpeg4Movie: UTType { UTType(_unchecked: "public.mpeg-4") }
    public static var quickTimeMovie: UTType { UTType(_unchecked: "com.apple.quicktime-movie") }
    public static var archive: UTType { UTType(_unchecked: "public.archive") }
    public static var zip: UTType { UTType(_unchecked: "public.zip-archive") }
    public static var gzip: UTType { UTType(_unchecked: "org.gnu.gnu-zip-archive") }
    public static var diskImage: UTType { UTType(_unchecked: "public.disk-image") }
    public static var database: UTType { UTType(_unchecked: "public.database") }
    public static var vCard: UTType { UTType(_unchecked: "public.vcard") }
    public static var font: UTType { UTType(_unchecked: "public.font") }
    public static var message: UTType { UTType(_unchecked: "public.message") }
    public static var emailMessage: UTType { UTType(_unchecked: "public.email-message") }
}

// URLResourceValues.contentType (a extensão mora neste módulo, como na Apple): o identificador do tipo que o
// Foundation do mOS informa; sem ele, o tipo pela extensão do nome
extension URLResourceValues {
    public var contentType: UTType? {
        if let id = typeIdentifier, let t = UTType(id) { return t }
        if let n = name, let dot = n.lastIndex(of: "."), dot != n.startIndex {
            return UTType(filenameExtension: String(n[n.index(after: dot)...]))
        }
        return nil
    }
}
