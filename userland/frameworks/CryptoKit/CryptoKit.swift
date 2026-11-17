// mOS — CryptoKit.framework, com a API pública e os nomes (mangling) do CryptoKit da Apple, compilado com evolução de
// biblioteca contra a stdlib e o Foundation Swift reais do mOS (tools/build-frameworks-objc.sh, swift-needs: Foundation).
// As primitivas são as do libSystem (userland/libsystem/cryptokit.c sobre kernel/net/crypto.c e pk.c):
// SHA-1/2, MD5, HMAC, HKDF, AES-GCM, AES-KW, ChaCha20-Poly1305, X25519, Ed25519, ECDSA/ECDH P-256/P-384/P-521.
// Não há Secure Enclave no mOS: SecureEnclave.isAvailable é false e criar uma chave lá lança, como num Mac sem o chip.
// HPKE (RFC 9180): modos base e auth com DHKEM X25519 e P-256/384/521.
import Foundation

// MARK: - núcleo C (libSystem)

@_silgen_name("mos_ck_hash_new") private func ck_hash_new(_ alg: Int32) -> UnsafeMutableRawPointer?
@_silgen_name("mos_ck_hash_copy") private func ck_hash_copy(_ h: UnsafeRawPointer) -> UnsafeMutableRawPointer?
@_silgen_name("mos_ck_hash_update") private func ck_hash_update(_ h: UnsafeMutableRawPointer, _ d: UnsafeRawPointer?, _ n: Int)
@_silgen_name("mos_ck_hash_final") private func ck_hash_final(_ h: UnsafeRawPointer, _ out: UnsafeMutablePointer<UInt8>)
@_silgen_name("mos_ck_hash_free") private func ck_hash_free(_ h: UnsafeMutableRawPointer?)
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
@_silgen_name("mos_ck_ec_decompress") private func ck_ec_decompress(_ bits: Int32, _ x: UnsafePointer<UInt8>?, _ ybit: Int32, _ pub: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ec_sign") private func ck_ec_sign(_ bits: Int32, _ priv: UnsafePointer<UInt8>?, _ d: UnsafePointer<UInt8>?, _ dl: Int, _ rs: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_ec_verify") private func ck_ec_verify(_ bits: Int32, _ pub: UnsafePointer<UInt8>?, _ d: UnsafePointer<UInt8>?, _ dl: Int, _ rs: UnsafePointer<UInt8>?) -> Int32
@_silgen_name("mos_ck_ec_shared") private func ck_ec_shared(_ bits: Int32, _ priv: UnsafePointer<UInt8>?, _ peer: UnsafePointer<UInt8>?, _ out: UnsafeMutablePointer<UInt8>) -> Int32

private func bytes<D: DataProtocol>(_ d: D) -> [UInt8] { var a = [UInt8](); a.reserveCapacity(d.count); for r in d.regions { r.withUnsafeBytes { a.append(contentsOf: $0) } }; return a }
private func bytes<D: ContiguousBytes>(contiguous d: D) -> [UInt8] { d.withUnsafeBytes { Array($0) } }
private func randomBytes(_ n: Int) -> [UInt8] { var a = [UInt8](repeating: 0, count: n); if n > 0 { a.withUnsafeMutableBytes { ck_random($0.baseAddress!, n) } }; return a }
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

public enum CryptoKitError: Error, Sendable {
    case underlyingCoreCryptoError(error: Int32)
    case incorrectKeySize
    case incorrectParameterSize
    case authenticationFailure
    case wrapFailure
    case unwrapFailure
    case invalidParameter
}
public enum CryptoKitASN1Error: Error, Sendable {
    case invalidFieldIdentifier
    case unexpectedFieldType
    case invalidObjectIdentifier
    case invalidASN1Object
    case invalidASN1IntegerEncoding
    case truncatedASN1Field
    case unsupportedFieldLength
    case invalidPEMDocument
}

// MARK: - resumos

public protocol Digest: Hashable, ContiguousBytes, CustomStringConvertible, Sequence, Sendable where Element == UInt8 {
    static var byteCount: Int { get }
}
extension Digest {
    public func makeIterator() -> Array<UInt8>.Iterator { withUnsafeBytes { Array($0) }.makeIterator() }
    public static func == (lhs: Self, rhs: Self) -> Bool { safeCompare(bytes(contiguous: lhs), bytes(contiguous: rhs)) }
    public static func == <D: DataProtocol>(lhs: Self, rhs: D) -> Bool { safeCompare(bytes(contiguous: lhs), bytes(rhs)) }
}
public protocol HashFunction: Sendable {
    associatedtype Digest: CryptoKit.Digest
    static var blockByteCount: Int { get }
    init()
    mutating func update(bufferPointer: UnsafeRawBufferPointer)
    func finalize() -> Self.Digest
}
extension HashFunction {
    public static var byteCount: Int { Digest.byteCount }
    public mutating func update<D: DataProtocol>(data: D) {
        for r in data.regions { r.withUnsafeBytes { update(bufferPointer: $0) } }
    }
    public static func hash<D: DataProtocol>(data: D) -> Self.Digest {
        var h = Self()
        h.update(data: data)
        return h.finalize()
    }
}
/* estado do hash do libSystem; o struct copia ao escrever (semântica de valor, como na Apple) */
final class HashState: @unchecked Sendable {
    let alg: Int32
    let ctx: UnsafeMutableRawPointer
    init(alg: Int32) { self.alg = alg; ctx = ck_hash_new(alg)! }
    init(copying o: HashState) { alg = o.alg; ctx = ck_hash_copy(o.ctx)! }
    deinit { ck_hash_free(ctx) }
    func update(_ p: UnsafeRawBufferPointer) { if let b = p.baseAddress, p.count > 0 { ck_hash_update(ctx, b, p.count) } }
    func final(_ n: Int) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: n)
        out.withUnsafeMutableBufferPointer { ck_hash_final(ctx, $0.baseAddress!) }
        return out
    }
}

public struct SHA256Digest: CryptoKit.Digest {
    let b: [UInt8]
    init(_b: [UInt8]) { b = _b }
    public static var byteCount: Int { 32 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SHA256 digest: " + hexString(b) }
    public func hash(into hasher: inout Hasher) { hasher.combine(bytes: UnsafeRawBufferPointer(start: b, count: b.count)) }
}
public struct SHA256: HashFunction {
    public typealias Digest = SHA256Digest
    public static var blockByteCount: Int { 64 }
    public static var byteCount: Int { 32 }
    var st: HashState
    public init() { st = HashState(alg: 3) }
    public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
        if !isKnownUniquelyReferenced(&st) { st = HashState(copying: st) }
        st.update(bufferPointer)
    }
    public func finalize() -> SHA256Digest { SHA256Digest(_b: HashState(copying: st).final(32)) }
}

public struct SHA384Digest: CryptoKit.Digest {
    let b: [UInt8]
    init(_b: [UInt8]) { b = _b }
    public static var byteCount: Int { 48 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SHA384 digest: " + hexString(b) }
    public func hash(into hasher: inout Hasher) { hasher.combine(bytes: UnsafeRawBufferPointer(start: b, count: b.count)) }
}
public struct SHA384: HashFunction {
    public typealias Digest = SHA384Digest
    public static var blockByteCount: Int { 128 }
    public static var byteCount: Int { 48 }
    var st: HashState
    public init() { st = HashState(alg: 4) }
    public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
        if !isKnownUniquelyReferenced(&st) { st = HashState(copying: st) }
        st.update(bufferPointer)
    }
    public func finalize() -> SHA384Digest { SHA384Digest(_b: HashState(copying: st).final(48)) }
}

public struct SHA512Digest: CryptoKit.Digest {
    let b: [UInt8]
    init(_b: [UInt8]) { b = _b }
    public static var byteCount: Int { 64 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SHA512 digest: " + hexString(b) }
    public func hash(into hasher: inout Hasher) { hasher.combine(bytes: UnsafeRawBufferPointer(start: b, count: b.count)) }
}
public struct SHA512: HashFunction {
    public typealias Digest = SHA512Digest
    public static var blockByteCount: Int { 128 }
    public static var byteCount: Int { 64 }
    var st: HashState
    public init() { st = HashState(alg: 5) }
    public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
        if !isKnownUniquelyReferenced(&st) { st = HashState(copying: st) }
        st.update(bufferPointer)
    }
    public func finalize() -> SHA512Digest { SHA512Digest(_b: HashState(copying: st).final(64)) }
}

public enum Insecure {
    public struct MD5Digest: CryptoKit.Digest {
        let b: [UInt8]
        init(_b: [UInt8]) { b = _b }
        public static var byteCount: Int { 16 }
        public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
        public var description: String { "MD5 digest: " + hexString(b) }
        public func hash(into hasher: inout Hasher) { hasher.combine(bytes: UnsafeRawBufferPointer(start: b, count: b.count)) }
    }
    public struct MD5: HashFunction {
        public typealias Digest = MD5Digest
        public static var blockByteCount: Int { 64 }
        public static var byteCount: Int { 16 }
        var st: HashState
        public init() { st = HashState(alg: 1) }
        public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
            if !isKnownUniquelyReferenced(&st) { st = HashState(copying: st) }
            st.update(bufferPointer)
        }
        public func finalize() -> MD5Digest { MD5Digest(_b: HashState(copying: st).final(16)) }
    }
    public struct SHA1Digest: CryptoKit.Digest {
        let b: [UInt8]
        init(_b: [UInt8]) { b = _b }
        public static var byteCount: Int { 20 }
        public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
        public var description: String { "SHA1 digest: " + hexString(b) }
        public func hash(into hasher: inout Hasher) { hasher.combine(bytes: UnsafeRawBufferPointer(start: b, count: b.count)) }
    }
    public struct SHA1: HashFunction {
        public typealias Digest = SHA1Digest
        public static var blockByteCount: Int { 64 }
        public static var byteCount: Int { 20 }
        var st: HashState
        public init() { st = HashState(alg: 2) }
        public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
            if !isKnownUniquelyReferenced(&st) { st = HashState(copying: st) }
            st.update(bufferPointer)
        }
        public func finalize() -> SHA1Digest { SHA1Digest(_b: HashState(copying: st).final(20)) }
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

