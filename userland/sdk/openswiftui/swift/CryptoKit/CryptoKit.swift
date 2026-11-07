// mOS SDK — CryptoKit com a API pública da Apple, sobre o núcleo C do libSystem (userland/libsystem/cryptokit.c:
// SHA-2, MD5/SHA-1, HMAC, HKDF, AES-GCM, AES-KW, ChaCha20-Poly1305, X25519, Ed25519, P-256/P-384/P-521).
// Tipos de valor como na Apple: HashFunction copia o estado no primeiro update depois de uma cópia; chaves e
// segredos guardam os bytes; erros são CryptoKitError.
// Fora por enquanto: chaves públicas a partir de compactRepresentation/
// compressedRepresentation (exige raiz quadrada no corpo) e as chaves no Secure Enclave.

import Foundation

// MARK: - núcleo C

@_silgen_name("mos_ck_hash_new") private func ck_hash_new(_ alg: Int32) -> UnsafeMutableRawPointer?
@_silgen_name("mos_ck_hash_copy") private func ck_hash_copy(_ h: UnsafeRawPointer) -> UnsafeMutableRawPointer?
@_silgen_name("mos_ck_hash_update") private func ck_hash_update(_ h: UnsafeMutableRawPointer, _ d: UnsafeRawPointer?, _ n: Int)
@_silgen_name("mos_ck_hash_final") private func ck_hash_final(_ h: UnsafeRawPointer, _ out: UnsafeMutablePointer<UInt8>)
@_silgen_name("mos_ck_hash_free") private func ck_hash_free(_ h: UnsafeMutableRawPointer?)
@_silgen_name("mos_ck_hmac") private func ck_hmac(_ alg: Int32, _ k: UnsafePointer<UInt8>?, _ kl: Int, _ m: UnsafeRawPointer?, _ ml: Int, _ out: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_hkdf") private func ck_hkdf(_ alg: Int32, _ ikm: UnsafePointer<UInt8>?, _ il: Int, _ salt: UnsafePointer<UInt8>?, _ sl: Int, _ info: UnsafePointer<UInt8>?, _ nl: Int, _ out: UnsafeMutablePointer<UInt8>, _ ol: Int) -> Int32
@_silgen_name("mos_ck_aes_gcm_seal") private func ck_gcm_seal(_ k: UnsafePointer<UInt8>?, _ kl: Int, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ o: UnsafeMutablePointer<UInt8>?, _ t: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_aes_gcm_open") private func ck_gcm_open(_ k: UnsafePointer<UInt8>?, _ kl: Int, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ t: UnsafePointer<UInt8>?, _ o: UnsafeMutablePointer<UInt8>?) -> Int32
@_silgen_name("mos_ck_chachapoly_seal") private func ck_cc_seal(_ k: UnsafePointer<UInt8>?, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ o: UnsafeMutablePointer<UInt8>?, _ t: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_chachapoly_open") private func ck_cc_open(_ k: UnsafePointer<UInt8>?, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ t: UnsafePointer<UInt8>?, _ o: UnsafeMutablePointer<UInt8>?) -> Int32
@_silgen_name("mos_ck_aes_wrap") private func ck_wrap(_ kek: UnsafePointer<UInt8>?, _ kl: Int, _ key: UnsafePointer<UInt8>?, _ n: Int, _ out: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_aes_unwrap") private func ck_unwrap(_ kek: UnsafePointer<UInt8>?, _ kl: Int, _ inp: UnsafePointer<UInt8>?, _ n: Int, _ out: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_random") private func ck_random(_ buf: UnsafeMutableRawPointer, _ n: Int)
@_silgen_name("mos_ck_equal") private func ck_equal(_ a: UnsafeRawPointer?, _ b: UnsafeRawPointer?, _ n: Int) -> Int32
@_silgen_name("mos_ck_x25519_public") private func ck_x_pub(_ priv: UnsafePointer<UInt8>?, _ pub: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_x25519") private func ck_x(_ priv: UnsafePointer<UInt8>?, _ peer: UnsafePointer<UInt8>?, _ out: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ed25519_public") private func ck_ed_pub(_ seed: UnsafePointer<UInt8>?, _ pub: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ed25519_sign") private func ck_ed_sign(_ seed: UnsafePointer<UInt8>?, _ m: UnsafePointer<UInt8>?, _ n: Int, _ sig: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ed25519_verify") private func ck_ed_verify(_ pub: UnsafePointer<UInt8>?, _ m: UnsafePointer<UInt8>?, _ n: Int, _ sig: UnsafePointer<UInt8>?) -> Int32
@_silgen_name("mos_ck_ec_generate") private func ck_ec_gen(_ bits: Int32, _ priv: UnsafeMutablePointer<UInt8>, _ pub: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ec_public") private func ck_ec_pub(_ bits: Int32, _ priv: UnsafePointer<UInt8>?, _ pub: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ec_sign") private func ck_ec_sign(_ bits: Int32, _ priv: UnsafePointer<UInt8>?, _ d: UnsafePointer<UInt8>?, _ dl: Int, _ rs: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ec_verify") private func ck_ec_verify(_ bits: Int32, _ pub: UnsafePointer<UInt8>?, _ d: UnsafePointer<UInt8>?, _ dl: Int, _ rs: UnsafePointer<UInt8>?) -> Int32
@_silgen_name("mos_ck_ec_shared") private func ck_ec_shared(_ bits: Int32, _ priv: UnsafePointer<UInt8>?, _ peer: UnsafePointer<UInt8>?, _ out: UnsafeMutablePointer<UInt8>) -> Int32

private func bytes<D: DataProtocol>(_ d: D) -> [UInt8] { var a = [UInt8](); a.reserveCapacity(d.count); for r in d.regions { r.withUnsafeBytes { a.append(contentsOf: $0) } }; return a }
private func bytes<D: ContiguousBytes>(contiguous d: D) -> [UInt8] { d.withUnsafeBytes { Array($0) } }
private func randomBytes(_ n: Int) -> [UInt8] { var a = [UInt8](repeating: 0, count: n); a.withUnsafeMutableBytes { ck_random($0.baseAddress!, n) }; return a }
private func hexString(_ b: [UInt8]) -> String {
    let h: [Character] = Array("0123456789abcdef")
    var s = ""
    for x in b { s.append(h[Int(x >> 4)]); s.append(h[Int(x & 15)]) }
    return s
}
private func safeCompare(_ a: [UInt8], _ b: [UInt8]) -> Bool {
    a.count == b.count && a.withUnsafeBytes { pa in b.withUnsafeBytes { pb in ck_equal(pa.baseAddress, pb.baseAddress, a.count) != 0 } }
}

// MARK: - erros

public enum CryptoKitError: Error {
    case underlyingCoreCryptoError(error: Int32)
    case incorrectKeySize
    case incorrectParameterSize
    case authenticationFailure
    case wrapFailure
    case unwrapFailure
    case invalidParameter
}
public enum CryptoKitASN1Error: Error { case invalidFieldIdentifier, unexpectedFieldType, invalidObjectIdentifier, invalidASN1Object, invalidASN1IntegerEncoding, truncatedASN1Field, unsupportedFieldLength, invalidPEMDocument }

// MARK: - resumos (digests)

public protocol Digest: Hashable, ContiguousBytes, CustomStringConvertible, Sequence where Element == UInt8 {
    static var byteCount: Int { get }
}
extension Digest {
    public func makeIterator() -> Array<UInt8>.Iterator { withUnsafeBytes { Array($0) }.makeIterator() }
    public static func == <D: DataProtocol>(lhs: Self, rhs: D) -> Bool { safeCompare(bytes(contiguous: lhs), bytes(rhs)) }
}
public protocol _MOSDigestStorage { init(_mos bytes: [UInt8]) }

public struct SHA256Digest: Digest, _MOSDigestStorage {
    let b: [UInt8]
    public init(_mos bytes: [UInt8]) { b = bytes }
    public static var byteCount: Int { 32 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SHA256 digest: " + hexString(b) }
}
public struct SHA384Digest: Digest, _MOSDigestStorage {
    let b: [UInt8]
    public init(_mos bytes: [UInt8]) { b = bytes }
    public static var byteCount: Int { 48 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SHA384 digest: " + hexString(b) }
}
public struct SHA512Digest: Digest, _MOSDigestStorage {
    let b: [UInt8]
    public init(_mos bytes: [UInt8]) { b = bytes }
    public static var byteCount: Int { 64 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SHA512 digest: " + hexString(b) }
}

// MARK: - funções de hash

public protocol HashFunction {
    associatedtype Digest: CryptoKit.Digest
    static var blockByteCount: Int { get }
    init()
    mutating func update(bufferPointer: UnsafeRawBufferPointer)
    func finalize() -> Self.Digest
}
extension HashFunction {
    public static var byteCount: Int { Digest.byteCount }
    public mutating func update<D: DataProtocol>(data: D) { for r in data.regions { r.withUnsafeBytes { update(bufferPointer: $0) } } }
    public static func hash<D: DataProtocol>(data: D) -> Self.Digest { var h = Self(); h.update(data: data); return h.finalize() }
}
/// estado C de um hash; copiado antes de mudar quando o valor foi duplicado
public final class _MOSHashBox {
    let alg: Int32
    let ctx: UnsafeMutableRawPointer
    public init(alg: Int32) { self.alg = alg; ctx = ck_hash_new(alg)! }
    init(copying o: _MOSHashBox) { alg = o.alg; ctx = ck_hash_copy(o.ctx)! }
    deinit { ck_hash_free(ctx) }
    func final(_ n: Int) -> [UInt8] { var o = [UInt8](repeating: 0, count: n); o.withUnsafeMutableBufferPointer { ck_hash_final(ctx, $0.baseAddress!) }; return o }
}
public protocol _MOSHashFunction: HashFunction where Digest: _MOSDigestStorage {
    static var _mosAlg: Int32 { get }
    var _box: _MOSHashBox { get set }
}
extension _MOSHashFunction {
    public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
        if !isKnownUniquelyReferenced(&_box) { _box = _MOSHashBox(copying: _box) }
        ck_hash_update(_box.ctx, bufferPointer.baseAddress, bufferPointer.count)
    }
    public func finalize() -> Digest { Digest(_mos: _box.final(Digest.byteCount)) }
}
public struct SHA256: _MOSHashFunction {
    public typealias Digest = SHA256Digest
    public static var _mosAlg: Int32 { 3 }
    public var _box = _MOSHashBox(alg: 3)
    public static var blockByteCount: Int { 64 }
    public init() {}
}
public struct SHA384: _MOSHashFunction {
    public typealias Digest = SHA384Digest
    public static var _mosAlg: Int32 { 4 }
    public var _box = _MOSHashBox(alg: 4)
    public static var blockByteCount: Int { 128 }
    public init() {}
}
public struct SHA512: _MOSHashFunction {
    public typealias Digest = SHA512Digest
    public static var _mosAlg: Int32 { 5 }
    public var _box = _MOSHashBox(alg: 5)
    public static var blockByteCount: Int { 128 }
    public init() {}
}
public enum Insecure {
    public struct MD5Digest: CryptoKit.Digest, _MOSDigestStorage {
        let b: [UInt8]
        public init(_mos bytes: [UInt8]) { b = bytes }
        public static var byteCount: Int { 16 }
        public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
        public var description: String { "MD5 digest: " + hexString(b) }
    }
    public struct SHA1Digest: CryptoKit.Digest, _MOSDigestStorage {
        let b: [UInt8]
        public init(_mos bytes: [UInt8]) { b = bytes }
        public static var byteCount: Int { 20 }
        public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
        public var description: String { "SHA1 digest: " + hexString(b) }
    }
    public struct MD5: _MOSHashFunction {
        public typealias Digest = MD5Digest
        public static var _mosAlg: Int32 { 1 }
        public var _box = _MOSHashBox(alg: 1)
        public static var blockByteCount: Int { 64 }
        public init() {}
    }
    public struct SHA1: _MOSHashFunction {
        public typealias Digest = SHA1Digest
        public static var _mosAlg: Int32 { 2 }
        public var _box = _MOSHashBox(alg: 2)
        public static var blockByteCount: Int { 64 }
        public init() {}
    }
}

// MARK: - chaves simétricas

public struct SymmetricKeySize: Sendable {
    public let bitCount: Int
    public init(bitCount: Int) { precondition(bitCount > 0 && bitCount % 8 == 0); self.bitCount = bitCount }
    public static var bits128: SymmetricKeySize { .init(bitCount: 128) }
    public static var bits192: SymmetricKeySize { .init(bitCount: 192) }
    public static var bits256: SymmetricKeySize { .init(bitCount: 256) }
}
public struct SymmetricKey: ContiguousBytes, Sendable {
    let b: [UInt8]
    public init<D: ContiguousBytes>(data: D) { b = bytes(contiguous: data) }
    public init(size: SymmetricKeySize) { b = randomBytes(size.bitCount / 8) }
    public var bitCount: Int { b.count * 8 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
}
extension SymmetricKey: Equatable { public static func == (a: SymmetricKey, b: SymmetricKey) -> Bool { safeCompare(a.b, b.b) } }

// MARK: - HMAC

public protocol MessageAuthenticationCode: Hashable, ContiguousBytes, CustomStringConvertible, Sequence where Element == UInt8 {
    var byteCount: Int { get }
}
extension MessageAuthenticationCode {
    public func makeIterator() -> Array<UInt8>.Iterator { withUnsafeBytes { Array($0) }.makeIterator() }
    public static func == <D: DataProtocol>(lhs: Self, rhs: D) -> Bool { safeCompare(bytes(contiguous: lhs), bytes(rhs)) }
}
public struct HashedAuthenticationCode<H: HashFunction>: MessageAuthenticationCode {
    let b: [UInt8]
    public var byteCount: Int { b.count }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "HMAC with \(H.self): " + hexString(b) }
    public static func == (a: Self, b: Self) -> Bool { safeCompare(a.b, b.b) }
    public func hash(into h: inout Hasher) { h.combine(b) }
}
public struct HMAC<H: HashFunction> {
    public typealias MAC = HashedAuthenticationCode<H>
    private var inner: H, outerKey: [UInt8]
    public init(key: SymmetricKey) {
        let bs = H.blockByteCount
        var k = key.b
        if k.count > bs { k = H.hash(data: k).withUnsafeBytes { Array($0) } }
        k += [UInt8](repeating: 0, count: bs - k.count)
        inner = H()
        inner.update(data: k.map { $0 ^ 0x36 })
        outerKey = k.map { $0 ^ 0x5c }
    }
    public mutating func update<D: DataProtocol>(data: D) { inner.update(data: data) }
    public func finalize() -> MAC {
        var o = H()
        o.update(data: outerKey)
        o.update(data: inner.finalize().withUnsafeBytes { Array($0) })
        return MAC(b: o.finalize().withUnsafeBytes { Array($0) })
    }
    public static func authenticationCode<D: DataProtocol>(for data: D, using key: SymmetricKey) -> MAC {
        var h = HMAC(key: key); h.update(data: data); return h.finalize()
    }
    public static func isValidAuthenticationCode<D: DataProtocol>(_ mac: MAC, authenticating data: D, using key: SymmetricKey) -> Bool {
        authenticationCode(for: data, using: key) == mac
    }
    public static func isValidAuthenticationCode<C: ContiguousBytes, D: DataProtocol>(_ mac: C, authenticating data: D, using key: SymmetricKey) -> Bool {
        safeCompare(authenticationCode(for: data, using: key).b, bytes(contiguous: mac))
    }
}

// MARK: - HKDF

public struct HKDF<H: HashFunction> {
    public static func deriveKey<Salt: DataProtocol, Info: DataProtocol>(inputKeyMaterial: SymmetricKey, salt: Salt, info: Info, outputByteCount: Int) -> SymmetricKey {
        let prk = extract(inputKeyMaterial: inputKeyMaterial, salt: salt)
        return expand(pseudoRandomKey: prk, info: info, outputByteCount: outputByteCount)
    }
    public static func deriveKey<Info: DataProtocol>(inputKeyMaterial: SymmetricKey, info: Info, outputByteCount: Int) -> SymmetricKey {
        deriveKey(inputKeyMaterial: inputKeyMaterial, salt: [UInt8](), info: info, outputByteCount: outputByteCount)
    }
    public static func deriveKey(inputKeyMaterial: SymmetricKey, outputByteCount: Int) -> SymmetricKey {
        deriveKey(inputKeyMaterial: inputKeyMaterial, salt: [UInt8](), info: [UInt8](), outputByteCount: outputByteCount)
    }
    public static func extract<Salt: DataProtocol>(inputKeyMaterial: SymmetricKey, salt: Salt?) -> HashedAuthenticationCode<H> {
        var s = salt.map { bytes($0) } ?? []
        if s.isEmpty { s = [UInt8](repeating: 0, count: H.Digest.byteCount) }
        return HMAC<H>.authenticationCode(for: inputKeyMaterial.b, using: SymmetricKey(data: s))
    }
    public static func expand<PRK: ContiguousBytes, Info: DataProtocol>(pseudoRandomKey prk: PRK, info: Info?, outputByteCount: Int) -> SymmetricKey {
        let key = SymmetricKey(data: bytes(contiguous: prk)), inf = info.map { bytes($0) } ?? []
        var out = [UInt8](), t = [UInt8](), c: UInt8 = 1
        while out.count < outputByteCount {
            var h = HMAC<H>(key: key)
            h.update(data: t); h.update(data: inf); h.update(data: [c])
            t = h.finalize().b
            out += t
            c &+= 1
        }
        return SymmetricKey(data: Array(out.prefix(outputByteCount)))
    }
}

// MARK: - AEADs

public enum AES {
    public enum GCM {
        public struct Nonce: ContiguousBytes, Sequence, Sendable {
            let b: [UInt8]
            public init() { b = randomBytes(12) }
            public init<D: DataProtocol>(data: D) throws {
                let x = bytes(data)
                guard x.count >= 12 else { throw CryptoKitError.incorrectParameterSize }
                b = x
            }
            public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
            public func makeIterator() -> Array<UInt8>.Iterator { b.makeIterator() }
        }
        public struct SealedBox: Sendable {
            public let nonce: Nonce
            public let ciphertext: Data
            public let tag: Data
            public var combined: Data? { nonce.b.count == 12 ? Data(nonce.b) + ciphertext + tag : nil }
            public init<D: DataProtocol>(combined: D) throws {
                let x = bytes(combined)
                guard x.count >= 28 else { throw CryptoKitError.incorrectParameterSize }
                nonce = try Nonce(data: x[0..<12]); ciphertext = Data(x[12..<(x.count - 16)]); tag = Data(x[(x.count - 16)...])
            }
            public init<C: DataProtocol, T: DataProtocol>(nonce: Nonce, ciphertext: C, tag: T) throws {
                guard tag.count == 16 else { throw CryptoKitError.incorrectParameterSize }
                self.nonce = nonce; self.ciphertext = Data(bytes(ciphertext)); self.tag = Data(bytes(tag))
            }
            init(nonce: Nonce, ct: [UInt8], tag: [UInt8]) { self.nonce = nonce; ciphertext = Data(ct); self.tag = Data(tag) }
        }
        public static func seal<Plaintext: DataProtocol, AuthenticatedData: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil, authenticating ad: AuthenticatedData) throws -> SealedBox {
            let n = nonce ?? Nonce()
            guard n.b.count == 12 else { throw CryptoKitError.incorrectParameterSize }
            guard key.b.count == 16 || key.b.count == 24 || key.b.count == 32 else { throw CryptoKitError.incorrectKeySize }
            let m = bytes(message), a = bytes(ad)
            var o = [UInt8](repeating: 0, count: m.count), t = [UInt8](repeating: 0, count: 16)
            let r = key.b.withUnsafeBufferPointer { k in n.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in m.withUnsafeBufferPointer { mm in
                o.withUnsafeMutableBufferPointer { oo in t.withUnsafeMutableBufferPointer { tt in
                    ck_gcm_seal(k.baseAddress, k.count, nn.baseAddress, aa.baseAddress, aa.count, mm.baseAddress, mm.count, oo.baseAddress, tt.baseAddress!) } } } } } }
            guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
            return SealedBox(nonce: n, ct: o, tag: t)
        }
        public static func seal<Plaintext: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil) throws -> SealedBox {
            try seal(message, using: key, nonce: nonce, authenticating: [UInt8]())
        }
        public static func open<AuthenticatedData: DataProtocol>(_ box: SealedBox, using key: SymmetricKey, authenticating ad: AuthenticatedData) throws -> Data {
            guard key.b.count == 16 || key.b.count == 24 || key.b.count == 32 else { throw CryptoKitError.incorrectKeySize }
            let c = bytes(box.ciphertext), t = bytes(box.tag), a = bytes(ad)
            var o = [UInt8](repeating: 0, count: c.count)
            let r = key.b.withUnsafeBufferPointer { k in box.nonce.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in c.withUnsafeBufferPointer { cc in
                t.withUnsafeBufferPointer { tt in o.withUnsafeMutableBufferPointer { oo in
                    ck_gcm_open(k.baseAddress, k.count, nn.baseAddress, aa.baseAddress, aa.count, cc.baseAddress, cc.count, tt.baseAddress, oo.baseAddress) } } } } } }
            guard r == 0 else { throw CryptoKitError.authenticationFailure }
            return Data(o)
        }
        public static func open(_ box: SealedBox, using key: SymmetricKey) throws -> Data { try open(box, using: key, authenticating: [UInt8]()) }
    }
    public enum KeyWrap {
        public static func wrap(_ keyToWrap: SymmetricKey, using kek: SymmetricKey) throws -> Data {
            var o = [UInt8](repeating: 0, count: keyToWrap.b.count + 8)
            let r = kek.b.withUnsafeBufferPointer { k in keyToWrap.b.withUnsafeBufferPointer { w in o.withUnsafeMutableBufferPointer { ck_wrap(k.baseAddress, k.count, w.baseAddress, w.count, $0.baseAddress!) } } }
            guard r == 0 else { throw CryptoKitError.wrapFailure }
            return Data(o)
        }
        public static func unwrap<WrappedKey: DataProtocol>(_ wrapped: WrappedKey, using kek: SymmetricKey) throws -> SymmetricKey {
            let w = bytes(wrapped)
            guard w.count >= 24 else { throw CryptoKitError.unwrapFailure }
            var o = [UInt8](repeating: 0, count: w.count - 8)
            let r = kek.b.withUnsafeBufferPointer { k in w.withUnsafeBufferPointer { ww in o.withUnsafeMutableBufferPointer { ck_unwrap(k.baseAddress, k.count, ww.baseAddress, ww.count, $0.baseAddress!) } } }
            guard r == 0 else { throw CryptoKitError.unwrapFailure }
            return SymmetricKey(data: o)
        }
    }
}

