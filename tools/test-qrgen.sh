#!/bin/bash
# Teste de host do qrgen.c (CIQRCodeGenerator) contra a biblioteca qrcode do Python (referência), módulo a módulo:
# níveis L/M/Q/H, versões 1..40, as 8 máscaras forçadas e a versão automática. Precisa de um python com qrcode
# (QR_PY, padrão /tmp/qrref/venv/bin/python). Leve; rode pelo mos-pesado. Saída: "qrgen: N/N ok"
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=${QR_TMP:-/tmp/qr-test}; mkdir -p "$T"; PY=${QR_PY:-/tmp/qrref/venv/bin/python}
gcc -O1 -Wall -I"$R/userland/frameworks/CoreImage" "$R/tools/test-qrgen.c" "$R/userland/frameworks/CoreImage/qrgen.c" -o "$T/q" || exit 1
"$PY" -I - "$T/q" <<'P'
import sys, subprocess, random, qrcode
from qrcode.util import QRData, MODE_8BIT_BYTE
q = sys.argv[1]; random.seed(7)
LV = [qrcode.constants.ERROR_CORRECT_L, qrcode.constants.ERROR_CORRECT_M, qrcode.constants.ERROR_CORRECT_Q, qrcode.constants.ERROR_CORRECT_H]
def ref(data, lvl, ver, mask):
    c = qrcode.QRCode(version=ver, error_correction=LV[lvl], border=0, mask_pattern=mask)
    c.add_data(QRData(data, mode=MODE_8BIT_BYTE)); c.make(fit=ver is None)
    return c.version, [''.join('1' if b else '0' for b in row) for row in c.get_matrix()]
def ours(data, lvl, ver, mask):
    o = subprocess.run([q, str(lvl), str(ver or 0), str(mask), data.hex()], capture_output=True, text=True).stdout.split()
    return int(o[0]), o[2:]
ok = n = 0
cases = []
for ver in list(range(1, 41)):
    for lvl in range(4):
        mask = (ver * 3 + lvl) % 8
        cases.append((bytes(random.randrange(256) for _ in range(random.randrange(1, 12))), lvl, ver, mask))
for txt in [b'hello', b'https://example.com/mOS', b'A' * 200, bytes(range(256)) * 3]:
    for lvl in range(4):
        for mask in range(8): cases.append((txt, lvl, None, mask))
for d, lvl, ver, mask in cases:
    n += 1
    rv, rm = ref(d, lvl, ver, mask); ov, om = ours(d, lvl, ver, mask)
    if rv == ov and rm == om: ok += 1
    else: print('DIFERE: nível', lvl, 'versão', ver, rv, ov, 'máscara', mask, 'len', len(d))
print('qrgen: %d/%d ok' % (ok, n)); sys.exit(ok != n)
P