// MARK: - MAC, HMAC, HKDF

public protocol MessageAuthenticationCode: Hashable, ContiguousBytes, CustomStringConvertible, Sequence where Element == UInt8 {
    var byteCount: Int { get }
}
extension MessageAuthenticationCode {
    public var description: String { "\(Self.self): " + hexString(bytes(contiguous: self)) }
    public func makeIterator() -> Array<UInt8>.Iterator { withUnsafeBytes { Array($0) }.makeIterator() }
    public static func == (lhs: Self, rhs: Self) -> Bool { safeCompare(bytes(contiguous: lhs), bytes(contiguous: rhs)) }
    public static func == <D: DataProtocol>(lhs: Self, rhs: D) -> Bool { safeCompare(bytes(contiguous: lhs), bytes(rhs)) }
}
public struct HashedAuthenticationCode<H: HashFunction>: MessageAuthenticationCode, Sendable {
    let b: [UInt8]
    public var byteCount: Int { b.count }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public func hash(into hasher: inout Hasher) { hasher.combine(b) }
}
/* HMAC (RFC 2104) sobre a HashFunction do tipo: o hash interno já leva a chave XOR ipad; o externo, XOR opad */
public struct HMAC<H: HashFunction>: Sendable {
    public typealias Key = SymmetricKey
    public typealias MAC = HashedAuthenticationCode<H>
    var inner: H
    let outerKey: [UInt8]
    public init(key: SymmetricKey) {
        var k = key.b
        if k.count > H.blockByteCount { k = bytes(contiguous: H.hash(data: k)) }
        k += [UInt8](repeating: 0, count: H.blockByteCount - k.count)
        inner = H()
        inner.update(data: k.map { $0 ^ 0x36 })
        outerKey = k.map { $0 ^ 0x5c }
    }
    public mutating func update<D: DataProtocol>(data: D) { inner.update(data: data) }
    public func finalize() -> HashedAuthenticationCode<H> {
        var o = H()
        o.update(data: outerKey)
        o.update(data: bytes(contiguous: inner.finalize()))
        return HashedAuthenticationCode(b: bytes(contiguous: o.finalize()))
    }
    public static func authenticationCode<D: DataProtocol>(for data: D, using key: SymmetricKey) -> HashedAuthenticationCode<H> {
        var h = HMAC(key: key)
        h.update(data: data)
        return h.finalize()
    }
    public static func isValidAuthenticationCode<D: DataProtocol>(_ mac: HashedAuthenticationCode<H>, authenticating data: D, using key: SymmetricKey) -> Bool {
        authenticationCode(for: data, using: key) == mac
    }
    public static func isValidAuthenticationCode<C: ContiguousBytes, D: DataProtocol>(_ mac: C, authenticating data: D, using key: SymmetricKey) -> Bool {
        safeCompare(bytes(contiguous: authenticationCode(for: data, using: key)), bytes(contiguous: mac))
    }
}
/* HKDF (RFC 5869) */
public struct HKDF<H: HashFunction>: Sendable {
    public static func extract<Salt: DataProtocol>(inputKeyMaterial: SymmetricKey, salt: Salt?) -> HashedAuthenticationCode<H> {
        let s = salt.map { bytes($0) } ?? [UInt8](repeating: 0, count: H.Digest.byteCount)
        return HMAC<H>.authenticationCode(for: inputKeyMaterial.b, using: SymmetricKey(data: s))
    }
    public static func expand<PRK: ContiguousBytes, Info: DataProtocol>(pseudoRandomKey prk: PRK, info: Info?, outputByteCount: Int) -> SymmetricKey {
        let key = SymmetricKey(data: bytes(contiguous: prk)), inf = info.map { bytes($0) } ?? []
        var out = [UInt8](), t = [UInt8](), i: UInt8 = 1
        while out.count < outputByteCount {
            var h = HMAC<H>(key: key)
            h.update(data: t); h.update(data: inf); h.update(data: [i])
            t = bytes(contiguous: h.finalize())
            out += t
            i &+= 1
        }
        return SymmetricKey(data: Array(out.prefix(outputByteCount)))
    }
    public static func deriveKey<Salt: DataProtocol, Info: DataProtocol>(inputKeyMaterial: SymmetricKey, salt: Salt, info: Info, outputByteCount: Int) -> SymmetricKey {
        expand(pseudoRandomKey: extract(inputKeyMaterial: inputKeyMaterial, salt: salt), info: info, outputByteCount: outputByteCount)
    }
    public static func deriveKey<Info: DataProtocol>(inputKeyMaterial: SymmetricKey, info: Info, outputByteCount: Int) -> SymmetricKey {
        expand(pseudoRandomKey: extract(inputKeyMaterial: inputKeyMaterial, salt: Optional<[UInt8]>.none), info: info, outputByteCount: outputByteCount)
    }
    public static func deriveKey(inputKeyMaterial: SymmetricKey, outputByteCount: Int) -> SymmetricKey {
        deriveKey(inputKeyMaterial: inputKeyMaterial, info: [UInt8](), outputByteCount: outputByteCount)
    }
}

// MARK: - AES

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
            /* combined só existe com o nonce padrão de 12 bytes, como na Apple */
            public var combined: Data? { nonce.b.count == 12 ? Data(nonce.b) + ciphertext + tag : nil }
            public init<D: DataProtocol>(combined: D) throws {
                let x = bytes(combined)
                guard x.count >= 28 else { throw CryptoKitError.incorrectParameterSize }
                nonce = try Nonce(data: x.prefix(12)); ciphertext = Data(x[12..<(x.count - 16)]); tag = Data(x.suffix(16))
            }
            /* a variante de Data não lança (como na Apple): dado curto demais é erro de programação */
            public init(combined: Data) {
                let x = [UInt8](combined)
                precondition(x.count >= 28, "AES.GCM.SealedBox(combined:): menos de 28 bytes")
                nonce = try! Nonce(data: x.prefix(12)); ciphertext = Data(x[12..<(x.count - 16)]); tag = Data(x.suffix(16))
            }
            public init<C: DataProtocol, T: DataProtocol>(nonce: Nonce, ciphertext: C, tag: T) throws {
                let t = bytes(tag)
                guard t.count == 16 else { throw CryptoKitError.incorrectParameterSize }
                self.nonce = nonce; self.ciphertext = Data(bytes(ciphertext)); self.tag = Data(t)
            }
            init(nonce: Nonce, ct: [UInt8], tag: [UInt8]) { self.nonce = nonce; ciphertext = Data(ct); self.tag = Data(tag) }
        }
        public static func seal<Plaintext: DataProtocol, AuthenticatedData: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil, authenticating authenticatedData: AuthenticatedData) throws -> SealedBox {
            let k = key.b, n = nonce ?? Nonce(), m = bytes(message), a = bytes(authenticatedData)
            guard [16, 24, 32].contains(k.count) else { throw CryptoKitError.incorrectKeySize }
            guard n.b.count == 12 else { throw CryptoKitError.incorrectParameterSize }
            var ct = [UInt8](repeating: 0, count: m.count), tag = [UInt8](repeating: 0, count: 16)
            let r = k.withUnsafeBufferPointer { kk in n.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in m.withUnsafeBufferPointer { mm in ct.withUnsafeMutableBufferPointer { cc in tag.withUnsafeMutableBufferPointer { tt in
                ck_gcm_seal(kk.baseAddress, kk.count, nn.baseAddress, aa.baseAddress, aa.count, mm.baseAddress, mm.count, cc.baseAddress, tt.baseAddress!) } } } } } }
            guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
            return SealedBox(nonce: n, ct: ct, tag: tag)
        }
        public static func seal<Plaintext: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil) throws -> SealedBox {
            try seal(message, using: key, nonce: nonce, authenticating: [UInt8]())
        }
        public static func open<AuthenticatedData: DataProtocol>(_ box: SealedBox, using key: SymmetricKey, authenticating authenticatedData: AuthenticatedData) throws -> Data {
            let k = key.b, c = [UInt8](box.ciphertext), t = [UInt8](box.tag), a = bytes(authenticatedData)
            guard [16, 24, 32].contains(k.count) else { throw CryptoKitError.incorrectKeySize }
            guard box.nonce.b.count == 12 else { throw CryptoKitError.incorrectParameterSize }
            var p = [UInt8](repeating: 0, count: c.count)
            let r = k.withUnsafeBufferPointer { kk in box.nonce.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in c.withUnsafeBufferPointer { cc in t.withUnsafeBufferPointer { tt in p.withUnsafeMutableBufferPointer { pp in
                ck_gcm_open(kk.baseAddress, kk.count, nn.baseAddress, aa.baseAddress, aa.count, cc.baseAddress, cc.count, tt.baseAddress, pp.baseAddress) } } } } } }
            guard r == 0 else { throw CryptoKitError.authenticationFailure }
            return Data(p)
        }
        public static func open(_ box: SealedBox, using key: SymmetricKey) throws -> Data { try open(box, using: key, authenticating: [UInt8]()) }
    }
    /* AES Key Wrap (RFC 3394) */
    public enum KeyWrap {
        public static func wrap(_ keyToWrap: SymmetricKey, using kek: SymmetricKey) throws -> Data {
            let k = kek.b, w = keyToWrap.b
            guard [16, 24, 32].contains(k.count) else { throw CryptoKitError.incorrectKeySize }
            guard w.count >= 16, w.count % 8 == 0 else { throw CryptoKitError.incorrectParameterSize }
            var out = [UInt8](repeating: 0, count: w.count + 8)
            let r = k.withUnsafeBufferPointer { kk in w.withUnsafeBufferPointer { ww in out.withUnsafeMutableBufferPointer { ck_wrap(kk.baseAddress, kk.count, ww.baseAddress, ww.count, $0.baseAddress!) } } }
            guard r == 0 else { throw CryptoKitError.wrapFailure }
            return Data(out)
        }
        public static func unwrap<WrappedKey: DataProtocol>(_ wrappedKey: WrappedKey, using kek: SymmetricKey) throws -> SymmetricKey {
            let k = kek.b, w = bytes(wrappedKey)
            guard [16, 24, 32].contains(k.count) else { throw CryptoKitError.incorrectKeySize }
            guard w.count >= 24, w.count % 8 == 0 else { throw CryptoKitError.incorrectParameterSize }
            var out = [UInt8](repeating: 0, count: w.count - 8)
            let r = k.withUnsafeBufferPointer { kk in w.withUnsafeBufferPointer { ww in out.withUnsafeMutableBufferPointer { ck_unwrap(kk.baseAddress, kk.count, ww.baseAddress, ww.count, $0.baseAddress!) } } }
            guard r == 0 else { throw CryptoKitError.unwrapFailure }
            return SymmetricKey(data: out)
        }
    }
}

