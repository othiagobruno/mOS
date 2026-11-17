#!/usr/bin/env python3
"""mOS — PKCS#12, PEM, nomes X.509 e avaliação de cadeia do libSystem contra a biblioteca cryptography e o OpenSSL.

    python3 -I tools/seccerttest.py

Compila tools/seccerttest.c com seccert_core.inc, sectrust.c (x509.c do kernel) e o CCCrypt do libSystem.
Imprime "seccert: N/N ok"."""
import datetime, os, subprocess, sys, tempfile
from cryptography import x509
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, ec
from cryptography.hazmat.primitives.serialization import pkcs12

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = tempfile.mkdtemp(prefix="seccert-")
ZIG = subprocess.run(["which", "zig"], capture_output=True, text=True).stdout.strip()
ZD = os.path.dirname(os.path.realpath(ZIG))
LS = os.path.join(ROOT, "userland/libsystem")
CF = ["-target", "x86_64-freestanding-none", "-O2", "-ffreestanding", "-fno-stack-protector", "-fno-builtin", "-nostdlib",
      "-fPIC", "-mno-red-zone", "-w", "-I" + LS + "/kshim", "-I" + LS + "/fsinc", "-I" + ROOT + "/kernel/include", "-I" + LS,
      "-isystem", ZD + "/lib/include"]
for f in ("commoncrypto", "commoncrypto2"):
    subprocess.run([ZIG, "cc", *CF, "-c", f"{LS}/{f}.c", "-o", f"{T}/{f}.o"], check=True)
HF = ["-O1", "-w", "-DMOS_HOST_TEST", "-I" + ROOT + "/tools", "-I" + ROOT + "/kernel/include"]
subprocess.run(["gcc", *HF, "-I" + LS, "-c", f"{LS}/sectrust.c", "-o", f"{T}/sectrust.o"], check=True)
BIN = f"{T}/seccerttest"
subprocess.run(["gcc", *HF, f"{ROOT}/tools/seccerttest.c", f"{ROOT}/kernel/net/crypto.c", f"{ROOT}/kernel/net/pk.c",
                f"{T}/sectrust.o", f"{T}/commoncrypto.o", f"{T}/commoncrypto2.o", "-o", BIN], check=True)

ok = total = 0


def check(cond, what):
    global ok, total
    total += 1
    if cond:
        ok += 1
    else:
        print("FALHA:", what)


def run(*a):
    return subprocess.run([BIN, *a], capture_output=True, text=True).stdout


def put(name, data):
    p = os.path.join(T, name)
    with open(p, "wb") as f:
        f.write(data)
    return p


def get(name):
    with open(os.path.join(T, name), "rb") as f:
        return f.read()


NOW = datetime.datetime.now(datetime.timezone.utc)


def make(subject, key, issuer=None, ikey=None, ca=False, san=None, eku=None, days=365, start=None, email=None):
    names = [x509.NameAttribute(NameOID.COMMON_NAME, subject), x509.NameAttribute(NameOID.ORGANIZATION_NAME, "Time mOS")]
    if email:
        names.append(x509.NameAttribute(NameOID.EMAIL_ADDRESS, email))
    name = x509.Name(names)
    b = (x509.CertificateBuilder().subject_name(name).issuer_name(issuer.subject if issuer else name)
         .public_key(key.public_key()).serial_number(x509.random_serial_number())
         .not_valid_before(start or NOW - datetime.timedelta(days=1))
         .not_valid_after((start or NOW) + datetime.timedelta(days=days)))
    b = b.add_extension(x509.BasicConstraints(ca=ca, path_length=None), critical=True)
    if ca:
        b = b.add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
    if san:
        b = b.add_extension(x509.SubjectAlternativeName([x509.DNSName(s) for s in san]), critical=False)
    if eku:
        b = b.add_extension(x509.ExtendedKeyUsage(eku), critical=False)
    return b.sign(ikey or key, hashes.SHA256())


ca_key = rsa.generate_private_key(65537, 2048)
ca = make("Raiz de Teste mOS", ca_key, ca=True, days=3650)
in_key = ec.generate_private_key(ec.SECP384R1())
inter = make("Intermediária mOS", in_key, ca, ca_key, ca=True, days=1800)
leaf_key = ec.generate_private_key(ec.SECP256R1())
leaf = make("app.mos.test", leaf_key, inter, in_key, san=["app.mos.test", "*.mos.test"], email="dev@mos.test",
            eku=[ExtendedKeyUsageOID.SERVER_AUTH])
DER = serialization.Encoding.DER
p_ca, p_in, p_leaf = put("ca.der", ca.public_bytes(DER)), put("in.der", inter.public_bytes(DER)), put("leaf.der", leaf.public_bytes(DER))

