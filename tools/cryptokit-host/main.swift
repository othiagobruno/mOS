import Foundation
var ok = 0, total = 0
func check(_ c: Bool, _ w: String) { total += 1; if c { ok += 1 } else { print("FALHA:", w) } }
func hex(_ d: some ContiguousBytes) -> String { d.withUnsafeBytes { $0.map { String(format: "%02x", $0) }.joined() } }
func unhex(_ s: String) -> [UInt8] { var a = [UInt8](); var i = s.startIndex; while i < s.endIndex { let j = s.index(i, offsetBy: 2); a.append(UInt8(s[i..<j], radix: 16)!); i = j }; return a }
let abc = Array("abc".utf8)
check(hex(Insecure.SHA1.hash(data: abc)) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA1")
check(hex(Insecure.MD5.hash(data: abc)) == "900150983cd24fb0d6963f7d28e17f72", "MD5")
check(hex(SHA256.hash(data: abc)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256")
check(hex(SHA384.hash(data: abc)) == "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7", "SHA384")
check(hex(SHA512.hash(data: abc)) == "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f", "SHA512")
var h = SHA256(); h.update(data: Array("a".utf8)); let h2 = h; h.update(data: Array("bc".utf8))
check(hex(h.finalize()) == hex(SHA256.hash(data: abc)) && hex(h2.finalize()) == hex(SHA256.hash(data: Array("a".utf8))), "SHA256 incremental com cópia (valor)")
check(SHA256.byteCount == 32 && SHA384.byteCount == 48 && Insecure.SHA1.byteCount == 20 && SHA512.blockByteCount == 128, "byteCount/blockByteCount")
check(SHA256.hash(data: abc) == SHA256.hash(data: abc) && SHA256.hash(data: abc) == Data(unhex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")), "Digest == Digest e == Data")
// HMAC (RFC 4231 caso 2) e HKDF (RFC 5869 caso 1)
let mac = HMAC<SHA256>.authenticationCode(for: Array("what do ya want for nothing?".utf8), using: SymmetricKey(data: Array("Jefe".utf8)))
check(hex(mac) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "HMAC-SHA256 RFC 4231")
var hm = HMAC<SHA512>(key: SymmetricKey(data: Array("Jefe".utf8))); hm.update(data: Array("what do ya want ".utf8)); hm.update(data: Array("for nothing?".utf8))
check(hex(hm.finalize()) == "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737", "HMAC-SHA512 incremental")
check(HMAC<SHA256>.isValidAuthenticationCode(Data(unhex("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843")), authenticating: Array("what do ya want for nothing?".utf8), using: SymmetricKey(data: Array("Jefe".utf8))), "isValidAuthenticationCode")
check(mac.byteCount == 32 && mac.description.contains("5bdcc146"), "MAC byteCount/description")
let okm = HKDF<SHA256>.deriveKey(inputKeyMaterial: SymmetricKey(data: [UInt8](repeating: 0x0b, count: 22)), salt: unhex("000102030405060708090a0b0c"), info: unhex("f0f1f2f3f4f5f6f7f8f9"), outputByteCount: 42)
check(hex(okm) == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865", "HKDF RFC 5869")
let prk = HKDF<SHA256>.extract(inputKeyMaterial: SymmetricKey(data: [UInt8](repeating: 0x0b, count: 22)), salt: unhex("000102030405060708090a0b0c"))
check(hex(prk) == "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5", "HKDF extract")
// AES Key Wrap (RFC 3394 4.1) e GCM
let kek = SymmetricKey(data: unhex("000102030405060708090a0b0c0d0e0f"))
let w = try! AES.KeyWrap.wrap(SymmetricKey(data: unhex("00112233445566778899aabbccddeeff")), using: kek)
check(hex(w) == "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5", "AES KeyWrap RFC 3394")
check(hex(try! AES.KeyWrap.unwrap(w, using: kek)) == "00112233445566778899aabbccddeeff", "AES KeyUnwrap")
check((try? AES.KeyWrap.unwrap(Data(unhex("1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe6")), using: kek)) == nil, "KeyUnwrap adulterado falha")
let k256 = SymmetricKey(size: .bits256), box = try! AES.GCM.seal(abc, using: k256)
check(try! AES.GCM.open(try! AES.GCM.SealedBox(combined: box.combined!), using: k256) == Data(abc), "AES-GCM ida e volta")
check(try! AES.GCM.open(AES.GCM.SealedBox(combined: box.combined!), using: k256) == Data(abc), "SealedBox(combined: Data) sem throws")
// Curve25519 (RFC 8032 teste 1; RFC 7748 6.1)
let ed = try! Curve25519.Signing.PrivateKey(rawRepresentation: unhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"))
check(hex(ed.publicKey.rawRepresentation) == "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "Ed25519 chave pública")
let esig = try! ed.signature(for: [UInt8]())
check(hex(esig) == "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b", "Ed25519 assinatura RFC 8032")
check(ed.publicKey.isValidSignature(esig, for: [UInt8]()) && !ed.publicKey.isValidSignature(esig, for: [UInt8(1)]), "Ed25519 verifica")
let xa = try! Curve25519.KeyAgreement.PrivateKey(rawRepresentation: unhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"))
check(hex(xa.publicKey.rawRepresentation) == "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a", "X25519 pública")
let ss = try! xa.sharedSecretFromKeyAgreement(with: try! Curve25519.KeyAgreement.PublicKey(rawRepresentation: unhex("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f")))
check(hex(ss) == "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742", "X25519 segredo RFC 7748")
// curvas NIST: ida e volta das representações e interoperação com o Python (arquivos em argv[1])
let dir = CommandLine.arguments[1]
func rd(_ n: String) -> String { try! String(contentsOfFile: dir + "/" + n, encoding: .utf8) }
func wr(_ n: String, _ s: String) { try! s.write(toFile: dir + "/" + n, atomically: true, encoding: .utf8) }
let msg = Array("mensagem do mOS".utf8)
do {
    let k = try P256.Signing.PrivateKey(pemRepresentation: rd("p256.pem"))
    check(k.publicKey.derRepresentation == Data(unhex(rd("p256.spki").trimmingCharacters(in: .whitespacesAndNewlines))), "P256 PEM do Python -> SPKI igual")
    wr("p256.sig", hex(try k.signature(for: msg).derRepresentation))
    let pysig = try P256.Signing.ECDSASignature(derRepresentation: Data(unhex(rd("p256.pysig").trimmingCharacters(in: .whitespacesAndNewlines))))
    check(k.publicKey.isValidSignature(pysig, for: msg), "P256 verifica assinatura do Python")
    check(try P256.Signing.PrivateKey(derRepresentation: k.derRepresentation).rawRepresentation == k.rawRepresentation, "P256 DER ida e volta")
    check((try P256.Signing.PrivateKey(x963Representation: k.x963Representation)).rawRepresentation == k.rawRepresentation, "P256 x963 privada")
    let p = k.publicKey
    check((try P256.Signing.PublicKey(compressedRepresentation: p.compressedRepresentation)).x963Representation == p.x963Representation, "P256 comprimida")
    check((try P256.Signing.PublicKey(pemRepresentation: p.pemRepresentation)).rawRepresentation == p.rawRepresentation, "P256 PEM pública")
    let k384 = try P384.Signing.PrivateKey(pemRepresentation: rd("p384.pem"))
    wr("p384.sig", hex(try k384.signature(for: msg).rawRepresentation))
    let c = P384.Signing.PrivateKey(compactRepresentable: true)
    let cp = try P384.Signing.PublicKey(compactRepresentation: c.publicKey.compactRepresentation ?? Data())
    check(cp.x963Representation == c.publicKey.x963Representation, "P384 compacta")
    let ka = P521.KeyAgreement.PrivateKey(), kb = P521.KeyAgreement.PrivateKey()
    check(hex(try ka.sharedSecretFromKeyAgreement(with: kb.publicKey)) == hex(try kb.sharedSecretFromKeyAgreement(with: ka.publicKey)), "P521 ECDH")
    let pyp521 = try P521.KeyAgreement.PublicKey(x963Representation: Data(unhex(rd("p521.pub").trimmingCharacters(in: .whitespacesAndNewlines))))
    let k521 = try P521.KeyAgreement.PrivateKey(rawRepresentation: Data(unhex(rd("p521.priv").trimmingCharacters(in: .whitespacesAndNewlines))))
    wr("p521.ss", hex(try k521.sharedSecretFromKeyAgreement(with: pyp521)))
    check((try? P256.Signing.PublicKey(x963Representation: Data([4] + [UInt8](repeating: 7, count: 64)))) == nil, "ponto fora da curva recusado")
    check((try? P256.Signing.PrivateKey(pemRepresentation: rd("p384.pem"))) == nil, "PEM de outra curva recusado")
} catch { check(false, "erro: \(error)") }
check(SecureEnclave.isAvailable == false, "sem Secure Enclave")
// HPKE (RFC 9180): o Recipient abre a 1ª cifra e exporta igual aos vetores do CFRG; Sender -> Recipient ida e volta
func hpkeRun<SK: HPKEDiffieHellmanPrivateKey>(_ sk: SK, _ pkS: SK.PublicKey?, _ cs: HPKE.Ciphersuite, _ f: [String]) throws -> Bool {
    let info = Data(unhex(f[4])), enc = Data(unhex(f[7])), aad = unhex(f[8]), ct = unhex(f[9]), pt = unhex(f[10])
    var r = try pkS.map { try HPKE.Recipient(privateKey: sk, ciphersuite: cs, info: info, encapsulatedKey: enc, authenticatedBy: $0) }
        ?? HPKE.Recipient(privateKey: sk, ciphersuite: cs, info: info, encapsulatedKey: enc)
    let o1 = try r.open(ct, authenticating: aad)
    let ok1 = o1 == Data(pt)
    let ex = try r.exportSecret(context: f[11] == "-" ? [] : unhex(f[11]), outputByteCount: Int(f[12])!)
    let ok2 = hex(ex) == f[13]
    var s = try HPKE.Sender(recipientKey: sk.publicKey, ciphersuite: cs, info: info)
    let c = try s.seal(pt, authenticating: aad)
    var r2 = try HPKE.Recipient(privateKey: sk, ciphersuite: cs, info: info, encapsulatedKey: s.encapsulatedKey)
    let o2 = try r2.open(c, authenticating: aad)
    return ok1 && ok2 && o2 == Data(pt)
}
if CommandLine.arguments.count > 2, let vt = try? String(contentsOfFile: CommandLine.arguments[2], encoding: .utf8) {
    let kems: [Int: HPKE.KEM] = [0x10: .P256_HKDF_SHA256, 0x11: .P384_HKDF_SHA384, 0x12: .P521_HKDF_SHA512, 0x20: .Curve25519_HKDF_SHA256]
    let kdfs: [Int: HPKE.KDF] = [1: .HKDF_SHA256, 2: .HKDF_SHA384, 3: .HKDF_SHA512]
    let aeads: [Int: HPKE.AEAD] = [1: .AES_GCM_128, 2: .AES_GCM_256, 3: .chaChaPoly]
    var n = 0, good = 0
    for line in vt.split(separator: "\n") where !line.hasPrefix("#") {
        let f = line.split(separator: " ").map(String.init)
        guard f.count == 14, let kem = kems[Int(f[1])!], let kdf = kdfs[Int(f[2])!], let aead = aeads[Int(f[3])!] else { continue }
        let cs = HPKE.Ciphersuite(kem: kem, kdf: kdf, aead: aead), auth = f[0] == "2"
        n += 1
        do {
            let ok: Bool
            switch kem {
            case .Curve25519_HKDF_SHA256:
                ok = try hpkeRun(try Curve25519.KeyAgreement.PrivateKey(rawRepresentation: unhex(f[5])), auth ? try Curve25519.KeyAgreement.PublicKey(unhex(f[6]), kem: kem) : nil, cs, f)
            case .P256_HKDF_SHA256:
                ok = try hpkeRun(try P256.KeyAgreement.PrivateKey(rawRepresentation: unhex(f[5])), auth ? try P256.KeyAgreement.PublicKey(unhex(f[6]), kem: kem) : nil, cs, f)
            case .P384_HKDF_SHA384:
                ok = try hpkeRun(try P384.KeyAgreement.PrivateKey(rawRepresentation: unhex(f[5])), auth ? try P384.KeyAgreement.PublicKey(unhex(f[6]), kem: kem) : nil, cs, f)
            case .P521_HKDF_SHA512:
                ok = try hpkeRun(try P521.KeyAgreement.PrivateKey(rawRepresentation: unhex(f[5])), auth ? try P521.KeyAgreement.PublicKey(unhex(f[6]), kem: kem) : nil, cs, f)
            }
            if ok { good += 1 } else { print("HPKE falhou:", f[0...3].joined(separator: " ")) }
        } catch { print("HPKE erro:", f[0...3].joined(separator: " "), error) }
    }
    check(n > 30 && good == n, "HPKE: \(good)/\(n) vetores RFC 9180 (abrir, exportar e ida e volta)")
    let sA = Curve25519.KeyAgreement.PrivateKey(), rB = Curve25519.KeyAgreement.PrivateKey()
    var sn = try! HPKE.Sender(recipientKey: rB.publicKey, ciphersuite: .Curve25519_SHA256_ChachaPoly, info: Data(), authenticatedBy: sA)
    let c1 = try! sn.seal(abc, authenticating: [UInt8]())
    var rc = try! HPKE.Recipient(privateKey: rB, ciphersuite: .Curve25519_SHA256_ChachaPoly, info: Data(), encapsulatedKey: sn.encapsulatedKey, authenticatedBy: sA.publicKey)
    check((try? rc.open(c1, authenticating: [UInt8]())) == Data(abc), "HPKE modo auth: Sender -> Recipient")
    var rw = try! HPKE.Recipient(privateKey: rB, ciphersuite: .Curve25519_SHA256_ChachaPoly, info: Data(), encapsulatedKey: sn.encapsulatedKey,
                                 authenticatedBy: Curve25519.KeyAgreement.PrivateKey().publicKey)
    check((try? rw.open(c1, authenticating: [UInt8]())) == nil, "HPKE auth com remetente errado falha")
}
print("cryptokit: \(ok)/\(total)")
exit(ok == total ? 0 : 1)