// MARK: - ChaCha20-Poly1305

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
            nonce = try Nonce(data: x.prefix(12)); ciphertext = Data(x[12..<(x.count - 16)]); tag = Data(x.suffix(16))
        }
        public init<C: DataProtocol, T: DataProtocol>(nonce: Nonce, ciphertext: C, tag: T) throws {
            let t = bytes(tag)
            guard t.count == 16 else { throw CryptoKitError.incorrectParameterSize }
            self.nonce = nonce; self.ciphertext = Data(bytes(ciphertext)); self.tag = Data(t)
        }
        init(nonce: Nonce, ct: [UInt8], tag: [UInt8]) { self.nonce = nonce; ciphertext = Data(ct); self.tag = Data(tag) }
    }
    public static func seal<Plaintext: DataProtocol, AuthenticatedData: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil, authenticating authenticatedData: AuthenticatedData) throws -> SealedBox {
        let k = key.b, n = nonce ?? Nonce(), m = bytes(message), a = bytes(authenticatedData)
        guard k.count == 32 else { throw CryptoKitError.incorrectKeySize }
        var ct = [UInt8](repeating: 0, count: m.count), tag = [UInt8](repeating: 0, count: 16)
        let r = k.withUnsafeBufferPointer { kk in n.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in m.withUnsafeBufferPointer { mm in ct.withUnsafeMutableBufferPointer { cc in tag.withUnsafeMutableBufferPointer { tt in
            ck_cc_seal(kk.baseAddress, nn.baseAddress, aa.baseAddress, aa.count, mm.baseAddress, mm.count, cc.baseAddress, tt.baseAddress!) } } } } } }
        guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
        return SealedBox(nonce: n, ct: ct, tag: tag)
    }
    public static func seal<Plaintext: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil) throws -> SealedBox {
        try seal(message, using: key, nonce: nonce, authenticating: [UInt8]())
    }
    public static func open<AuthenticatedData: DataProtocol>(_ box: SealedBox, using key: SymmetricKey, authenticating authenticatedData: AuthenticatedData) throws -> Data {
        let k = key.b, c = [UInt8](box.ciphertext), t = [UInt8](box.tag), a = bytes(authenticatedData)
        guard k.count == 32 else { throw CryptoKitError.incorrectKeySize }
        var p = [UInt8](repeating: 0, count: c.count)
        let r = k.withUnsafeBufferPointer { kk in box.nonce.b.withUnsafeBufferPointer { nn in a.withUnsafeBufferPointer { aa in c.withUnsafeBufferPointer { cc in t.withUnsafeBufferPointer { tt in p.withUnsafeMutableBufferPointer { pp in
            ck_cc_open(kk.baseAddress, nn.baseAddress, aa.baseAddress, aa.count, cc.baseAddress, cc.count, tt.baseAddress, pp.baseAddress) } } } } } }
        guard r == 0 else { throw CryptoKitError.authenticationFailure }
        return Data(p)
    }
    public static func open(_ box: SealedBox, using key: SymmetricKey) throws -> Data { try open(box, using: key, authenticating: [UInt8]()) }
}

// MARK: - segredo compartilhado

public struct SharedSecret: ContiguousBytes, CustomStringConvertible, Sendable {
    let b: [UInt8]
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public var description: String { "SharedSecret: " + hexString(b) }
    public func hkdfDerivedSymmetricKey<H: HashFunction, Salt: DataProtocol, SI: DataProtocol>(using hashFunction: H.Type, salt: Salt, sharedInfo: SI, outputByteCount: Int) -> SymmetricKey {
        HKDF<H>.deriveKey(inputKeyMaterial: SymmetricKey(data: b), salt: salt, info: sharedInfo, outputByteCount: outputByteCount)
    }
    /* ANSI X9.63: K = H(Z || contador || sharedInfo) concatenados */
    public func x963DerivedSymmetricKey<H: HashFunction, SI: DataProtocol>(using hashFunction: H.Type, sharedInfo: SI, outputByteCount: Int) -> SymmetricKey {
        let info = bytes(sharedInfo)
        var out = [UInt8](), c: UInt32 = 1
        while out.count < outputByteCount {
            var h = H()
            h.update(data: b)
            h.update(data: [UInt8(c >> 24), UInt8((c >> 16) & 255), UInt8((c >> 8) & 255), UInt8(c & 255)])
            h.update(data: info)
            out += bytes(contiguous: h.finalize())
            c += 1
        }
        return SymmetricKey(data: Array(out.prefix(outputByteCount)))
    }
}
extension SharedSecret: Hashable {
    public static func == (a: SharedSecret, b: SharedSecret) -> Bool { safeCompare(a.b, b.b) }
    public func hash(into h: inout Hasher) { h.combine(b) }
}

// MARK: - Curve25519 (X25519 e Ed25519)

public enum Curve25519: Sendable {
    public enum KeyAgreement: Sendable {
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
            public func sharedSecretFromKeyAgreement(with publicKeyShare: PublicKey) throws -> SharedSecret {
                var s = [UInt8](repeating: 0, count: 32)
                let r = b.withUnsafeBufferPointer { k in publicKeyShare.b.withUnsafeBufferPointer { pp in s.withUnsafeMutableBufferPointer { ck_x(k.baseAddress, pp.baseAddress, $0.baseAddress!) } } }
                guard r == 0 else { throw CryptoKitError.invalidParameter }       /* ponto de ordem baixa: segredo todo zero */
                return SharedSecret(b: s)
            }
        }
    }
    public enum Signing: Sendable {
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
                let r = b.withUnsafeBufferPointer { k in m.withUnsafeBufferPointer { mm in s.withUnsafeMutableBufferPointer { ck_ed_sign(k.baseAddress, mm.baseAddress, mm.count, $0.baseAddress!) } } }
                guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
                return Data(s)
            }
        }
    }
}

