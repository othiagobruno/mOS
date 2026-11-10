// mOS — CryptoKit.framework (só Swift, como o da Apple). O carregador resolve o import
// /System/Library/Frameworks/CryptoKit.framework nas imagens de tools/build-frameworks-objc.ps1
// (-enable-library-evolution, -module-name CryptoKit). Os nomes mangled têm de ser os da Apple:
// o machocheck dos apps-alvo (nov. 2026) importa 68 símbolos distintos; o do VS Code 1.145.1
// são 39 (46 imports). Compilado contra o Foundation de ABI "Foundation", o objeto exporta
// esses nomes. O carregador abre o Mach-O em
// /System/Library/Frameworks/CryptoKit.framework/Versions/A/CryptoKit, e o machocheck
// --disco conta os exports desse arquivo. O build.ps1 copia a saída de
// tools/build-frameworks-objc.ps1. A trie de tools/ckhost/mkexports.py não entra na imagem.
//
// Funcional (primitiva real, falha de verdade):
//   SHA-256 (CC_SHA256_* do CommonCrypto), Insecure.MD5 (CC_MD5_*), HMAC e HKDF sobre o HashFunction,
//   AES-GCM 128/192/256 e ChaCha20-Poly1305 (mos_ck_* → kernel/net/crypto.c), SymmetricKey com
//   mos_ck_random. Tag GCM/Poly1305 errada lança CryptoKitError.authenticationFailure e não devolve
//   texto. Tamanho de chave ou de nonce fora do contrato lança incorrectKeySize/incorrectParameterSize.
//
// Não entra aqui (nenhum app-alvo importa): SHA-384/512, Insecure.SHA1, AES.KeyWrap, Curve25519,
// P-256/P-384/P-521, HPKE, Secure Enclave. Esses continuam no módulo do SDK
// (userland/sdk/openswiftui/swift/CryptoKit), que os apps compilados pelo mOS usam.
//
// Limitação: não há Secure Enclave. O AES do kernel é a versão com S-box em tabela (não é constante
// no tempo de cache); ChaCha20-Poly1305 e o GHASH do GCM são. O Data/DataProtocol/ContiguousBytes
// vêm do overlay Foundation com -module-abi-name Foundation ($s10Foundation4DataV).

import Swift
@_exported import CryptoKit
import Foundation

@_silgen_name("CC_SHA256_Init") private func _cc_sha256_init(_ c: UnsafeMutableRawPointer) -> Int32
@_silgen_name("CC_SHA256_Update") private func _cc_sha256_update(_ c: UnsafeMutableRawPointer, _ d: UnsafeRawPointer?, _ n: UInt32) -> Int32
@_silgen_name("CC_SHA256_Final") private func _cc_sha256_final(_ md: UnsafeMutableRawPointer, _ c: UnsafeMutableRawPointer) -> Int32
@_silgen_name("CC_MD5_Init") private func _cc_md5_init(_ c: UnsafeMutableRawPointer) -> Int32
@_silgen_name("CC_MD5_Update") private func _cc_md5_update(_ c: UnsafeMutableRawPointer, _ d: UnsafeRawPointer?, _ n: UInt32) -> Int32
@_silgen_name("CC_MD5_Final") private func _cc_md5_final(_ md: UnsafeMutableRawPointer, _ c: UnsafeMutableRawPointer) -> Int32
@_silgen_name("mos_ck_aes_gcm_seal") private func ck_gcm_seal(_ k: UnsafePointer<UInt8>?, _ kl: Int, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ o: UnsafeMutablePointer<UInt8>?, _ t: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_aes_gcm_open") private func ck_gcm_open(_ k: UnsafePointer<UInt8>?, _ kl: Int, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ t: UnsafePointer<UInt8>?, _ o: UnsafeMutablePointer<UInt8>?) -> Int32
@_silgen_name("mos_ck_chachapoly_seal") private func ck_cc_seal(_ k: UnsafePointer<UInt8>?, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ o: UnsafeMutablePointer<UInt8>?, _ t: UnsafeMutablePointer<UInt8>) -> Int32
@_silgen_name("mos_ck_chachapoly_open") private func ck_cc_open(_ k: UnsafePointer<UInt8>?, _ n: UnsafePointer<UInt8>?, _ a: UnsafePointer<UInt8>?, _ al: Int, _ i: UnsafePointer<UInt8>?, _ il: Int, _ t: UnsafePointer<UInt8>?, _ o: UnsafeMutablePointer<UInt8>?) -> Int32
@_silgen_name("mos_ck_random") private func ck_random(_ buf: UnsafeMutableRawPointer, _ n: Int)

private func bytes<D: DataProtocol>(_ d: D) -> [UInt8] { Array(Data(d)) }
private func randomBytes(_ n: Int) -> [UInt8] {
    var a = [UInt8](repeating: 0, count: n)
    if n > 0 { a.withUnsafeMutableBytes { ck_random($0.baseAddress!, n) } }
    return a
}
private func hexString(_ b: [UInt8]) -> String {
    let h: [Character] = Array("0123456789abcdef")
    var s = ""
    for x in b { s.append(h[Int(x >> 4)]); s.append(h[Int(x & 15)]) }
    return s
}
private func ccUpdate(_ ctx: UnsafeMutableRawPointer, _ data: UnsafeRawBufferPointer, _ step: (UnsafeMutableRawPointer, UnsafeRawPointer?, UInt32) -> Int32) {
    guard let base = data.baseAddress, data.count > 0 else { return }
    var off = 0
    while off < data.count {
        let n = min(data.count - off, 1 << 30)
        _ = step(ctx, base + off, UInt32(n))
        off += n
    }
}

// MARK: - resumos

public protocol Digest: Hashable, ContiguousBytes, CustomStringConvertible, Sequence, Sendable where Element == UInt8 {
    static var byteCount: Int { get }
}
extension Digest {
    public func makeIterator() -> Array<UInt8>.Iterator {
        var a = [UInt8]()
        a.reserveCapacity(Self.byteCount)
        withUnsafeBytes { a.append(contentsOf: $0) }
        return a.makeIterator()
    }
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
        let b = bytes(data)
        b.withUnsafeBytes { update(bufferPointer: $0) }
    }
    public static func hash<D: DataProtocol>(data: D) -> Self.Digest {
        var h = Self()
        h.update(data: data)
        return h.finalize()
    }
}