public enum ChaChaPoly {
    public struct Nonce: ContiguousBytes, Sequence, Sendable {
        let b: [UInt8]
        public init() { b = randomBytes(12) }
        public init<D: DataProtocol>(data: D) throws {
            let x = bytes(data)
            guard x.count == 12 else { throw CryptoKitError.incorrectParameterSize }
            b = x
        }
        public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
        public func makeIterator() -> Array<UInt8>.Iterator { b.makeIterator() }
    }
    public struct SealedBox: Sendable {
        public let nonce: Nonce
        public let ciphertext: Data
        public let tag: Data
        public var combined: Data { Data(nonce.b) + ciphertext + tag }
        public init<D: DataProtocol>(combined: D) throws {
            let x = bytes(combined)
            guard x.count >= 28 else { throw CryptoKitError.incorrectParameterSize }
            nonce = try Nonce(data: x[0..<12]); ciphertext = Data(x[12..<(x.count - 16)]); tag = Data(x[(x.count - 16)...])
        }
        public init<C: DataProtocol, T: DataProtocol>(nonce: Nonce, ciphertext: C, tag: T) throws {
            guard tag.count == 16 else { throw CryptoKitError.incorrectParameterSize }
            self.nonce = nonce; self.ciphertext = Data(bytes(ciphertext)); self.tag = Data(bytes(tag))
        }
        init(nonce: Nonce, ct: [UInt8], tag: [UInt8]) { self.nonce = nonce; ciphertext = Data(ct); self.tag = Data(tag) }
    }
    public static func seal<Plaintext: DataProtocol, AuthenticatedData: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil, authenticating ad: AuthenticatedData) throws -> SealedBox {
        guard key.b.count == 32 else { throw CryptoKitError.incorrectKeySize }
        let n = nonce ?? Nonce(), m = bytes(message), a = bytes(ad)
        var o = [UInt8](repeating: 0, count: m.count), t = [UInt8](repeating: 0, count: 16)
        _ = key.b.withUnsafeBufferPointer { k in n.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in m.withUnsafeBufferPointer { mm in
            o.withUnsafeMutableBufferPointer { oo in t.withUnsafeMutableBufferPointer { tt in
                ck_cc_seal(k.baseAddress, nn.baseAddress, aa.baseAddress, aa.count, mm.baseAddress, mm.count, oo.baseAddress, tt.baseAddress!) } } } } } }
        return SealedBox(nonce: n, ct: o, tag: t)
    }
    public static func seal<Plaintext: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil) throws -> SealedBox {
        try seal(message, using: key, nonce: nonce, authenticating: [UInt8]())
    }
    public static func open<AuthenticatedData: DataProtocol>(_ box: SealedBox, using key: SymmetricKey, authenticating ad: AuthenticatedData) throws -> Data {
        guard key.b.count == 32 else { throw CryptoKitError.incorrectKeySize }
        let c = bytes(box.ciphertext), t = bytes(box.tag), a = bytes(ad)
        var o = [UInt8](repeating: 0, count: c.count)
        let r = key.b.withUnsafeBufferPointer { k in box.nonce.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in c.withUnsafeBufferPointer { cc in
            t.withUnsafeBufferPointer { tt in o.withUnsafeMutableBufferPointer { oo in
                ck_cc_open(k.baseAddress, nn.baseAddress, aa.baseAddress, aa.count, cc.baseAddress, cc.count, tt.baseAddress, oo.baseAddress) } } } } } }
        guard r == 0 else { throw CryptoKitError.authenticationFailure }
        return Data(o)
    }
    public static func open(_ box: SealedBox, using key: SymmetricKey) throws -> Data { try open(box, using: key, authenticating: [UInt8]()) }
}