public enum P256: Sendable {
    public enum Signing: Sendable {
        public struct PublicKey: Sendable {
            let q: [UInt8]
            init(_q: [UInt8]) { q = _q }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { q = try ECCore.point([4] + bytes(contiguous: data), 32) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws { q = try ECCore.point(bytes(contiguous: data), 32) }
            public init<D: ContiguousBytes>(compressedRepresentation data: D) throws { q = try ECCore.decompress(bytes(contiguous: data), 256) }
            public init<D: ContiguousBytes>(compactRepresentation data: D) throws { q = try ECCore.fromCompact(bytes(contiguous: data), 256) }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 { q = try ECCore.fromSPKI(Array(data), 256) }
            public init(pemRepresentation s: String) throws { q = try ECCore.fromSPKI(try ECCore.unpem(s, "PUBLIC KEY"), 256) }
            public var rawRepresentation: Data { Data(q.dropFirst()) }
            public var x963Representation: Data { Data(q) }
            public var compressedRepresentation: Data { Data(ECCore.compressed(q, 32)) }
            public var compactRepresentation: Data? { ECCore.compact(q, 256).map { Data($0) } }
            public var derRepresentation: Data { Data(ECCore.spki(q, 256)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.spki(q, 256), "PUBLIC KEY") }
            public func isValidSignature<D: Digest>(_ signature: ECDSASignature, for digest: D) -> Bool {
                ECCore.verify(q, digest: bytes(contiguous: digest), rs: signature.rs, 256)
            }
            public func isValidSignature<D: DataProtocol>(_ signature: ECDSASignature, for data: D) -> Bool {
                isValidSignature(signature, for: SHA256.hash(data: data))
            }
        }
        public struct PrivateKey: Sendable {
            let d: [UInt8], q: [UInt8]
            public init(compactRepresentable: Bool = true) { (d, q) = ECCore.generate(256, compact: compactRepresentable) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { d = bytes(contiguous: data); q = try ECCore.publicFrom(d, 256) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 1 + 3 * 32 else { throw CryptoKitError.incorrectKeySize }
                d = Array(x.suffix(32)); q = try ECCore.publicFrom(d, 256)
                guard Array(x.prefix(1 + 2 * 32)) == q else { throw CryptoKitError.invalidParameter }
            }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 {
                d = try ECCore.privFromDER(Array(data), 256); q = try ECCore.publicFrom(d, 256)
            }
            public init(pemRepresentation s: String) throws {
                d = try ECCore.privFromDER(try ECCore.unpem(s, nil), 256); q = try ECCore.publicFrom(d, 256)
            }
            public var rawRepresentation: Data { Data(d) }
            public var x963Representation: Data { Data(q + d) }
            public var derRepresentation: Data { Data(ECCore.pkcs8(d, q, 256)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.pkcs8(d, q, 256), "PRIVATE KEY") }
            public var publicKey: PublicKey { PublicKey(_q: q) }
            public func signature<D: Digest>(for digest: D) throws -> ECDSASignature {
                ECDSASignature(_rs: try ECCore.sign(d, digest: bytes(contiguous: digest), 256))
            }
            public func signature<D: DataProtocol>(for data: D) throws -> ECDSASignature { try signature(for: SHA256.hash(data: data)) }
        }
        public struct ECDSASignature: ContiguousBytes, Sendable {
            let rs: [UInt8]
            init(_rs: [UInt8]) { rs = _rs }
            public init<D: DataProtocol>(rawRepresentation data: D) throws {
                let x = bytes(data)
                guard x.count == 2 * 32 else { throw CryptoKitError.incorrectParameterSize }
                rs = x
            }
            public init<D: DataProtocol>(derRepresentation data: D) throws { rs = try ECCore.rsFromDER(bytes(data), 32) }
            public var rawRepresentation: Data { Data(rs) }
            public var derRepresentation: Data { Data(ECCore.rsToDER(rs, 32)) }
            public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try rs.withUnsafeBytes(body) }
        }
    }
    public enum KeyAgreement: Sendable {
        public struct PublicKey: Sendable {
            let q: [UInt8]
            init(_q: [UInt8]) { q = _q }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { q = try ECCore.point([4] + bytes(contiguous: data), 32) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws { q = try ECCore.point(bytes(contiguous: data), 32) }
            public init<D: ContiguousBytes>(compressedRepresentation data: D) throws { q = try ECCore.decompress(bytes(contiguous: data), 256) }
            public init<D: ContiguousBytes>(compactRepresentation data: D) throws { q = try ECCore.fromCompact(bytes(contiguous: data), 256) }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 { q = try ECCore.fromSPKI(Array(data), 256) }
            public init(pemRepresentation s: String) throws { q = try ECCore.fromSPKI(try ECCore.unpem(s, "PUBLIC KEY"), 256) }
            public var rawRepresentation: Data { Data(q.dropFirst()) }
            public var x963Representation: Data { Data(q) }
            public var compressedRepresentation: Data { Data(ECCore.compressed(q, 32)) }
            public var compactRepresentation: Data? { ECCore.compact(q, 256).map { Data($0) } }
            public var derRepresentation: Data { Data(ECCore.spki(q, 256)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.spki(q, 256), "PUBLIC KEY") }
        }
        public struct PrivateKey: Sendable {
            let d: [UInt8], q: [UInt8]
            public init(compactRepresentable: Bool = true) { (d, q) = ECCore.generate(256, compact: compactRepresentable) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { d = bytes(contiguous: data); q = try ECCore.publicFrom(d, 256) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 1 + 3 * 32 else { throw CryptoKitError.incorrectKeySize }
                d = Array(x.suffix(32)); q = try ECCore.publicFrom(d, 256)
                guard Array(x.prefix(1 + 2 * 32)) == q else { throw CryptoKitError.invalidParameter }
            }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 {
                d = try ECCore.privFromDER(Array(data), 256); q = try ECCore.publicFrom(d, 256)
            }
            public init(pemRepresentation s: String) throws {
                d = try ECCore.privFromDER(try ECCore.unpem(s, nil), 256); q = try ECCore.publicFrom(d, 256)
            }
            public var rawRepresentation: Data { Data(d) }
            public var x963Representation: Data { Data(q + d) }
            public var derRepresentation: Data { Data(ECCore.pkcs8(d, q, 256)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.pkcs8(d, q, 256), "PRIVATE KEY") }
            public var publicKey: PublicKey { PublicKey(_q: q) }
            public func sharedSecretFromKeyAgreement(with publicKeyShare: PublicKey) throws -> SharedSecret {
                SharedSecret(b: try ECCore.shared(d, publicKeyShare.q, 256))
            }
        }
    }
}

public enum P384: Sendable {
    public enum Signing: Sendable {
        public struct PublicKey: Sendable {
            let q: [UInt8]
            init(_q: [UInt8]) { q = _q }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { q = try ECCore.point([4] + bytes(contiguous: data), 48) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws { q = try ECCore.point(bytes(contiguous: data), 48) }
            public init<D: ContiguousBytes>(compressedRepresentation data: D) throws { q = try ECCore.decompress(bytes(contiguous: data), 384) }
            public init<D: ContiguousBytes>(compactRepresentation data: D) throws { q = try ECCore.fromCompact(bytes(contiguous: data), 384) }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 { q = try ECCore.fromSPKI(Array(data), 384) }
            public init(pemRepresentation s: String) throws { q = try ECCore.fromSPKI(try ECCore.unpem(s, "PUBLIC KEY"), 384) }
            public var rawRepresentation: Data { Data(q.dropFirst()) }
            public var x963Representation: Data { Data(q) }
            public var compressedRepresentation: Data { Data(ECCore.compressed(q, 48)) }
            public var compactRepresentation: Data? { ECCore.compact(q, 384).map { Data($0) } }
            public var derRepresentation: Data { Data(ECCore.spki(q, 384)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.spki(q, 384), "PUBLIC KEY") }
            public func isValidSignature<D: Digest>(_ signature: ECDSASignature, for digest: D) -> Bool {
                ECCore.verify(q, digest: bytes(contiguous: digest), rs: signature.rs, 384)
            }
            public func isValidSignature<D: DataProtocol>(_ signature: ECDSASignature, for data: D) -> Bool {
                isValidSignature(signature, for: SHA384.hash(data: data))
            }
        }
        public struct PrivateKey: Sendable {
            let d: [UInt8], q: [UInt8]
            public init(compactRepresentable: Bool = true) { (d, q) = ECCore.generate(384, compact: compactRepresentable) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { d = bytes(contiguous: data); q = try ECCore.publicFrom(d, 384) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 1 + 3 * 48 else { throw CryptoKitError.incorrectKeySize }
                d = Array(x.suffix(48)); q = try ECCore.publicFrom(d, 384)
                guard Array(x.prefix(1 + 2 * 48)) == q else { throw CryptoKitError.invalidParameter }
            }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 {
                d = try ECCore.privFromDER(Array(data), 384); q = try ECCore.publicFrom(d, 384)
            }
            public init(pemRepresentation s: String) throws {
                d = try ECCore.privFromDER(try ECCore.unpem(s, nil), 384); q = try ECCore.publicFrom(d, 384)
            }
            public var rawRepresentation: Data { Data(d) }
            public var x963Representation: Data { Data(q + d) }
            public var derRepresentation: Data { Data(ECCore.pkcs8(d, q, 384)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.pkcs8(d, q, 384), "PRIVATE KEY") }
            public var publicKey: PublicKey { PublicKey(_q: q) }
            public func signature<D: Digest>(for digest: D) throws -> ECDSASignature {
                ECDSASignature(_rs: try ECCore.sign(d, digest: bytes(contiguous: digest), 384))
            }
            public func signature<D: DataProtocol>(for data: D) throws -> ECDSASignature { try signature(for: SHA384.hash(data: data)) }
        }
        public struct ECDSASignature: ContiguousBytes, Sendable {
            let rs: [UInt8]
            init(_rs: [UInt8]) { rs = _rs }
            public init<D: DataProtocol>(rawRepresentation data: D) throws {
                let x = bytes(data)
                guard x.count == 2 * 48 else { throw CryptoKitError.incorrectParameterSize }
                rs = x
            }
            public init<D: DataProtocol>(derRepresentation data: D) throws { rs = try ECCore.rsFromDER(bytes(data), 48) }
            public var rawRepresentation: Data { Data(rs) }
            public var derRepresentation: Data { Data(ECCore.rsToDER(rs, 48)) }
            public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try rs.withUnsafeBytes(body) }
        }
    }
    public enum KeyAgreement: Sendable {
        public struct PublicKey: Sendable {
            let q: [UInt8]
            init(_q: [UInt8]) { q = _q }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { q = try ECCore.point([4] + bytes(contiguous: data), 48) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws { q = try ECCore.point(bytes(contiguous: data), 48) }
            public init<D: ContiguousBytes>(compressedRepresentation data: D) throws { q = try ECCore.decompress(bytes(contiguous: data), 384) }
            public init<D: ContiguousBytes>(compactRepresentation data: D) throws { q = try ECCore.fromCompact(bytes(contiguous: data), 384) }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 { q = try ECCore.fromSPKI(Array(data), 384) }
            public init(pemRepresentation s: String) throws { q = try ECCore.fromSPKI(try ECCore.unpem(s, "PUBLIC KEY"), 384) }
            public var rawRepresentation: Data { Data(q.dropFirst()) }
            public var x963Representation: Data { Data(q) }
            public var compressedRepresentation: Data { Data(ECCore.compressed(q, 48)) }
            public var compactRepresentation: Data? { ECCore.compact(q, 384).map { Data($0) } }
            public var derRepresentation: Data { Data(ECCore.spki(q, 384)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.spki(q, 384), "PUBLIC KEY") }
        }
        public struct PrivateKey: Sendable {
            let d: [UInt8], q: [UInt8]
            public init(compactRepresentable: Bool = true) { (d, q) = ECCore.generate(384, compact: compactRepresentable) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { d = bytes(contiguous: data); q = try ECCore.publicFrom(d, 384) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 1 + 3 * 48 else { throw CryptoKitError.incorrectKeySize }
                d = Array(x.suffix(48)); q = try ECCore.publicFrom(d, 384)
                guard Array(x.prefix(1 + 2 * 48)) == q else { throw CryptoKitError.invalidParameter }
            }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 {
                d = try ECCore.privFromDER(Array(data), 384); q = try ECCore.publicFrom(d, 384)
            }
            public init(pemRepresentation s: String) throws {
                d = try ECCore.privFromDER(try ECCore.unpem(s, nil), 384); q = try ECCore.publicFrom(d, 384)
            }
            public var rawRepresentation: Data { Data(d) }
            public var x963Representation: Data { Data(q + d) }
            public var derRepresentation: Data { Data(ECCore.pkcs8(d, q, 384)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.pkcs8(d, q, 384), "PRIVATE KEY") }
            public var publicKey: PublicKey { PublicKey(_q: q) }
            public func sharedSecretFromKeyAgreement(with publicKeyShare: PublicKey) throws -> SharedSecret {
                SharedSecret(b: try ECCore.shared(d, publicKeyShare.q, 384))
            }
        }
    }
}

public enum P521: Sendable {
    public enum Signing: Sendable {
        public struct PublicKey: Sendable {
            let q: [UInt8]
            init(_q: [UInt8]) { q = _q }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { q = try ECCore.point([4] + bytes(contiguous: data), 66) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws { q = try ECCore.point(bytes(contiguous: data), 66) }
            public init<D: ContiguousBytes>(compressedRepresentation data: D) throws { q = try ECCore.decompress(bytes(contiguous: data), 521) }
            public init<D: ContiguousBytes>(compactRepresentation data: D) throws { q = try ECCore.fromCompact(bytes(contiguous: data), 521) }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 { q = try ECCore.fromSPKI(Array(data), 521) }
            public init(pemRepresentation s: String) throws { q = try ECCore.fromSPKI(try ECCore.unpem(s, "PUBLIC KEY"), 521) }
            public var rawRepresentation: Data { Data(q.dropFirst()) }
            public var x963Representation: Data { Data(q) }
            public var compressedRepresentation: Data { Data(ECCore.compressed(q, 66)) }
            public var compactRepresentation: Data? { ECCore.compact(q, 521).map { Data($0) } }
            public var derRepresentation: Data { Data(ECCore.spki(q, 521)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.spki(q, 521), "PUBLIC KEY") }
            public func isValidSignature<D: Digest>(_ signature: ECDSASignature, for digest: D) -> Bool {
                ECCore.verify(q, digest: bytes(contiguous: digest), rs: signature.rs, 521)
            }
            public func isValidSignature<D: DataProtocol>(_ signature: ECDSASignature, for data: D) -> Bool {
                isValidSignature(signature, for: SHA512.hash(data: data))
            }
        }
        public struct PrivateKey: Sendable {
            let d: [UInt8], q: [UInt8]
            public init(compactRepresentable: Bool = true) { (d, q) = ECCore.generate(521, compact: compactRepresentable) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { d = bytes(contiguous: data); q = try ECCore.publicFrom(d, 521) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 1 + 3 * 66 else { throw CryptoKitError.incorrectKeySize }
                d = Array(x.suffix(66)); q = try ECCore.publicFrom(d, 521)
                guard Array(x.prefix(1 + 2 * 66)) == q else { throw CryptoKitError.invalidParameter }
            }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 {
                d = try ECCore.privFromDER(Array(data), 521); q = try ECCore.publicFrom(d, 521)
            }
            public init(pemRepresentation s: String) throws {
                d = try ECCore.privFromDER(try ECCore.unpem(s, nil), 521); q = try ECCore.publicFrom(d, 521)
            }
            public var rawRepresentation: Data { Data(d) }
            public var x963Representation: Data { Data(q + d) }
            public var derRepresentation: Data { Data(ECCore.pkcs8(d, q, 521)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.pkcs8(d, q, 521), "PRIVATE KEY") }
            public var publicKey: PublicKey { PublicKey(_q: q) }
            public func signature<D: Digest>(for digest: D) throws -> ECDSASignature {
                ECDSASignature(_rs: try ECCore.sign(d, digest: bytes(contiguous: digest), 521))
            }
            public func signature<D: DataProtocol>(for data: D) throws -> ECDSASignature { try signature(for: SHA512.hash(data: data)) }
        }
        public struct ECDSASignature: ContiguousBytes, Sendable {
            let rs: [UInt8]
            init(_rs: [UInt8]) { rs = _rs }
            public init<D: DataProtocol>(rawRepresentation data: D) throws {
                let x = bytes(data)
                guard x.count == 2 * 66 else { throw CryptoKitError.incorrectParameterSize }
                rs = x
            }
            public init<D: DataProtocol>(derRepresentation data: D) throws { rs = try ECCore.rsFromDER(bytes(data), 66) }
            public var rawRepresentation: Data { Data(rs) }
            public var derRepresentation: Data { Data(ECCore.rsToDER(rs, 66)) }
            public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try rs.withUnsafeBytes(body) }
        }
    }
    public enum KeyAgreement: Sendable {
        public struct PublicKey: Sendable {
            let q: [UInt8]
            init(_q: [UInt8]) { q = _q }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { q = try ECCore.point([4] + bytes(contiguous: data), 66) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws { q = try ECCore.point(bytes(contiguous: data), 66) }
            public init<D: ContiguousBytes>(compressedRepresentation data: D) throws { q = try ECCore.decompress(bytes(contiguous: data), 521) }
            public init<D: ContiguousBytes>(compactRepresentation data: D) throws { q = try ECCore.fromCompact(bytes(contiguous: data), 521) }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 { q = try ECCore.fromSPKI(Array(data), 521) }
            public init(pemRepresentation s: String) throws { q = try ECCore.fromSPKI(try ECCore.unpem(s, "PUBLIC KEY"), 521) }
            public var rawRepresentation: Data { Data(q.dropFirst()) }
            public var x963Representation: Data { Data(q) }
            public var compressedRepresentation: Data { Data(ECCore.compressed(q, 66)) }
            public var compactRepresentation: Data? { ECCore.compact(q, 521).map { Data($0) } }
            public var derRepresentation: Data { Data(ECCore.spki(q, 521)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.spki(q, 521), "PUBLIC KEY") }
        }
        public struct PrivateKey: Sendable {
            let d: [UInt8], q: [UInt8]
            public init(compactRepresentable: Bool = true) { (d, q) = ECCore.generate(521, compact: compactRepresentable) }
            public init<D: ContiguousBytes>(rawRepresentation data: D) throws { d = bytes(contiguous: data); q = try ECCore.publicFrom(d, 521) }
            public init<D: ContiguousBytes>(x963Representation data: D) throws {
                let x = bytes(contiguous: data)
                guard x.count == 1 + 3 * 66 else { throw CryptoKitError.incorrectKeySize }
                d = Array(x.suffix(66)); q = try ECCore.publicFrom(d, 521)
                guard Array(x.prefix(1 + 2 * 66)) == q else { throw CryptoKitError.invalidParameter }
            }
            public init<Bytes: RandomAccessCollection>(derRepresentation data: Bytes) throws where Bytes.Element == UInt8 {
                d = try ECCore.privFromDER(Array(data), 521); q = try ECCore.publicFrom(d, 521)
            }
            public init(pemRepresentation s: String) throws {
                d = try ECCore.privFromDER(try ECCore.unpem(s, nil), 521); q = try ECCore.publicFrom(d, 521)
            }
            public var rawRepresentation: Data { Data(d) }
            public var x963Representation: Data { Data(q + d) }
            public var derRepresentation: Data { Data(ECCore.pkcs8(d, q, 521)) }
            public var pemRepresentation: String { ECCore.pem(ECCore.pkcs8(d, q, 521), "PRIVATE KEY") }
            public var publicKey: PublicKey { PublicKey(_q: q) }
            public func sharedSecretFromKeyAgreement(with publicKeyShare: PublicKey) throws -> SharedSecret {
                SharedSecret(b: try ECCore.shared(d, publicKeyShare.q, 521))
            }
        }
    }
}

// MARK: - núcleo comum das curvas NIST

enum ECCore {
    static func size(_ bits: Int32) -> Int { bits == 256 ? 32 : bits == 384 ? 48 : 66 }
    static func oid(_ bits: Int32) -> [UInt8] {
        bits == 256 ? [0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07]
            : bits == 384 ? [0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22] : [0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23]
    }
    static let ecPublicKey: [UInt8] = [0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01]
    static func len(_ n: Int) -> [UInt8] { n < 128 ? [UInt8(n)] : n < 256 ? [0x81, UInt8(n)] : [0x82, UInt8(n >> 8), UInt8(n & 255)] }
    static func tlv(_ tag: UInt8, _ v: [UInt8]) -> [UInt8] { [tag] + len(v.count) + v }
    static func read(_ d: [UInt8], _ i: inout Int) throws -> (UInt8, [UInt8]) {
        guard i + 2 <= d.count else { throw CryptoKitASN1Error.truncatedASN1Field }
        let tag = d[i]
        var n = Int(d[i + 1]); i += 2
        if n & 0x80 != 0 {
            let k = n & 0x7f
            guard k >= 1, k <= 3, i + k <= d.count else { throw CryptoKitASN1Error.unsupportedFieldLength }
            n = 0
            for _ in 0..<k { n = n << 8 | Int(d[i]); i += 1 }
        }
        guard i + n <= d.count else { throw CryptoKitASN1Error.truncatedASN1Field }
        defer { i += n }
        return (tag, Array(d[i..<(i + n)]))
    }
    /* ponto 04||X||Y do tamanho certo e na curva: a descompressão de X com a paridade de Y tem de dar o mesmo ponto */
    static func point(_ q: [UInt8], _ size: Int) throws -> [UInt8] {
        guard q.count == 1 + 2 * size, q[0] == 4 else { throw CryptoKitError.incorrectKeySize }
        let bits: Int32 = size == 32 ? 256 : size == 48 ? 384 : 521
        guard let p = decomp(Array(q[1...size]), Int32(q[q.count - 1] & 1), bits), p == q else { throw CryptoKitError.invalidParameter }
        return q
    }
    static func decomp(_ x: [UInt8], _ ybit: Int32, _ bits: Int32) -> [UInt8]? {
        var out = [UInt8](repeating: 0, count: 1 + 2 * size(bits))
        let r = x.withUnsafeBufferPointer { xx in out.withUnsafeMutableBufferPointer { ck_ec_decompress(bits, xx.baseAddress, ybit, $0.baseAddress!) } }
        return r == 0 ? out : nil
    }
    static func decompress(_ c: [UInt8], _ bits: Int32) throws -> [UInt8] {
        let n = size(bits)
        guard c.count == 1 + n, c[0] == 2 || c[0] == 3 else { throw CryptoKitError.incorrectKeySize }
        guard let q = decomp(Array(c.dropFirst()), Int32(c[0] & 1), bits) else { throw CryptoKitError.invalidParameter }
        return q
    }
    static func compressed(_ q: [UInt8], _ n: Int) -> [UInt8] { [UInt8(2 + (q[q.count - 1] & 1))] + Array(q[1...n]) }
    /* forma compacta (só X): vale quando Y é o menor entre Y e p − Y */
    static func compact(_ q: [UInt8], _ bits: Int32) -> [UInt8]? {
        let n = size(bits), x = Array(q[1...n]), y = Array(q[(n + 1)...])
        guard let other = decomp(x, Int32((q[q.count - 1] & 1) ^ 1), bits) else { return nil }
        let y2 = Array(other[(n + 1)...])
        return y.lexicographicallyPrecedes(y2) || y == y2 ? x : nil
    }
    static func fromCompact(_ x: [UInt8], _ bits: Int32) throws -> [UInt8] {
        let n = size(bits)
        guard x.count == n else { throw CryptoKitError.incorrectKeySize }
        guard let a = decomp(x, 0, bits), let b = decomp(x, 1, bits) else { throw CryptoKitError.invalidParameter }
        return Array(a[(n + 1)...]).lexicographicallyPrecedes(Array(b[(n + 1)...])) ? a : b
    }
    static func generate(_ bits: Int32, compact need: Bool) -> ([UInt8], [UInt8]) {
        while true {
            var d = [UInt8](repeating: 0, count: size(bits)), q = [UInt8](repeating: 0, count: 1 + 2 * size(bits))
            _ = d.withUnsafeMutableBufferPointer { dd in q.withUnsafeMutableBufferPointer { ck_ec_gen(bits, dd.baseAddress!, $0.baseAddress!) } }
            if !need || compact(q, bits) != nil { return (d, q) }
        }
    }
    static func publicFrom(_ d: [UInt8], _ bits: Int32) throws -> [UInt8] {
        guard d.count == size(bits) else { throw CryptoKitError.incorrectKeySize }
        var q = [UInt8](repeating: 0, count: 1 + 2 * size(bits))
        let r = d.withUnsafeBufferPointer { dd in q.withUnsafeMutableBufferPointer { ck_ec_pub(bits, dd.baseAddress, $0.baseAddress!) } }
        guard r == 0 else { throw CryptoKitError.invalidParameter }
        return q
    }
    static func sign(_ d: [UInt8], digest: [UInt8], _ bits: Int32) throws -> [UInt8] {
        var rs = [UInt8](repeating: 0, count: 2 * size(bits))
        let r = d.withUnsafeBufferPointer { dd in digest.withUnsafeBufferPointer { g in rs.withUnsafeMutableBufferPointer { ck_ec_sign(bits, dd.baseAddress, g.baseAddress, g.count, $0.baseAddress!) } } }
        guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
        return rs
    }
    static func verify(_ q: [UInt8], digest: [UInt8], rs: [UInt8], _ bits: Int32) -> Bool {
        guard rs.count == 2 * size(bits) else { return false }
        return q.withUnsafeBufferPointer { qq in digest.withUnsafeBufferPointer { g in rs.withUnsafeBufferPointer { ck_ec_verify(bits, qq.baseAddress, g.baseAddress, g.count, $0.baseAddress) == 0 } } }
    }
    static func shared(_ d: [UInt8], _ q: [UInt8], _ bits: Int32) throws -> [UInt8] {
        var s = [UInt8](repeating: 0, count: size(bits))
        let r = d.withUnsafeBufferPointer { dd in q.withUnsafeBufferPointer { qq in s.withUnsafeMutableBufferPointer { ck_ec_shared(bits, dd.baseAddress, qq.baseAddress, $0.baseAddress!) } } }
        guard r == 0 else { throw CryptoKitError.invalidParameter }
        return s
    }
    /* SubjectPublicKeyInfo { AlgorithmIdentifier { id-ecPublicKey, curva }, BIT STRING ponto } */
    static func spki(_ q: [UInt8], _ bits: Int32) -> [UInt8] { tlv(0x30, tlv(0x30, ecPublicKey + oid(bits)) + tlv(0x03, [0] + q)) }
    static func fromSPKI(_ der: [UInt8], _ bits: Int32) throws -> [UInt8] {
        var i = 0
        let (t, seq) = try read(der, &i)
        guard t == 0x30 else { throw CryptoKitASN1Error.unexpectedFieldType }
        var j = 0
        let (t1, alg) = try read(seq, &j), (t2, bs) = try read(seq, &j)
        guard t1 == 0x30, t2 == 0x03, alg == ecPublicKey + oid(bits) else { throw CryptoKitASN1Error.invalidObjectIdentifier }
        guard bs.first == 0 else { throw CryptoKitASN1Error.invalidASN1Object }
        let p = Array(bs.dropFirst())
        if p.first == 2 || p.first == 3 { return try decompress(p, bits) }
        return try point(p, size(bits))
    }
    /* PKCS#8 { 0, { id-ecPublicKey, curva }, OCTET STRING SEC1 { 1, OCTET d, [1] BIT STRING ponto } }, como a Apple gera */
    static func pkcs8(_ d: [UInt8], _ q: [UInt8], _ bits: Int32) -> [UInt8] {
        let sec1 = tlv(0x30, [0x02, 0x01, 0x01] + tlv(0x04, d) + tlv(0xa1, tlv(0x03, [0] + q)))
        return tlv(0x30, [0x02, 0x01, 0x00] + tlv(0x30, ecPublicKey + oid(bits)) + tlv(0x04, sec1))
    }
    /* aceita PKCS#8 e SEC1 ("EC PRIVATE KEY"); a curva, quando vem, tem de ser a do tipo */
    static func privFromDER(_ der: [UInt8], _ bits: Int32) throws -> [UInt8] {
        var i = 0
        let (t, seq) = try read(der, &i)
        guard t == 0x30 else { throw CryptoKitASN1Error.unexpectedFieldType }
        var j = 0
        let (tv, ver) = try read(seq, &j)
        guard tv == 0x02 else { throw CryptoKitASN1Error.unexpectedFieldType }
        if ver == [0] {                                                         /* PKCS#8 */
            let (ta, alg) = try read(seq, &j), (to, inner) = try read(seq, &j)
            guard ta == 0x30, to == 0x04, alg == ecPublicKey + oid(bits) else { throw CryptoKitASN1Error.invalidObjectIdentifier }
            return try privFromDER(inner, bits)
        }
        guard ver == [1] else { throw CryptoKitASN1Error.invalidASN1Object }      /* SEC1 */
        let (td, d) = try read(seq, &j)
        guard td == 0x04, d.count == size(bits) else { throw CryptoKitError.incorrectKeySize }
        while j < seq.count {
            let (tc, c) = try read(seq, &j)
            if tc == 0xa0 && c != oid(bits) { throw CryptoKitASN1Error.invalidObjectIdentifier }
        }
        return d
    }
    static func pem(_ der: [UInt8], _ label: String) -> String {
        let b64 = Data(der).base64EncodedString()
        var lines = [String](), i = b64.startIndex
        while i < b64.endIndex { let j = b64.index(i, offsetBy: 64, limitedBy: b64.endIndex) ?? b64.endIndex; lines.append(String(b64[i..<j])); i = j }
        return "-----BEGIN \(label)-----\n" + lines.joined(separator: "\n") + "\n-----END \(label)-----"
    }
    /* o primeiro bloco PEM com o rótulo pedido (ou qualquer rótulo de chave privada, se nil) */
    static func unpem(_ s: String, _ label: String?) throws -> [UInt8] {
        let ls = s.split(whereSeparator: { $0 == "\n" || $0 == "\r" }).map { $0.trimmingCharacters(in: .whitespaces) }
        var body = "", inside = false
        for l in ls {
            if l.hasPrefix("-----BEGIN ") {
                let lab = String(l.dropFirst(11).dropLast(5))
                inside = label.map { $0 == lab } ?? (lab == "PRIVATE KEY" || lab == "EC PRIVATE KEY")
                continue
            }
            if l.hasPrefix("-----END ") { if inside { break }; continue }
            if inside { body += l }
        }
        guard !body.isEmpty, let d = Data(base64Encoded: body) else { throw CryptoKitASN1Error.invalidPEMDocument }
        return [UInt8](d)
    }
    /* r||s <-> DER SEQUENCE { INTEGER r, INTEGER s } */
    static func int(_ v: ArraySlice<UInt8>) -> [UInt8] {
        var x = Array(v.drop { $0 == 0 })
        if x.isEmpty { x = [0] }
        if x[0] & 0x80 != 0 { x.insert(0, at: 0) }
        return tlv(0x02, x)
    }
    static func rsToDER(_ rs: [UInt8], _ n: Int) -> [UInt8] { tlv(0x30, int(rs[0..<n]) + int(rs[n...])) }
    static func rsFromDER(_ d: [UInt8], _ n: Int) throws -> [UInt8] {
        var i = 0
        let (t, seq) = try read(d, &i)
        guard t == 0x30, i == d.count else { throw CryptoKitASN1Error.unexpectedFieldType }
        var j = 0
        let (t1, r) = try read(seq, &j), (t2, s) = try read(seq, &j)
        guard t1 == 2, t2 == 2, j == seq.count else { throw CryptoKitASN1Error.unexpectedFieldType }
        func fit(_ v: [UInt8]) throws -> [UInt8] {
            let x = Array(v.drop { $0 == 0 })
            guard x.count <= n else { throw CryptoKitASN1Error.invalidASN1IntegerEncoding }
            return [UInt8](repeating: 0, count: n - x.count) + x
        }
        return try fit(r) + fit(s)
    }
}

// MARK: - Secure Enclave (o mOS não tem: nenhuma chave pode ser criada, como num Mac sem o chip)

#if canImport(ObjectiveC)
public enum SecureEnclave {
    public static var isAvailable: Bool { false }
    public enum P256 {
        public enum Signing {
            public struct PrivateKey {
                let never: Never
                public init(compactRepresentable: Bool, accessControl: SecAccessControl, authenticationContext: LAContext?) throws {
                    throw CryptoKitError.underlyingCoreCryptoError(error: -4)   /* errSecUnimplemented: sem Secure Enclave */
                }
                public init(dataRepresentation: Data, authenticationContext: LAContext?) throws {
                    throw CryptoKitError.underlyingCoreCryptoError(error: -4)
                }
                public var dataRepresentation: Data { switch never {} }
                public var publicKey: CryptoKit.P256.Signing.PublicKey { switch never {} }
                public func signature<D: Digest>(for digest: D) throws -> CryptoKit.P256.Signing.ECDSASignature { switch never {} }
                public func signature<D: DataProtocol>(for data: D) throws -> CryptoKit.P256.Signing.ECDSASignature { switch never {} }
            }
        }
        public enum KeyAgreement {
            public struct PrivateKey {
                let never: Never
                public init(compactRepresentable: Bool, accessControl: SecAccessControl, authenticationContext: LAContext?) throws {
                    throw CryptoKitError.underlyingCoreCryptoError(error: -4)
                }
                public init(dataRepresentation: Data, authenticationContext: LAContext?) throws {
                    throw CryptoKitError.underlyingCoreCryptoError(error: -4)
                }
                public var dataRepresentation: Data { switch never {} }
                public var publicKey: CryptoKit.P256.KeyAgreement.PublicKey { switch never {} }
                public func sharedSecretFromKeyAgreement(with publicKeyShare: CryptoKit.P256.KeyAgreement.PublicKey) throws -> SharedSecret { switch never {} }
            }
        }
    }
}

#else
public enum SecureEnclave { public static var isAvailable: Bool { false } }   /* teste de host no Linux */
#endif

// MARK: - comparação em tempo constante (DataProtocol <-> Digest)

extension DataProtocol {
    public static func == <D: Digest>(lhs: Self, rhs: D) -> Bool { rhs == lhs }
}

// MARK: - HPKE (RFC 9180): modos base e auth, DHKEM(X25519, P-256, P-384, P-521), HKDF-SHA2, AES-GCM e ChaCha20-Poly1305

public protocol HPKEDiffieHellmanPublicKey: Sendable {
    init<D: ContiguousBytes>(_ serialization: D, kem: HPKE.KEM) throws
    func export(as kem: HPKE.KEM) throws -> Data
}
public protocol HPKEDiffieHellmanPrivateKey: Sendable {
    associatedtype PublicKey: HPKEDiffieHellmanPublicKey
    var publicKey: PublicKey { get }
    func sharedSecretFromKeyAgreement(with publicKeyShare: PublicKey) throws -> SharedSecret
}
public protocol HPKEDiffieHellmanPrivateKeyGeneration: HPKEDiffieHellmanPrivateKey {
    init()
}

public enum HPKE: Sendable {
    public enum Errors: Error, Sendable {
        case inconsistentParameters, inconsistentCiphersuiteAndKey, exportOnlyMode, inconsistentPSKInputs, expectedPSK, unexpectedPSK,
             outOfRangeSequenceNumber, ciphertextTooShort
    }
    public enum KEM: CaseIterable, Hashable, Sendable {
        case P256_HKDF_SHA256, P384_HKDF_SHA384, P521_HKDF_SHA512, Curve25519_HKDF_SHA256
        var id: UInt16 { switch self { case .P256_HKDF_SHA256: 0x10; case .P384_HKDF_SHA384: 0x11; case .P521_HKDF_SHA512: 0x12; case .Curve25519_HKDF_SHA256: 0x20 } }
        var kdf: KDF { switch self { case .P256_HKDF_SHA256, .Curve25519_HKDF_SHA256: .HKDF_SHA256; case .P384_HKDF_SHA384: .HKDF_SHA384; case .P521_HKDF_SHA512: .HKDF_SHA512 } }
        var nSecret: Int { switch self { case .P256_HKDF_SHA256, .Curve25519_HKDF_SHA256: 32; case .P384_HKDF_SHA384: 48; case .P521_HKDF_SHA512: 64 } }
        var nEnc: Int { switch self { case .Curve25519_HKDF_SHA256: 32; case .P256_HKDF_SHA256: 65; case .P384_HKDF_SHA384: 97; case .P521_HKDF_SHA512: 133 } }
        var bits: Int32 { switch self { case .P256_HKDF_SHA256: 256; case .P384_HKDF_SHA384: 384; case .P521_HKDF_SHA512: 521; case .Curve25519_HKDF_SHA256: 0 } }
    }
    public enum KDF: CaseIterable, Hashable, Sendable {
        case HKDF_SHA256, HKDF_SHA384, HKDF_SHA512
        var id: UInt16 { switch self { case .HKDF_SHA256: 1; case .HKDF_SHA384: 2; case .HKDF_SHA512: 3 } }
        var nH: Int { switch self { case .HKDF_SHA256: 32; case .HKDF_SHA384: 48; case .HKDF_SHA512: 64 } }
        func extract(_ salt: [UInt8], _ ikm: [UInt8]) -> [UInt8] {
            let k = SymmetricKey(data: ikm)
            switch self {
            case .HKDF_SHA256: return bytes(contiguous: HKDF<SHA256>.extract(inputKeyMaterial: k, salt: salt))
            case .HKDF_SHA384: return bytes(contiguous: HKDF<SHA384>.extract(inputKeyMaterial: k, salt: salt))
            case .HKDF_SHA512: return bytes(contiguous: HKDF<SHA512>.extract(inputKeyMaterial: k, salt: salt))
            }
        }
        func expand(_ prk: [UInt8], _ info: [UInt8], _ n: Int) -> [UInt8] {
            switch self {
            case .HKDF_SHA256: return bytes(contiguous: HKDF<SHA256>.expand(pseudoRandomKey: prk, info: info, outputByteCount: n))
            case .HKDF_SHA384: return bytes(contiguous: HKDF<SHA384>.expand(pseudoRandomKey: prk, info: info, outputByteCount: n))
            case .HKDF_SHA512: return bytes(contiguous: HKDF<SHA512>.expand(pseudoRandomKey: prk, info: info, outputByteCount: n))
            }
        }
    }
    public enum AEAD: CaseIterable, Hashable, Sendable {
        case AES_GCM_128, AES_GCM_256, chaChaPoly, exportOnly
        var id: UInt16 { switch self { case .AES_GCM_128: 1; case .AES_GCM_256: 2; case .chaChaPoly: 3; case .exportOnly: 0xffff } }
        var nK: Int { switch self { case .AES_GCM_128: 16; case .AES_GCM_256, .chaChaPoly: 32; case .exportOnly: 0 } }
    }
    public struct Ciphersuite: Sendable {
        public let kem: KEM
        public let kdf: KDF
        public let aead: AEAD
        public init(kem: KEM, kdf: KDF, aead: AEAD) { self.kem = kem; self.kdf = kdf; self.aead = aead }
        public static let P256_SHA256_AES_GCM_256 = Ciphersuite(kem: .P256_HKDF_SHA256, kdf: .HKDF_SHA256, aead: .AES_GCM_256)
        public static let P384_SHA384_AES_GCM_256 = Ciphersuite(kem: .P384_HKDF_SHA384, kdf: .HKDF_SHA384, aead: .AES_GCM_256)
        public static let P521_SHA512_AES_GCM_256 = Ciphersuite(kem: .P521_HKDF_SHA512, kdf: .HKDF_SHA512, aead: .AES_GCM_256)
        public static let Curve25519_SHA256_ChachaPoly = Ciphersuite(kem: .Curve25519_HKDF_SHA256, kdf: .HKDF_SHA256, aead: .chaChaPoly)
    }