public struct SHA256Digest: Digest {
    var b: (UInt64, UInt64, UInt64, UInt64) = (0, 0, 0, 0)
    public static var byteCount: Int { 32 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try Swift.withUnsafeBytes(of: b, body) }
    public var description: String { "SHA256 digest: " + hexString(withUnsafeBytes { Array($0) }) }
    public static func == (l: SHA256Digest, r: SHA256Digest) -> Bool { l.b == r.b }
    public func hash(into h: inout Hasher) { h.combine(b.0); h.combine(b.1); h.combine(b.2); h.combine(b.3) }
}

public struct SHA256: HashFunction {
    public typealias Digest = SHA256Digest
    public static var blockByteCount: Int { 64 }
    // CC_SHA256_CTX: count[2], hash[8], wbuf[16] (u32) = 104 bytes
    var ctx: (UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64) =
        (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    public init() { _ = Swift.withUnsafeMutableBytes(of: &ctx) { _cc_sha256_init($0.baseAddress!) } }
    public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
        Swift.withUnsafeMutableBytes(of: &ctx) { ccUpdate($0.baseAddress!, bufferPointer, _cc_sha256_update) }
    }
    public func finalize() -> SHA256Digest {
        var c = ctx
        var d = SHA256Digest()
        Swift.withUnsafeMutableBytes(of: &d.b) { md in _ = Swift.withUnsafeMutableBytes(of: &c) { _cc_sha256_final(md.baseAddress!, $0.baseAddress!) } }
        return d
    }
}