# ---------------------------------------------------------------- PKCS#12
PW = b"senha-do-mOS"
pk8 = lambda k: k.private_bytes(DER, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
variants = {
    "pbes2-aes256": pkcs12.serialize_key_and_certificates(b"Minha Identidade", leaf_key, leaf, [inter, ca],
                                                          serialization.BestAvailableEncryption(PW)),
    "3des-sha1": pkcs12.serialize_key_and_certificates(
        b"Minha Identidade", leaf_key, leaf, [inter, ca],
        serialization.PrivateFormat.PKCS12.encryption_builder().kdf_rounds(2048)
        .key_cert_algorithm(pkcs12.PBES.PBESv1SHA1And3KeyTripleDESCBC).hmac_hash(hashes.SHA1()).build(PW)),
    "pbes2-sha256": pkcs12.serialize_key_and_certificates(
        b"Minha Identidade", leaf_key, leaf, [inter, ca],
        serialization.PrivateFormat.PKCS12.encryption_builder().kdf_rounds(5000)
        .key_cert_algorithm(pkcs12.PBES.PBESv2SHA256AndAES256CBC).hmac_hash(hashes.SHA256()).build(PW)),
}
nopw = pkcs12.serialize_key_and_certificates(b"Minha Identidade", leaf_key, leaf, [inter, ca], serialization.NoEncryption())
# openssl pkcs12 -legacy (RC2-40 nos certificados, 3DES na chave) e o padrão do OpenSSL 3
put("leaf_key.pem", leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
put("leaf.pem", leaf.public_bytes(serialization.Encoding.PEM))
put("chain.pem", inter.public_bytes(serialization.Encoding.PEM) + ca.public_bytes(serialization.Encoding.PEM))
for name, extra in (("openssl-legacy", ["-legacy"]), ("openssl-padrao", [])):
    out = os.path.join(T, name + ".p12")
    r = subprocess.run(["openssl", "pkcs12", "-export", *extra, "-provider", "legacy", "-provider", "default",
                        "-inkey", os.path.join(T, "leaf_key.pem"), "-in", os.path.join(T, "leaf.pem"),
                        "-certfile", os.path.join(T, "chain.pem"), "-name", "Minha Identidade",
                        "-passout", "pass:" + PW.decode(), "-out", out], capture_output=True)
    check(r.returncode == 0, f"openssl gerou {name}")
    with open(out, "rb") as f:
        variants[name] = f.read()

for name, blob in list(variants.items()) + [("sem-senha", nopw)]:
    p = put(name + ".p12", blob)
    d = os.path.join(T, name)
    os.makedirs(d, exist_ok=True)
    out = run("p12", p, "-" if name == "sem-senha" else PW.decode(), d)
    check(out.startswith("certs 3 keys 1"), f"{name}: contagem ({out.splitlines()[0] if out else 'nada'})")
    if not out.startswith("certs"):
        continue
    got = {open(os.path.join(d, f"cert{i}.der"), "rb").read() for i in range(3)}
    check(got == {ca.public_bytes(DER), inter.public_bytes(DER), leaf.public_bytes(DER)}, f"{name}: certificados")
    k = serialization.load_der_private_key(open(os.path.join(d, "key0.der"), "rb").read(), None)
    check(k.private_numbers() == leaf_key.private_numbers(), f"{name}: chave")
    check("nome=Minha Identidade" in out, f"{name}: friendlyName")
    check("par " in out, f"{name}: localKeyId liga chave e certificado")
    if name != "sem-senha":
        bad = run("p12", p, "senha-errada", d)
        check(bad.startswith("erro 2"), f"{name}: senha errada recusada pelo MAC ({bad.strip()})")

# ---------------------------------------------------------------- PEM
ec_trad = leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL, serialization.NoEncryption())
enc = leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.BestAvailableEncryption(b"x"))
bundle = leaf.public_bytes(serialization.Encoding.PEM) + b"lixo entre blocos\n" + ec_trad + enc + \
    ca_key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
pd = os.path.join(T, "pem")
os.makedirs(pd, exist_ok=True)
out = run("pem", put("bundle.pem", bundle), pd).splitlines()
check(out == ["CERTIFICATE", "EC PRIVATE KEY", "ENCRYPTED PRIVATE KEY (cifrado)", "PUBLIC KEY"], f"rótulos PEM {out}")
check(open(os.path.join(pd, "pem0.der"), "rb").read() == leaf.public_bytes(DER), "PEM: DER do certificado")
check(open(os.path.join(pd, "pem1.der"), "rb").read() ==
      leaf_key.private_bytes(DER, serialization.PrivateFormat.TraditionalOpenSSL, serialization.NoEncryption()), "PEM: DER da chave EC")