    // ---- primitivas da RFC 9180 sobre bytes
    static func i2osp(_ v: Int, _ n: Int) -> [UInt8] { (0..<n).map { UInt8((v >> (8 * (n - 1 - $0))) & 255) } }
    static let v1 = Array("HPKE-v1".utf8)
    static func labeledExtract(_ kdf: KDF, _ suite: [UInt8], _ salt: [UInt8], _ label: String, _ ikm: [UInt8]) -> [UInt8] {
        kdf.extract(salt, v1 + suite + Array(label.utf8) + ikm)
    }
    static func labeledExpand(_ kdf: KDF, _ suite: [UInt8], _ prk: [UInt8], _ label: String, _ info: [UInt8], _ n: Int) -> [UInt8] {
        kdf.expand(prk, i2osp(n, 2) + v1 + suite + Array(label.utf8) + info, n)
    }
    /* DH e serialização por KEM; as chaves andam como bytes (X25519 cru, NIST em x963) */
    static func generate(_ kem: KEM) -> (priv: [UInt8], pub: [UInt8]) {
        if kem == .Curve25519_HKDF_SHA256 { let k = Curve25519.KeyAgreement.PrivateKey(); return (k.b, k.publicKey.b) }
        let (d, q) = ECCore.generate(kem.bits, compact: false)
        return (d, q)
    }
    static func publicOf(_ kem: KEM, _ priv: [UInt8]) throws -> [UInt8] {
        if kem == .Curve25519_HKDF_SHA256 { return try Curve25519.KeyAgreement.PrivateKey(rawRepresentation: priv).publicKey.b }
        return try ECCore.publicFrom(priv, kem.bits)
    }
    static func dh(_ kem: KEM, _ priv: [UInt8], _ pub: [UInt8]) throws -> [UInt8] {
        if kem == .Curve25519_HKDF_SHA256 {
            let s = try Curve25519.KeyAgreement.PrivateKey(rawRepresentation: priv).sharedSecretFromKeyAgreement(with: try Curve25519.KeyAgreement.PublicKey(rawRepresentation: pub))
            return s.b
        }
        return try ECCore.shared(priv, try ECCore.point(pub, ECCore.size(kem.bits)), kem.bits)
    }
    static func kemSecret(_ kem: KEM, _ dh: [UInt8], _ ctx: [UInt8]) -> [UInt8] {
        let suite = Array("KEM".utf8) + i2osp(Int(kem.id), 2)
        let prk = labeledExtract(kem.kdf, suite, [], "eae_prk", dh)
        return labeledExpand(kem.kdf, suite, prk, "shared_secret", ctx, kem.nSecret)
    }
    struct Context: Sendable {
        let suite: Ciphersuite
        let key: [UInt8], baseNonce: [UInt8], exporter: [UInt8]
        var seq: UInt64 = 0
        init(_ cs: Ciphersuite, mode: UInt8, shared: [UInt8], info: [UInt8]) {
            suite = cs
            let sid = Array("HPKE".utf8) + i2osp(Int(cs.kem.id), 2) + i2osp(Int(cs.kdf.id), 2) + i2osp(Int(cs.aead.id), 2)
            let pskIdHash = labeledExtract(cs.kdf, sid, [], "psk_id_hash", [])
            let infoHash = labeledExtract(cs.kdf, sid, [], "info_hash", info)
            let ctx = [mode] + pskIdHash + infoHash
            let secret = labeledExtract(cs.kdf, sid, shared, "secret", [])
            key = cs.aead == .exportOnly ? [] : labeledExpand(cs.kdf, sid, secret, "key", ctx, cs.aead.nK)
            baseNonce = cs.aead == .exportOnly ? [] : labeledExpand(cs.kdf, sid, secret, "base_nonce", ctx, 12)
            exporter = labeledExpand(cs.kdf, sid, secret, "exp", ctx, cs.kdf.nH)
        }
        func nonce() -> [UInt8] {
            let s = i2osp(0, 4) + (0..<8).map { UInt8((seq >> (8 * (7 - UInt64($0)))) & 255) }
            return zip(baseNonce, s).map { $0 ^ $1 }
        }
        mutating func seal(_ pt: [UInt8], _ aad: [UInt8]) throws -> Data {
            guard suite.aead != .exportOnly else { throw Errors.exportOnlyMode }
            guard seq < UInt64.max else { throw Errors.outOfRangeSequenceNumber }
            let k = SymmetricKey(data: key), n = nonce()
            let out: Data
            if suite.aead == .chaChaPoly {
                let b = try ChaChaPoly.seal(pt, using: k, nonce: try ChaChaPoly.Nonce(data: n), authenticating: aad)
                out = b.ciphertext + b.tag
            } else {
                let b = try AES.GCM.seal(pt, using: k, nonce: try AES.GCM.Nonce(data: n), authenticating: aad)
                out = b.ciphertext + b.tag
            }
            seq += 1
            return out
        }
        mutating func open(_ ct: [UInt8], _ aad: [UInt8]) throws -> Data {
            guard suite.aead != .exportOnly else { throw Errors.exportOnlyMode }
            guard ct.count >= 16 else { throw Errors.ciphertextTooShort }
            let k = SymmetricKey(data: key), n = nonce(), body = Array(ct.prefix(ct.count - 16)), tag = Array(ct.suffix(16))
            let out: Data
            if suite.aead == .chaChaPoly {
                out = try ChaChaPoly.open(try ChaChaPoly.SealedBox(nonce: try ChaChaPoly.Nonce(data: n), ciphertext: body, tag: tag), using: k, authenticating: aad)
            } else {
                out = try AES.GCM.open(try AES.GCM.SealedBox(nonce: try AES.GCM.Nonce(data: n), ciphertext: body, tag: tag), using: k, authenticating: aad)
            }
            seq += 1
            return out
        }
        func export(_ context: [UInt8], _ n: Int) -> SymmetricKey {
            let sid = Array("HPKE".utf8) + i2osp(Int(suite.kem.id), 2) + i2osp(Int(suite.kdf.id), 2) + i2osp(Int(suite.aead.id), 2)
            return SymmetricKey(data: labeledExpand(suite.kdf, sid, exporter, "sec", context, n))
        }
    }

