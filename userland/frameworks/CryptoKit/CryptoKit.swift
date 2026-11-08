// mOS — CryptoKit.framework (só Swift, como o da Apple): as funções de hash que os binários Swift importam
// (o helper "Computer Use" do Grok Bot usa SHA256). O hash é o CC_SHA256_* do CommonCrypto do libSystem
// (userland/libsystem/commoncrypto.c). tools/build-frameworks-objc.ps1

@_exported import CryptoKit

public protocol Digest: Hashable, CustomStringConvertible, Sequence, Sendable where Element == UInt8 {
    static var byteCount: Int { get }
}

public protocol HashFunction: Sendable {
    associatedtype Digest: CryptoKit.Digest
    static var blockByteCount: Int { get }
    init()
    mutating func update(bufferPointer: UnsafeRawBufferPointer)
    func finalize() -> Self.Digest
}

extension HashFunction {
    public static func hash(bufferPointer: UnsafeRawBufferPointer) -> Digest {
        var h = Self()
        h.update(bufferPointer: bufferPointer)
        return h.finalize()
    }
    public static func hash<D: Sequence>(data: D) -> Digest where D.Element == UInt8 {
        var h = Self()
        h.update(data: data)
        return h.finalize()
    }
    public mutating func update<D: Sequence>(data: D) where D.Element == UInt8 {
        let bytes = Array(data)
        bytes.withUnsafeBytes { update(bufferPointer: $0) }
    }
}

@_silgen_name("CC_SHA256_Init") internal func _cc_sha256_init(_ c: UnsafeMutableRawPointer) -> Int32
@_silgen_name("CC_SHA256_Update") internal func _cc_sha256_update(_ c: UnsafeMutableRawPointer, _ d: UnsafeRawPointer?, _ n: UInt32) -> Int32
@_silgen_name("CC_SHA256_Final") internal func _cc_sha256_final(_ md: UnsafeMutableRawPointer, _ c: UnsafeMutableRawPointer) -> Int32

public struct SHA256Digest: Digest {
    var b: (UInt64, UInt64, UInt64, UInt64) = (0, 0, 0, 0)
    public static var byteCount: Int { 32 }
    var bytes: [UInt8] { Swift.withUnsafeBytes(of: b) { Array($0) } }
    public func makeIterator() -> Array<UInt8>.Iterator { bytes.makeIterator() }
    public var description: String { "SHA256 digest: " + bytes.map { ($0 < 16 ? "0" : "") + String($0, radix: 16) }.joined() }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try Swift.withUnsafeBytes(of: b, body) }
    public static func == (l: SHA256Digest, r: SHA256Digest) -> Bool { l.b == r.b }
    public func hash(into h: inout Hasher) { h.combine(b.0); h.combine(b.1); h.combine(b.2); h.combine(b.3) }
}

public struct SHA256: HashFunction {
    public typealias Digest = SHA256Digest
    public static var blockByteCount: Int { 64 }
    public static var byteCount: Int { 32 }
    // CC_SHA256_CTX: count[2], hash[8], wbuf[16] (u32) = 104 bytes
    var ctx: (UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64) =
        (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    public init() { _ = Swift.withUnsafeMutableBytes(of: &ctx) { _cc_sha256_init($0.baseAddress!) } }
    public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
        var off = 0
        while off < bufferPointer.count {                       // CC_SHA256_Update recebe u32
            let n = min(bufferPointer.count - off, 1 << 30)
            _ = Swift.withUnsafeMutableBytes(of: &ctx) { _cc_sha256_update($0.baseAddress!, bufferPointer.baseAddress! + off, UInt32(n)) }
            off += n
        }
    }
    public func finalize() -> SHA256Digest {
        var c = ctx
        var d = SHA256Digest()
        Swift.withUnsafeMutableBytes(of: &d.b) { md in _ = Swift.withUnsafeMutableBytes(of: &c) { _cc_sha256_final(md.baseAddress!, $0.baseAddress!) } }
        return d
    }
}