# ---------------------------------------------------------------- nomes
acc_key = ec.generate_private_key(ec.SECP256R1())
acc = make("Conceição São João ★", acc_key, email="ana@exemplo.com.br")
out = run("name", put("acc.der", acc.public_bytes(DER)))
check("CN=Conceição São João ★" in out, "CN UTF-8")
check("O=Time mOS" in out and "EMAIL=ana@exemplo.com.br" in out, "O e e-mail")
check("ATTR 2.5.4.3=Conceição" in out and "ATTR 1.2.840.113549.1.9.1=ana@exemplo.com.br" in out, "OIDs pontilhados")
nb = [l for l in out.splitlines() if l.startswith("NB=")][0][3:]
want = acc.not_valid_before_utc.timestamp() - 978307200
check(abs(float(nb) - want) < 1, f"notBefore em CFAbsoluteTime ({nb} vs {want})")
out = run("name", p_leaf)
check("SAN=app.mos.test" in out, "SAN dNSName")

# ---------------------------------------------------------------- confiança
def trust(policy, host, at, system, roots, certs, anchors=()):
    a = ["trust", str(policy), host or "-", str(at), str(system), roots or "-", *certs]
    if anchors:
        a += ["--", *anchors]
    return run(*a).split(" ", 1)[0]


check(trust(1, "app.mos.test", -1, 0, None, [p_leaf, p_in], [p_ca]) == "0", "cadeia com âncora do app")
check(trust(1, "x.mos.test", -1, 0, None, [p_leaf, p_in], [p_ca]) == "0", "curinga *.mos.test")
check(trust(1, "outro.exemplo", -1, 0, None, [p_leaf, p_in], [p_ca]) == "11", "nome errado -> X509_E_NAME")
check(trust(0, None, -1, 0, None, [p_leaf, p_in], [p_ca]) == "0", "política básica sem nome")
check(trust(1, "app.mos.test", -1, 0, None, [p_leaf], [p_ca]) == "2", "sem intermediário -> emissor desconhecido")
later = int((NOW + datetime.timedelta(days=400)).timestamp())
check(trust(1, "app.mos.test", later, 0, None, [p_leaf, p_in], [p_ca]) == "5", "data depois da validade -> expirado")
before = int((NOW - datetime.timedelta(days=30)).timestamp())
check(trust(1, "app.mos.test", before, 0, None, [p_leaf, p_in], [p_ca]) == "6", "data antes da validade -> ainda não válido")
roots = put("roots.pem", ca.public_bytes(serialization.Encoding.PEM))
check(trust(1, "app.mos.test", -1, 1, roots, [p_leaf, p_in]) == "0", "raiz do sistema")
check(trust(1, "app.mos.test", -1, 0, roots, [p_leaf, p_in]) != "0", "só âncoras (sem as do sistema) recusa")
check(trust(0, None, -1, 0, None, [p_ca], [p_ca]) == "0", "a própria âncora")
other_key = rsa.generate_private_key(65537, 2048)
other = make("Outra Raiz", other_key, ca=True)
check(trust(1, "app.mos.test", -1, 0, None, [p_leaf, p_in], [put("other.der", other.public_bytes(DER))]) != "0", "âncora errada")
bad = bytearray(leaf.public_bytes(DER)); bad[-5] ^= 0x40
check(trust(1, "app.mos.test", -1, 0, None, [put("bad.der", bytes(bad)), p_in], [p_ca]) in ("7", "2", "1"), "assinatura adulterada")
# SecureTransport com SSLSetAllowsAnyRoot (securetransport.c, verify_chain): o topo da cadeia vira âncora e o resto
# continua conferido
check(trust(1, "app.mos.test", -1, 0, None, [p_leaf, p_in]) == "2", "sem âncora: emissor desconhecido")
check(trust(1, "app.mos.test", -1, 0, None, [p_leaf, p_in], [p_in]) == "0", "AllowsAnyRoot: topo da cadeia como âncora")
check(trust(1, "outro.exemplo", -1, 0, None, [p_leaf, p_in], [p_in]) == "11", "AllowsAnyRoot ainda confere o nome")
check(trust(1, "app.mos.test", later, 0, None, [p_leaf, p_in], [p_in]) == "5", "AllowsAnyRoot ainda confere a data")
# SSLSetAllowsExpiredCerts: confere de novo no maior notBefore da cadeia
nb_max = int(max(leaf.not_valid_before_utc, inter.not_valid_before_utc).timestamp()) + 1
check(trust(1, "app.mos.test", nb_max, 0, None, [p_leaf, p_in], [p_ca]) == "0", "AllowsExpiredCerts: instante em que todos valem")
cl_key = ec.generate_private_key(ec.SECP256R1())
cl = make("cliente.mos.test", cl_key, inter, in_key, san=["cliente.mos.test"], eku=[ExtendedKeyUsageOID.CLIENT_AUTH])
p_cl = put("cl.der", cl.public_bytes(DER))
check(trust(0, None, -1, 0, None, [p_cl, p_in], [p_ca]) == "0", "certificado de cliente na política básica")
check(trust(1, "cliente.mos.test", -1, 0, None, [p_cl, p_in], [p_ca]) == "12", "certificado de cliente no SSL de servidor -> uso de chave")

print(f"seccert: {ok}/{total} {'ok' if ok == total else 'FALHOU'}")
sys.exit(0 if ok == total else 1)
