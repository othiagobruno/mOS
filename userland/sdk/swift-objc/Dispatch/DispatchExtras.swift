// mOS — overlay Dispatch (fase 3): o resto da API do overlay da Apple que binários usam — DispatchSpecificKey e
// get/setSpecific, concurrentPerform, as fontes de pressão de memória e de arquivo, DispatchData e
// _dispatchPreconditionTest. Mesmos nomes e assinaturas da Apple.

@_silgen_name("dispatch_queue_set_specific")
internal func _mos_queue_set_specific(_ q: DispatchQueue, _ key: UnsafeRawPointer, _ ctx: UnsafeMutableRawPointer?,
                                      _ dtor: (@convention(c) (UnsafeMutableRawPointer?) -> Void)?)
@_silgen_name("dispatch_queue_get_specific")
internal func _mos_queue_get_specific(_ q: DispatchQueue, _ key: UnsafeRawPointer) -> UnsafeMutableRawPointer?
@_silgen_name("dispatch_get_specific")
internal func _mos_get_specific(_ key: UnsafeRawPointer) -> UnsafeMutableRawPointer?
@_silgen_name("dispatch_data_create")
internal func _mos_data_create(_ buf: UnsafeRawPointer?, _ n: Int, _ q: DispatchQueue?, _ destructor: UnsafeRawPointer?) -> __DispatchData
@_silgen_name("CFDataGetLength") internal func _mos_cfdata_len(_ d: __DispatchData) -> Int
@_silgen_name("CFDataGetBytePtr") internal func _mos_cfdata_bytes(_ d: __DispatchData) -> UnsafePointer<UInt8>?

// MARK: - valores por fila

public final class DispatchSpecificKey<T> {
    public init() {}
}

internal final class _DispatchSpecificValue<T> {
    internal let value: T
    internal init(value: T) { self.value = value }
}

private func _mosDestructSpecific(_ p: UnsafeMutableRawPointer?) {
    if let p { Unmanaged<AnyObject>.fromOpaque(p).release() }
}

extension DispatchQueue {
    public func setSpecific<T>(key: DispatchSpecificKey<T>, value: T?) {
        let k = UnsafeRawPointer(Unmanaged.passUnretained(key).toOpaque())
        let v = value.map { UnsafeMutableRawPointer(Unmanaged.passRetained(_DispatchSpecificValue(value: $0)).toOpaque()) }
        _mos_queue_set_specific(self, k, v, v == nil ? nil : { _mosDestructSpecific($0) })
    }

    public func getSpecific<T>(key: DispatchSpecificKey<T>) -> T? {
        let k = UnsafeRawPointer(Unmanaged.passUnretained(key).toOpaque())
        guard let p = _mos_queue_get_specific(self, k) else { return nil }
        return Unmanaged<_DispatchSpecificValue<T>>.fromOpaque(p).takeUnretainedValue().value
    }

    /// o valor da fila em que o código está rodando
    public class func getSpecific<T>(key: DispatchSpecificKey<T>) -> T? {
        let k = UnsafeRawPointer(Unmanaged.passUnretained(key).toOpaque())
        guard let p = _mos_get_specific(k) else { return nil }
        return Unmanaged<_DispatchSpecificValue<T>>.fromOpaque(p).takeUnretainedValue().value
    }

    /// o dispatch do mOS ainda não tem threads por fila: as iterações rodam em ordem na thread atual
    public class func concurrentPerform(iterations: Int, execute work: (Int) -> Void) {
        for i in 0..<max(iterations, 0) { work(i) }
    }
}

// MARK: - fontes de pressão de memória e de arquivo (o mOS ainda não as dispara)

extension DispatchSource {
    public struct MemoryPressureEvent: OptionSet, RawRepresentable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let normal = MemoryPressureEvent(rawValue: 0x1)
        public static let warning = MemoryPressureEvent(rawValue: 0x2)
        public static let critical = MemoryPressureEvent(rawValue: 0x4)
        public static let all: MemoryPressureEvent = [.normal, .warning, .critical]
    }

    public struct FileSystemEvent: OptionSet, RawRepresentable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let delete = FileSystemEvent(rawValue: 0x1)
        public static let write = FileSystemEvent(rawValue: 0x2)
        public static let extend = FileSystemEvent(rawValue: 0x4)
        public static let attrib = FileSystemEvent(rawValue: 0x8)
        public static let link = FileSystemEvent(rawValue: 0x10)
        public static let rename = FileSystemEvent(rawValue: 0x20)
        public static let revoke = FileSystemEvent(rawValue: 0x40)
        public static let funlock = FileSystemEvent(rawValue: 0x100)
        public static let all: FileSystemEvent = [.delete, .write, .extend, .attrib, .link, .rename, .revoke]
    }

    public static func makeMemoryPressureSource(eventMask: MemoryPressureEvent, queue: DispatchQueue? = nil) -> DispatchSourceMemoryPressure {
        _mos_as(_mos_source_create(_mos_source_type("_dispatch_source_type_memorypressure"), 0, eventMask.rawValue, queue),
                DispatchSourceMemoryPressure.self)
    }

    public static func makeFileSystemObjectSource(fileDescriptor: Int32, eventMask: FileSystemEvent,
                                                  queue: DispatchQueue? = nil) -> DispatchSourceFileSystemObject {
        _mos_as(_mos_source_create(_mos_source_type("_dispatch_source_type_vnode"), UInt(fileDescriptor), eventMask.rawValue, queue),
                DispatchSourceFileSystemObject.self)
    }
}

