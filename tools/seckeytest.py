#!/usr/bin/env python3
"""mOS — conferência cruzada do núcleo do SecKey (tools/seckeytest.c) com a biblioteca cryptography (OpenSSL).

    python3 -I tools/seckeytest.py /tmp/seckeytest

Para cada caso: chaves geradas pelo mOS lidas pelo OpenSSL e vice-versa; assinaturas do mOS verificadas pelo
OpenSSL e as do OpenSSL verificadas pelo mOS; cifras nos dois sentidos; ECDH (com e sem KDF X9.63) igual nos
dois lados. Imprime "seckey cruzado: N/N ok"."""
import os, subprocess, sys, tempfile
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, ec, padding, utils
from cryptography.hazmat.primitives.kdf.x963kdf import X963KDF

BIN = os.path.abspath(sys.argv[1])
T = tempfile.mkdtemp(prefix="seckey-")
ok = total = 0
H = {"SHA1": hashes.SHA1, "SHA224": hashes.SHA224, "SHA256": hashes.SHA256, "SHA384": hashes.SHA384, "SHA512": hashes.SHA512}


def p(name):
    return os.path.join(T, name)


def put(name, data):
    with open(p(name), "wb") as f:
        f.write(data)


def get(name):
    with open(p(name), "rb") as f:
        return f.read()


def run(*a):
    return subprocess.run([BIN, *a], stderr=subprocess.DEVNULL, cwd=T).returncode


def check(cond, what):
    global ok, total
    total += 1
    if cond:
        ok += 1
    else:
        print("FALHA:", what)


MSG = b"mOS: assinatura conferida contra o OpenSSL" * 3
put("msg", MSG)

