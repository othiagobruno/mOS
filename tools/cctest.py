#!/usr/bin/env python3
"""mOS — CommonCrypto do libSystem contra o OpenSSL 3 (cryptography, hashlib e `openssl enc -provider legacy`).

    python3 -I tools/cctest.py        (compila tools/cctest.c com os .c do libSystem e roda tudo)

Imprime "cctest: N/N ok"."""
import hashlib, hmac, os, subprocess, sys, tempfile
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import padding as sympad

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = tempfile.mkdtemp(prefix="cctest-")
ZIG = subprocess.run(["which", "zig"], capture_output=True, text=True).stdout.strip()
ZD = os.path.dirname(os.path.realpath(ZIG))
LS = os.path.join(ROOT, "userland/libsystem")
CF = ["-target", "x86_64-freestanding-none", "-O2", "-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-nostdlib",
      "-fPIC", "-mno-red-zone", "-w", "-I" + LS + "/kshim", "-I" + LS + "/fsinc", "-I" + ROOT + "/kernel/include", "-I" + LS,
      "-isystem", ZD + "/lib/include"]
for f in ("commoncrypto", "commoncrypto2"):
    subprocess.run([ZIG, "cc", *CF, "-c", f"{LS}/{f}.c", "-o", f"{T}/{f}.o"], check=True)
BIN = f"{T}/cctest"
subprocess.run(["gcc", "-O2", os.path.join(ROOT, "tools/cctest.c"), f"{T}/commoncrypto.o", f"{T}/commoncrypto2.o", "-o", BIN],
               check=True)

ok = total = 0


def check(cond, what):
    global ok, total
    total += 1
    if cond:
        ok += 1
    else:
        print("FALHA:", what)


def run(args, data=b""):
    r = subprocess.run([BIN, *args], input=data, capture_output=True)
    return r.returncode, r.stdout, r.stderr


def ossl(cipher, key, iv, data, dec=False, nopad=False):
    a = ["openssl", "enc", "-" + cipher, "-K", key.hex(), "-provider", "legacy", "-provider", "default"]
    if iv is not None:
        a += ["-iv", iv.hex()]
    if dec:
        a.append("-d")
    if nopad:
        a.append("-nopad")
    return subprocess.run(a, input=data, capture_output=True).stdout


R = os.urandom
AES, DES, DES3, CAST, RC4, RC2, BF = 0, 1, 2, 3, 4, 5, 6
ECB, CBC, CFB, CTR, OFB, XTS, M_RC4, CFB8 = 1, 2, 3, 4, 7, 8, 9, 10

