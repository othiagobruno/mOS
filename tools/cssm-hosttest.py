#!/usr/bin/env python3
"""mOS — teste no host do CSSM (userland/libsystem/cssm.c): CL sobre um certificado gerado aqui e o CSP simétrico/PBKDF2
sobre o CommonCrypto do libSystem, conferidos com a `cryptography` e o hashlib.

    python3 -I tools/cssm-hosttest.py

Imprime "cssm: N/N ok"."""
import datetime, hashlib, os, shutil, subprocess, sys, tempfile
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization, padding as sympad
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LS = os.path.join(ROOT, "userland/libsystem")
T = tempfile.mkdtemp(prefix="cssm-")
ZIG = shutil.which("zig")
ZD = os.path.dirname(os.path.realpath(ZIG))
CF = ["-target", "x86_64-freestanding-none", "-O2", "-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-nostdlib",
      "-fPIC", "-mno-red-zone", "-w", "-I" + LS + "/kshim", "-I" + LS + "/fsinc", "-I" + ROOT + "/kernel/include", "-I" + LS,
      "-isystem", ZD + "/lib/include"]
for f in ("commoncrypto", "commoncrypto2"):
    subprocess.run([ZIG, "cc", *CF, "-c", f"{LS}/{f}.c", "-o", f"{T}/{f}.o"], check=True)
with open(f"{T}/libc.h", "w") as h:
    h.write("#pragma once\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n"
            "typedef uint8_t u8;\ntypedef uint32_t u32;\ntypedef uint64_t u64;\n")
shutil.copy(f"{LS}/cssm.c", T)
BIN = f"{T}/t"
subprocess.run(["gcc", "-O1", "-Wall", "-Wno-unused-parameter", "-Wno-unused-function", "-I" + T, f"{T}/cssm.c",
                os.path.join(ROOT, "tools/cssm_host.c"), f"{T}/commoncrypto.o", f"{T}/commoncrypto2.o", "-o", BIN], check=True)

# certificado com KU, EKU, BC, SKI e um ponto de distribuição de CRL (extensão sem interpretação)
key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "mOS teste"), x509.NameAttribute(NameOID.ORGANIZATION_NAME, "mOS")])
ski = x509.SubjectKeyIdentifier.from_public_key(key.public_key())
now = datetime.datetime(2026, 1, 2, 3, 4, 5, tzinfo=datetime.timezone.utc)
cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
        .serial_number(0x00ABCDEF0123456789).not_valid_before(now).not_valid_after(now + datetime.timedelta(days=30))
        .add_extension(x509.KeyUsage(True, False, True, False, False, False, False, False, False), critical=True)
        .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.CLIENT_AUTH, ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        .add_extension(ski, critical=False)
        .add_extension(x509.CRLDistributionPoints([x509.DistributionPoint([x509.UniformResourceIdentifier("http://crl.example/a.crl")], None, None, None)]), critical=False)
        .sign(key, hashes.SHA256()))
der = cert.public_bytes(serialization.Encoding.DER)
open(f"{T}/c.der", "wb").write(der)
aes = os.urandom(16); iv = os.urandom(16); pt = os.urandom(77)
out = subprocess.run([BIN, f"{T}/c.der", aes.hex(), iv.hex(), pt.hex()], capture_output=True, text=True)
lines = {}
for ln in out.stdout.splitlines():
    k, _, v = ln.partition(" ")
    lines.setdefault(k, []).append(v)
ok = total = 0


def check(cond, what):
    global ok, total
    total += 1
    if cond: ok += 1
    else: print("FALHOU:", what, file=sys.stderr)


g = lambda k, i=0: lines.get(k, [""])[i]
check(g("clh") == "0" and g("cache") == "0" and g("sigalg") == "0", "CL handle/cache")
check(g("sigoid") == "2a864886f70d01010b" and g("sigparams") == "0500", "algoritmo de assinatura")
check(g("next") == "1", "sem próximo valor")
check(g("serial") == "00abcdef0123456789", "serial (conteúdo DER, com o zero de sinal)")
check(g("version") == "02", "versão")
check(g("nbtag") == "17" and g("nb") == "260102030405Z", "validade")
check(g("subject") == name.public_bytes().hex(), "sujeito em DER")
check(g("spkialg") == "2a864886f70d010101", "algoritmo da chave pública")
check(bytes.fromhex(g("sig")) == cert.signature, "assinatura")
check(g("ku") == "crit=1 fmt=1 val=a000", "KeyUsage")
check(g("eku") == "n=2" and lines.get("ekuoid") == ["2b06010505070302", "2b06010505070301"], "ExtendedKeyUsage")
check(g("bc") == "ca=0 pl=0/0 crit=1", "BasicConstraints")
check(g("ski") == ski.digest.hex(), "SubjectKeyIdentifier")
crl_ext = cert.extensions.get_extension_for_class(x509.CRLDistributionPoints)
check(g("crl") == "fmt=0 tag=04" and bytes.fromhex(g("crlber")) == crl_ext.value.public_bytes(), "extensão em BER")
check(g("san") == "1 n=0", "extensão ausente")
check(g("tag") == "1", "campo desconhecido")
check(g("nocache") == "0", "CertGetFirstFieldValue")
check(g("abort") == "0 1", "AbortCache")
p = sympad.PKCS7(128).padder(); padded = p.update(pt) + p.finalize()
e = Cipher(algorithms.AES(aes), modes.CBC(iv)).encryptor(); ct = e.update(padded) + e.finalize()
check(g("symctx") == "0" and g("enc") == "0" and g("ct") == ct.hex(), "AES-CBC-PKCS7")
check(g("roundtrip") == "1", "decifra")
check(g("init") == "0" and g("final") == "0" and g("stream") == "1", "decifra em etapas")
check(g("deleted") == "1", "contexto apagado")
check(g("dctx") == "0" and g("derive") == "0" and g("dk") == hashlib.pbkdf2_hmac("sha1", b"senha", b"sal-do-mOS", 1000, 32).hex(), "PBKDF2")
check(g("dkhdr") == "1 2 256", "cabeçalho da chave derivada")
check(g("wrap") == "0" and g("wrapraw") == "1" and g("free") == "0 1", "wrap nulo e FreeKey")
check(g("ref") == "1 2 2 128" and g("refctx") == "0" and g("refenc") == "1", "CSSM_KEY de referência")
if out.returncode or ok != total: print(out.stdout, out.stderr, file=sys.stderr)
shutil.rmtree(T, ignore_errors=True)
print(f"cssm: {ok}/{total} ok")
sys.exit(0 if ok == total else 1)