// MARK: - segredo compartilhado

public struct SharedSecret: ContiguousBytes, CustomStringConvertible, Sendable {
    let b: [UInt8]
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SharedSecret: " + hexString(b) }
    public func hkdfDerivedSymmetricKey<H: HashFunction, Salt: DataProtocol, SI: DataProtocol>(using hash: H.Type, salt: Salt, sharedInfo: SI, outputByteCount: Int) -> SymmetricKey {
        HKDF<H>.deriveKey(inputKeyMaterial: SymmetricKey(data: b), salt: salt, info: sharedInfo, outputByteCount: outputByteCount)
    }
    /// ANSI X9.63: K = H(Z || contador || sharedInfo) ...
    public func x963DerivedSymmetricKey<H: HashFunction, SI: DataProtocol>(using hash: H.Type, sharedInfo: SI, outputByteCount: Int) -> SymmetricKey {
        let info = bytes(sharedInfo)
        var out = [UInt8](), c: UInt32 = 1
        while out.count < outputByteCount {
            var h = H()
            h.update(data: b)
            h.update(data: [UInt8(c >> 24), UInt8((c >> 16) & 255), UInt8((c >> 8) & 255), UInt8(c & 255)])
            h.update(data: info)
            out += h.finalize().withUnsafeBytes { Array($0) }
            c += 1
        }
        return SymmetricKey(data: Array(out.prefix(outputByteCount)))
    }
}
extension SharedSecret: Equatable { public static func == (a: SharedSecret, b: SharedSecret) -> Bool { safeCompare(a.b, b.b) } }