for size in (0, 1, 15, 16, 17, 100, 1000):
    msg = R(size)
    # AES em todos os modos, contra cryptography
    for kl in (16, 24, 32):
        key, iv = R(kl), R(16)
        for mode, cmode in ((CBC, modes.CBC(iv)), (ECB, modes.ECB()), (CFB, modes.CFB(iv)), (CFB8, modes.CFB8(iv)),
                            (CTR, modes.CTR(iv)), (OFB, modes.OFB(iv))):
            block = mode in (CBC, ECB)
            for pad in ((1, 0) if block else (0,)):
                if block and not pad and size % 16:
                    continue
                data = msg
                if block and pad:
                    p = sympad.PKCS7(128).padder(); data = p.update(msg) + p.finalize()
                want = Cipher(algorithms.AES(key), cmode).encryptor().update(data)
                rc, out, _ = run(["crypt", str(AES), str(mode), str(pad), "0", key.hex(), iv.hex()], msg)
                check(rc == 0 and out == want, f"AES-{kl * 8} modo {mode} pad {pad} tam {size}")
                rc, back, _ = run(["crypt", str(AES), str(mode), str(pad), "1", key.hex(), iv.hex()], out)
                check(rc == 0 and back == msg, f"AES-{kl * 8} volta modo {mode} pad {pad} tam {size}")
    # cifras de 8 bytes em CBC com PKCS#7, contra o openssl legacy
    for alg, name, kl in ((DES, "des-cbc", 8), (DES3, "des-ede3-cbc", 24), (BF, "bf-cbc", 16), (RC2, "rc2-cbc", 16)):
        key, iv = R(kl), R(8)
        extra = []
        if alg == RC2:
            want = ossl("rc2-cbc", key, iv, msg)              # o openssl usa 128 bits efetivos = 8·16
        else:
            want = ossl(name, key, iv, msg)
        rc, out, _ = run(["crypt", str(alg), str(CBC), "1", "0", key.hex(), iv.hex()], msg)
        check(rc == 0 and out == want and len(want) > 0, f"{name} tam {size}")
        rc, back, _ = run(["crypt", str(alg), str(CBC), "1", "1", key.hex(), iv.hex()], want)
        check(rc == 0 and back == msg, f"{name} volta tam {size}")
    key = R(16)
    want = ossl("rc4", key, None, msg)
    rc, out, _ = run(["crypt", str(RC4), str(M_RC4), "0", "0", key.hex(), ""], msg)
    check(rc == 0 and out == want, f"rc4 tam {size}")
    # GCM com IV de 12 e de 16 bytes
    for ivl in (12, 16, 8):
        key, iv, aad = R(32), R(ivl), R(size % 37)
        want = AESGCM(key).encrypt(iv, msg, aad)
        rc, out, err = run(["gcm", "enc", key.hex(), iv.hex(), aad.hex(), "16"], msg)
        check(rc == 0 and out + bytes.fromhex(err.decode().strip()) == want, f"gcm enc iv {ivl} tam {size}")
        rc, back, _ = run(["gcm", "dec", key.hex(), iv.hex(), aad.hex(), "16", want[-16:].hex()], want[:-16])
        check(rc == 0 and back == msg, f"gcm dec iv {ivl} tam {size}")
        bad = bytes([want[-1] ^ 1])
        rc, _, _ = run(["gcm", "dec", key.hex(), iv.hex(), aad.hex(), "16", (want[-16:-1] + bad).hex()], want[:-16])
        check(rc != 0, f"gcm recusa tag ruim iv {ivl} tam {size}")
    # XTS (unidades de 16 a 1000 bytes, com roubo de texto cifrado)
    if size >= 16:
        k1, k2, tweak = R(32), R(32), R(16)
        want = Cipher(algorithms.AES(k1 + k2), modes.XTS(tweak)).encryptor().update(msg)
        rc, out, _ = run(["xts", "0", k1.hex(), k2.hex(), tweak.hex()], msg)
        check(rc == 0 and out == want, f"xts tam {size}")
        rc, back, _ = run(["xts", "1", k1.hex(), k2.hex(), tweak.hex()], want)
        check(rc == 0 and back == msg, f"xts volta tam {size}")
    # hashes
    for h in ("md5", "sha1", "sha224", "sha256", "sha384", "sha512"):
        rc, out, _ = run(["hash", h], msg)
        check(out.decode().strip() == hashlib.new(h, msg).hexdigest(), f"{h} tam {size}")
    for h in ("md4",):
        want = subprocess.run(["openssl", "dgst", "-" + h, "-provider", "legacy", "-provider", "default", "-r"], input=msg,
                              capture_output=True).stdout.split()[0].decode()
        rc, out, _ = run(["hash", h], msg)
        check(out.decode().strip() == want, f"{h} tam {size}")
    # HMAC (kCCHmacAlgSHA1, MD5, SHA256, SHA384, SHA512, SHA224)
    for alg, h in enumerate(("sha1", "md5", "sha256", "sha384", "sha512", "sha224")):
        key = R(size % 150 + 1)
        rc, out, _ = run(["hmac", str(alg), key.hex()], msg)
        check(out.decode().strip() == hmac.new(key, msg, h).hexdigest(), f"hmac {h} tam {size}")

# MD2: vetores da RFC 1319
for m, want in ((b"", "8350e5a3e24c153df2275c9f80692773"), (b"abc", "da853b0d3f88d99b30283a69e6ded6bb"),
                (b"message digest", "ab4f496bfb2a530b219ff33031fe06b0"),
                (b"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                 "d5976f79d83d3a0dc9806c3c66f3efd8")):
    rc, out, _ = run(["hash", "md2"], m)
    check(out.decode().strip() == want, f"md2 {m[:10]}")
# PBKDF2 em todos os PRFs (kCCPRFHmacAlgSHA1..SHA512)
for prf, h in ((1, "sha1"), (2, "sha224"), (3, "sha256"), (4, "sha384"), (5, "sha512")):
    salt = R(16)
    rc, out, _ = run(["pbkdf", str(prf), "senha do keychain", salt.hex(), "1000", "48"])
    check(out.decode().strip() == hashlib.pbkdf2_hmac(h, b"senha do keychain", salt, 1000, 48).hex(), f"pbkdf2 {h}")
rc, out, _ = run(["calibrate"])
check(rc == 0 and int(out) >= 10000, "CCCalibratePBKDF")
rc, _, _ = run(["crypt", str(CAST), str(CBC), "1", "0", R(16).hex(), R(8).hex()], b"x")
check(rc != 0, "CAST5 recusa (kCCUnimplemented)")

print(f"cctest: {ok}/{total} {'ok' if ok == total else 'FALHOU'}")
sys.exit(0 if ok == total else 1)