# ---------------------------------------------------------------- RSA
for bits in (1024, 2048, 3072):
    # chave do mOS
    check(run("gen", "rsa", str(bits), "m.der") == 0, f"gen rsa {bits}")
    mkey = serialization.load_der_private_key(get("m.der"), None)  # PKCS#1 RSAPrivateKey
    check(mkey.key_size == bits, f"tamanho rsa {bits}")
    # chave do OpenSSL no formato externo do macOS (PKCS#1)
    okey = rsa.generate_private_key(65537, bits)
    put("o.der", okey.private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.TraditionalOpenSSL,
                                    serialization.NoEncryption()))
    put("o.pub", okey.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.PKCS1))
    check(run("pub", "m.der", "m.pub") == 0 and get("m.pub") == mkey.public_key().public_bytes(
        serialization.Encoding.DER, serialization.PublicFormat.PKCS1), f"pub pkcs1 {bits}")
    check(run("spki", "m.der", "m.spki") == 0 and get("m.spki") == mkey.public_key().public_bytes(
        serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo), f"spki {bits}")
    # PKCS#8 e SPKI na importação
    put("o.p8", okey.private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    check(run("import", "o.p8", "o.back") == 0 and get("o.back") == get("o.der"), f"import pkcs8 {bits}")
    for h in ("SHA1", "SHA224", "SHA256", "SHA384", "SHA512"):
        hl = H[h].digest_size
        # PKCS#1 v1.5 (mensagem)
        alg = f"algid:RSASignatureMessagePKCS1v15{h}"
        check(run("sign", alg, "m.der", "msg", "sig") == 0, f"sign {alg} {bits}")
        try:
            mkey.public_key().verify(get("sig"), MSG, padding.PKCS1v15(), H[h]())
            check(True, "")
        except Exception:
            check(False, f"openssl verifica {alg} {bits}")
        put("osig", okey.sign(MSG, padding.PKCS1v15(), H[h]()))
        check(run("verify", alg, "o.pub", "msg", "osig") == 0, f"mOS verifica {alg} {bits}")
        put("bad", MSG + b"!")
        check(run("verify", alg, "o.pub", "bad", "osig") != 0, f"recusa {alg} {bits}")
        # PSS (digest), sal = hLen
        if bits // 8 >= 2 * hl + 2:
            alg = f"algid:RSASignatureDigestPSS{h}"
            dg = hashes.Hash(H[h]()); dg.update(MSG); d = dg.finalize()
            put("dg", d)
            check(run("sign", alg, "m.der", "dg", "sig") == 0, f"sign {alg} {bits}")
            try:
                mkey.public_key().verify(get("sig"), d, padding.PSS(padding.MGF1(H[h]()), hl), utils.Prehashed(H[h]()))
                check(True, "")
            except Exception:
                check(False, f"openssl verifica {alg} {bits}")
            put("osig", okey.sign(d, padding.PSS(padding.MGF1(H[h]()), padding.PSS.MAX_LENGTH), utils.Prehashed(H[h]())))
            check(run("verify", alg, "o.pub", "dg", "osig") == 0, f"mOS verifica PSS sal máximo {h} {bits}")
        # OAEP nos dois sentidos
        if bits // 8 >= 2 * hl + 2:
            alg = f"algid:RSAEncryptionOAEP{h}"
            put("pt", b"segredo do keychain")
            check(run("enc", alg, "o.pub", "pt", "ct") == 0, f"enc {alg} {bits}")
            try:
                pt = okey.decrypt(get("ct"), padding.OAEP(padding.MGF1(H[h]()), H[h](), None))
                check(pt == b"segredo do keychain", f"openssl decifra {alg}")
            except Exception:
                check(False, f"openssl decifra {alg} {bits}")
            put("oct", mkey.public_key().encrypt(b"outra coisa", padding.OAEP(padding.MGF1(H[h]()), H[h](), None)))
            check(run("dec", alg, "m.der", "oct", "opt") == 0 and get("opt") == b"outra coisa", f"mOS decifra {alg} {bits}")
    # PKCS#1 v1.5 cifra
    put("pt", b"chave AES de 16b")
    check(run("enc", "algid:RSAEncryptionPKCS1", "o.pub", "pt", "ct") == 0 and
          okey.decrypt(get("ct"), padding.PKCS1v15()) == b"chave AES de 16b", f"enc pkcs1 {bits}")
    put("oct", mkey.public_key().encrypt(b"de volta", padding.PKCS1v15()))
    check(run("dec", "algid:RSAEncryptionPKCS1", "m.der", "oct", "opt") == 0 and get("opt") == b"de volta", f"dec pkcs1 {bits}")

# ---------------------------------------------------------------- EC
CURVES = {256: ec.SECP256R1, 384: ec.SECP384R1, 521: ec.SECP521R1}
for bits, curve in CURVES.items():
    check(run("gen", "ec", str(bits), "m.ec") == 0, f"gen ec {bits}")
    raw = get("m.ec")
    B = (bits + 7) // 8
    mpriv = ec.derive_private_key(int.from_bytes(raw[1 + 2 * B:], "big"), curve())
    check(mpriv.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint) ==
          raw[:1 + 2 * B], f"ponto público {bits}")
    opriv = ec.generate_private_key(curve())
    opub = opriv.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    put("o.ecpub", opub)
    put("o.ec", opub + opriv.private_numbers().private_value.to_bytes(B, "big"))
    put("o.sec1", opriv.private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.TraditionalOpenSSL,
                                      serialization.NoEncryption()))
    check(run("import", "o.sec1", "o.back") == 0 and get("o.back") == get("o.ec"), f"import sec1 {bits}")
    put("o.spki", opriv.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo))
    check(run("import", "o.spki", "o.back") == 0 and get("o.back") == opub, f"import spki ec {bits}")
    for h in ("SHA1", "SHA256", "SHA384", "SHA512"):
        alg = f"algid:ECDSASignatureMessageX962{h}"
        check(run("sign", alg, "m.ec", "msg", "sig") == 0, f"sign {alg}")
        try:
            mpriv.public_key().verify(get("sig"), MSG, ec.ECDSA(H[h]()))
            check(True, "")
        except Exception:
            check(False, f"openssl verifica {alg} {bits}")
        put("osig", opriv.sign(MSG, ec.ECDSA(H[h]())))
        check(run("verify", alg, "o.ecpub", "msg", "osig") == 0, f"mOS verifica {alg} {bits}")
    # RFC 4754 (r||s) sobre o digest
    dg = hashes.Hash(hashes.SHA256()); dg.update(MSG); d = dg.finalize()
    put("dg", d)
    check(run("sign", "algid:ECDSASignatureRFC4754", "m.ec", "dg", "sig") == 0, "sign rfc4754")
    s = get("sig")
    try:
        mpriv.public_key().verify(utils.encode_dss_signature(int.from_bytes(s[:B], "big"), int.from_bytes(s[B:], "big")),
                                  d, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
        check(len(s) == 2 * B, "")
    except Exception:
        check(False, f"openssl verifica rfc4754 {bits}")
    # ECDH
    z = mpriv.exchange(ec.ECDH(), opriv.public_key())
    check(run("ecdh", "algid:ECDHKeyExchangeStandard", "m.ec", "o.ecpub", "z") == 0 and get("z") == z, f"ecdh {bits}")
    want = X963KDF(hashes.SHA256(), 42, b"info").derive(z)
    check(run("ecdh", "algid:ECDHKeyExchangeCofactorX963SHA256", "m.ec", "o.ecpub", "z", "42", "info") == 0 and
          get("z") == want, f"ecdh x963 {bits}")

print(f"seckey cruzado: {ok}/{total} {'ok' if ok == total else 'FALHOU'}")
sys.exit(0 if ok == total else 1)