// MARK: - Curve25519

public enum Curve25519 {
    public enum KeyAgreement {
        public struct PublicKey: Sendable {
            let b: [UInt8]
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 32 else { throw CryptoKitError.incorrectKeySize }
                b = x
            }
            public var rawRepresentation: Data { Data(b) }
        }
        public struct PrivateKey: Sendable {
            let b: [UInt8]
            public init() { b = randomBytes(32) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 32 else { throw CryptoKitError.incorrectKeySize }
                b = x
            }
            public var rawRepresentation: Data { Data(b) }
            public var publicKey: PublicKey {
                var p = [UInt8](repeating: 0, count: 32)
                _ = b.withUnsafeBufferPointer { k in p.withUnsafeMutableBufferPointer { ck_x_pub(k.baseAddress, $0.baseAddress!) } }
                return try! PublicKey(rawRepresentation: p)
            }
            public func sharedSecretFromKeyAgreement(with peer: PublicKey) throws -> SharedSecret {
                var s = [UInt8](repeating: 0, count: 32)
                let r = b.withUnsafeBufferPointer { k in peer.b.withUnsafeBufferPointer { pp in s.withUnsafeMutableBufferPointer { ck_x(k.baseAddress, pp.baseAddress, $0.baseAddress!) } } }
                guard r == 0 else { throw CryptoKitError.invalidParameter }
                return SharedSecret(b: s)
            }
        }
    }
    public enum Signing {
        public struct PublicKey: Sendable {
            let b: [UInt8]
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 32 else { throw CryptoKitError.incorrectKeySize }
                b = x
            }
            public var rawRepresentation: Data { Data(b) }
            public func isValidSignature<S: DataProtocol, D: DataProtocol>(_ signature: S, for data: D) -> Bool {
                let s = bytes(signature), m = bytes(data)
                guard s.count == 64 else { return false }
                return b.withUnsafeBufferPointer { k in m.withUnsafeBufferPointer { mm in s.withUnsafeBufferPointer { ss in ck_ed_verify(k.baseAddress, mm.baseAddress, mm.count, ss.baseAddress) == 0 } } }
            }
        }
        public struct PrivateKey: Sendable {
            let b: [UInt8]
            public init() { b = randomBytes(32) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 32 else { throw CryptoKitError.incorrectKeySize }
                b = x
            }
            public var rawRepresentation: Data { Data(b) }
            public var publicKey: PublicKey {
                var p = [UInt8](repeating: 0, count: 32)
                _ = b.withUnsafeBufferPointer { k in p.withUnsafeMutableBufferPointer { ck_ed_pub(k.baseAddress, $0.baseAddress!) } }
                return try! PublicKey(rawRepresentation: p)
            }
            public func signature<D: DataProtocol>(for data: D) throws -> Data {
                let m = bytes(data)
                var s = [UInt8](repeating: 0, count: 64)
                _ = b.withUnsafeBufferPointer { k in m.withUnsafeBufferPointer { mm in s.withUnsafeMutableBufferPointer { ck_ed_sign(k.baseAddress, mm.baseAddress, mm.count, $0.baseAddress!) } } }
                return Data(s)
            }
        }
    }
}