public enum Insecure {
    public struct MD5Digest: CryptoKit.Digest {
        var b: (UInt64, UInt64) = (0, 0)
        public static var byteCount: Int { 16 }
        public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try Swift.withUnsafeBytes(of: b, body) }
        public var description: String { "MD5 digest: " + hexString(withUnsafeBytes { Array($0) }) }
        public static func == (l: MD5Digest, r: MD5Digest) -> Bool { l.b == r.b }
        public func hash(into h: inout Hasher) { h.combine(b.0); h.combine(b.1) }
    }
    public struct MD5: HashFunction {
        public typealias Digest = MD5Digest
        public static var blockByteCount: Int { 64 }
        // CC_MD5_CTX: A,B,C,D,Nl,Nh + data[16] + num = 92 bytes
        var ctx: (UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64, UInt64) =
            (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        public init() { _ = Swift.withUnsafeMutableBytes(of: &ctx) { _cc_md5_init($0.baseAddress!) } }
        public mutating func update(bufferPointer: UnsafeRawBufferPointer) {
            Swift.withUnsafeMutableBytes(of: &ctx) { ccUpdate($0.baseAddress!, bufferPointer, _cc_md5_update) }
        }
        public func finalize() -> MD5Digest {
            var c = ctx
            var d = MD5Digest()
            Swift.withUnsafeMutableBytes(of: &d.b) { md in _ = Swift.withUnsafeMutableBytes(of: &c) { _cc_md5_final(md.baseAddress!, $0.baseAddress!) } }
            return d
        }
    }
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

// MARK: - chaves simétricas

public struct SymmetricKeySize: Sendable {
    public let bitCount: Int
    public init(bitCount: Int) {
        precondition(bitCount > 0 && bitCount % 8 == 0)
        self.bitCount = bitCount
    }
    public static var bits128: SymmetricKeySize { .init(bitCount: 128) }
    public static var bits192: SymmetricKeySize { .init(bitCount: 192) }
    public static var bits256: SymmetricKeySize { .init(bitCount: 256) }
}

public struct SymmetricKey: ContiguousBytes, Sendable {
    let b: [UInt8]
    public init<D: ContiguousBytes>(data: D) { b = data.withUnsafeBytes { Array($0) } }
    public init(size: SymmetricKeySize) { b = randomBytes(size.bitCount / 8) }
    public var bitCount: Int { b.count * 8 }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
}

// MARK: - HMAC e HKDF

public struct HashedAuthenticationCode<H: HashFunction>: ContiguousBytes, Sequence, Hashable, Sendable {
    let b: [UInt8]
    public var byteCount: Int { b.count }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try b.withUnsafeBytes(body) }
    public func makeIterator() -> Array<UInt8>.Iterator { b.makeIterator() }
    public static func == (l: Self, r: Self) -> Bool { l.b == r.b }
    public func hash(into hasher: inout Hasher) { hasher.combine(b) }
}

public struct HMAC<H: HashFunction> {
    /// RFC 2104. Chave maior que o bloco vira H(chave); o resto é preenchido com zeros.
    public static func authenticationCode<D: DataProtocol>(for data: D, using key: SymmetricKey) -> HashedAuthenticationCode<H> {
        let bs = H.blockByteCount
        var k = key.b
        if k.count > bs { k = H.hash(data: Data(k)).withUnsafeBytes { Array($0) } }
        if k.count < bs { k += [UInt8](repeating: 0, count: bs - k.count) }
        let ipad = k.map { $0 ^ 0x36 }
        let opad = k.map { $0 ^ 0x5c }
        var inner = H()
        inner.update(data: Data(ipad))
        inner.update(data: data)
        var outer = H()
        outer.update(data: Data(opad))
        let innerDigest = inner.finalize().withUnsafeBytes { Data($0) }
        outer.update(data: innerDigest)
        return HashedAuthenticationCode(b: outer.finalize().withUnsafeBytes { Array($0) })
    }
}

public struct HKDF<H: HashFunction> {
    /// RFC 5869. Salt vazio vira HashLen bytes zero. outputByteCount fora de 1...255*HashLen é erro do chamador (precondition), não um segredo falso.
    public static func deriveKey<Salt: DataProtocol, Info: DataProtocol>(inputKeyMaterial: SymmetricKey, salt: Salt, info: Info, outputByteCount: Int) -> SymmetricKey {
        let hashLen = H.Digest.byteCount
        precondition(outputByteCount > 0 && outputByteCount <= 255 * hashLen, "HKDF: outputByteCount inválido")
        var saltBytes = bytes(salt)
        if saltBytes.isEmpty { saltBytes = [UInt8](repeating: 0, count: hashLen) }
        let prk = HMAC<H>.authenticationCode(for: Data(inputKeyMaterial.b), using: SymmetricKey(data: Data(saltBytes)))
        let infoBytes = bytes(info)
        let prkKey = SymmetricKey(data: Data(prk.b))
        var out = [UInt8](), prev = [UInt8](), c: UInt8 = 1
        while out.count < outputByteCount {
            var msg = prev
            msg += infoBytes
            msg.append(c)
            prev = HMAC<H>.authenticationCode(for: Data(msg), using: prkKey).b
            out += prev
            c &+= 1
        }
        return SymmetricKey(data: Data(out.prefix(outputByteCount)))
    }
}

