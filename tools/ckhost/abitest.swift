// Vetores conhecidos da ABI Swift do CryptoKit.framework (o que os apps-alvo importam).
// Liga no CC_SHA256_*/CC_MD5_* do CommonCrypto e no AES-GCM/ChaCha20-Poly1305 de kernel/net/crypto.c.
import CryptoKit
import Foundation
import Glibc

var fails = 0
func check(_ name: String, _ ok: Bool) {
    print(name + (ok ? " ok" : " FALHOU"))
    if !ok { fails += 1 }
}
func hex(_ d: [UInt8]) -> String {
    let h: [Character] = Array("0123456789abcdef")
    var s = ""
    for x in d { s.append(h[Int(x >> 4)]); s.append(h[Int(x & 15)]) }
    return s
}
func rep(_ n: Int, _ v: UInt8) -> Data { Data([UInt8](repeating: v, count: n)) }
func unhex(_ s: String) -> [UInt8] {
    var o: [UInt8] = []
    var i = s.startIndex
    while i < s.endIndex {
        let j = s.index(i, offsetBy: 2)
        o.append(UInt8(s[i..<j], radix: 16)!)
        i = j
    }
    return o
}
func bytes<T: ContiguousBytes>(_ x: T) -> [UInt8] { x.withUnsafeBytes { Array($0) } }

func testHash() {
    let abc = Data("abc".utf8)
    check("SHA256(\"abc\")", hex(bytes(SHA256.hash(data: abc))) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    check("SHA256(\"\")", hex(bytes(SHA256.hash(data: Data()))) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
    var h = SHA256()
    let long = rep(100, 0x61)
    h.update(data: Data(long.prefix(40)))
    h.update(data: Data(long.suffix(60)))
    check("SHA256 incremental", h.finalize() == SHA256.hash(data: long))
    check("MD5(\"abc\")", hex(bytes(Insecure.MD5.hash(data: abc))) == "900150983cd24fb0d6963f7d28e17f72")
    check("MD5(\"\")", hex(bytes(Insecure.MD5.hash(data: Data()))) == "d41d8cd98f00b204e9800998ecf8427e")
}

func testMac() {
    let key = SymmetricKey(data: rep(20, 0x0b))
    let mac = HMAC<SHA256>.authenticationCode(for: Data("Hi There".utf8), using: key)
    check("HMAC-SHA256 RFC 4231", hex(bytes(mac)) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7")
    let ikm = SymmetricKey(data: rep(22, 0x0b))
    let okm = HKDF<SHA256>.deriveKey(inputKeyMaterial: ikm, salt: Data(unhex("000102030405060708090a0b0c")), info: Data(unhex("f0f1f2f3f4f5f6f7f8f9")), outputByteCount: 42)
    check("HKDF-SHA256 RFC 5869", hex(bytes(okm)) == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865")
}

func testGCM() throws {
    let key = SymmetricKey(data: rep(16, 0))
    let nonce = try AES.GCM.Nonce(data: rep(12, 0))
    let box = try AES.GCM.seal(Data(), using: key, nonce: nonce)
    check("AES-128-GCM tag NIST", hex(bytes(box.tag)) == "58e2fccefa7e3061367f1d57a4e7455a")
    check("AES-128-GCM open", try AES.GCM.open(box, using: key) == Data())
    let bad = try AES.GCM.SealedBox(nonce: nonce, ciphertext: box.ciphertext, tag: rep(16, 0))
    var refused = false
    do { _ = try AES.GCM.open(bad, using: key) } catch CryptoKitError.authenticationFailure { refused = true } catch { refused = false }
    check("AES-128-GCM recusa tag", refused)
    let k192 = SymmetricKey(data: rep(24, 0))
    let k256 = SymmetricKey(data: rep(32, 0))
    check("AES-192-GCM tag NIST", hex(bytes(try AES.GCM.seal(Data(), using: k192, nonce: nonce).tag)) == "cd33b28ac773f74ba00ed1f312572435")
    check("AES-256-GCM tag NIST", hex(bytes(try AES.GCM.seal(Data(), using: k256, nonce: nonce).tag)) == "530f8afbc74536b9a963b4f1c4cb738b")
    var wrongKey = false
    do { _ = try AES.GCM.seal(Data([1, 2, 3]), using: SymmetricKey(data: rep(15, 0)), nonce: nonce) }
    catch CryptoKitError.incorrectKeySize { wrongKey = true } catch { wrongKey = false }
    check("AES chave de 15 bytes", wrongKey)
    let msg = Data("mOS".utf8)
    let aad = Data("aad".utf8)
    let sealed = try AES.GCM.seal(msg, using: k256, nonce: nonce, authenticating: aad)
    check("AES-GCM com AAD", try AES.GCM.open(sealed, using: k256, authenticating: aad) == msg)
    let combined = try AES.GCM.SealedBox(combined: sealed.combined!)
    check("AES-GCM combined", try AES.GCM.open(combined, using: k256, authenticating: aad) == msg)
}

func testChaCha() throws {
    let key = SymmetricKey(data: Data(unhex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f")))
    let nonce = try ChaChaPoly.Nonce(data: Data(unhex("070000004041424344454647")))
    let aad = Data(unhex("50515253c0c1c2c3c4c5c6c7"))
    let pt = Data("Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would it be?".utf8)
    let box = try ChaChaPoly.seal(pt, using: key, nonce: nonce, authenticating: aad)
    check("ChaChaPoly RFC 8439 texto", hex(bytes(box.ciphertext)) == "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d2658ddfc6407007")
    check("ChaChaPoly RFC 8439 tag", hex(bytes(box.tag)) == "045a97c8077b2505b9aaa3b1c14df55f")
    check("ChaChaPoly abre", try ChaChaPoly.open(box, using: key, authenticating: aad) == pt)
    let flipped = Data([box.ciphertext[box.ciphertext.startIndex] ^ 1]) + Data(box.ciphertext.dropFirst())
    let bad = try ChaChaPoly.SealedBox(nonce: nonce, ciphertext: flipped, tag: box.tag)
    var refused = false
    do { _ = try ChaChaPoly.open(bad, using: key, authenticating: aad) } catch CryptoKitError.authenticationFailure { refused = true } catch { refused = false }
    check("ChaChaPoly recusa texto", refused)
    var wrongKey = false
    do { _ = try ChaChaPoly.seal(pt, using: SymmetricKey(data: rep(16, 1)), nonce: nonce) }
    catch CryptoKitError.incorrectKeySize { wrongKey = true } catch { wrongKey = false }
    check("ChaChaPoly chave curta", wrongKey)
}

testHash()
testMac()
do {
    try testGCM()
    try testChaCha()
} catch {
    print("excecao inesperada: \(error)")
    fails += 1
}
print(fails == 0 ? "TUDO OK" : "FALHAS")
exit(fails == 0 ? 0 : 1)