// MARK: - NIST P-256 / P-384 / P-521 (implementação comum)

/// DER de SubjectPublicKeyInfo (id-ecPublicKey + curva) antes do ponto 04 || X || Y
private func spkiPrefix(_ bits: Int32) -> [UInt8] {
    switch bits {
    case 256: return [0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00]
    case 384: return [0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00]
    default: return [0x30, 0x81, 0x9b, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23, 0x03, 0x81, 0x86, 0x00]
    }
}
private func pem(_ der: [UInt8], _ label: String) -> String {
    let b64 = Data(der).base64EncodedString()
    var lines = [String](), i = b64.startIndex
    while i < b64.endIndex { let j = b64.index(i, offsetBy: 64, limitedBy: b64.endIndex) ?? b64.endIndex; lines.append(String(b64[i..<j])); i = j }
    return "-----BEGIN \(label)-----\n" + lines.joined(separator: "\n") + "\n-----END \(label)-----"
}
private func unpem(_ s: String) throws -> [UInt8] {
    let body = s.split(separator: "\n").filter { !$0.hasPrefix("-----") }.joined()
    guard let d = Data(base64Encoded: body) else { throw CryptoKitASN1Error.invalidPEMDocument }
    return [UInt8](d)
}
/// ECDSA r || s ↔ DER SEQUENCE { INTEGER r, INTEGER s }
private func derInt(_ v: ArraySlice<UInt8>) -> [UInt8] {
    var x = Array(v.drop { $0 == 0 })
    if x.isEmpty { x = [0] }
    if x[0] & 0x80 != 0 { x.insert(0, at: 0) }
    return [0x02] + derLen(x.count) + x
}
private func derLen(_ n: Int) -> [UInt8] { n < 128 ? [UInt8(n)] : n < 256 ? [0x81, UInt8(n)] : [0x82, UInt8(n >> 8), UInt8(n & 255)] }
private func derRead(_ d: [UInt8], _ i: inout Int) throws -> (UInt8, ArraySlice<UInt8>) {
    guard i + 2 <= d.count else { throw CryptoKitASN1Error.truncatedASN1Field }
    let tag = d[i]; var n = Int(d[i + 1]); i += 2
    if n & 0x80 != 0 {
        let k = n & 0x7f
        guard k <= 2, i + k <= d.count else { throw CryptoKitASN1Error.unsupportedFieldLength }
        n = 0; for _ in 0..<k { n = n << 8 | Int(d[i]); i += 1 }
    }
    guard i + n <= d.count else { throw CryptoKitASN1Error.truncatedASN1Field }
    defer { i += n }
    return (tag, d[i..<(i + n)])
}
private func fit(_ v: ArraySlice<UInt8>, _ size: Int) throws -> [UInt8] {
    let x = Array(v.drop { $0 == 0 })
    guard x.count <= size else { throw CryptoKitASN1Error.invalidASN1IntegerEncoding }
    return [UInt8](repeating: 0, count: size - x.count) + x
}