// MARK: - AES-GCM e ChaCha20-Poly1305

private func keyBytes(_ key: SymmetricKey) -> [UInt8] { key.b }

private func gcmSeal(key: [UInt8], nonce: [UInt8], aad: [UInt8], pt: [UInt8]) throws -> (ct: [UInt8], tag: [UInt8]) {
    guard key.count == 16 || key.count == 24 || key.count == 32 else { throw CryptoKitError.incorrectKeySize }
    guard nonce.count == 12 else { throw CryptoKitError.incorrectParameterSize }
    var ct = [UInt8](repeating: 0, count: pt.count)
    var tag = [UInt8](repeating: 0, count: 16)
    let r = key.withUnsafeBufferPointer { k in nonce.withUnsafeBufferPointer { n in aad.withUnsafeBufferPointer { a in pt.withUnsafeBufferPointer { p in
        ct.withUnsafeMutableBufferPointer { c in tag.withUnsafeMutableBufferPointer { t in
            ck_gcm_seal(k.baseAddress, k.count, n.baseAddress, a.baseAddress, a.count, p.baseAddress, p.count, c.baseAddress, t.baseAddress!)
        } }
    } } } }
    guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
    return (ct, tag)
}
private func gcmOpen(key: [UInt8], nonce: [UInt8], aad: [UInt8], ct: [UInt8], tag: [UInt8]) throws -> [UInt8] {
    guard key.count == 16 || key.count == 24 || key.count == 32 else { throw CryptoKitError.incorrectKeySize }
    guard nonce.count == 12, tag.count == 16 else { throw CryptoKitError.incorrectParameterSize }
    var pt = [UInt8](repeating: 0, count: ct.count)
    let r = key.withUnsafeBufferPointer { k in nonce.withUnsafeBufferPointer { n in aad.withUnsafeBufferPointer { a in ct.withUnsafeBufferPointer { c in
        tag.withUnsafeBufferPointer { t in pt.withUnsafeMutableBufferPointer { o in
            ck_gcm_open(k.baseAddress, k.count, n.baseAddress, a.baseAddress, a.count, c.baseAddress, c.count, t.baseAddress, o.baseAddress)
        } }
    } } } }
    // o núcleo confere a tag antes de escrever o texto; -1 não é sucesso
    guard r == 0 else { throw CryptoKitError.authenticationFailure }
    return pt
}
private func ccSeal(key: [UInt8], nonce: [UInt8], aad: [UInt8], pt: [UInt8]) throws -> (ct: [UInt8], tag: [UInt8]) {
    guard key.count == 32 else { throw CryptoKitError.incorrectKeySize }
    guard nonce.count == 12 else { throw CryptoKitError.incorrectParameterSize }
    var ct = [UInt8](repeating: 0, count: pt.count)
    var tag = [UInt8](repeating: 0, count: 16)
    let r = key.withUnsafeBufferPointer { k in nonce.withUnsafeBufferPointer { n in aad.withUnsafeBufferPointer { a in pt.withUnsafeBufferPointer { p in
        ct.withUnsafeMutableBufferPointer { c in tag.withUnsafeMutableBufferPointer { t in
            ck_cc_seal(k.baseAddress, n.baseAddress, a.baseAddress, a.count, p.baseAddress, p.count, c.baseAddress, t.baseAddress!)
        } }
    } } } }
    guard r == 0 else { throw CryptoKitError.underlyingCoreCryptoError(error: r) }
    return (ct, tag)
}
private func ccOpen(key: [UInt8], nonce: [UInt8], aad: [UInt8], ct: [UInt8], tag: [UInt8]) throws -> [UInt8] {
    guard key.count == 32 else { throw CryptoKitError.incorrectKeySize }
    guard nonce.count == 12, tag.count == 16 else { throw CryptoKitError.incorrectParameterSize }
    var pt = [UInt8](repeating: 0, count: ct.count)
    let r = key.withUnsafeBufferPointer { k in nonce.withUnsafeBufferPointer { n in aad.withUnsafeBufferPointer { a in ct.withUnsafeBufferPointer { c in
        tag.withUnsafeBufferPointer { t in pt.withUnsafeMutableBufferPointer { o in
            ck_cc_open(k.baseAddress, n.baseAddress, a.baseAddress, a.count, c.baseAddress, c.count, t.baseAddress, o.baseAddress)
        } }
    } } } }
    guard r == 0 else { throw CryptoKitError.authenticationFailure }
    return pt
}

