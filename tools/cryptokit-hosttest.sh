#!/bin/sh
# Teste de host do CryptoKit (userland/frameworks/CryptoKit/CryptoKit.swift): o mesmo fonte compilado para Linux com a
# toolchain oficial (Foundation do Linux), ligado ao núcleo C do libSystem (cryptokit.c, crypto.c, pk.c, ed25519.c,
# CommonCrypto). Confere vetores das RFCs (SHA, HMAC 4231, HKDF 5869, KeyWrap 3394, Ed25519 8032, X25519 7748) e a
# interoperação de P-256/P-384/P-521 com a biblioteca cryptography do Python, nos dois sentidos.
#   SWIFTC=.../swift-6.4.2/usr/bin/swiftc sh tools/cryptokit-hosttest.sh   -> "cryptokit: N/N" e "python confere: 3/3"
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SWIFTC=${SWIFTC:-$ROOT/.tools/swift-6.4.2/usr/bin/swiftc}
L=$ROOT/userland/libsystem
ZD=$(dirname "$(readlink -f "$(which zig)")")
T=$(mktemp -d "${TMPDIR:-/tmp}/ck-host.XXXXXX")
trap 'rm -rf "$T"' EXIT
for f in "$L/cryptokit.c" "$ROOT/kernel/net/crypto.c" "$ROOT/kernel/net/pk.c" "$ROOT/kernel/net/ed25519.c"; do
    gcc -O1 -w -fPIC -DMOS_HOST_TEST -I"$L/kshim" -I"$L" -I"$ROOT/kernel/include" -I"$ROOT/tools" -c "$f" -o "$T/$(basename "$f" .c).o"
done
for f in commoncrypto commoncrypto2; do
    zig cc -target x86_64-freestanding-none -O2 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -fPIC -mno-red-zone -w \
        -I"$L/kshim" -I"$L/fsinc" -I"$ROOT/kernel/include" -I"$L" -isystem "$ZD/lib/include" -c "$L/$f.c" -o "$T/$f.o"
done
printf 'unsigned long long timer_ticks(void){return 1;}\nunsigned long long mach_absolute_time(void){return 1;}\nint backtrace_from_fp(void*a,void**b,int c){(void)a;(void)b;(void)c;return 0;}\n' > "$T/stubs.c"
gcc -O1 -c "$T/stubs.c" -o "$T/stubs.o"
mkdir -p "$T/io"
python3 -I - "$T/io" <<'PY'
import sys, pickle
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization, hashes
d = sys.argv[1]
k = ec.generate_private_key(ec.SECP256R1())
open(d + '/p256.pem', 'wb').write(k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
spki = k.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
open(d + '/p256.spki', 'w').write(spki.hex())
open(d + '/p256.pysig', 'w').write(k.sign(b"mensagem do mOS", ec.ECDSA(hashes.SHA256())).hex())
k3 = ec.generate_private_key(ec.SECP384R1())
open(d + '/p384.pem', 'wb').write(k3.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL, serialization.NoEncryption()))
a, b = ec.generate_private_key(ec.SECP521R1()), ec.generate_private_key(ec.SECP521R1())
open(d + '/p521.pub', 'w').write(b.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint).hex())
open(d + '/p521.priv', 'w').write(a.private_numbers().private_value.to_bytes(66, 'big').hex())
open(d + '/p521.expect', 'w').write(a.exchange(ec.ECDH(), b.public_key()).hex())
pickle.dump((spki, k3.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)), open(d + '/pubs.pk', 'wb'))
PY
"$SWIFTC" -O -module-name CryptoKit "$ROOT/tools/cryptokit-host/main.swift" "$ROOT/userland/frameworks/CryptoKit/CryptoKit.swift" \
    "$T"/cryptokit.o "$T"/crypto.o "$T"/pk.o "$T"/ed25519.o "$T"/commoncrypto.o "$T"/commoncrypto2.o "$T"/stubs.o -o "$T/t"
"$T/t" "$T/io"
python3 -I - "$T/io" <<'PY'
import sys, pickle
from cryptography.hazmat.primitives.asymmetric import ec, utils
from cryptography.hazmat.primitives import hashes, serialization
d = sys.argv[1]; m = b"mensagem do mOS"; ok = 0
p256, p384 = pickle.load(open(d + '/pubs.pk', 'rb'))
try: serialization.load_der_public_key(p256).verify(bytes.fromhex(open(d + '/p256.sig').read()), m, ec.ECDSA(hashes.SHA256())); ok += 1
except Exception as e: print("FALHA P-256:", e)
raw = bytes.fromhex(open(d + '/p384.sig').read())
try:
    serialization.load_der_public_key(p384).verify(utils.encode_dss_signature(int.from_bytes(raw[:48], 'big'), int.from_bytes(raw[48:], 'big')), m, ec.ECDSA(hashes.SHA384())); ok += 1
except Exception as e: print("FALHA P-384:", e)
ok += open(d + '/p521.ss').read().strip() == open(d + '/p521.expect').read().strip()
print(f"python confere: {ok}/3"); sys.exit(ok != 3)
PY