public struct _MOSECKeys {
    let bits: Int32
    var size: Int { bits == 256 ? 32 : bits == 384 ? 48 : 66 }
    func generate() -> ([UInt8], [UInt8]) {
        var d = [UInt8](repeating: 0, count: size), q = [UInt8](repeating: 0, count: 1 + 2 * size)
        _ = d.withUnsafeMutableBufferPointer { dd in q.withUnsafeMutableBufferPointer { ck_ec_gen(bits, dd.baseAddress!, $0.baseAddress!) } }
        return (d, q)
    }
    func publicFrom(_ d: [UInt8]) throws -> [UInt8] {
        guard d.count == size else { throw CryptoKitError.incorrectKeySize }
        var q = [UInt8](repeating: 0, count: 1 + 2 * size)
        let r = d.withUnsafeBufferPointer { dd in q.withUnsafeMutableBufferPointer { ck_ec_pub(bits, dd.baseAddress, $0.baseAddress!) } }
        guard r == 0 else { throw CryptoKitError.invalidParameter }
        return q
    }
    func checkPoint(_ q: [UInt8]) throws -> [UInt8] {
        guard q.count == 1 + 2 * size, q[0] == 4 else { throw CryptoKitError.incorrectKeySize }
        /* o ponto é validado (na curva) na primeira verificação/ECDH; aqui conferimos o formato */
        return q
    }
    func sign(_ d: [UInt8], digest: [UInt8]) throws -> [UInt8] {
        var rs = [UInt8](repeating: 0, count: 2 * size)
        let r = d.withUnsafeBufferPointer { dd in digest.withUnsafeBufferPointer { g in rs.withUnsafeMutableBufferPointer { ck_ec_sign(bits, dd.baseAddress, g.baseAddress, g.count, $0.baseAddress!) } } }
        guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
        return rs
    }
    func verify(_ q: [UInt8], digest: [UInt8], rs: [UInt8]) -> Bool {
        guard rs.count == 2 * size else { return false }
        return q.withUnsafeBufferPointer { qq in digest.withUnsafeBufferPointer { g in rs.withUnsafeBufferPointer { ck_ec_verify(bits, qq.baseAddress, g.baseAddress, g.count, $0.baseAddress) == 0 } } }
    }
    func shared(_ d: [UInt8], _ q: [UInt8]) throws -> SharedSecret {
        var s = [UInt8](repeating: 0, count: size)
        let r = d.withUnsafeBufferPointer { dd in q.withUnsafeBufferPointer { qq in s.withUnsafeMutableBufferPointer { ck_ec_shared(bits, dd.baseAddress, qq.baseAddress, $0.baseAddress!) } } }
        guard r == 0 else { throw CryptoKitError.invalidParameter }
        return SharedSecret(b: s)
    }
    func digest<D: DataProtocol>(_ data: D) -> [UInt8] {
        switch bits {
        case 256: return SHA256.hash(data: data).withUnsafeBytes { Array($0) }
        case 384: return SHA384.hash(data: data).withUnsafeBytes { Array($0) }
        default: return SHA512.hash(data: data).withUnsafeBytes { Array($0) }
        }
    }
    func spki(_ q: [UInt8]) -> [UInt8] { spkiPrefix(bits) + q }
    func fromSPKI(_ der: [UInt8]) throws -> [UInt8] {
        let p = spkiPrefix(bits)
        guard der.count == p.count + 1 + 2 * size, Array(der.prefix(p.count)) == p else { throw CryptoKitASN1Error.invalidObjectIdentifier }
        return try checkPoint(Array(der.suffix(1 + 2 * size)))
    }
    func compressed(_ q: [UInt8]) -> [UInt8] { [UInt8(2 + (q[q.count - 1] & 1))] + Array(q[1...size]) }
}