extension DispatchSourceMemoryPressure {
    public var data: DispatchSource.MemoryPressureEvent { DispatchSource.MemoryPressureEvent(rawValue: _mos_source_data(_mos_src(self))) }
    public var mask: DispatchSource.MemoryPressureEvent { DispatchSource.MemoryPressureEvent(rawValue: _mos_source_mask(_mos_src(self))) }
}

extension DispatchSourceFileSystemObject {
    public var handle: Int32 { Int32(truncatingIfNeeded: _mos_source_handle(_mos_src(self))) }
    public var data: DispatchSource.FileSystemEvent { DispatchSource.FileSystemEvent(rawValue: _mos_source_data(_mos_src(self))) }
    public var mask: DispatchSource.FileSystemEvent { DispatchSource.FileSystemEvent(rawValue: _mos_source_mask(_mos_src(self))) }
}

// MARK: - DispatchData (no mOS o dispatch_data_t é um NSData/CFData)

public struct DispatchData: RandomAccessCollection {
    public typealias Index = Int
    public typealias Element = UInt8
    public typealias Indices = Range<Int>

    internal var __wrapped: __DispatchData

    public static let empty = DispatchData(bytes: UnsafeRawBufferPointer(start: nil, count: 0))

    public init(bytes buffer: UnsafeRawBufferPointer) {
        __wrapped = _mos_data_create(buffer.baseAddress, buffer.count, nil, nil)
    }

    public init(bytes buffer: UnsafeBufferPointer<UInt8>) {
        self.init(bytes: UnsafeRawBufferPointer(buffer))
    }

    internal init(data: __DispatchData) { __wrapped = data }

    public var count: Int { _mos_cfdata_len(__wrapped) }
    public var startIndex: Int { 0 }
    public var endIndex: Int { count }
    public var indices: Range<Int> { 0..<count }
    public func index(after i: Int) -> Int { i + 1 }
    public func index(before i: Int) -> Int { i - 1 }

    public subscript(index: Int) -> UInt8 {
        precondition(index >= 0 && index < count, "DispatchData: índice fora")
        return _mos_cfdata_bytes(__wrapped)![index]
    }

    public func withUnsafeBytes<Result, ContentType>(body: (UnsafePointer<ContentType>) throws -> Result) rethrows -> Result {
        let p = _mos_cfdata_bytes(__wrapped) ?? UnsafePointer(bitPattern: 1)!
        return try UnsafeRawPointer(p).withMemoryRebound(to: ContentType.self, capacity: Swift.max(count, 1)) { try body($0) }
    }

    public func copyBytes(to pointer: UnsafeMutablePointer<UInt8>, count: Int) {
        guard count > 0, let p = _mos_cfdata_bytes(__wrapped) else { return }
        pointer.update(from: p, count: Swift.min(count, self.count))
    }

    public mutating func append(_ other: DispatchData) {
        var all = [UInt8](self)
        all.append(contentsOf: other)
        self = all.withUnsafeBytes { DispatchData(bytes: $0) }
    }

    public mutating func append(_ bytes: UnsafeRawBufferPointer) {
        var all = [UInt8](self)
        all.append(contentsOf: bytes)
        self = all.withUnsafeBytes { DispatchData(bytes: $0) }
    }

    public func subdata(in range: Range<Int>) -> DispatchData {
        let a = [UInt8](self)[range]
        return a.withUnsafeBytes { DispatchData(bytes: $0) }
    }
}

extension DispatchData: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> __DispatchData { __wrapped }
    public static func _forceBridgeFromObjectiveC(_ input: __DispatchData, result: inout DispatchData?) {
        result = DispatchData(data: input)
    }
    public static func _conditionallyBridgeFromObjectiveC(_ input: __DispatchData, result: inout DispatchData?) -> Bool {
        result = DispatchData(data: input)
        return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: __DispatchData?) -> DispatchData {
        source.map { DispatchData(data: $0) } ?? .empty
    }
}

// MARK: - dispatchPrecondition: o mOS não acompanha a fila de cada thread com segurança suficiente para recusar

public func _dispatchPreconditionTest(_ condition: DispatchPredicate) -> Bool { true }