    public struct Sender: Sendable {
        var ctx: Context
        public let encapsulatedKey: Data
        /* modo base: Encap(pkR) */
        public init<PK: HPKEDiffieHellmanPublicKey>(recipientKey: PK, ciphersuite: Ciphersuite, info: Data) throws {
            let kem = ciphersuite.kem, pkR = [UInt8](try recipientKey.export(as: kem))
            let e = HPKE.generate(kem)
            let shared = HPKE.kemSecret(kem, try HPKE.dh(kem, e.priv, pkR), e.pub + pkR)
            ctx = Context(ciphersuite, mode: 0, shared: shared, info: [UInt8](info))
            encapsulatedKey = Data(e.pub)
        }
        /* modo auth: AuthEncap(pkR, skS) */
        public init<SK: HPKEDiffieHellmanPrivateKey>(recipientKey: SK.PublicKey, ciphersuite: Ciphersuite, info: Data, authenticatedBy authenticationKey: SK) throws {
            let kem = ciphersuite.kem, pkR = [UInt8](try recipientKey.export(as: kem)), pkS = [UInt8](try authenticationKey.publicKey.export(as: kem))
            let e = HPKE.generate(kem)
            let dhS = bytes(contiguous: try authenticationKey.sharedSecretFromKeyAgreement(with: recipientKey))
            let shared = HPKE.kemSecret(kem, try HPKE.dh(kem, e.priv, pkR) + dhS, e.pub + pkR + pkS)
            ctx = Context(ciphersuite, mode: 2, shared: shared, info: [UInt8](info))
            encapsulatedKey = Data(e.pub)
        }
        public mutating func seal<M: DataProtocol, AD: DataProtocol>(_ msg: M, authenticating aad: AD) throws -> Data { try ctx.seal(bytes(msg), bytes(aad)) }
        public mutating func seal<M: DataProtocol>(_ msg: M) throws -> Data { try ctx.seal(bytes(msg), []) }
        public func exportSecret<C: DataProtocol>(context: C, outputByteCount: Int) throws -> SymmetricKey { ctx.export(bytes(context), outputByteCount) }
    }
    public struct Recipient: Sendable {
        var ctx: Context
        public init<SK: HPKEDiffieHellmanPrivateKey>(privateKey: SK, ciphersuite: Ciphersuite, info: Data, encapsulatedKey: Data) throws {
            let kem = ciphersuite.kem, enc = [UInt8](encapsulatedKey), pkR = [UInt8](try privateKey.publicKey.export(as: kem))
            guard enc.count == kem.nEnc else { throw Errors.inconsistentParameters }
            let pkE = try SK.PublicKey(enc, kem: kem)
            let dh = bytes(contiguous: try privateKey.sharedSecretFromKeyAgreement(with: pkE))
            ctx = Context(ciphersuite, mode: 0, shared: HPKE.kemSecret(kem, dh, enc + pkR), info: [UInt8](info))
        }
        public init<SK: HPKEDiffieHellmanPrivateKey>(privateKey: SK, ciphersuite: Ciphersuite, info: Data, encapsulatedKey: Data, authenticatedBy authenticationKey: SK.PublicKey) throws {
            let kem = ciphersuite.kem, enc = [UInt8](encapsulatedKey), pkR = [UInt8](try privateKey.publicKey.export(as: kem))
            let pkS = [UInt8](try authenticationKey.export(as: kem))
            guard enc.count == kem.nEnc else { throw Errors.inconsistentParameters }
            let pkE = try SK.PublicKey(enc, kem: kem)
            let dh = bytes(contiguous: try privateKey.sharedSecretFromKeyAgreement(with: pkE)) + bytes(contiguous: try privateKey.sharedSecretFromKeyAgreement(with: authenticationKey))
            ctx = Context(ciphersuite, mode: 2, shared: HPKE.kemSecret(kem, dh, enc + pkR + pkS), info: [UInt8](info))
        }
        public mutating func open<C: DataProtocol, AD: DataProtocol>(_ ciphertext: C, authenticating aad: AD) throws -> Data { try ctx.open(bytes(ciphertext), bytes(aad)) }
        public mutating func open<C: DataProtocol>(_ ciphertext: C) throws -> Data { try ctx.open(bytes(ciphertext), []) }
        public func exportSecret<C: DataProtocol>(context: C, outputByteCount: Int) throws -> SymmetricKey { ctx.export(bytes(context), outputByteCount) }
    }
}

/* chaves que servem ao HPKE: a serialização é a da RFC 9180 (X25519 crua; NIST não comprimida) e o KEM tem de bater */
extension Curve25519.KeyAgreement.PublicKey: HPKEDiffieHellmanPublicKey {
    public init<D: ContiguousBytes>(_ serialization: D, kem: HPKE.KEM) throws {
        guard kem == .Curve25519_HKDF_SHA256 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        try self.init(rawRepresentation: serialization)
    }
    public func export(as kem: HPKE.KEM) throws -> Data {
        guard kem == .Curve25519_HKDF_SHA256 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        return rawRepresentation
    }
}
extension Curve25519.KeyAgreement.PrivateKey: HPKEDiffieHellmanPrivateKeyGeneration {}

extension P256.KeyAgreement.PublicKey: HPKEDiffieHellmanPublicKey {
    public init<D: ContiguousBytes>(_ serialization: D, kem: HPKE.KEM) throws {
        guard kem == .P256_HKDF_SHA256 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        try self.init(x963Representation: serialization)
    }
    public func export(as kem: HPKE.KEM) throws -> Data {
        guard kem == .P256_HKDF_SHA256 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        return x963Representation
    }
}
extension P256.KeyAgreement.PrivateKey: HPKEDiffieHellmanPrivateKeyGeneration {
    public init() { self.init(compactRepresentable: false) }
}

extension P384.KeyAgreement.PublicKey: HPKEDiffieHellmanPublicKey {
    public init<D: ContiguousBytes>(_ serialization: D, kem: HPKE.KEM) throws {
        guard kem == .P384_HKDF_SHA384 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        try self.init(x963Representation: serialization)
    }
    public func export(as kem: HPKE.KEM) throws -> Data {
        guard kem == .P384_HKDF_SHA384 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        return x963Representation
    }
}
extension P384.KeyAgreement.PrivateKey: HPKEDiffieHellmanPrivateKeyGeneration {
    public init() { self.init(compactRepresentable: false) }
}

extension P521.KeyAgreement.PublicKey: HPKEDiffieHellmanPublicKey {
    public init<D: ContiguousBytes>(_ serialization: D, kem: HPKE.KEM) throws {
        guard kem == .P521_HKDF_SHA512 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        try self.init(x963Representation: serialization)
    }
    public func export(as kem: HPKE.KEM) throws -> Data {
        guard kem == .P521_HKDF_SHA512 else { throw HPKE.Errors.inconsistentCiphersuiteAndKey }
        return x963Representation
    }
}
extension P521.KeyAgreement.PrivateKey: HPKEDiffieHellmanPrivateKeyGeneration {
    public init() { self.init(compactRepresentable: false) }
}