/// P256/P384/P521: o mesmo código, com o tamanho da curva no tipo
public protocol _MOSCurve { static var _keys: _MOSECKeys { get } }
public struct _MOSECDSASignature<C: _MOSCurve>: ContiguousBytes, Sendable {
    let rs: [UInt8]
    public init<D: DataProtocol>(rawRepresentation data: D) throws {
        let x = bytes(data)
        guard x.count == 2 * C._keys.size else { throw CryptoKitError.incorrectParameterSize }
        rs = x
    }
    public init<D: DataProtocol>(derRepresentation data: D) throws {
        let d = bytes(data); var i = 0
        let (t, seq) = try derRead(d, &i)
        guard t == 0x30 else { throw CryptoKitASN1Error.unexpectedFieldType }
        let s = Array(seq); var j = 0
        let (t1, r) = try derRead(s, &j), (t2, ss) = try derRead(s, &j)
        guard t1 == 2, t2 == 2 else { throw CryptoKitASN1Error.unexpectedFieldType }
        rs = try fit(r, C._keys.size) + fit(ss, C._keys.size)
    }
    init(rs: [UInt8]) { self.rs = rs }
    public var rawRepresentation: Data { Data(rs) }
    public var derRepresentation: Data {
        let n = C._keys.size
        let body = derInt(rs[0..<n]) + derInt(rs[n...])
        return Data([0x30] + derLen(body.count) + body)
    }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try rs.withUnsafeBytes(body) }
}
public struct _MOSECPublicKey<C: _MOSCurve>: Sendable {
    let q: [UInt8]
    init(q: [UInt8]) { self.q = q }
    public init<D: ContiguousBytes>(rawRepresentation data: D) throws { q = try C._keys.checkPoint([4] + bytes(contiguous: data)) }
    public init<D: ContiguousBytes>(x963Representation data: D) throws { q = try C._keys.checkPoint(bytes(contiguous: data)) }
    public init<D: DataProtocol>(derRepresentation data: D) throws { q = try C._keys.fromSPKI(bytes(data)) }
    public init(pemRepresentation s: String) throws { q = try C._keys.fromSPKI(try unpem(s)) }
    public var rawRepresentation: Data { Data(q.dropFirst()) }
    public var x963Representation: Data { Data(q) }
    public var compressedRepresentation: Data { Data(C._keys.compressed(q)) }
    public var derRepresentation: Data { Data(C._keys.spki(q)) }
    public var pemRepresentation: String { pem(C._keys.spki(q), "PUBLIC KEY") }
    public func isValidSignature<D: DataProtocol>(_ signature: _MOSECDSASignature<C>, for data: D) -> Bool {
        C._keys.verify(q, digest: C._keys.digest(data), rs: signature.rs)
    }
    public func isValidSignature<D: Digest>(_ signature: _MOSECDSASignature<C>, for digest: D) -> Bool {
        C._keys.verify(q, digest: bytes(contiguous: digest), rs: signature.rs)
    }
}
public struct _MOSECPrivateKey<C: _MOSCurve>: Sendable {
    let d: [UInt8], q: [UInt8]
    public init(compactRepresentable: Bool = true) { (d, q) = C._keys.generate() }
    public init<D: ContiguousBytes>(rawRepresentation data: D) throws { d = bytes(contiguous: data); q = try C._keys.publicFrom(d) }
    public init<D: ContiguousBytes>(x963Representation data: D) throws {
        let x = bytes(contiguous: data), n = C._keys.size
        guard x.count == 1 + 3 * n else { throw CryptoKitError.incorrectKeySize }
        d = Array(x.suffix(n)); q = try C._keys.publicFrom(d)
    }
    public var rawRepresentation: Data { Data(d) }
    public var x963Representation: Data { Data(q + d) }
    public var publicKey: _MOSECPublicKey<C> { _MOSECPublicKey(q: q) }
    public func signature<D: DataProtocol>(for data: D) throws -> _MOSECDSASignature<C> {
        _MOSECDSASignature(rs: try C._keys.sign(d, digest: C._keys.digest(data)))
    }
    public func signature<D: Digest>(for digest: D) throws -> _MOSECDSASignature<C> {
        _MOSECDSASignature(rs: try C._keys.sign(d, digest: bytes(contiguous: digest)))
    }
    public func sharedSecretFromKeyAgreement(with peer: _MOSECPublicKey<C>) throws -> SharedSecret { try C._keys.shared(d, peer.q) }
}