public enum AES {
    public enum GCM {
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
            public var combined: Data? {
                guard nonce.b.count == 12 else { return nil }
                return Data(nonce.b) + ciphertext + tag
            }
            public init<D: DataProtocol>(combined: D) throws {
                let x = bytes(combined)
                guard x.count >= 28 else { throw CryptoKitError.incorrectParameterSize }
                nonce = try Nonce(data: Data(x[0..<12]))
                ciphertext = Data(x[12..<(x.count - 16)])
                tag = Data(x[(x.count - 16)...])
            }
            public init<C: DataProtocol, T: DataProtocol>(nonce: Nonce, ciphertext: C, tag: T) throws {
                let t = bytes(tag)
                guard t.count == 16 else { throw CryptoKitError.incorrectParameterSize }
                self.nonce = nonce
                self.ciphertext = Data(ciphertext)
                self.tag = Data(t)
            }
            init(nonce: Nonce, ct: [UInt8], tag: [UInt8]) {
                self.nonce = nonce
                ciphertext = Data(ct)
                self.tag = Data(tag)
            }
        }
        public static func seal<Plaintext: DataProtocol, AuthenticatedData: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil, authenticating authenticatedData: AuthenticatedData) throws -> SealedBox {
            let n = nonce ?? Nonce()
            let s = try gcmSeal(key: keyBytes(key), nonce: n.b, aad: bytes(authenticatedData), pt: bytes(message))
            return SealedBox(nonce: n, ct: s.ct, tag: s.tag)
        }
        public static func seal<Plaintext: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil) throws -> SealedBox {
            try seal(message, using: key, nonce: nonce, authenticating: Data())
        }
        public static func open<AuthenticatedData: DataProtocol>(_ box: SealedBox, using key: SymmetricKey, authenticating authenticatedData: AuthenticatedData) throws -> Data {
            Data(try gcmOpen(key: keyBytes(key), nonce: box.nonce.b, aad: bytes(authenticatedData), ct: bytes(box.ciphertext), tag: bytes(box.tag)))
        }
        public static func open(_ box: SealedBox, using key: SymmetricKey) throws -> Data {
            try open(box, using: key, authenticating: Data())
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
            nonce = try Nonce(data: Data(x[0..<12]))
            ciphertext = Data(x[12..<(x.count - 16)])
            tag = Data(x[(x.count - 16)...])
        }
        public init<C: DataProtocol, T: DataProtocol>(nonce: Nonce, ciphertext: C, tag: T) throws {
            let t = bytes(tag)
            guard t.count == 16 else { throw CryptoKitError.incorrectParameterSize }
            self.nonce = nonce
            self.ciphertext = Data(ciphertext)
            self.tag = Data(t)
        }
        init(nonce: Nonce, ct: [UInt8], tag: [UInt8]) {
            self.nonce = nonce
            ciphertext = Data(ct)
            self.tag = Data(tag)
        }
    }
    public static func seal<Plaintext: DataProtocol, AuthenticatedData: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil, authenticating authenticatedData: AuthenticatedData) throws -> SealedBox {
        let n = nonce ?? Nonce()
        let s = try ccSeal(key: keyBytes(key), nonce: n.b, aad: bytes(authenticatedData), pt: bytes(message))
        return SealedBox(nonce: n, ct: s.ct, tag: s.tag)
    }
    public static func seal<Plaintext: DataProtocol>(_ message: Plaintext, using key: SymmetricKey, nonce: Nonce? = nil) throws -> SealedBox {
        try seal(message, using: key, nonce: nonce, authenticating: Data())
    }
    public static func open<AuthenticatedData: DataProtocol>(_ box: SealedBox, using key: SymmetricKey, authenticating authenticatedData: AuthenticatedData) throws -> Data {
        Data(try ccOpen(key: keyBytes(key), nonce: box.nonce.b, aad: bytes(authenticatedData), ct: bytes(box.ciphertext), tag: bytes(box.tag)))
    }
    public static func open(_ box: SealedBox, using key: SymmetricKey) throws -> Data {
        try open(box, using: key, authenticating: Data())
    }
}
