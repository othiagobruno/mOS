"""Teste de host do núcleo CMS (userland/libsystem/cms_core.inc) contra o openssl e a biblioteca cryptography.
    python3 -I tools/cms-hosttest.py      -> "cms: N/N ok"
Decodifica mensagens que o `openssl cms -sign` gera (RSA e EC, com e sem atributos, anexada e destacada) e confere
conteúdo, certificados, resumo e a assinatura sobre o SET de atributos; monta um SignedData com o cms_build (assinatura
feita aqui pela cryptography) e confere que o `openssl cms -verify` aceita."""
import datetime, hashlib, os, subprocess, sys, tempfile
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, padding, rsa
from cryptography.x509.oid import NameOID

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = tempfile.mkdtemp(prefix="cms-")
BIN = os.path.join(T, "cmsh")
subprocess.run(["gcc", "-O1", "-w", "-I" + ROOT + "/userland/libsystem", ROOT + "/tools/cms_host.c", "-o", BIN], check=True)
ok = total = 0
def check(c, w):
    global ok, total
    total += 1
    if c: ok += 1
    else: print("FALHA:", w)
def run(*a):
    out = subprocess.run([BIN, *a], capture_output=True, text=True).stdout
    d = {}
    for l in out.splitlines():
        k, _, v = l.partition(" ")
        d.setdefault(k, []).append(v)
    return d
def mkcert(key, name):
    now = datetime.datetime.now(datetime.timezone.utc)
    n = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, name)])
    c = x509.CertificateBuilder().subject_name(n).issuer_name(n).public_key(key.public_key()).serial_number(0x1234abcd) \
        .not_valid_before(now - datetime.timedelta(days=1)).not_valid_after(now + datetime.timedelta(days=30)).sign(key, hashes.SHA256())
    return c
msg = b"conteudo assinado do mOS\n"
open(os.path.join(T, "msg"), "wb").write(msg)
for kind, key in (("rsa", rsa.generate_private_key(65537, 2048)), ("ec", ec.generate_private_key(ec.SECP256R1()))):
    cert = mkcert(key, "assinante " + kind)
    kp, cp = os.path.join(T, kind + ".key"), os.path.join(T, kind + ".pem")
    open(kp, "wb").write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    open(cp, "wb").write(cert.public_bytes(serialization.Encoding.PEM))
    der_cert = cert.public_bytes(serialization.Encoding.DER)
    open(os.path.join(T, kind + ".der"), "wb").write(der_cert)
    for variant, extra in (("attrs", []), ("noattr", ["-noattr"]), ("detached", [])):
        out = os.path.join(T, f"{kind}-{variant}.der")
        args = ["openssl", "cms", "-sign", "-in", os.path.join(T, "msg"), "-signer", cp, "-inkey", kp, "-outform", "DER", "-binary",
                "-md", "sha256", "-out", out] + extra + ([] if variant == "detached" else ["-nodetach"])
        subprocess.run(args, check=True, capture_output=True)
        d = run("parse", out)
        w = f"{kind}/{variant}"
        check(d.get("rc") == ["0"] and d.get("nsigners") == ["1"] and d.get("ncerts") == ["1"], w + ": decodifica 1 assinante e 1 certificado")
        check(bytes.fromhex(d["cert"][0]) == der_cert, w + ": certificado igual")
        check((variant == "detached") == (d["has_content"] == ["0"]), w + ": conteúdo anexado ou não")
        if variant != "detached":
            check(bytes.fromhex(d["content"][0]) == msg, w + ": conteúdo igual")
        check(d["digest_len"] == ["32"], w + ": SHA-256")
        check(int(d["serial"][0], 16) == 0x1234abcd and d["issuer"][0] != "", w + ": emissor e série do assinante")
        sig = bytes.fromhex(d["sig"][0])
        signed = bytes.fromhex(d["signed_set"][0]) if d["signed_set"][0] else msg
        if variant != "noattr":
            check(bytes.fromhex(d["msg_digest"][0]) == hashlib.sha256(msg).digest(), w + ": messageDigest = SHA-256 do conteúdo")
            check(len(d["time"][0]) > 0, w + ": signingTime")
            agora = datetime.datetime.now(datetime.timezone.utc).timestamp() - 978307200
            check(abs(float(d["abs_time"][0]) - agora) < 120, w + f": signingTime em CFAbsoluteTime ({d['abs_time'][0]} vs {agora:.0f})")
        try:
            if kind == "rsa": key.public_key().verify(sig, signed, padding.PKCS1v15(), hashes.SHA256())
            else: key.public_key().verify(sig, signed, ec.ECDSA(hashes.SHA256()))
            check(True, "")
        except Exception as e:
            check(False, w + f": assinatura sobre o SET ({e})")
    # montagem: atributos -> assinatura (cryptography) -> SignedData -> openssl cms -verify
    for detached in (0, 1):
        utc = datetime.datetime.now(datetime.timezone.utc).strftime("%y%m%d%H%M%SZ")
        st = run("attrs", "2a864886f70d010701", hashlib.sha256(msg).hexdigest(), utc)["set"][0]
        sb = bytes.fromhex(st)
        sig = key.sign(sb, padding.PKCS1v15(), hashes.SHA256()) if kind == "rsa" else key.sign(sb, ec.ECDSA(hashes.SHA256()))
        iss = cert.issuer.public_bytes()
        ser = (0x1234abcd).to_bytes(4, "big").hex()
        out = os.path.join(T, f"built-{kind}-{detached}.der")
        r = subprocess.run([BIN, "build", msg.hex(), str(detached), st, sig.hex(), os.path.join(T, kind + ".der"), iss.hex(), ser, kind, out])
        check(r.returncode == 0, f"build {kind} {detached}")
        v = ["openssl", "cms", "-verify", "-inform", "DER", "-in", out, "-noverify", "-binary", "-out", os.path.join(T, "vout")]
        if detached: v += ["-content", os.path.join(T, "msg")]
        rv = subprocess.run(v, capture_output=True, text=True)
        check(rv.returncode == 0 and open(os.path.join(T, "vout"), "rb").read() == msg,
              f"openssl aceita o SignedData montado ({kind}, {'destacado' if detached else 'anexado'}): {rv.stderr.strip()[:120]}")
        d = run("parse", out)
        check(d.get("rc") == ["0"] and bytes.fromhex(d["signed_set"][0]) == sb, f"montado {kind} {detached} relido igual")
print(f"cms: {ok}/{total} {'ok' if ok == total else 'FALHOU'}")
sys.exit(ok != total)