public enum P256: _MOSCurve {
    public static var _keys: _MOSECKeys { _MOSECKeys(bits: 256) }
    public enum Signing {
        public typealias PrivateKey = _MOSECPrivateKey<P256>
        public typealias PublicKey = _MOSECPublicKey<P256>
        public typealias ECDSASignature = _MOSECDSASignature<P256>
    }
    public enum KeyAgreement {
        public typealias PrivateKey = _MOSECPrivateKey<P256>
        public typealias PublicKey = _MOSECPublicKey<P256>
    }
}
public enum P384: _MOSCurve {
    public static var _keys: _MOSECKeys { _MOSECKeys(bits: 384) }
    public enum Signing {
        public typealias PrivateKey = _MOSECPrivateKey<P384>
        public typealias PublicKey = _MOSECPublicKey<P384>
        public typealias ECDSASignature = _MOSECDSASignature<P384>
    }
    public enum KeyAgreement {
        public typealias PrivateKey = _MOSECPrivateKey<P384>
        public typealias PublicKey = _MOSECPublicKey<P384>
    }
}
public enum P521: _MOSCurve {
    public static var _keys: _MOSECKeys { _MOSECKeys(bits: 521) }
    public enum Signing {
        public typealias PrivateKey = _MOSECPrivateKey<P521>
        public typealias PublicKey = _MOSECPublicKey<P521>
        public typealias ECDSASignature = _MOSECDSASignature<P521>
    }
    public enum KeyAgreement {
        public typealias PrivateKey = _MOSECPrivateKey<P521>
        public typealias PublicKey = _MOSECPublicKey<P521>
    }
}

// MARK: - comparação em tempo constante (DataProtocol ↔ Digest/MAC)

extension DataProtocol {
    public static func == <D: Digest>(lhs: Self, rhs: D) -> Bool { rhs == lhs }
}
