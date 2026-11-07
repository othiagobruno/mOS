// Cripto — teste do CryptoKit do mOS (hash, HMAC, HKDF, AES-GCM, ChaChaPoly, Curve25519, P-256/384/521)
import Foundation
import CryptoKit

var falhas = 0
func check(_ nome: String, _ ok: Bool) { print((ok ? "ok     " : "FALHOU ") + nome); if !ok { falhas += 1 } }
func hex<S: Sequence>(_ s: S) -> String where S.Element == UInt8 { s.map { String($0, radix: 16).count == 1 ? "0" + String($0, radix: 16) : String($0, radix: 16) }.joined() }

@main enum CriptoTeste {
static func main() {
let abc = Data("abc".utf8)
check("SHA256", hex(SHA256.hash(data: abc)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
check("SHA384", hex(SHA384.hash(data: abc)).hasPrefix("cb00753f45a35e8bb5a03d699ac65007"))
check("SHA512", hex(SHA512.hash(data: abc)).hasPrefix("ddaf35a193617aba"))
check("Insecure.MD5", hex(Insecure.MD5.hash(data: abc)) == "900150983cd24fb0d6963f7d28e17f72")
check("Insecure.SHA1", hex(Insecure.SHA1.hash(data: abc)) == "a9993e364706816aba3e25717850c26c9cd0d89d")
var inc = SHA256(); inc.update(data: Data("a".utf8)); let copia = inc; inc.update(data: Data("bc".utf8))
check("SHA256 incremental e cópia de valor", hex(inc.finalize()) == hex(SHA256.hash(data: abc)) && hex(copia.finalize()) == hex(SHA256.hash(data: Data("a".utf8))))
let jefe = SymmetricKey(data: Data("Jefe".utf8))
let mac = HMAC<SHA256>.authenticationCode(for: Data("what do ya want for nothing?".utf8), using: jefe)
check("HMAC<SHA256> (RFC 4231)", hex(mac) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843")
check("HMAC validação", HMAC<SHA256>.isValidAuthenticationCode(mac, authenticating: Data("what do ya want for nothing?".utf8), using: jefe))
let ikm = SymmetricKey(data: Data(repeating: 0x0b, count: 22))
let salt = Data((0..<13).map { UInt8($0) }), info = Data((0..<10).map { UInt8(0xf0 + $0) })
let okm = HKDF<SHA256>.deriveKey(inputKeyMaterial: ikm, salt: salt, info: info, outputByteCount: 42)
check("HKDF<SHA256> (RFC 5869)", okm.withUnsafeBytes { hex($0) } == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865")
let chave = SymmetricKey(size: .bits256), msg = Data("mensagem secreta do mOS".utf8)
do {
    let box = try AES.GCM.seal(msg, using: chave, authenticating: Data("cabeçalho".utf8))
    let volta = try AES.GCM.open(try AES.GCM.SealedBox(combined: box.combined!), using: chave, authenticating: Data("cabeçalho".utf8))
    check("AES.GCM ida e volta", volta == msg)
    var ruim = box.combined!; ruim[13] ^= 1
    check("AES.GCM recusa adulterado", (try? AES.GCM.open(try AES.GCM.SealedBox(combined: ruim), using: chave, authenticating: Data("cabeçalho".utf8))) == nil)
    let cbox = try ChaChaPoly.seal(msg, using: chave)
    check("ChaChaPoly ida e volta", try ChaChaPoly.open(try ChaChaPoly.SealedBox(combined: cbox.combined), using: chave) == msg)
    let kek = SymmetricKey(data: Data((0..<16).map { UInt8($0) }))
    let wrapped = try AES.KeyWrap.wrap(SymmetricKey(data: Data((0..<16).map { UInt8($0 * 17) })), using: kek)
    check("AES.KeyWrap (RFC 3394)", hex(wrapped) == "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5")
    check("AES.KeyWrap unwrap", try AES.KeyWrap.unwrap(wrapped, using: kek) == SymmetricKey(data: Data((0..<16).map { UInt8($0 * 17) })))
    let alice = Curve25519.KeyAgreement.PrivateKey(), bob = Curve25519.KeyAgreement.PrivateKey()
    let s1 = try alice.sharedSecretFromKeyAgreement(with: bob.publicKey), s2 = try bob.sharedSecretFromKeyAgreement(with: alice.publicKey)
    check("Curve25519.KeyAgreement", s1 == s2)
    let k1 = s1.hkdfDerivedSymmetricKey(using: SHA256.self, salt: Data(), sharedInfo: Data("mOS".utf8), outputByteCount: 32)
    let k2 = s2.hkdfDerivedSymmetricKey(using: SHA256.self, salt: Data(), sharedInfo: Data("mOS".utf8), outputByteCount: 32)
    check("SharedSecret.hkdfDerivedSymmetricKey", k1 == k2)
    let ed = try Curve25519.Signing.PrivateKey(rawRepresentation: Data([0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60, 0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4, 0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19, 0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60]))
    check("Curve25519.Signing chave (RFC 8032)", hex(ed.publicKey.rawRepresentation) == "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a")
    let sig = try ed.signature(for: msg)
    check("Curve25519.Signing assina e verifica", ed.publicKey.isValidSignature(sig, for: msg) && !ed.publicKey.isValidSignature(sig, for: abc))
    let p = P256.Signing.PrivateKey()
    let ps = try p.signature(for: msg)
    check("P256 ECDSA", p.publicKey.isValidSignature(ps, for: msg) && !p.publicKey.isValidSignature(ps, for: abc))
    check("P256 DER da assinatura", p.publicKey.isValidSignature(try P256.Signing.ECDSASignature(derRepresentation: ps.derRepresentation), for: msg))
    check("P256 chave em PEM", try P256.Signing.PublicKey(pemRepresentation: p.publicKey.pemRepresentation).rawRepresentation == p.publicKey.rawRepresentation)
    let pa = P384.KeyAgreement.PrivateKey(), pb = P384.KeyAgreement.PrivateKey()
    check("P384 ECDH", try pa.sharedSecretFromKeyAgreement(with: pb.publicKey) == pb.sharedSecretFromKeyAgreement(with: pa.publicKey))
    let q = P521.Signing.PrivateKey()
    check("P521 ECDSA", q.publicKey.isValidSignature(try q.signature(for: msg), for: msg))
    let r = try P256.Signing.PrivateKey(rawRepresentation: p.rawRepresentation)
    check("P256 importar chave privada", r.publicKey.x963Representation == p.publicKey.x963Representation)
} catch {
    check("sem exceções: \(error)", false)
}
print(falhas == 0 ? "CryptoKit: TUDO OK" : "CryptoKit: \(falhas) falha(s)")
}}
